#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

namespace {

void acknowledgePendingRenderCommit(ImageViewport& item)
{
    item.acknowledgeRenderCommitForTest(item.pendingRenderGenerationForTest(),
        item.activeRequestIdForTest(), item.pendingRenderPayloadIdForTest());
}

}

class ImageViewportProviderContractTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderContractTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void providerPublicValueTypesValidateTiming();
    void providerFactoryRejectsBaseAdapterWithoutSessionFactory();
    void providerFactoryRejectsContradictoryConstructionFacts();
    void providerFactoryRejectsInvalidKnownMetadata();
    void providerFactoryRejectsPublishedKnownMetadataLimits();
    void providerSessionEntryPointsUseSessionAffinity();
    void providerThreadSafeSessionEntryPointsUseControllerAffinity();
    void providerSequenceOpensSessionAfterAdapterDestruction();
    void providerSharedSequenceUsesIndependentViewportSessions();
    void providerSessionOpenFailureKeepsReplacementObservable();
    void reassigningSameProviderSequenceStartsNewGeneration();
};

void ImageViewportProviderContractTest::providerPublicValueTypesValidateTiming()
{
    const ImageSequenceProviderRequestToken defaultToken;
    QCOMPARE(defaultToken.id(), 0U);
    QCOMPARE(defaultToken.isValid(), false);
    QCOMPARE(defaultToken, ImageSequenceProviderRequestToken());

    const ImageSequenceProviderRequestToken token(42);
    QCOMPARE(token.id(), 42U);
    QCOMPARE(token.isValid(), true);
    QVERIFY(token != defaultToken);
    QCOMPARE(token, ImageSequenceProviderRequestToken(42));

    const ImageSequenceProviderMetadata emptyMetadata;
    QCOMPARE(emptyMetadata.isSpecified(), false);
    QCOMPARE(emptyMetadata.isValid(), false);
    QCOMPARE(emptyMetadata.isStill(), false);
    QCOMPARE(emptyMetadata.isTimedFrameList(), false);
    QCOMPARE(emptyMetadata.frameDurations(), QVector<int>());

    const ImageSequenceProviderMetadata stillMetadata
        = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    QCOMPARE(stillMetadata.isSpecified(), true);
    QCOMPARE(stillMetadata.isValid(), true);
    QCOMPARE(stillMetadata.isStill(), true);
    QCOMPARE(stillMetadata.isTimedFrameList(), false);
    QCOMPARE(stillMetadata.logicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(stillMetadata.frameDurations(), QVector<int>());

    const ImageSequenceProviderMetadata fixedDurationMetadata
        = ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100);
    QCOMPARE(fixedDurationMetadata.isSpecified(), true);
    QCOMPARE(fixedDurationMetadata.isValid(), true);
    QCOMPARE(fixedDurationMetadata.isStill(), false);
    QCOMPARE(fixedDurationMetadata.isTimedFrameList(), true);
    QCOMPARE(fixedDurationMetadata.frameDurations(), QVector<int>({ 100, 100, 100 }));

    const ImageSequenceProviderMetadata overLimitFixedDurationMetadata
        = ImageSequenceProviderMetadata::fixedDurationFrames(
            QSizeF(16.0, 8.0), ImageSequenceLimits::maximumTimedListFrameCount() + 2, 100);
    QCOMPARE(overLimitFixedDurationMetadata.isSpecified(), true);
    QCOMPARE(overLimitFixedDurationMetadata.isTimedFrameList(), true);
    QCOMPARE(overLimitFixedDurationMetadata.frameDurations().size(),
        ImageSequenceLimits::maximumTimedListFrameCount() + 1);

    const ImageSequenceProviderMetadata zeroDurationMetadata
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 0 });
    QCOMPARE(zeroDurationMetadata.isSpecified(), true);
    QCOMPARE(zeroDurationMetadata.isValid(), false);

    const ImageSequenceProviderMetadata negativeDurationMetadata
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, -1 });
    QCOMPARE(negativeDurationMetadata.isSpecified(), true);
    QCOMPARE(negativeDurationMetadata.isValid(), false);

    const ImageSequenceProviderMetadata infiniteSizeMetadata = ImageSequenceProviderMetadata::still(
        QSizeF(std::numeric_limits<double>::infinity(), 8.0));
    QCOMPARE(infiniteSizeMetadata.isSpecified(), true);
    QCOMPARE(infiniteSizeMetadata.isValid(), false);

    const ImageSequenceProviderMetadata fractionalSizeMetadata
        = ImageSequenceProviderMetadata::still(QSizeF(16.5, 8.0));
    QCOMPARE(fractionalSizeMetadata.isSpecified(), true);
    QCOMPARE(fractionalSizeMetadata.isValid(), false);

    const ImageSequenceProviderMetadata invalidFixedDurationMetadata
        = ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 2, 0);
    QCOMPARE(invalidFixedDurationMetadata.isSpecified(), true);
    QCOMPARE(invalidFixedDurationMetadata.isValid(), false);

    const ImageSequenceProviderFrameMetadata emptyFrameMetadata;
    QCOMPARE(emptyFrameMetadata.isValid(), false);
    QCOMPARE(emptyFrameMetadata.isStillFrame(), false);
    QCOMPARE(emptyFrameMetadata.isTimedFrame(), false);
    QCOMPARE(emptyFrameMetadata.frame(), -1);
    QCOMPARE(emptyFrameMetadata.frameStartPosition(), -1);
    QCOMPARE(emptyFrameMetadata.frameDuration(), -1);

    const ImageSequenceProviderFrameMetadata stillFrameMetadata
        = ImageSequenceProviderFrameMetadata::stillFrame();
    QCOMPARE(stillFrameMetadata.isValid(), true);
    QCOMPARE(stillFrameMetadata.isStillFrame(), true);
    QCOMPARE(stillFrameMetadata.isTimedFrame(), false);
    QCOMPARE(stillFrameMetadata.frame(), 0);
    QCOMPARE(stillFrameMetadata.frameStartPosition(), -1);
    QCOMPARE(stillFrameMetadata.frameDuration(), -1);

    const ImageSequenceProviderFrameMetadata timedFrameMetadata
        = ImageSequenceProviderFrameMetadata::timedFrame(1, 100, 250);
    QCOMPARE(timedFrameMetadata.isValid(), true);
    QCOMPARE(timedFrameMetadata.isStillFrame(), false);
    QCOMPARE(timedFrameMetadata.isTimedFrame(), true);
    QCOMPARE(timedFrameMetadata.frame(), 1);
    QCOMPARE(timedFrameMetadata.frameStartPosition(), 100);
    QCOMPARE(timedFrameMetadata.frameDuration(), 250);

    const ImageSequenceProviderFrameMetadata unknownDurationTimedFrameMetadata
        = ImageSequenceProviderFrameMetadata::timedFrame(1, 100);
    QCOMPARE(unknownDurationTimedFrameMetadata.isValid(), true);
    QCOMPARE(unknownDurationTimedFrameMetadata.isTimedFrame(), true);
    QCOMPARE(unknownDurationTimedFrameMetadata.frame(), 1);
    QCOMPARE(unknownDurationTimedFrameMetadata.frameStartPosition(), 100);
    QCOMPARE(unknownDurationTimedFrameMetadata.frameDuration(), -1);

    QCOMPARE(ImageSequenceProviderFrameMetadata::timedFrame(-1, 100).isValid(), false);
    QCOMPARE(ImageSequenceProviderFrameMetadata::timedFrame(1, -1).isValid(), false);
    QCOMPARE(ImageSequenceProviderFrameMetadata::timedFrame(1, 100, 0).isValid(), false);

    const ImageSequenceProviderKnownFacts emptyFacts;
    QCOMPARE(emptyFacts.isSpecified(), false);
    QCOMPARE(emptyFacts.isValid(), false);
    QCOMPARE(emptyFacts.isComplete(), false);
    QCOMPARE(emptyFacts.logicalSize(), QSizeF());
    QCOMPARE(emptyFacts.frameCount(), -1);
    QCOMPARE(emptyFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderKnownFacts logicalSizeFacts
        = ImageSequenceProviderKnownFacts::logicalSize(QSizeF(16.0, 8.0));
    QCOMPARE(logicalSizeFacts.isSpecified(), true);
    QCOMPARE(logicalSizeFacts.isValid(), true);
    QCOMPARE(logicalSizeFacts.isComplete(), false);
    QCOMPARE(logicalSizeFacts.logicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(logicalSizeFacts.frameCount(), -1);
    QCOMPARE(logicalSizeFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderKnownFacts stillFacts
        = ImageSequenceProviderKnownFacts::still(QSizeF(16.0, 8.0));
    QCOMPARE(stillFacts.isSpecified(), true);
    QCOMPARE(stillFacts.isValid(), true);
    QCOMPARE(stillFacts.isComplete(), true);
    QCOMPARE(stillFacts.logicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(stillFacts.frameCount(), 1);
    QCOMPARE(stillFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderKnownFacts countFacts
        = ImageSequenceProviderKnownFacts::timedFrameCount(QSizeF(16.0, 8.0), 3);
    QCOMPARE(countFacts.isSpecified(), true);
    QCOMPARE(countFacts.isValid(), true);
    QCOMPARE(countFacts.isComplete(), false);
    QCOMPARE(countFacts.logicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(countFacts.frameCount(), 3);
    QCOMPARE(countFacts.frameDurations(), QVector<int>());

    const ImageSequenceProviderKnownFacts fixedFacts
        = ImageSequenceProviderKnownFacts::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100);
    QCOMPARE(fixedFacts.isSpecified(), true);
    QCOMPARE(fixedFacts.isValid(), true);
    QCOMPARE(fixedFacts.isComplete(), true);
    QCOMPARE(fixedFacts.frameCount(), 3);
    QCOMPARE(fixedFacts.frameDurations(), QVector<int>({ 100, 100, 100 }));

    const ImageSequenceProviderKnownFacts listFacts
        = ImageSequenceProviderKnownFacts::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    QCOMPARE(listFacts.isSpecified(), true);
    QCOMPARE(listFacts.isValid(), true);
    QCOMPARE(listFacts.isComplete(), true);
    QCOMPARE(listFacts.frameCount(), 2);
    QCOMPARE(listFacts.frameDurations(), QVector<int>({ 100, 250 }));

    PlaybackFallbackSession session;
    session.requestPlayback(token, 7, 125);
    QCOMPARE(session.frameRequestCount, 1);
    QCOMPARE(session.lastFrameToken, token);
    QCOMPARE(session.lastFrame, 7);
    session.requestPosition(token, 8, 349);
    QCOMPARE(session.frameRequestCount, 2);
    QCOMPARE(session.lastFrameToken, token);
    QCOMPARE(session.lastFrame, 8);

    NullSessionFactoryProviderAdapter nullAdapter;
    QCOMPARE(
        nullAdapter.threadingContract(), ImageSequenceProviderThreadingContract::AffinityBound);
}

void ImageViewportProviderContractTest::providerFactoryRejectsBaseAdapterWithoutSessionFactory()
{
    ImageSequenceFactory factory;
    NullSessionFactoryProviderAdapter adapter;

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("session")));
}

void ImageViewportProviderContractTest::providerFactoryRejectsContradictoryConstructionFacts()
{
    auto verifyRejectedConstructionFacts
        = [](const ImageSequenceProviderMetadata& metadata,
              ImageSequenceProviderAdapter::CapabilitySupport timedPlaybackSupport,
              ImageSequenceProviderAdapter::CapabilitySupport frameSeekSupport,
              ImageSequenceProviderAdapter::CapabilitySupport positionSeekSupport) {
              ImageSequenceFactory factory;
              const auto sessionCount = std::make_shared<int>(0);
              const auto metadataRequestCount = std::make_shared<int>(0);
              const auto frameRequestCount = std::make_shared<int>(0);
              const auto lastRequestedFrame = std::make_shared<int>(-1);
              const auto closeCount = std::make_shared<int>(0);
              auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
                  metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
              CountingProviderAdapter adapter(sessionFactory, metadata, timedPlaybackSupport,
                  frameSeekSupport, positionSeekSupport);

              QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

              QVERIFY(result);
              QCOMPARE(result->sequence(), nullptr);
              QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
              QVERIFY(result->errorString().contains(QStringLiteral("provider metadata")));
              QCOMPARE(*sessionCount, 0);
              QCOMPARE(*metadataRequestCount, 0);
              QCOMPARE(*frameRequestCount, 0);
          };

    verifyRejectedConstructionFacts(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable);
    verifyRejectedConstructionFacts(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable);
    verifyRejectedConstructionFacts(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable);
    verifyRejectedConstructionFacts(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    verifyRejectedConstructionFacts(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue);
}

void ImageViewportProviderContractTest::providerFactoryRejectsInvalidKnownMetadata()
{
    auto verifyRejectedKnownMetadata
        = [](const ImageSequenceProviderMetadata& metadata, const QString& expectedDiagnostic) {
              ImageSequenceFactory factory;
              const auto sessionCount = std::make_shared<int>(0);
              const auto metadataRequestCount = std::make_shared<int>(0);
              const auto frameRequestCount = std::make_shared<int>(0);
              const auto lastRequestedFrame = std::make_shared<int>(-1);
              const auto closeCount = std::make_shared<int>(0);
              auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
                  metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
              CountingProviderAdapter adapter(sessionFactory, metadata);

              QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

              QVERIFY(result);
              QCOMPARE(result->sequence(), nullptr);
              QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
              QVERIFY(result->errorString().contains(expectedDiagnostic));
              QCOMPARE(*sessionCount, 0);
              QCOMPARE(*metadataRequestCount, 0);
              QCOMPARE(*frameRequestCount, 0);
          };

    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::still(QSizeF(std::numeric_limits<double>::infinity(), 8.0)),
        QStringLiteral("provider metadata is invalid"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::still(QSizeF(16.5, 8.0)),
        QStringLiteral("provider metadata is invalid"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 0, 100),
        QStringLiteral("provider metadata is invalid"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {}),
        QStringLiteral("provider metadata is invalid"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 0 }),
        QStringLiteral("positive"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, -1 }),
        QStringLiteral("positive"));
}

void ImageViewportProviderContractTest::providerFactoryRejectsPublishedKnownMetadataLimits()
{
    auto verifyRejectedKnownMetadata
        = [](const ImageSequenceProviderMetadata& metadata, const QString& expectedDiagnostic) {
              ImageSequenceFactory factory;
              const auto sessionCount = std::make_shared<int>(0);
              const auto metadataRequestCount = std::make_shared<int>(0);
              const auto frameRequestCount = std::make_shared<int>(0);
              const auto lastRequestedFrame = std::make_shared<int>(-1);
              const auto closeCount = std::make_shared<int>(0);
              auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
                  metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
              CountingProviderAdapter adapter(sessionFactory, metadata);

              QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

              QVERIFY(result);
              QCOMPARE(result->sequence(), nullptr);
              QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
              QVERIFY(result->errorString().contains(expectedDiagnostic));
              QCOMPARE(*sessionCount, 0);
              QCOMPARE(*metadataRequestCount, 0);
              QCOMPARE(*frameRequestCount, 0);
          };

    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::still(
                                    QSizeF(ImageSequenceLimits::maximumLogicalWidth() + 1, 8.0)),
        QStringLiteral("maximumLogicalWidth"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::still(
                                    QSizeF(16.0, ImageSequenceLimits::maximumLogicalHeight() + 1)),
        QStringLiteral("maximumLogicalHeight"));
    verifyRejectedKnownMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0),
            QVector<int>(ImageSequenceLimits::maximumTimedListFrameCount() + 1, 1)),
        QStringLiteral("maximumTimedListFrameCount"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0),
                                    { ImageSequenceLimits::maximumFrameDuration() + 1 }),
        QStringLiteral("maximumFrameDuration"));
    verifyRejectedKnownMetadata(ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0),
                                    { ImageSequenceLimits::maximumTotalSequenceDuration(), 1 }),
        QStringLiteral("maximumTotalSequenceDuration"));
}

void ImageViewportProviderContractTest::providerSessionEntryPointsUseSessionAffinity()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    const auto metadataRequestThread = std::make_shared<QThread*>(nullptr);
    const auto frameRequestThread = std::make_shared<QThread*>(nullptr);
    const auto playbackRequestThread = std::make_shared<QThread*>(nullptr);
    const auto cancelRequestThread = std::make_shared<QThread*>(nullptr);
    const auto closeThread = std::make_shared<QThread*>(nullptr);
    auto sessionFactory
        = std::make_shared<AffinityProviderSessionFactory>(&workerThread, metadataRequestThread,
            frameRequestThread, playbackRequestThread, cancelRequestThread, closeThread);
    CountingProviderAdapter adapter(sessionFactory);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    {
        ImageViewport item;
        item.setSize(QSizeF(100.0, 100.0));
        item.setSequence(result->sequence());

        AffinityProviderSession* session = sessionFactory->lastSession();
        QVERIFY(session);
        QCOMPARE(*metadataRequestThread, &workerThread);

        emit session->metadataReady(session->lastMetadataToken(),
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
        drainQueuedProviderResults();
        QCOMPARE(*frameRequestThread, &workerThread);

        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        ImageFrame frame(image);
        emit session->imageFrameWithMetadataReady(session->lastFrameToken(), &frame,
            ImageSequenceProviderFrameMetadata::timedFrame(0, 0));
        drainQueuedProviderResults();
        acknowledgePendingRenderCommit(item);

        QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
        item.advancePlaybackForTest(100);
        QCOMPARE(*playbackRequestThread, &workerThread);
        QVERIFY(session->lastPlaybackToken().isValid());

        QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
        QCOMPARE(*cancelRequestThread, &workerThread);
    }

    QTRY_COMPARE(*closeThread, &workerThread);
}

void ImageViewportProviderContractTest::providerThreadSafeSessionEntryPointsUseControllerAffinity()
{
    QThread workerThread;
    workerThread.start();
    const auto workerCleanup = qScopeGuard([&workerThread]() {
        workerThread.quit();
        workerThread.wait(1000);
    });

    QThread* controllerThread = QThread::currentThread();
    const auto metadataRequestThread = std::make_shared<QThread*>(nullptr);
    const auto frameRequestThread = std::make_shared<QThread*>(nullptr);
    const auto playbackRequestThread = std::make_shared<QThread*>(nullptr);
    const auto cancelRequestThread = std::make_shared<QThread*>(nullptr);
    const auto closeThread = std::make_shared<QThread*>(nullptr);
    auto sessionFactory
        = std::make_shared<AffinityProviderSessionFactory>(&workerThread, metadataRequestThread,
            frameRequestThread, playbackRequestThread, cancelRequestThread, closeThread);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderThreadingContract::ThreadSafe);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    {
        ImageViewport item;
        item.setSize(QSizeF(100.0, 100.0));
        item.setSequence(result->sequence());

        AffinityProviderSession* session = sessionFactory->lastSession();
        QVERIFY(session);
        QCOMPARE(*metadataRequestThread, controllerThread);

        emit session->metadataReady(session->lastMetadataToken(),
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
        QCOMPARE(*frameRequestThread, nullptr);
        drainQueuedProviderResults();
        QCOMPARE(*frameRequestThread, controllerThread);

        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        ImageFrame frame(image);
        emit session->imageFrameWithMetadataReady(session->lastFrameToken(), &frame,
            ImageSequenceProviderFrameMetadata::timedFrame(0, 0));
        drainQueuedProviderResults();
        acknowledgePendingRenderCommit(item);

        QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
        item.advancePlaybackForTest(100);
        QCOMPARE(*playbackRequestThread, controllerThread);
        QVERIFY(session->lastPlaybackToken().isValid());

        QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
        QCOMPARE(*cancelRequestThread, controllerThread);
    }

    QTRY_COMPARE(*closeThread, &workerThread);
}

void ImageViewportProviderContractTest::providerSequenceOpensSessionAfterAdapterDestruction()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);

    QScopedPointer<ImageSequenceFactoryResult> result;
    {
        CountingProviderAdapter adapter(sessionFactory);
        result.reset(factory.fromProvider(&adapter));
    }

    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), -1);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), -1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), -1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
}

void ImageViewportProviderContractTest::providerSharedSequenceUsesIndependentViewportSessions()
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

    ImageViewport first;
    ImageViewport second;
    first.setSequence(result->sequence());
    second.setSequence(result->sequence());
    const QMetaObject* metaObject = first.metaObject();

    QCOMPARE(*sessionCount, 2);
    QCOMPARE(*metadataRequestCount, 2);
    QCOMPARE(*frameRequestCount, 0);
    CountingProviderSession* firstSession = sessionFactory->sessionAt(0);
    CountingProviderSession* secondSession = sessionFactory->sessionAt(1);
    QVERIFY(firstSession);
    QVERIFY(secondSession);
    QVERIFY(firstSession != secondSession);
    QCOMPARE(firstSession->lastMetadataToken().id(), secondSession->lastMetadataToken().id());

    emit firstSession->metadataReady(
        firstSession->lastMetadataToken(), ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(
        first.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(first.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(first.property("requestedFrame").toInt(), 0);
    QCOMPARE(first.property("requestedPosition").toInt(), -1);
    QCOMPARE(first.property("frameCount").toInt(), 1);
    QCOMPARE(second.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(second.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(second.property("requestedFrame").toInt(), -1);
    QCOMPARE(second.property("requestedPosition").toInt(), -1);
    QCOMPARE(second.property("frameCount").toInt(), -1);

    emit secondSession->metadataReady(secondSession->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(20.0, 10.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(first.property("frameCount").toInt(), 1);
    QCOMPARE(first.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(second.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(second.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(second.property("requestedFrame").toInt(), 0);
    QCOMPARE(second.property("requestedPosition").toInt(), -1);
    QCOMPARE(second.property("frameCount").toInt(), 1);
}

void ImageViewportProviderContractTest::providerSessionOpenFailureKeepsReplacementObservable()
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
    auto sessionFactory = std::make_shared<FailingProviderSessionFactory>(sessionCount);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse);
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
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");

    item.setSequence(replacementResult->sequence());

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("session")));

    const RevisionToken failedRequestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
}

void ImageViewportProviderContractTest::reassigningSameProviderSequenceStartsNewGeneration()
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
    const RevisionToken initialRequestRevision = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*closeCount, 0);

    item.setSequence(result->sequence());

    QCOMPARE(*sessionCount, 2);
    QCOMPARE(*metadataRequestCount, 2);
    QCOMPARE(*frameRequestCount, 0);
    drainQueuedProviderResults();
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    verifyRevisionChanged(item, "requestRevision", initialRequestRevision);
}

QTEST_MAIN(ImageViewportProviderContractTest)

#include "tst_imageviewport_provider_contract.moc"
