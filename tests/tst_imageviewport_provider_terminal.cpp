#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

namespace {

void acknowledgePendingRenderCommit(ImageViewport& item)
{
    const quint64 generation = item.pendingRenderGenerationForTest();
    const quint64 requestId = item.activeRequestIdForTest();
    const quint64 primaryPayloadId = item.pendingRenderPayloadIdForTest();
    const quint64 secondaryPayloadId = item.secondaryPendingRenderPayloadIdForTest();
    if (secondaryPayloadId > 0) {
        item.acknowledgeRenderCommitForTest(
            generation, requestId, primaryPayloadId, secondaryPayloadId);
        return;
    }
    item.acknowledgeRenderCommitForTest(generation, requestId, primaryPayloadId);
}

void emitTerminal(CountingProviderSession* session, ImageSequenceProviderRequestToken token,
    int terminalKind, int unsupportedCause, const QString& diagnostic)
{
    if (terminalKind == 0) {
        emit session->providerFailed(token, diagnostic);
    } else if (terminalKind == 1) {
        emit session->providerUnsupportedWithCause(token,
            static_cast<ImageSequenceProviderSession::UnsupportedCause>(unsupportedCause),
            diagnostic);
    } else {
        emit session->providerCancelled(token, diagnostic);
    }
    drainQueuedProviderResults();
}

}

class ImageViewportProviderTerminalTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderTerminalTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void providerFrameUnsupportedOperationReportsUnsupportedRequest();
    void providerPlaybackUnsupportedPayloadReportsPayloadRejection();
    void providerMetadataFailureReportsProviderFailure();
    void secondaryProviderMetadataFailureReportsAggregateProviderFailure();
    void targetSpreadTerminalProjectionPrefersErrorOverUnsupported_data();
    void targetSpreadTerminalProjectionPrefersErrorOverUnsupported();
    void secondaryTerminalFailureSealsSpreadAgainstLatePrimaryReady();
    void primaryTerminalFailureSealsSpreadAgainstLateSecondaryReady();
    void clearAndReplacementEscapeSealedTargetSpread();
    void secondaryProviderFrameTerminalResultsProjectThroughSpread_data();
    void secondaryProviderFrameTerminalResultsProjectThroughSpread();
    void secondaryProviderPlaybackTerminalResultsProjectThroughSpread_data();
    void secondaryProviderPlaybackTerminalResultsProjectThroughSpread();
    void providerInvalidTerminalTokenBeforeMetadataIsIgnored();
    void providerInvalidTerminalTokenAfterMetadataIsIgnored();
    void providerDiagnosticsUseUnicodeScalarLimit();
    void providerDiagnosticsRedactPrivateDetails();
    void providerUnsupportedAndCancellationDiagnosticsArePublicSafe();
    void providerDiagnosticsArePlainText();
    void providerMetadataFailureStopsPendingPlayback();
    void providerGenerationTerminalFailureRejectsDisplayCommands();
    void providerGenerationTerminalFailureAcceptsControlCommands();
    void providerFrameFailureKeepsGenerationSeekable();
    void providerFrameFailureRetainsDisplayAndClearsOnSeek();
    void providerFrameFailureKeepsGenerationPositionSeekable();
    void providerTimedPlayAfterFrameFailureRestartsPlaybackRequest();
    void providerFrameFailureAcceptsControlCommands();
    void providerMetadataUnsupportedReportsUnsupportedRequest();
    void providerGenerationTerminalUnsupportedAcceptsControlCommands();
    void providerMetadataUnsupportedRetainsReplacementDisplayOnlyAsFallback();
    void providerFrameUnsupportedKeepsGenerationSeekable();
    void providerFrameUnsupportedRetainsDisplayAndClearsOnSeek();
    void providerFrameUnsupportedKeepsGenerationPositionSeekable();
    void providerMetadataCancellationReportsProviderFailure();
    void providerFrameCancellationReportsProviderFailure();
    void providerFrameCancellationRetainsDisplayAndClearsOnSeek();
    void providerMetadataEndOfSequenceReportsProtocolViolation();
    void providerFrameEndOfSequenceReportsProtocolViolation();
    void providerTotalDurationSeekEndOfSequenceReportsProtocolViolation();
    void providerTimedPlaybackStopsOnFrameFailure();
    void providerTimedPlayAfterPlaybackFailureRestartsPlaybackRequest();
    void providerTimedPlaybackCancellationReportsProviderFailure();
    void providerTimedPlayAfterPlaybackCancellationRestartsPlaybackRequest();
    void providerTimedPlaybackUnsupportedReportsUnsupportedRequest();
    void providerTimedPlayAfterPlaybackUnsupportedRestartsPlaybackRequest();
};

void ImageViewportProviderTerminalTest::providerFrameUnsupportedOperationReportsUnsupportedRequest()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();

    emit sessionFactory->lastSession()->providerUnsupportedWithCause(frameToken,
        ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest,
        QStringLiteral("frame operation unsupported"));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("frame operation unsupported")));
}

void ImageViewportProviderTerminalTest::providerPlaybackUnsupportedPayloadReportsPayloadRejection()
{
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    QCOMPARE(*playbackRequestCount, 1);
    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();

    emit sessionFactory->lastSession()->providerUnsupportedWithCause(playbackToken,
        ImageSequenceProviderSession::UnsupportedCause::PayloadRejection,
        QStringLiteral("playback payload unsupported"));
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("playback payload unsupported")));
}

void ImageViewportProviderTerminalTest::providerMetadataFailureReportsProviderFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("metadata service unavailable")));
}

void ImageViewportProviderTerminalTest::
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("secondary metadata service unavailable"));
    drainQueuedProviderResults();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("primaryRequestedFrame").toInt(), 0);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), -1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("secondary metadata service unavailable")));
}

void ImageViewportProviderTerminalTest::clearAndReplacementEscapeSealedTargetSpread()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata generation failed"));
    drainQueuedProviderResults();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("errorString").toString(), QString());

    item.setSequence(result->sequence());
    QCOMPARE(*sessionCount, 2);
    QCOMPARE(*metadataRequestCount, 2);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::
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

    const int primary = static_cast<int>(ImageViewport::PageRole::Primary);
    const int secondary = static_cast<int>(ImageViewport::PageRole::Secondary);
    const int unsupportedRequest
        = static_cast<int>(ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest);
    const int payloadRejection
        = static_cast<int>(ImageSequenceProviderSession::UnsupportedCause::PayloadRejection);

    QTest::newRow("primary-error-then-secondary-unsupported")
        << primary << 0 << payloadRejection << QStringLiteral("primary frame failed") << secondary
        << 1 << payloadRejection << QStringLiteral("secondary payload unsupported")
        << QStringLiteral("Error") << QStringLiteral("ProviderFailure")
        << QStringLiteral("primary frame failed");
    QTest::newRow("secondary-error-then-primary-unsupported")
        << secondary << 0 << payloadRejection << QStringLiteral("secondary frame failed")
        << primary << 1 << unsupportedRequest << QStringLiteral("primary operation unsupported")
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

void ImageViewportProviderTerminalTest::
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    CountingProviderSession* primarySession = primarySessionFactory->lastSession();
    CountingProviderSession* secondarySession = secondarySessionFactory->lastSession();
    QVERIFY(primarySession);
    QVERIFY(secondarySession);
    emit primarySession->metadataReady(primarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    emit secondarySession->metadataReady(secondarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(20.0, 10.0), { 100, 250 }));
    drainQueuedProviderResults();
    QCOMPARE(*primaryFrameRequestCount, 1);
    QCOMPARE(*secondaryFrameRequestCount, 1);

    const auto emitForRole = [&](int role, int kind, int unsupportedCause,
                                 const QString& diagnostic) {
        CountingProviderSession* session
            = role == static_cast<int>(ImageViewport::PageRole::Primary) ? primarySession
                                                                         : secondarySession;
        emitTerminal(session, session->lastFrameToken(), kind, unsupportedCause, diagnostic);
    };

    emitForRole(firstRole, firstKind, firstUnsupportedCause, firstDiagnostic);
    emitForRole(secondRole, secondKind, secondUnsupportedCause, secondDiagnostic);

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", expectedStatus.toUtf8().constData()));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", expectedReason.toUtf8().constData()));
    QVERIFY(item.property("errorString").toString().contains(expectedDiagnostic));
}

void ImageViewportProviderTerminalTest::
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    CountingProviderSession* primarySession = primarySessionFactory->lastSession();
    CountingProviderSession* secondarySession = secondarySessionFactory->lastSession();
    QVERIFY(primarySession);
    QVERIFY(secondarySession);
    emit primarySession->metadataReady(primarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    emit secondarySession->metadataReady(secondarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(20.0, 10.0), { 100, 250 }));
    drainQueuedProviderResults();

    emit secondarySession->providerFailed(
        secondarySession->lastFrameToken(), QStringLiteral("secondary frame failed"));
    drainQueuedProviderResults();

    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    emitTimedProviderFrameReady(primarySession, &primaryFrame, 0, 0);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("secondary frame failed")));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
}

void ImageViewportProviderTerminalTest::
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    CountingProviderSession* primarySession = primarySessionFactory->lastSession();
    CountingProviderSession* secondarySession = secondarySessionFactory->lastSession();
    QVERIFY(primarySession);
    QVERIFY(secondarySession);
    emit primarySession->metadataReady(primarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    emit secondarySession->metadataReady(secondarySession->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(20.0, 10.0), { 100, 250 }));
    drainQueuedProviderResults();

    emit primarySession->providerFailed(
        primarySession->lastFrameToken(), QStringLiteral("primary frame failed"));
    drainQueuedProviderResults();

    QImage secondaryImage(20, 10, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    emitTimedProviderFrameReady(secondarySession, &secondaryFrame, 0, 0);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("primary frame failed")));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
}

void ImageViewportProviderTerminalTest::
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
        << 1 << static_cast<int>(ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest)
        << QStringLiteral("secondary frame operation unsupported") << QStringLiteral("Unsupported")
        << QStringLiteral("UnsupportedRequest");
    QTest::newRow("cancellation") << 2 << 0
                                  << QStringLiteral("secondary frame cancelled unexpectedly")
                                  << QStringLiteral("Error") << QStringLiteral("ProviderFailure");
}

void ImageViewportProviderTerminalTest::secondaryProviderFrameTerminalResultsProjectThroughSpread()
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(20.0, 10.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    if (terminalKind == 0) {
        emit sessionFactory->lastSession()->providerFailed(frameToken, diagnostic);
    } else if (terminalKind == 1) {
        emit sessionFactory->lastSession()->providerUnsupportedWithCause(frameToken,
            static_cast<ImageSequenceProviderSession::UnsupportedCause>(unsupportedCause),
            diagnostic);
    } else {
        emit sessionFactory->lastSession()->providerCancelled(frameToken, diagnostic);
    }
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", expectedStatus.toUtf8().constData()));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", expectedReason.toUtf8().constData()));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("primaryRequestedFrame").toInt(), 0);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 0);
    QVERIFY(item.property("errorString").toString().contains(diagnostic));
}

void ImageViewportProviderTerminalTest::
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
        << 1 << static_cast<int>(ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest)
        << QStringLiteral("secondary playback operation unsupported")
        << QStringLiteral("Unsupported") << QStringLiteral("UnsupportedRequest");
    QTest::newRow("payload-rejection")
        << 1 << static_cast<int>(ImageSequenceProviderSession::UnsupportedCause::PayloadRejection)
        << QStringLiteral("secondary playback payload rejected") << QStringLiteral("Unsupported")
        << QStringLiteral("PayloadRejection");
    QTest::newRow("cancellation") << 2 << 0
                                  << QStringLiteral("secondary playback cancelled unexpectedly")
                                  << QStringLiteral("Error") << QStringLiteral("ProviderFailure");
}

void ImageViewportProviderTerminalTest::
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage secondaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::black);
    ImageFrame secondaryFrame(secondaryImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &secondaryFrame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(
        item.play(ImageViewport::PageRole::Secondary), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), 100);

    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();
    if (terminalKind == 0) {
        emit sessionFactory->lastSession()->providerFailed(playbackToken, diagnostic);
    } else if (terminalKind == 1) {
        emit sessionFactory->lastSession()->providerUnsupportedWithCause(playbackToken,
            static_cast<ImageSequenceProviderSession::UnsupportedCause>(unsupportedCause),
            diagnostic);
    } else {
        emit sessionFactory->lastSession()->providerCancelled(playbackToken, diagnostic);
    }
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", expectedStatus.toUtf8().constData()));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", expectedReason.toUtf8().constData()));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("primaryRequestedFrame").toInt(), 0);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), 1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), 100);
    QCOMPARE(item.property("secondaryDisplayedFrame").toInt(), 0);
    QVERIFY(item.property("errorString").toString().contains(diagnostic));
}

void ImageViewportProviderTerminalTest::providerInvalidTerminalTokenBeforeMetadataIsIgnored()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    emit sessionFactory->lastSession()->providerFailed(
        ImageSequenceProviderRequestToken(), QStringLiteral("invalid token failure"));
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerUnsupported(
        ImageSequenceProviderRequestToken(), QStringLiteral("invalid token unsupported"));
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerCancelled(
        ImageSequenceProviderRequestToken(), QStringLiteral("invalid token cancellation"));
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->endOfSequence(ImageSequenceProviderRequestToken());
    drainQueuedProviderResults();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportProviderTerminalTest::providerInvalidTerminalTokenAfterMetadataIsIgnored()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    emit sessionFactory->lastSession()->providerFailed(
        ImageSequenceProviderRequestToken(), QStringLiteral("invalid token failure"));
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerUnsupported(
        ImageSequenceProviderRequestToken(), QStringLiteral("invalid token unsupported"));
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerCancelled(
        ImageSequenceProviderRequestToken(), QStringLiteral("invalid token cancellation"));
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->endOfSequence(ImageSequenceProviderRequestToken());
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportProviderTerminalTest::providerDiagnosticsUseUnicodeScalarLimit()
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

    const int limit = ImageSequenceLimits::maximumDiagnosticStringLength();
    const char32_t codePoint[] = { 0x1F642 };
    const QString scalar = QString::fromUcs4(codePoint, 1);
    QString diagnostic;
    QString expected;
    diagnostic.reserve((limit + 1) * scalar.size());
    expected.reserve(limit * scalar.size());
    for (int i = 0; i < limit; ++i) {
        diagnostic += scalar;
        expected += scalar;
    }
    diagnostic += scalar;
    diagnostic += QStringLiteral("tail");

    ImageViewport item;
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(), diagnostic);
    drainQueuedProviderResults();

    const QString errorString = item.property("errorString").toString();
    QCOMPARE(errorString.toUcs4().size(), limit);
    QCOMPARE(errorString, expected);
}

void ImageViewportProviderTerminalTest::providerDiagnosticsRedactPrivateDetails()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("decoder failed for https://user:secret@example.test/image.png token=abc123 "
                       "path /home/ops/private/image.png and C:\\Users\\ops\\secret.png"));
    drainQueuedProviderResults();

    const QString errorString = item.property("errorString").toString();
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(!errorString.contains(QStringLiteral("https://")));
    QVERIFY(!errorString.contains(QStringLiteral("user:secret")));
    QVERIFY(!errorString.contains(QStringLiteral("token=abc123")));
    QVERIFY(!errorString.contains(QStringLiteral("/home/ops/private")));
    QVERIFY(!errorString.contains(QStringLiteral("C:\\Users\\ops")));
    QVERIFY(errorString.contains(QStringLiteral("[redacted")));
}

void ImageViewportProviderTerminalTest::providerUnsupportedAndCancellationDiagnosticsArePublicSafe()
{
    const auto verifyDiagnostic = [](auto emitTerminalResult) {
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
        item.setSequence(result->sequence());

        QVERIFY(sessionFactory->lastSession());
        emitTerminalResult(
            sessionFactory->lastSession(), sessionFactory->lastSession()->lastMetadataToken());
        drainQueuedProviderResults();

        const QString errorString = item.property("errorString").toString();
        QVERIFY(!errorString.contains(QStringLiteral("https://")));
        QVERIFY(!errorString.contains(QStringLiteral("user:secret")));
        QVERIFY(!errorString.contains(QStringLiteral("token=abc123")));
        QVERIFY(!errorString.contains(QStringLiteral("/home/ops/private")));
        QVERIFY(!errorString.contains(QStringLiteral("C:\\Users\\ops")));
        QVERIFY(errorString.contains(QStringLiteral("[redacted")));
    };

    const QString diagnostic = QStringLiteral(
        "terminal result for https://user:secret@example.test/image.png token=abc123 path "
        "/home/ops/private/image.png and C:\\Users\\ops\\secret.png");
    verifyDiagnostic(
        [&diagnostic](CountingProviderSession* session, ImageSequenceProviderRequestToken token) {
            emit session->providerUnsupported(token, diagnostic);
        });
    verifyDiagnostic(
        [&diagnostic](CountingProviderSession* session, ImageSequenceProviderRequestToken token) {
            emit session->providerCancelled(token, diagnostic);
        });
}

void ImageViewportProviderTerminalTest::providerDiagnosticsArePlainText()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("decoder <b>failed</b>\n<script>alert(1)</script>\ttry again"));
    drainQueuedProviderResults();

    const QString errorString = item.property("errorString").toString();
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(!errorString.contains(QLatin1Char('<')));
    QVERIFY(!errorString.contains(QLatin1Char('>')));
    QVERIFY(!errorString.contains(QLatin1Char('\n')));
    QVERIFY(!errorString.contains(QLatin1Char('\t')));
    QCOMPARE(errorString, QStringLiteral("decoder failed alert(1) try again"));
}

void ImageViewportProviderTerminalTest::providerMetadataFailureStopsPendingPlayback()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
}

void ImageViewportProviderTerminalTest::providerGenerationTerminalFailureRejectsDisplayCommands()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.seekToPosition(-1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportProviderTerminalTest::providerGenerationTerminalFailureAcceptsControlCommands()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    const RevisionToken failedRequestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    const RevisionToken unsupportedCommandRevision = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", unsupportedCommandRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);

    const RevisionToken clearedCommandRevision = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), clearedCommandRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
}

void ImageViewportProviderTerminalTest::providerFrameFailureKeepsGenerationSeekable()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerFailed(
        frameToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("frame decode failed")));

    const RevisionToken terminalRequestRevision = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    emit sessionFactory->lastSession()->providerProgress(frameToken, 1.0);
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerWaiting(frameToken);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), terminalRequestRevision);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("frame decode failed")));
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->imageFrameReady(frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);

    diagnosticsSpy.clear();

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(diagnosticsSpy.count(), 1);
}

void ImageViewportProviderTerminalTest::providerFrameFailureRetainsDisplayAndClearsOnSeek()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken failedToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->providerFailed(
        failedToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("frame decode failed")));

    const RevisionToken failedRequestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken retainedDisplayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken failedCommandRevision = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    verifyRevisionChanged(item, "commandRevision", failedCommandRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), retainedDisplayRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::providerFrameFailureKeepsGenerationPositionSeekable()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken failedToken
        = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerFailed(
        failedToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("frame decode failed")));

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::providerTimedPlayAfterFrameFailureRestartsPlaybackRequest()
{
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken failedToken
        = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerFailed(
        failedToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("frame decode failed")));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QVERIFY(playbackToken != failedToken);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), playbackToken, &frame, 1, 100);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::providerFrameFailureAcceptsControlCommands()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerFailed(
        frameToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    const RevisionToken failedRequestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const RevisionToken invalidCommandRevision = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", invalidCommandRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("frame decode failed")));

    const RevisionToken clearedCommandRevision = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), clearedCommandRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("frame decode failed")));
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*closeCount, 0);
}

void ImageViewportProviderTerminalTest::providerMetadataUnsupportedReportsUnsupportedRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerUnsupported(
        sessionFactory->lastSession()->lastMetadataToken(), QStringLiteral("unsupported codec"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("unsupported codec")));

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportProviderTerminalTest::
    providerGenerationTerminalUnsupportedAcceptsControlCommands()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerUnsupported(
        sessionFactory->lastSession()->lastMetadataToken(), QStringLiteral("unsupported codec"));
    drainQueuedProviderResults();

    const RevisionToken unsupportedRequestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    const RevisionToken unsupportedCommandRevision = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", unsupportedCommandRevision);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), unsupportedRequestRevision);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("unsupported codec")));

    const RevisionToken clearedCommandRevision = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), clearedCommandRevision);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), unsupportedRequestRevision);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("unsupported codec")));
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
}

void ImageViewportProviderTerminalTest::
    providerMetadataUnsupportedRetainsReplacementDisplayOnlyAsFallback()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> previousResult(factory.fromTimedFrameList(&list));
    QVERIFY(previousResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromProvider(&adapter));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(previousResult->sequence());
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));

    item.setSequence(replacementResult->sequence());

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("timedPlaybackSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerUnsupported(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("unsupported replacement metadata"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("timedPlaybackSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("unsupported replacement metadata")));

    const RevisionToken failedRequestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
}

void ImageViewportProviderTerminalTest::providerFrameUnsupportedKeepsGenerationSeekable()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerUnsupported(
        frameToken, QStringLiteral("unsupported frame shape"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("unsupported frame shape")));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->imageFrameReady(frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
}

void ImageViewportProviderTerminalTest::providerFrameUnsupportedRetainsDisplayAndClearsOnSeek()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken unsupportedToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->providerUnsupported(
        unsupportedToken, QStringLiteral("unsupported frame shape"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("unsupported frame shape")));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::providerFrameUnsupportedKeepsGenerationPositionSeekable()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken unsupportedToken
        = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerUnsupported(
        unsupportedToken, QStringLiteral("unsupported frame shape"));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("unsupported frame shape")));

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::providerMetadataCancellationReportsProviderFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerCancelled(
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("metadata cancelled by provider")));
}

void ImageViewportProviderTerminalTest::providerFrameCancellationReportsProviderFailure()
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerCancelled(
        frameToken, QStringLiteral("cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("cancelled by provider")));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->imageFrameReady(frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
}

void ImageViewportProviderTerminalTest::providerFrameCancellationRetainsDisplayAndClearsOnSeek()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken cancelledToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->providerCancelled(
        cancelledToken, QStringLiteral("cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("cancelled by provider")));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::providerMetadataEndOfSequenceReportsProtocolViolation()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->endOfSequence(
        sessionFactory->lastSession()->lastMetadataToken());
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("provider protocol violation")));

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
}

void ImageViewportProviderTerminalTest::providerFrameEndOfSequenceReportsProtocolViolation()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    emit sessionFactory->lastSession()->endOfSequence(
        sessionFactory->lastSession()->lastFrameToken());
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("provider protocol violation")));

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
}

void ImageViewportProviderTerminalTest::
    providerTotalDurationSeekEndOfSequenceReportsProtocolViolation()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RequestQueued"));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken totalDurationToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);

    emit sessionFactory->lastSession()->endOfSequence(totalDurationToken);
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 1);
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("provider protocol violation")));

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
}

void ImageViewportProviderTerminalTest::providerTimedPlaybackStopsOnFrameFailure()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));

    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastFrameToken(), QStringLiteral("playback frame failed"));
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("playback frame failed")));

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::
    providerTimedPlayAfterPlaybackFailureRestartsPlaybackRequest()
{
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    const ImageSequenceProviderRequestToken failedToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);

    emit sessionFactory->lastSession()->providerFailed(
        failedToken, QStringLiteral("playback frame failed"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("playback frame failed")));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken retryToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*playbackRequestCount, 2);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QVERIFY(retryToken != failedToken);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), retryToken, &frame, 1, 100);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::providerTimedPlaybackCancellationReportsProviderFailure()
{
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->providerCancelled(
        playbackToken, QStringLiteral("playback cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("playback cancelled by provider")));
}

void ImageViewportProviderTerminalTest::
    providerTimedPlayAfterPlaybackCancellationRestartsPlaybackRequest()
{
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    const ImageSequenceProviderRequestToken cancelledToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);

    emit sessionFactory->lastSession()->providerCancelled(
        cancelledToken, QStringLiteral("playback cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("playback cancelled by provider")));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken retryToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*playbackRequestCount, 2);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QVERIFY(retryToken != cancelledToken);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), retryToken, &frame, 1, 100);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderTerminalTest::providerTimedPlaybackUnsupportedReportsUnsupportedRequest()
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
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);

    emit sessionFactory->lastSession()->providerUnsupportedWithCause(playbackToken,
        ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest,
        QStringLiteral("playback request unsupported"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("playback request unsupported")));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
}

void ImageViewportProviderTerminalTest::
    providerTimedPlayAfterPlaybackUnsupportedRestartsPlaybackRequest()
{
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    const ImageSequenceProviderRequestToken unsupportedToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);

    emit sessionFactory->lastSession()->providerUnsupportedWithCause(unsupportedToken,
        ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest,
        QStringLiteral("playback request unsupported"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("playback request unsupported")));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken retryToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*playbackRequestCount, 2);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QVERIFY(retryToken != unsupportedToken);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), retryToken, &frame, 1, 100);
    acknowledgePendingRenderCommit(item);

    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("errorString").toString(), QString());
}

QTEST_MAIN(ImageViewportProviderTerminalTest)

#include "tst_imageviewport_provider_terminal.moc"
