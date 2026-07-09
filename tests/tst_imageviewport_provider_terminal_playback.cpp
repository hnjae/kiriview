#include "imageviewport_provider_test_support.h"

class ImageViewportProviderTerminalPlaybackTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderTerminalPlaybackTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void providerTimedPlaybackStopsOnFrameFailure();
    void providerTimedPlayAfterPlaybackFailureRestartsPlaybackRequest();
    void providerTimedPlaybackCancellationReportsProviderFailure();
    void providerTimedPlayAfterPlaybackCancellationRestartsPlaybackRequest();
    void providerTimedPlaybackUnsupportedReportsUnsupportedRequest();
    void providerTimedPlayAfterPlaybackUnsupportedRestartsPlaybackRequest();
};

void ImageViewportProviderTerminalPlaybackTest::providerTimedPlaybackStopsOnFrameFailure()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.play().outcome(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));

    emitProviderFailed(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastFrameToken(), QStringLiteral("playback frame failed"));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("playback frame failed")));

    QCOMPARE(item.seekToPosition(0).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalPlaybackTest::
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.play().outcome(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
    const ImageSequenceProviderRequestToken failedToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);

    emitProviderFailed(
        sessionFactory->lastSession(), failedToken, QStringLiteral("playback frame failed"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("playback frame failed")));

    QCOMPARE(item.play().outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken retryToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*playbackRequestCount, 2);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QVERIFY(retryToken != failedToken);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), retryToken, &frame, 1, 100);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalPlaybackTest::
    providerTimedPlaybackCancellationReportsProviderFailure()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.play().outcome(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    emitProviderCancelled(sessionFactory->lastSession(), playbackToken,
        QStringLiteral("playback cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("playback cancelled by provider")));
}

void ImageViewportProviderTerminalPlaybackTest::
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.play().outcome(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
    const ImageSequenceProviderRequestToken cancelledToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);

    emitProviderCancelled(sessionFactory->lastSession(), cancelledToken,
        QStringLiteral("playback cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("playback cancelled by provider")));

    QCOMPARE(item.play().outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken retryToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*playbackRequestCount, 2);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QVERIFY(retryToken != cancelledToken);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), retryToken, &frame, 1, 100);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalPlaybackTest::
    providerTimedPlaybackUnsupportedReportsUnsupportedRequest()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.play().outcome(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);

    emitProviderUnsupported(sessionFactory->lastSession(), playbackToken,
        ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest,
        QStringLiteral("playback request unsupported"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(
        requestReasonValue(item), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("playback request unsupported")));

    QCOMPARE(item.seek(0).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
}

void ImageViewportProviderTerminalPlaybackTest::
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.play().outcome(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
    const ImageSequenceProviderRequestToken unsupportedToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);

    emitProviderUnsupported(sessionFactory->lastSession(), unsupportedToken,
        ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest,
        QStringLiteral("playback request unsupported"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(
        requestReasonValue(item), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("playback request unsupported")));

    QCOMPARE(item.play().outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken retryToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*playbackRequestCount, 2);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QVERIFY(retryToken != unsupportedToken);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), retryToken, &frame, 1, 100);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
    QCOMPARE(viewportErrorString(item), QString());
}
QTEST_MAIN(ImageViewportProviderTerminalPlaybackTest)

#include "tst_imageviewport_provider_terminal_playback.moc"
