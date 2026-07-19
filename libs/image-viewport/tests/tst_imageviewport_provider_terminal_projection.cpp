// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_provider_test_support.h"

namespace {
void emitTerminal(CountingProviderSession* session, ImageSequenceProviderRequestToken token,
    int terminalKind, int unsupportedCause, const QString& diagnostic)
{
    if (terminalKind == 0) {
        emitProviderFailed(session, token, diagnostic);
    } else if (terminalKind == 1) {
        emitProviderUnsupported(session, token,
            static_cast<ImageSequenceProviderUnsupportedCause>(unsupportedCause), diagnostic);
    } else {
        emitProviderCancelled(session, token, diagnostic);
    }
    drainQueuedProviderResults();
}

}
class ImageViewportProviderTerminalProjectionTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderTerminalProjectionTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void secondaryProviderMetadataFailureReportsAggregateProviderFailure();
    void providerMetadataUnsupportedCauseProjectsRequestReason_data();
    void providerMetadataUnsupportedCauseProjectsRequestReason();
    void targetSpreadTerminalProjectionPrefersErrorOverUnsupported_data();
    void targetSpreadTerminalProjectionPrefersErrorOverUnsupported();
    void terminalSiblingMalformedMetadataBecomesError_data();
    void terminalSiblingMalformedMetadataBecomesError();
    void terminalSiblingMalformedFrameBecomesError_data();
    void terminalSiblingMalformedFrameBecomesError();
    void primaryMalformedFrameReplacesSecondaryErrorDiagnostic();
    void terminalSiblingFrameSuccessPreservesTerminalSpread_data();
    void terminalSiblingFrameSuccessPreservesTerminalSpread();
    void terminalSiblingEndOfSequenceBecomesError_data();
    void terminalSiblingEndOfSequenceBecomesError();
    void secondaryTerminalFailureSealsSpreadAgainstLatePrimaryReady();
    void primaryTerminalFailureSealsSpreadAgainstLateSecondaryReady();
    void clearAndReplacementEscapeSealedTargetSpread();
    void secondaryProviderFrameTerminalResultsProjectThroughSpread_data();
    void secondaryProviderFrameTerminalResultsProjectThroughSpread();
    void secondaryProviderPlaybackTerminalResultsProjectThroughSpread_data();
    void secondaryProviderPlaybackTerminalResultsProjectThroughSpread();
    void invalidUnsupportedCauseIsProtocolViolation_data();
    void invalidUnsupportedCauseIsProtocolViolation();
    void providerInvalidTokenBeforeMetadataIsProtocolViolation();
    void providerInvalidTokenAfterMetadataIsProtocolViolation();
};

void ImageViewportProviderTerminalProjectionTest::
    secondaryProviderMetadataFailureReportsAggregateProviderFailure()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromProvider(&adapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderFailed(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("secondary metadata service unavailable"));
    drainQueuedProviderResults();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), -1);
    verifyUntrustedProviderDiagnostic(
        item, QStringLiteral("secondary metadata service unavailable"));
}

void ImageViewportProviderTerminalProjectionTest::
    providerMetadataUnsupportedCauseProjectsRequestReason_data()
{
    QTest::addColumn<bool>("secondaryRole");
    QTest::addColumn<int>("unsupportedCause");
    QTest::addColumn<QString>("diagnostic");
    QTest::addColumn<QString>("expectedReason");

    QTest::newRow("primary-unsupported-request")
        << false << static_cast<int>(ImageSequenceProviderUnsupportedCause::UnsupportedRequest)
        << QStringLiteral("metadata operation unsupported") << QStringLiteral("UnsupportedRequest");
    QTest::newRow("primary-payload-rejection")
        << false << static_cast<int>(ImageSequenceProviderUnsupportedCause::PayloadRejection)
        << QStringLiteral("metadata payload rejected") << QStringLiteral("PayloadRejection");
    QTest::newRow("secondary-unsupported-request")
        << true << static_cast<int>(ImageSequenceProviderUnsupportedCause::UnsupportedRequest)
        << QStringLiteral("secondary metadata operation unsupported")
        << QStringLiteral("UnsupportedRequest");
    QTest::newRow("secondary-payload-rejection")
        << true << static_cast<int>(ImageSequenceProviderUnsupportedCause::PayloadRejection)
        << QStringLiteral("secondary metadata payload rejected")
        << QStringLiteral("PayloadRejection");
}

void ImageViewportProviderTerminalProjectionTest::
    providerMetadataUnsupportedCauseProjectsRequestReason()
{
    QFETCH(bool, secondaryRole);
    QFETCH(int, unsupportedCause);
    QFETCH(QString, diagnostic);
    QFETCH(QString, expectedReason);

    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> primaryResult;
    if (secondaryRole) {
        QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
        primaryImage.fill(Qt::transparent);
        ImageFrame primaryFrame(primaryImage);
        primaryResult.reset(factory.fromFrame(&primaryFrame));
        QVERIFY(primaryResult->sequence());
    }

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    if (secondaryRole) {
        QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                                primaryResult->sequence(), result->sequence()),
                         PresentationTargetTransitionPolicy {})
                     .outcome(),
            ImageViewportCommandOutcome::Accepted);
    } else {
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});
    }
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    emitProviderUnsupported(sessionFactory->lastSession(), metadataToken,
        static_cast<ImageSequenceProviderUnsupportedCause>(unsupportedCause), diagnostic);
    drainQueuedProviderResults();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", expectedReason.toUtf8().constData()));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    if (secondaryRole) {
        QCOMPARE(primaryRequestedFrame(item), 0);
        QCOMPARE(secondaryRequestedFrame(item), -1);
    } else {
        QCOMPARE(primaryRequestedFrame(item), -1);
        QCOMPARE(primaryRequestedPosition(item), -1);
    }
    verifyUntrustedProviderDiagnostic(item, diagnostic);
}

void ImageViewportProviderTerminalProjectionTest::
    targetSpreadTerminalProjectionPrefersErrorOverUnsupported_data()
{
    QTest::addColumn<int>("firstRole");
    QTest::addColumn<int>("firstKind");
    QTest::addColumn<int>("firstUnsupportedCause");
    QTest::addColumn<QString>("firstDiagnostic");
    QTest::addColumn<int>("secondRole");
    QTest::addColumn<int>("secondKind");
    QTest::addColumn<int>("secondUnsupportedCause");
    QTest::addColumn<QString>("secondDiagnostic");
    QTest::addColumn<QString>("expectedStatus");
    QTest::addColumn<QString>("expectedReason");
    QTest::addColumn<QString>("expectedDiagnostic");

    const int primary = static_cast<int>(ImageViewportPageRole::Primary);
    const int secondary = static_cast<int>(ImageViewportPageRole::Secondary);
    const int unsupportedRequest
        = static_cast<int>(ImageSequenceProviderUnsupportedCause::UnsupportedRequest);
    const int payloadRejection
        = static_cast<int>(ImageSequenceProviderUnsupportedCause::PayloadRejection);

    QTest::newRow("primary-error-then-secondary-unsupported")
        << primary << 0 << payloadRejection << QStringLiteral("primary frame failed") << secondary
        << 1 << payloadRejection << QStringLiteral("secondary payload unsupported")
        << QStringLiteral("Error") << QStringLiteral("ProviderFailure")
        << QStringLiteral("primary frame failed");
    QTest::newRow("secondary-error-then-primary-unsupported")
        << secondary << 0 << payloadRejection << QStringLiteral("secondary frame failed") << primary
        << 1 << unsupportedRequest << QStringLiteral("primary operation unsupported")
        << QStringLiteral("Error") << QStringLiteral("ProviderFailure")
        << QStringLiteral("secondary frame failed");
    QTest::newRow("primary-unsupported-then-secondary-unsupported")
        << primary << 1 << unsupportedRequest << QStringLiteral("primary operation unsupported")
        << secondary << 1 << payloadRejection << QStringLiteral("secondary payload unsupported")
        << QStringLiteral("Unsupported") << QStringLiteral("UnsupportedRequest")
        << QStringLiteral("primary operation unsupported");
    QTest::newRow("secondary-unsupported-then-primary-unsupported")
        << secondary << 1 << payloadRejection << QStringLiteral("secondary payload unsupported")
        << primary << 1 << unsupportedRequest << QStringLiteral("primary operation unsupported")
        << QStringLiteral("Unsupported") << QStringLiteral("UnsupportedRequest")
        << QStringLiteral("primary operation unsupported");
}

void ImageViewportProviderTerminalProjectionTest::
    targetSpreadTerminalProjectionPrefersErrorOverUnsupported()
{
    QFETCH(int, firstRole);
    QFETCH(int, firstKind);
    QFETCH(int, firstUnsupportedCause);
    QFETCH(QString, firstDiagnostic);
    QFETCH(int, secondRole);
    QFETCH(int, secondKind);
    QFETCH(int, secondUnsupportedCause);
    QFETCH(QString, secondDiagnostic);
    QFETCH(QString, expectedStatus);
    QFETCH(QString, expectedReason);
    QFETCH(QString, expectedDiagnostic);

    ImageSequenceFactory factory;
    const auto primarySessionCount = std::make_shared<int>(0);
    const auto primaryMetadataRequestCount = std::make_shared<int>(0);
    const auto primaryFrameRequestCount = std::make_shared<int>(0);
    const auto primaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto primaryCloseCount = std::make_shared<int>(0);
    auto primarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        primarySessionCount, primaryMetadataRequestCount, primaryFrameRequestCount,
        primaryLastRequestedFrame, primaryCloseCount);
    CountingProviderAdapter primaryAdapter(primarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromProvider(&primaryAdapter));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    CountingProviderSession* primarySession = primarySessionFactory->lastSession();
    CountingProviderSession* secondarySession = secondarySessionFactory->lastSession();
    QVERIFY(primarySession);
    QVERIFY(secondarySession);
    emitProviderMetadataReady(primarySession, primarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    emitProviderMetadataReady(secondarySession, secondarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(20.0, 10.0), { 100, 250 }));
    drainQueuedProviderResults();
    QCOMPARE(*primaryFrameRequestCount, 1);
    QCOMPARE(*secondaryFrameRequestCount, 1);

    const auto emitForRole
        = [&](int role, int kind, int unsupportedCause, const QString& diagnostic) {
              CountingProviderSession* session
                  = role == static_cast<int>(ImageViewportPageRole::Primary) ? primarySession
                                                                             : secondarySession;
              emitTerminal(session, session->lastFrameToken(), kind, unsupportedCause, diagnostic);
          };

    emitForRole(firstRole, firstKind, firstUnsupportedCause, firstDiagnostic);
    emitForRole(secondRole, secondKind, secondUnsupportedCause, secondDiagnostic);

    QCOMPARE(requestStatusValue(item),
        enumValue(metaObject, "RequestStatus", expectedStatus.toUtf8().constData()));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", expectedReason.toUtf8().constData()));
    verifyUntrustedProviderDiagnostic(item, expectedDiagnostic);
}

void ImageViewportProviderTerminalProjectionTest::
    terminalSiblingMalformedMetadataBecomesError_data()
{
    QTest::addColumn<bool>("unsupportedPrimary");
    QTest::newRow("primary-unsupported-secondary-malformed") << true;
    QTest::newRow("secondary-unsupported-primary-malformed") << false;
}

void ImageViewportProviderTerminalProjectionTest::terminalSiblingMalformedMetadataBecomesError()
{
    QFETCH(bool, unsupportedPrimary);

    ImageSequenceFactory factory;
    const auto primarySessionCount = std::make_shared<int>(0);
    const auto primaryMetadataRequestCount = std::make_shared<int>(0);
    const auto primaryFrameRequestCount = std::make_shared<int>(0);
    const auto primaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto primaryCloseCount = std::make_shared<int>(0);
    auto primaryFactory = std::make_shared<CountingProviderSessionFactory>(primarySessionCount,
        primaryMetadataRequestCount, primaryFrameRequestCount, primaryLastRequestedFrame,
        primaryCloseCount);
    CountingProviderAdapter primaryAdapter(primaryFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromProvider(&primaryAdapter));

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondaryFactory = std::make_shared<CountingProviderSessionFactory>(secondarySessionCount,
        secondaryMetadataRequestCount, secondaryFrameRequestCount, secondaryLastRequestedFrame,
        secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondaryFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    auto* unsupportedSession
        = unsupportedPrimary ? primaryFactory->lastSession() : secondaryFactory->lastSession();
    auto* malformedSession
        = unsupportedPrimary ? secondaryFactory->lastSession() : primaryFactory->lastSession();
    emitProviderUnsupported(unsupportedSession, unsupportedSession->lastMetadataToken(),
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest,
        QStringLiteral("metadata unsupported"));
    drainQueuedProviderResults();
    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Unsupported);

    emitProviderMetadataReady(
        malformedSession, malformedSession->lastMetadataToken(), ImageSequenceProviderMetadata {});
    drainQueuedProviderResults();

    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(item.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QVERIFY(!viewportErrorString(item).contains(QStringLiteral("metadata unsupported")));
}

void ImageViewportProviderTerminalProjectionTest::terminalSiblingMalformedFrameBecomesError_data()
{
    QTest::addColumn<bool>("unsupportedPrimary");
    QTest::newRow("primary-unsupported-secondary-malformed") << true;
    QTest::newRow("secondary-unsupported-primary-malformed") << false;
}

void ImageViewportProviderTerminalProjectionTest::terminalSiblingMalformedFrameBecomesError()
{
    QFETCH(bool, unsupportedPrimary);

    ImageSequenceFactory factory;
    const auto primarySessionCount = std::make_shared<int>(0);
    const auto primaryMetadataRequestCount = std::make_shared<int>(0);
    const auto primaryFrameRequestCount = std::make_shared<int>(0);
    const auto primaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto primaryCloseCount = std::make_shared<int>(0);
    auto primaryFactory = std::make_shared<CountingProviderSessionFactory>(primarySessionCount,
        primaryMetadataRequestCount, primaryFrameRequestCount, primaryLastRequestedFrame,
        primaryCloseCount);
    CountingProviderAdapter primaryAdapter(primaryFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromProvider(&primaryAdapter));

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondaryFactory = std::make_shared<CountingProviderSessionFactory>(secondarySessionCount,
        secondaryMetadataRequestCount, secondaryFrameRequestCount, secondaryLastRequestedFrame,
        secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondaryFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    auto* primarySession = primaryFactory->lastSession();
    auto* secondarySession = secondaryFactory->lastSession();
    emitProviderMetadataReady(primarySession, primarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    emitProviderMetadataReady(secondarySession, secondarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    auto* unsupportedSession = unsupportedPrimary ? primarySession : secondarySession;
    auto* malformedSession = unsupportedPrimary ? secondarySession : primarySession;
    emitProviderUnsupported(unsupportedSession, unsupportedSession->lastFrameToken(),
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest,
        QStringLiteral("frame unsupported"));
    drainQueuedProviderResults();
    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Unsupported);

    QImage malformedImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    ImageFrame malformedFrame(malformedImage);
    emitProviderFrameReady(malformedSession, malformedSession->lastFrameToken(), &malformedFrame);
    drainQueuedProviderResults();

    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(item.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QVERIFY(!viewportErrorString(item).contains(QStringLiteral("frame unsupported")));
}

void ImageViewportProviderTerminalProjectionTest::
    primaryMalformedFrameReplacesSecondaryErrorDiagnostic()
{
    ImageSequenceFactory factory;
    const auto makeProvider = [&factory](
                                  std::shared_ptr<CountingProviderSessionFactory>& outFactory,
                                  std::unique_ptr<CountingProviderAdapter>& outAdapter) {
        outFactory = std::make_shared<CountingProviderSessionFactory>(std::make_shared<int>(0),
            std::make_shared<int>(0), std::make_shared<int>(0), std::make_shared<int>(-1),
            std::make_shared<int>(0));
        outAdapter = std::make_unique<CountingProviderAdapter>(outFactory);
        return QScopedPointer<ImageSequenceFactoryResult>(factory.fromProvider(outAdapter.get()));
    };
    std::shared_ptr<CountingProviderSessionFactory> primaryFactory;
    std::shared_ptr<CountingProviderSessionFactory> secondaryFactory;
    std::unique_ptr<CountingProviderAdapter> primaryAdapter;
    std::unique_ptr<CountingProviderAdapter> secondaryAdapter;
    auto primaryResult = makeProvider(primaryFactory, primaryAdapter);
    auto secondaryResult = makeProvider(secondaryFactory, secondaryAdapter);
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    auto* primarySession = primaryFactory->lastSession();
    auto* secondarySession = secondaryFactory->lastSession();
    emitProviderMetadataReady(primarySession, primarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    emitProviderMetadataReady(secondarySession, secondarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QImage malformedImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    ImageFrame malformedFrame(malformedImage);
    emitProviderFrameReady(secondarySession, secondarySession->lastFrameToken(), &malformedFrame);
    drainQueuedProviderResults();
    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Error);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("logical size mismatch")));

    emitProviderFrameReady(primarySession, primarySession->lastFrameToken(), nullptr);
    drainQueuedProviderResults();

    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(item.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("payload is invalid")));
}

void ImageViewportProviderTerminalProjectionTest::
    terminalSiblingFrameSuccessPreservesTerminalSpread_data()
{
    QTest::addColumn<bool>("unsupportedPrimary");
    QTest::newRow("primary-unsupported-secondary-success") << true;
    QTest::newRow("secondary-unsupported-primary-success") << false;
}

void ImageViewportProviderTerminalProjectionTest::
    terminalSiblingFrameSuccessPreservesTerminalSpread()
{
    QFETCH(bool, unsupportedPrimary);

    ImageSequenceFactory factory;
    const auto makeProvider = [&factory](
                                  std::shared_ptr<CountingProviderSessionFactory>& outFactory,
                                  std::unique_ptr<CountingProviderAdapter>& outAdapter) {
        outFactory = std::make_shared<CountingProviderSessionFactory>(std::make_shared<int>(0),
            std::make_shared<int>(0), std::make_shared<int>(0), std::make_shared<int>(-1),
            std::make_shared<int>(0));
        outAdapter = std::make_unique<CountingProviderAdapter>(outFactory);
        return QScopedPointer<ImageSequenceFactoryResult>(factory.fromProvider(outAdapter.get()));
    };
    std::shared_ptr<CountingProviderSessionFactory> primaryFactory;
    std::shared_ptr<CountingProviderSessionFactory> secondaryFactory;
    std::unique_ptr<CountingProviderAdapter> primaryAdapter;
    std::unique_ptr<CountingProviderAdapter> secondaryAdapter;
    auto primaryResult = makeProvider(primaryFactory, primaryAdapter);
    auto secondaryResult = makeProvider(secondaryFactory, secondaryAdapter);

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    auto* primarySession = primaryFactory->lastSession();
    auto* secondarySession = secondaryFactory->lastSession();
    emitProviderMetadataReady(primarySession, primarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    emitProviderMetadataReady(secondarySession, secondarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    auto* unsupportedSession = unsupportedPrimary ? primarySession : secondarySession;
    auto* successfulSession = unsupportedPrimary ? secondarySession : primarySession;
    emitProviderUnsupported(unsupportedSession, unsupportedSession->lastFrameToken(),
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest,
        QStringLiteral("frame unsupported"));
    drainQueuedProviderResults();
    const QString terminalDiagnostic = viewportErrorString(item);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    ImageFrame frame(image);
    emitProviderFrameReady(successfulSession, successfulSession->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Unsupported);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Empty);
    QCOMPARE(viewportErrorString(item), terminalDiagnostic);
    QVERIFY(!hasPendingRenderCommitForTest(item));
}

void ImageViewportProviderTerminalProjectionTest::terminalSiblingEndOfSequenceBecomesError_data()
{
    QTest::addColumn<bool>("metadataToken");
    QTest::addColumn<bool>("unsupportedPrimary");
    QTest::newRow("metadata-primary-unsupported-secondary-eos") << true << true;
    QTest::newRow("metadata-secondary-unsupported-primary-eos") << true << false;
    QTest::newRow("frame-primary-unsupported-secondary-eos") << false << true;
    QTest::newRow("frame-secondary-unsupported-primary-eos") << false << false;
}

void ImageViewportProviderTerminalProjectionTest::terminalSiblingEndOfSequenceBecomesError()
{
    QFETCH(bool, metadataToken);
    QFETCH(bool, unsupportedPrimary);

    ImageSequenceFactory factory;
    const auto makeProvider = [&factory](
                                  std::shared_ptr<CountingProviderSessionFactory>& outFactory,
                                  std::unique_ptr<CountingProviderAdapter>& outAdapter) {
        outFactory = std::make_shared<CountingProviderSessionFactory>(std::make_shared<int>(0),
            std::make_shared<int>(0), std::make_shared<int>(0), std::make_shared<int>(-1),
            std::make_shared<int>(0));
        outAdapter = std::make_unique<CountingProviderAdapter>(outFactory);
        return QScopedPointer<ImageSequenceFactoryResult>(factory.fromProvider(outAdapter.get()));
    };
    std::shared_ptr<CountingProviderSessionFactory> primaryFactory;
    std::shared_ptr<CountingProviderSessionFactory> secondaryFactory;
    std::unique_ptr<CountingProviderAdapter> primaryAdapter;
    std::unique_ptr<CountingProviderAdapter> secondaryAdapter;
    auto primaryResult = makeProvider(primaryFactory, primaryAdapter);
    auto secondaryResult = makeProvider(secondaryFactory, secondaryAdapter);

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    auto* primarySession = primaryFactory->lastSession();
    auto* secondarySession = secondaryFactory->lastSession();
    if (!metadataToken) {
        emitProviderMetadataReady(primarySession, primarySession->lastMetadataToken(),
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        emitProviderMetadataReady(secondarySession, secondarySession->lastMetadataToken(),
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        drainQueuedProviderResults();
    }

    auto* unsupportedSession = unsupportedPrimary ? primarySession : secondarySession;
    auto* eosSession = unsupportedPrimary ? secondarySession : primarySession;
    const auto tokenFor = [metadataToken](CountingProviderSession* session) {
        return metadataToken ? session->lastMetadataToken() : session->lastFrameToken();
    };
    emitProviderUnsupported(unsupportedSession, tokenFor(unsupportedSession),
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest,
        QStringLiteral("operation unsupported"));
    drainQueuedProviderResults();
    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Unsupported);

    emitProviderEndOfSequence(eosSession, tokenFor(eosSession));
    drainQueuedProviderResults();

    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(item.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider protocol violation")));
    const auto observations = internalObservationsForTest(item);
    QVERIFY(!observations.isEmpty());
    const InternalObservationForTest observation = observations.constLast();
    QCOMPARE(observation.subsystem, InternalObservationSubsystemForTest::Engine);
    QCOMPARE(observation.category, InternalObservationCategoryForTest::AdmissionFailure);
    QCOMPARE(observation.cause, InternalObservationCauseForTest::ProviderProtocolEventKindMismatch);
    QVERIFY(observation.identity.roleValid);
    QCOMPARE(observation.identity.role,
        unsupportedPrimary ? ImageViewportPageRole::Secondary : ImageViewportPageRole::Primary);
    QVERIFY(observation.identity.generation > 0);
    QVERIFY(observation.identity.sessionSerial > 0);
    QVERIFY(observation.identity.requestId > 0);
    QCOMPARE(
        observation.identity.providerToken, providerRequestTokenValueForTest(tokenFor(eosSession)));
}

void ImageViewportProviderTerminalProjectionTest::
    secondaryTerminalFailureSealsSpreadAgainstLatePrimaryReady()
{
    ImageSequenceFactory factory;
    const auto primarySessionCount = std::make_shared<int>(0);
    const auto primaryMetadataRequestCount = std::make_shared<int>(0);
    const auto primaryFrameRequestCount = std::make_shared<int>(0);
    const auto primaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto primaryCloseCount = std::make_shared<int>(0);
    auto primarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        primarySessionCount, primaryMetadataRequestCount, primaryFrameRequestCount,
        primaryLastRequestedFrame, primaryCloseCount);
    CountingProviderAdapter primaryAdapter(primarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromProvider(&primaryAdapter));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    CountingProviderSession* primarySession = primarySessionFactory->lastSession();
    CountingProviderSession* secondarySession = secondarySessionFactory->lastSession();
    QVERIFY(primarySession);
    QVERIFY(secondarySession);
    emitProviderMetadataReady(primarySession, primarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    emitProviderMetadataReady(secondarySession, secondarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(20.0, 10.0), { 100, 250 }));
    drainQueuedProviderResults();

    emitProviderFailed(secondarySession, secondarySession->lastFrameToken(),
        QStringLiteral("secondary frame failed"));
    drainQueuedProviderResults();

    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    emitTimedProviderFrameReady(primarySession, &primaryFrame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    verifyUntrustedProviderDiagnostic(item, QStringLiteral("secondary frame failed"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
}

void ImageViewportProviderTerminalProjectionTest::
    primaryTerminalFailureSealsSpreadAgainstLateSecondaryReady()
{
    ImageSequenceFactory factory;
    const auto primarySessionCount = std::make_shared<int>(0);
    const auto primaryMetadataRequestCount = std::make_shared<int>(0);
    const auto primaryFrameRequestCount = std::make_shared<int>(0);
    const auto primaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto primaryCloseCount = std::make_shared<int>(0);
    auto primarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        primarySessionCount, primaryMetadataRequestCount, primaryFrameRequestCount,
        primaryLastRequestedFrame, primaryCloseCount);
    CountingProviderAdapter primaryAdapter(primarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromProvider(&primaryAdapter));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    CountingProviderSession* primarySession = primarySessionFactory->lastSession();
    CountingProviderSession* secondarySession = secondarySessionFactory->lastSession();
    QVERIFY(primarySession);
    QVERIFY(secondarySession);
    emitProviderMetadataReady(primarySession, primarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    emitProviderMetadataReady(secondarySession, secondarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(20.0, 10.0), { 100, 250 }));
    drainQueuedProviderResults();

    emitProviderFailed(
        primarySession, primarySession->lastFrameToken(), QStringLiteral("primary frame failed"));
    drainQueuedProviderResults();

    QImage secondaryImage(20, 10, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    emitTimedProviderFrameReady(secondarySession, &secondaryFrame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    verifyUntrustedProviderDiagnostic(item, QStringLiteral("primary frame failed"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
}

void ImageViewportProviderTerminalProjectionTest::clearAndReplacementEscapeSealedTargetSpread()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderFailed(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata generation failed"));
    drainQueuedProviderResults();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Unsupported);

    QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(viewportErrorString(item), QString());

    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    QCOMPARE(*sessionCount, 2);
    QCOMPARE(*metadataRequestCount, 2);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalProjectionTest::
    secondaryProviderFrameTerminalResultsProjectThroughSpread_data()
{
    QTest::addColumn<int>("terminalKind");
    QTest::addColumn<int>("unsupportedCause");
    QTest::addColumn<QString>("diagnostic");
    QTest::addColumn<QString>("expectedStatus");
    QTest::addColumn<QString>("expectedReason");

    QTest::newRow("failure") << 0 << 0 << QStringLiteral("secondary frame provider failed")
                             << QStringLiteral("Error") << QStringLiteral("ProviderFailure");
    QTest::newRow("unsupported-request")
        << 1 << static_cast<int>(ImageSequenceProviderUnsupportedCause::UnsupportedRequest)
        << QStringLiteral("secondary frame operation unsupported") << QStringLiteral("Unsupported")
        << QStringLiteral("UnsupportedRequest");
    QTest::newRow("cancellation") << 2 << 0
                                  << QStringLiteral("secondary frame cancelled unexpectedly")
                                  << QStringLiteral("Error") << QStringLiteral("ProviderFailure");
}

void ImageViewportProviderTerminalProjectionTest::
    secondaryProviderFrameTerminalResultsProjectThroughSpread()
{
    QFETCH(int, terminalKind);
    QFETCH(int, unsupportedCause);
    QFETCH(QString, diagnostic);
    QFETCH(QString, expectedStatus);
    QFETCH(QString, expectedReason);

    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromProvider(&adapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(20.0, 10.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    if (terminalKind == 0) {
        emitProviderFailed(sessionFactory->lastSession(), frameToken, diagnostic);
    } else if (terminalKind == 1) {
        emitProviderUnsupported(sessionFactory->lastSession(), frameToken,
            static_cast<ImageSequenceProviderUnsupportedCause>(unsupportedCause), diagnostic);
    } else {
        emitProviderCancelled(sessionFactory->lastSession(), frameToken, diagnostic);
    }
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item),
        enumValue(metaObject, "RequestStatus", expectedStatus.toUtf8().constData()));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", expectedReason.toUtf8().constData()));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 0);
    verifyUntrustedProviderDiagnostic(item, diagnostic);
}

void ImageViewportProviderTerminalProjectionTest::
    secondaryProviderPlaybackTerminalResultsProjectThroughSpread_data()
{
    QTest::addColumn<int>("terminalKind");
    QTest::addColumn<int>("unsupportedCause");
    QTest::addColumn<QString>("diagnostic");
    QTest::addColumn<QString>("expectedStatus");
    QTest::addColumn<QString>("expectedReason");

    QTest::newRow("failure") << 0 << 0 << QStringLiteral("secondary playback provider failed")
                             << QStringLiteral("Error") << QStringLiteral("ProviderFailure");
    QTest::newRow("unsupported-request")
        << 1 << static_cast<int>(ImageSequenceProviderUnsupportedCause::UnsupportedRequest)
        << QStringLiteral("secondary playback operation unsupported")
        << QStringLiteral("Unsupported") << QStringLiteral("UnsupportedRequest");
    QTest::newRow("payload-rejection")
        << 1 << static_cast<int>(ImageSequenceProviderUnsupportedCause::PayloadRejection)
        << QStringLiteral("secondary playback payload rejected") << QStringLiteral("Unsupported")
        << QStringLiteral("PayloadRejection");
    QTest::newRow("cancellation") << 2 << 0
                                  << QStringLiteral("secondary playback cancelled unexpectedly")
                                  << QStringLiteral("Error") << QStringLiteral("ProviderFailure");
}

void ImageViewportProviderTerminalProjectionTest::
    secondaryProviderPlaybackTerminalResultsProjectThroughSpread()
{
    QFETCH(int, terminalKind);
    QFETCH(int, unsupportedCause);
    QFETCH(QString, diagnostic);
    QFETCH(QString, expectedStatus);
    QFETCH(QString, expectedReason);

    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        playbackRequestCount, lastPlaybackFrame, lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromProvider(&adapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage secondaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::black);
    ImageFrame secondaryFrame(secondaryImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &secondaryFrame, 0, 0);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.play(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(item, 100, ImageViewportPageRole::Secondary);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(playbackPhaseValue(item, ImageViewportPageRole::Secondary),
        enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);

    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();
    if (terminalKind == 0) {
        emitProviderFailed(sessionFactory->lastSession(), playbackToken, diagnostic);
    } else if (terminalKind == 1) {
        emitProviderUnsupported(sessionFactory->lastSession(), playbackToken,
            static_cast<ImageSequenceProviderUnsupportedCause>(unsupportedCause), diagnostic);
    } else {
        emitProviderCancelled(sessionFactory->lastSession(), playbackToken, diagnostic);
    }
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(playbackPhaseValue(item, ImageViewportPageRole::Secondary),
        enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item),
        enumValue(metaObject, "RequestStatus", expectedStatus.toUtf8().constData()));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", expectedReason.toUtf8().constData()));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
    QCOMPARE(secondaryDisplayedFrame(item), 0);
    verifyUntrustedProviderDiagnostic(item, diagnostic);
}

void ImageViewportProviderTerminalProjectionTest::invalidUnsupportedCauseIsProtocolViolation_data()
{
    QTest::addColumn<int>("tokenScope");
    QTest::addColumn<QString>("suppliedDiagnostic");
    QTest::addColumn<QString>("expectedDisplayStatus");

    QTest::newRow("metadata") << 0 << QStringLiteral("metadata unsupported cause private detail")
                              << QStringLiteral("Empty");
    QTest::newRow("frame") << 1 << QStringLiteral("frame unsupported cause private detail")
                           << QStringLiteral("Empty");
    QTest::newRow("playback") << 2 << QStringLiteral("playback unsupported cause private detail")
                              << QStringLiteral("Retained");
}

void ImageViewportProviderTerminalProjectionTest::invalidUnsupportedCauseIsProtocolViolation()
{
    QFETCH(int, tokenScope);
    QFETCH(QString, suppliedDiagnostic);
    QFETCH(QString, expectedDisplayStatus);

    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        playbackRequestCount, lastPlaybackFrame, lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    ImageSequenceProviderRequestToken terminalToken;
    if (tokenScope == 0) {
        terminalToken = sessionFactory->lastSession()->lastMetadataToken();
    } else if (tokenScope == 1) {
        emitProviderMetadataReady(sessionFactory->lastSession(),
            sessionFactory->lastSession()->lastMetadataToken(),
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        drainQueuedProviderResults();
        terminalToken = sessionFactory->lastSession()->lastFrameToken();
    } else {
        emitProviderMetadataReady(sessionFactory->lastSession(),
            sessionFactory->lastSession()->lastMetadataToken(),
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
        drainQueuedProviderResults();
        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        ImageFrame frame(image);
        emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
        acknowledgePendingRenderCommitForTest(item);
        QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
            ImageViewportCommandOutcome::Accepted);
        advancePlaybackForTest(item, 100);
        terminalToken = sessionFactory->lastSession()->lastFrameToken();
        QCOMPARE(*playbackRequestCount, 1);
    }
    QVERIFY(terminalToken.isValid());

    emitProviderUnsupported(sessionFactory->lastSession(), terminalToken,
        static_cast<ImageSequenceProviderUnsupportedCause>(-1), suppliedDiagnostic);
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item),
        enumValue(metaObject, "DisplayStatus", expectedDisplayStatus.toUtf8().constData()));
    if (tokenScope == 2) {
        QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    }
    const QString errorString = viewportErrorString(item);
    QVERIFY(errorString.contains(QStringLiteral("provider protocol violation")));
    QVERIFY(!errorString.contains(suppliedDiagnostic));
    const auto observations = internalObservationsForTest(item);
    QVERIFY(!observations.isEmpty());
    const InternalObservationForTest observation = observations.constLast();
    QCOMPARE(observation.subsystem, InternalObservationSubsystemForTest::Engine);
    QCOMPARE(observation.category, InternalObservationCategoryForTest::AdmissionFailure);
    QCOMPARE(
        observation.cause, InternalObservationCauseForTest::ProviderProtocolEventShapeMismatch);
    QVERIFY(observation.identity.roleValid);
    QCOMPARE(observation.identity.role, ImageViewportPageRole::Primary);
    QVERIFY(observation.identity.generation > 0);
    QVERIFY(observation.identity.sessionSerial > 0);
    QVERIFY(observation.identity.requestId > 0);
    QCOMPARE(observation.identity.providerToken, providerRequestTokenValueForTest(terminalToken));
}

void ImageViewportProviderTerminalProjectionTest::
    providerInvalidTokenBeforeMetadataIsProtocolViolation()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderFailed(sessionFactory->lastSession(), ImageSequenceProviderRequestToken(),
        QStringLiteral("invalid token failure"));
    drainQueuedProviderResults();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider protocol violation")));
    const auto observations = internalObservationsForTest(item);
    QVERIFY(!observations.isEmpty());
    const InternalObservationForTest observation = observations.constLast();
    QCOMPARE(observation.subsystem, InternalObservationSubsystemForTest::Engine);
    QCOMPARE(observation.category, InternalObservationCategoryForTest::AdmissionFailure);
    QCOMPARE(observation.cause, InternalObservationCauseForTest::ProviderProtocolTokenMismatch);
    QCOMPARE(observation.identity.providerToken, quint64(0));
}

void ImageViewportProviderTerminalProjectionTest::
    providerInvalidTokenAfterMetadataIsProtocolViolation()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    emitProviderFailed(sessionFactory->lastSession(), ImageSequenceProviderRequestToken(),
        QStringLiteral("invalid token failure"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider protocol violation")));
}
QTEST_MAIN(ImageViewportProviderTerminalProjectionTest)

#include "tst_imageviewport_provider_terminal_projection.moc"
