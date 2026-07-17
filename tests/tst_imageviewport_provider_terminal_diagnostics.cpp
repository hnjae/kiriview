#include "imageviewport_provider_test_support.h"

namespace {

class DiagnosticFailingSessionAdapter final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderAdapter
{
public:
    explicit DiagnosticFailingSessionAdapter(QString diagnostic, QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_diagnostic(std::move(diagnostic))
    {
    }

    ImageSequenceProviderDescriptor descriptor() const override
    {
        const QString diagnostic = m_diagnostic;
        return ImageSequenceProviderDescriptor(ImageSequenceProviderMetadata::still(QSizeF(4, 2)),
            ImageSequenceProviderThreadingContract::AffinityBound, [diagnostic]() {
                return ImageSequenceProviderSessionFactoryResult::failed(diagnostic);
            });
    }

private:
    QString m_diagnostic;
};

}

class ImageViewportProviderTerminalDiagnosticsTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderTerminalDiagnosticsTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void providerDiagnosticsUseUnicodeScalarLimit();
    void providerDiagnosticsRedactPrivateDetails();
    void providerUnsupportedAndCancellationDiagnosticsArePublicSafe();
    void invalidUnsupportedCauseUsesProtocolDiagnostic();
    void providerDiagnosticsArePlainText();
    void sessionFactoryDiagnosticsArePublicSafe();
    void emptySessionFactoryDiagnosticUsesSafeFallback();
};

void ImageViewportProviderTerminalDiagnosticsTest::providerDiagnosticsUseUnicodeScalarLimit()
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

    const int limit = ImageSequenceLimits::maximumDiagnosticCharacters();
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QVERIFY(sessionFactory->lastSession());
    emitProviderFailed(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(), diagnostic);
    drainQueuedProviderResults();

    const QString errorString = viewportErrorString(item);
    QCOMPARE(errorString.toUcs4().size(), limit);
    QCOMPARE(errorString, expected);
}

void ImageViewportProviderTerminalDiagnosticsTest::providerDiagnosticsRedactPrivateDetails()
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
        QStringLiteral("decoder failed for https://user:secret@example.test/image.png token=abc123 "
                       "path /home/ops/private/image.png and C:\\Users\\ops\\secret.png"));
    drainQueuedProviderResults();

    const QString errorString = viewportErrorString(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(!errorString.isEmpty());
    QVERIFY(!errorString.contains(QStringLiteral("https://")));
    QVERIFY(!errorString.contains(QStringLiteral("user:secret")));
    QVERIFY(!errorString.contains(QStringLiteral("token=abc123")));
    QVERIFY(!errorString.contains(QStringLiteral("/home/ops/private")));
    QVERIFY(!errorString.contains(QStringLiteral("C:\\Users\\ops")));
}

void ImageViewportProviderTerminalDiagnosticsTest::
    providerUnsupportedAndCancellationDiagnosticsArePublicSafe()
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
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});

        QVERIFY(sessionFactory->lastSession());
        emitTerminalResult(
            sessionFactory->lastSession(), sessionFactory->lastSession()->lastMetadataToken());
        drainQueuedProviderResults();

        const QString errorString = viewportErrorString(item);
        QVERIFY(!errorString.isEmpty());
        QVERIFY(!errorString.contains(QStringLiteral("https://")));
        QVERIFY(!errorString.contains(QStringLiteral("user:secret")));
        QVERIFY(!errorString.contains(QStringLiteral("token=abc123")));
        QVERIFY(!errorString.contains(QStringLiteral("/home/ops/private")));
        QVERIFY(!errorString.contains(QStringLiteral("C:\\Users\\ops")));
    };

    const QString diagnostic = QStringLiteral(
        "terminal result for https://user:secret@example.test/image.png token=abc123 path "
        "/home/ops/private/image.png and C:\\Users\\ops\\secret.png");
    verifyDiagnostic(
        [&diagnostic](CountingProviderSession* session, ImageSequenceProviderRequestToken token) {
            emitProviderUnsupported(session, token,
                ImageSequenceProviderUnsupportedCause::UnsupportedRequest, diagnostic);
        });
    verifyDiagnostic(
        [&diagnostic](CountingProviderSession* session, ImageSequenceProviderRequestToken token) {
            emitProviderCancelled(session, token, diagnostic);
        });
}

void ImageViewportProviderTerminalDiagnosticsTest::invalidUnsupportedCauseUsesProtocolDiagnostic()
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

    const QString suppliedDiagnostic = QStringLiteral(
        "invalid cause for https://user:secret@example.test/image.png token=abc123");
    QVERIFY(sessionFactory->lastSession());
    emitProviderUnsupported(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        static_cast<ImageSequenceProviderUnsupportedCause>(-1), suppliedDiagnostic);
    drainQueuedProviderResults();

    const QString errorString = viewportErrorString(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QVERIFY(!errorString.isEmpty());
    QVERIFY(!errorString.contains(QStringLiteral("https://")));
    QVERIFY(!errorString.contains(QStringLiteral("user:secret")));
    QVERIFY(!errorString.contains(QStringLiteral("token=abc123")));
    QVERIFY(errorString.size() <= ImageSequenceLimits::maximumDiagnosticCharacters());
}

void ImageViewportProviderTerminalDiagnosticsTest::providerDiagnosticsArePlainText()
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
        QStringLiteral("decoder <b>failed</b>\n<script>alert(1)</script>\ttry again"));
    drainQueuedProviderResults();

    const QString errorString = viewportErrorString(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(!errorString.contains(QLatin1Char('<')));
    QVERIFY(!errorString.contains(QLatin1Char('>')));
    QVERIFY(!errorString.contains(QLatin1Char('\n')));
    QVERIFY(!errorString.contains(QLatin1Char('\t')));
    QVERIFY(!errorString.isEmpty());
}

void ImageViewportProviderTerminalDiagnosticsTest::sessionFactoryDiagnosticsArePublicSafe()
{
    const int limit = ImageSequenceLimits::maximumDiagnosticCharacters();
    const char32_t codePoint[] = { 0x1F642 };
    const QString scalar = QString::fromUcs4(codePoint, 1);
    QString diagnostic
        = QStringLiteral("open failed for https://user:secret@example.test/image.png token=abc123 "
                         "path /home/ops/private/image.png and C:\\Users\\ops\\secret.png "
                         "<b>retry</b>\n");
    diagnostic += scalar.repeated(limit + 1);

    DiagnosticFailingSessionAdapter adapter(diagnostic);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    const QString errorString = viewportErrorString(item);
    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(item.state().request().reason(), ImageViewportRequestReason::ProviderFailure);
    QVERIFY(!errorString.isEmpty());
    QVERIFY(errorString.toUcs4().size() <= limit);
    QVERIFY(!errorString.contains(QStringLiteral("https://")));
    QVERIFY(!errorString.contains(QStringLiteral("user:secret")));
    QVERIFY(!errorString.contains(QStringLiteral("token=abc123")));
    QVERIFY(!errorString.contains(QStringLiteral("/home/ops/private")));
    QVERIFY(!errorString.contains(QStringLiteral("C:\\Users\\ops")));
    QVERIFY(!errorString.contains(QLatin1Char('<')));
    QVERIFY(!errorString.contains(QLatin1Char('>')));
    QVERIFY(!errorString.contains(QLatin1Char('\n')));
}

void ImageViewportProviderTerminalDiagnosticsTest::emptySessionFactoryDiagnosticUsesSafeFallback()
{
    DiagnosticFailingSessionAdapter adapter({});
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(item.state().request().reason(), ImageViewportRequestReason::ProviderFailure);
    QVERIFY(!viewportErrorString(item).isEmpty());
}

QTEST_MAIN(ImageViewportProviderTerminalDiagnosticsTest)

#include "tst_imageviewport_provider_terminal_diagnostics.moc"
