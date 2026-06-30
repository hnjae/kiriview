#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

namespace {

void acknowledgePendingRenderFailure(ImageViewport& item)
{
    item.acknowledgeRenderFailureForTest(item.pendingRenderGenerationForTest(),
        item.activeRequestIdForTest(), item.pendingRenderPayloadIdForTest());
}

void acknowledgePendingRenderCommit(ImageViewport& item)
{
    item.acknowledgeRenderCommitForTest(item.pendingRenderGenerationForTest(),
        item.activeRequestIdForTest(), item.pendingRenderPayloadIdForTest());
}

}

class ImageViewportProviderLifecycleTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderLifecycleTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void replacementClearsRetainedDisplayDiagnostics();
    void providerTokenOverflowClosesSessionWithoutInvalidRequest();
    void providerKnownMetadataTokenOverflowClosesSessionWithoutFrameRequest();
    void providerTokenOverflowDoesNotPoisonReplacementSession();
    void providerTokenOverflowDuringSeekFailsAcceptedRequest();
    void providerSessionClosesWhenViewportIsDestroyed();
    void providerDestructionCancelsActiveFrameRequestBeforeClose();
    void providerReplacementCancelsActiveFrameRequestBeforeClose();
    void providerClearCancelsActiveFrameRequestBeforeClose();
    void providerClearDoesNotBlockOnSessionCleanup();
    void providerNullSequenceCancelsActiveFrameRequestBeforeClose();
    void providerReplacementIgnoresCancelledMetadataAcknowledgement();
    void providerClearIgnoresCancelledMetadataAcknowledgement();
    void providerClosedGenerationTokenCollisionIsIgnoredAfterClear();
    void providerClearIgnoresCancelledFrameAcknowledgement();
    void providerResultsAreQueuedFromSessionEntryPoint();
    void providerQueuedMetadataFromClosedGenerationIsIgnoredAfterReplacement();
    void providerFrameResultsAreQueuedFromSessionEntryPoint();
    void providerTerminalResultsAreQueuedFromSessionEntryPoint();
    void providerUnsupportedResultsAreQueuedFromSessionEntryPoint();
};

void ImageViewportProviderLifecycleTest::replacementClearsRetainedDisplayDiagnostics()
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
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromTimedFrameList(&list));
    QVERIFY(firstResult->sequence());

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
    item.setSequence(firstResult->sequence());
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    item.setSize(QSizeF(0.0, 100.0));
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(100.0, 100.0));
    acknowledgePendingRenderFailure(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("render commit failed")));

    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);
    item.setSequence(replacementResult->sequence());

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 0);
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
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(diagnosticsSpy.count(), 1);
}

void ImageViewportProviderLifecycleTest::providerTokenOverflowClosesSessionWithoutInvalidRequest()
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
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);

    ImageViewport item;
    item.setNextProviderRequestTokenForTest(std::numeric_limits<quint64>::max());
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    drainQueuedProviderResults();
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("provider request token")));
}

void ImageViewportProviderLifecycleTest::
    providerKnownMetadataTokenOverflowClosesSessionWithoutFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setNextProviderRequestTokenForTest(std::numeric_limits<quint64>::max());
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    drainQueuedProviderResults();
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("provider request token")));
}

void ImageViewportProviderLifecycleTest::providerTokenOverflowDoesNotPoisonReplacementSession()
{
    ImageSequenceFactory factory;
    const auto firstSessionCount = std::make_shared<int>(0);
    const auto firstMetadataRequestCount = std::make_shared<int>(0);
    const auto firstFrameRequestCount = std::make_shared<int>(0);
    const auto firstLastRequestedFrame = std::make_shared<int>(-1);
    const auto firstCloseCount = std::make_shared<int>(0);
    auto firstSessionFactory = std::make_shared<CountingProviderSessionFactory>(firstSessionCount,
        firstMetadataRequestCount, firstFrameRequestCount, firstLastRequestedFrame,
        firstCloseCount);
    CountingProviderAdapter firstAdapter(firstSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromProvider(&firstAdapter));
    QVERIFY(firstResult->sequence());

    ImageViewport item;
    item.setNextProviderRequestTokenForTest(std::numeric_limits<quint64>::max());
    item.setSequence(firstResult->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*firstSessionCount, 1);
    QCOMPARE(*firstMetadataRequestCount, 0);
    drainQueuedProviderResults();
    QCOMPARE(*firstCloseCount, 1);
    QCOMPARE(firstSessionFactory->lastSession(), nullptr);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));

    const auto replacementSessionCount = std::make_shared<int>(0);
    const auto replacementMetadataRequestCount = std::make_shared<int>(0);
    const auto replacementFrameRequestCount = std::make_shared<int>(0);
    const auto replacementLastRequestedFrame = std::make_shared<int>(-1);
    const auto replacementCloseCount = std::make_shared<int>(0);
    auto replacementSessionFactory = std::make_shared<CountingProviderSessionFactory>(
        replacementSessionCount, replacementMetadataRequestCount, replacementFrameRequestCount,
        replacementLastRequestedFrame, replacementCloseCount);
    CountingProviderAdapter replacementAdapter(replacementSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromProvider(&replacementAdapter));
    QVERIFY(replacementResult->sequence());

    item.setSequence(replacementResult->sequence());

    QCOMPARE(*replacementSessionCount, 1);
    QCOMPARE(*replacementMetadataRequestCount, 1);
    QCOMPARE(*replacementFrameRequestCount, 0);
    QCOMPARE(*replacementCloseCount, 0);
    QVERIFY(replacementSessionFactory->lastSession());
    QCOMPARE(replacementSessionFactory->lastSession()->lastMetadataToken().id(), quint64(1));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QVERIFY(!item.property("errorString")
            .toString()
            .contains(QStringLiteral("provider request token")));
}

void ImageViewportProviderLifecycleTest::providerTokenOverflowDuringSeekFailsAcceptedRequest()
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
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    item.setNextProviderRequestTokenForTest(std::numeric_limits<quint64>::max());
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    drainQueuedProviderResults();
    QCOMPARE(*closeCount, 1);
    QCOMPARE(sessionFactory->lastSession(), nullptr);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("provider request token")));
}

void ImageViewportProviderLifecycleTest::providerSessionClosesWhenViewportIsDestroyed()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledTokenId = std::make_shared<quint64>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount,
        lastCancelledTokenId);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageSequenceProviderRequestToken metadataToken;
    {
        ImageViewport item;
        item.setSequence(result->sequence());
        QCOMPARE(*sessionCount, 1);
        QCOMPARE(*metadataRequestCount, 1);
        QVERIFY(sessionFactory->lastSession());
        metadataToken = sessionFactory->lastSession()->lastMetadataToken();
        QVERIFY(metadataToken.isValid());
        QCOMPARE(*cancelRequestCount, 0);
        QCOMPARE(*closeCount, 0);
    }

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledTokenId, metadataToken.id());
    QCOMPARE(*closeCount, 1);
}

void ImageViewportProviderLifecycleTest::providerDestructionCancelsActiveFrameRequestBeforeClose()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledTokenId = std::make_shared<quint64>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount,
        lastCancelledTokenId);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageSequenceProviderRequestToken frameToken;
    {
        ImageViewport item;
        item.setSequence(result->sequence());

        QVERIFY(sessionFactory->lastSession());
        emit sessionFactory->lastSession()->metadataReady(
            sessionFactory->lastSession()->lastMetadataToken(),
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        drainQueuedProviderResults();

        frameToken = sessionFactory->lastSession()->lastFrameToken();
        QVERIFY(frameToken.isValid());
        QCOMPARE(*frameRequestCount, 1);
        QCOMPARE(*cancelRequestCount, 0);
        QCOMPARE(*closeCount, 0);
    }

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledTokenId, frameToken.id());
    QCOMPARE(*closeCount, 1);
}

void ImageViewportProviderLifecycleTest::providerReplacementCancelsActiveFrameRequestBeforeClose()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledTokenId = std::make_shared<quint64>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount,
        lastCancelledTokenId);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::transparent);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 1);

    item.setSequence(replacementResult->sequence());

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledTokenId, frameToken.id());
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
}

void ImageViewportProviderLifecycleTest::providerClearCancelsActiveFrameRequestBeforeClose()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledTokenId = std::make_shared<quint64>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount,
        lastCancelledTokenId);
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
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledTokenId, frameToken.id());
    QCOMPARE(*closeCount, 1);

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
}

void ImageViewportProviderLifecycleTest::providerClearDoesNotBlockOnSessionCleanup()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    ImageSequenceFactory factory;
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<SlowCleanupProviderSessionFactory>(
        &workerThread, cancelRequestCount, closeCount, 250);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    QVERIFY(sessionFactory->lastSession());
    QVERIFY(sessionFactory->lastSession()->lastMetadataToken().isValid());

    QElapsedTimer elapsed;
    elapsed.start();
    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);
    const qint64 clearElapsedMilliseconds = elapsed.elapsed();

    QVERIFY2(clearElapsedMilliseconds < 100,
        qPrintable(QStringLiteral("clear() blocked for %1 ms").arg(clearElapsedMilliseconds)));
    QTRY_COMPARE(*cancelRequestCount, 1);
    QTRY_COMPARE(*closeCount, 1);
}

void ImageViewportProviderLifecycleTest::providerNullSequenceCancelsActiveFrameRequestBeforeClose()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledTokenId = std::make_shared<quint64>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount,
        lastCancelledTokenId);
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
    QCOMPARE(*frameRequestCount, 1);

    item.setSequence(nullptr);

    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledTokenId, frameToken.id());
    QCOMPARE(*closeCount, 1);

    QCOMPARE(item.sequence(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
}

void ImageViewportProviderLifecycleTest::
    providerReplacementIgnoresCancelledMetadataAcknowledgement()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, cancelRequestCount, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::transparent);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));

    item.setSequence(replacementResult->sequence());
    const uint replacementRequestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("errorString").toString(), QString());

    drainQueuedProviderResults();

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestRevision").toUInt(), replacementRequestRevision);
    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderLifecycleTest::providerClearIgnoresCancelledMetadataAcknowledgement()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, cancelRequestCount, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);
    const uint clearedRequestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("errorString").toString(), QString());

    drainQueuedProviderResults();

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestRevision").toUInt(), clearedRequestRevision);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderLifecycleTest::providerClosedGenerationTokenCollisionIsIgnoredAfterClear()
{
    ImageSequenceFactory factory;
    const auto staleSessionCount = std::make_shared<int>(0);
    const auto staleMetadataRequestCount = std::make_shared<int>(0);
    const auto staleFrameRequestCount = std::make_shared<int>(0);
    const auto staleCancelRequestCount = std::make_shared<int>(0);
    const auto staleCloseCount = std::make_shared<int>(0);
    auto staleSessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(
        staleSessionCount, staleMetadataRequestCount, staleFrameRequestCount,
        staleCancelRequestCount, staleCloseCount);
    CountingProviderAdapter staleAdapter(staleSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> staleResult(factory.fromProvider(&staleAdapter));
    QVERIFY(staleResult->sequence());

    const auto replacementSessionCount = std::make_shared<int>(0);
    const auto replacementMetadataRequestCount = std::make_shared<int>(0);
    const auto replacementFrameRequestCount = std::make_shared<int>(0);
    const auto replacementLastRequestedFrame = std::make_shared<int>(-1);
    const auto replacementCloseCount = std::make_shared<int>(0);
    auto replacementSessionFactory = std::make_shared<CountingProviderSessionFactory>(
        replacementSessionCount, replacementMetadataRequestCount, replacementFrameRequestCount,
        replacementLastRequestedFrame, replacementCloseCount);
    CountingProviderAdapter replacementAdapter(replacementSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromProvider(&replacementAdapter));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSequence(staleResult->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(staleSessionFactory->lastSession());
    const ImageSequenceProviderRequestToken staleToken
        = staleSessionFactory->lastSession()->lastMetadataToken();
    QVERIFY(staleToken.isValid());
    QCOMPARE(*staleMetadataRequestCount, 1);

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);

    item.setNextProviderRequestTokenForTest(staleToken.id() - 1);
    item.setSequence(replacementResult->sequence());

    QVERIFY(replacementSessionFactory->lastSession());
    QCOMPARE(replacementSessionFactory->lastSession()->lastMetadataToken(), staleToken);
    const uint replacementRequestRevision = item.property("requestRevision").toUInt();

    drainQueuedProviderResults();

    QCOMPARE(*staleCancelRequestCount, 1);
    QCOMPARE(*staleCloseCount, 1);
    QCOMPARE(*replacementSessionCount, 1);
    QCOMPARE(*replacementMetadataRequestCount, 1);
    QCOMPARE(*replacementFrameRequestCount, 0);
    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(item.property("requestRevision").toUInt(), replacementRequestRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderLifecycleTest::providerClearIgnoresCancelledFrameAcknowledgement()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, cancelRequestCount, closeCount);
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

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);
    const uint clearedRequestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("errorString").toString(), QString());

    drainQueuedProviderResults();

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestRevision").toUInt(), clearedRequestRevision);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportProviderLifecycleTest::providerResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<SynchronousMetadataProviderSessionFactory>(
        metadataRequestCount, frameRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);

    QCoreApplication::processEvents();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
}

void ImageViewportProviderLifecycleTest::
    providerQueuedMetadataFromClosedGenerationIsIgnoredAfterReplacement()
{
    ImageSequenceFactory factory;
    const auto staleMetadataRequestCount = std::make_shared<int>(0);
    const auto staleFrameRequestCount = std::make_shared<int>(0);
    auto staleSessionFactory = std::make_shared<SynchronousMetadataProviderSessionFactory>(
        staleMetadataRequestCount, staleFrameRequestCount);
    CountingProviderAdapter staleAdapter(staleSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> staleResult(factory.fromProvider(&staleAdapter));
    QVERIFY(staleResult->sequence());

    const auto replacementSessionCount = std::make_shared<int>(0);
    const auto replacementMetadataRequestCount = std::make_shared<int>(0);
    const auto replacementFrameRequestCount = std::make_shared<int>(0);
    const auto replacementLastRequestedFrame = std::make_shared<int>(-1);
    const auto replacementCloseCount = std::make_shared<int>(0);
    auto replacementSessionFactory = std::make_shared<CountingProviderSessionFactory>(
        replacementSessionCount, replacementMetadataRequestCount, replacementFrameRequestCount,
        replacementLastRequestedFrame, replacementCloseCount);
    CountingProviderAdapter replacementAdapter(replacementSessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromProvider(&replacementAdapter));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSequence(staleResult->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*staleMetadataRequestCount, 1);
    QCOMPARE(*staleFrameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);

    item.setSequence(replacementResult->sequence());
    const uint replacementRequestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(*replacementSessionCount, 1);
    QCOMPARE(*replacementMetadataRequestCount, 1);
    QCOMPARE(*replacementFrameRequestCount, 0);
    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);

    drainQueuedProviderResults();

    QCOMPARE(*staleFrameRequestCount, 0);
    QCOMPARE(*replacementFrameRequestCount, 0);
    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(item.property("requestRevision").toUInt(), replacementRequestRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);

    QVERIFY(replacementSessionFactory->lastSession());
    emit replacementSessionFactory->lastSession()->metadataReady(
        replacementSessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(20.0, 10.0)));
    drainQueuedProviderResults();

    QCOMPARE(*replacementFrameRequestCount, 1);
    QCOMPARE(*replacementLastRequestedFrame, 0);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
}

void ImageViewportProviderLifecycleTest::providerFrameResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto frameRequestCount = std::make_shared<int>(0);
    auto sessionFactory
        = std::make_shared<SynchronousFrameProviderSessionFactory>(frameRequestCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));

    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
}

void ImageViewportProviderLifecycleTest::providerTerminalResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto metadataRequestCount = std::make_shared<int>(0);
    auto sessionFactory
        = std::make_shared<SynchronousFailureProviderSessionFactory>(metadataRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());

    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    verifyRequestStatusReasonPair(item);
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("metadata failed synchronously")));
}

void ImageViewportProviderLifecycleTest::providerUnsupportedResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto metadataRequestCount = std::make_shared<int>(0);
    auto sessionFactory
        = std::make_shared<SynchronousUnsupportedProviderSessionFactory>(metadataRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());

    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    verifyRequestStatusReasonPair(item);
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("metadata unsupported synchronously")));
}

QTEST_MAIN(ImageViewportProviderLifecycleTest)

#include "tst_imageviewport_provider_lifecycle.moc"
