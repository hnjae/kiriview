#include "imageviewport_provider_test_support.h"

class ImageViewportProviderTerminalRecoveryTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderTerminalRecoveryTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void providerMetadataFailureStopsPendingPlayback();
    void providerGenerationTerminalFailureRejectsDisplayCommands();
    void providerGenerationTerminalFailureAcceptsControlCommands();
    void providerFrameFailureKeepsGenerationSeekable();
    void providerFrameFailureRetainsDisplayAndClearsOnSeek();
    void providerFrameFailureKeepsGenerationPositionSeekable();
    void providerTimedPlayAfterFrameFailureRestartsPlaybackRequest();
    void providerFrameFailureAcceptsControlCommands();
    void providerGenerationTerminalUnsupportedAcceptsControlCommands();
    void providerMetadataUnsupportedRetainsReplacementDisplayOnlyAsFallback();
    void providerFrameUnsupportedKeepsGenerationSeekable();
    void providerFrameUnsupportedRetainsDisplayAndClearsOnSeek();
    void providerFrameUnsupportedKeepsGenerationPositionSeekable();
    void providerFrameCancellationReportsProviderFailure();
    void providerFrameCancellationRetainsDisplayAndClearsOnSeek();
};

void ImageViewportProviderTerminalRecoveryTest::providerMetadataFailureStopsPendingPlayback()
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

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));

    QVERIFY(sessionFactory->lastSession());
    emitProviderFailed(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
}

void ImageViewportProviderTerminalRecoveryTest::
    providerGenerationTerminalFailureRejectsDisplayCommands()
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
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, -1).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, -1).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportProviderTerminalRecoveryTest::
    providerGenerationTerminalFailureAcceptsControlCommands()
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
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    const ImageViewportRevisionToken failedRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    const ImageViewportRevisionToken unsupportedCommandRevision
        = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", unsupportedCommandRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);

    const ImageViewportRevisionToken clearedCommandRevision
        = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), clearedCommandRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
}

void ImageViewportProviderTerminalRecoveryTest::providerFrameFailureKeepsGenerationSeekable()
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

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    emitProviderFailed(
        sessionFactory->lastSession(), frameToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryFrameCount(item), 1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("frame decode failed")));

    const ImageViewportRevisionToken terminalRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    emitProviderProgress(sessionFactory->lastSession(), frameToken, 1.0);
    drainQueuedProviderResults();
    emitProviderWaiting(sessionFactory->lastSession(), frameToken);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), terminalRequestRevision);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("frame decode failed")));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalRecoveryTest::providerFrameFailureRetainsDisplayAndClearsOnSeek()
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

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken failedToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    emitProviderFailed(
        sessionFactory->lastSession(), failedToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("frame decode failed")));

    const ImageViewportRevisionToken failedRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken retainedDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken failedCommandRevision
        = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, -1).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    verifyRevisionChanged(item, "commandRevision", failedCommandRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), retainedDisplayRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalRecoveryTest::
    providerFrameFailureKeepsGenerationPositionSeekable()
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

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken failedToken
        = sessionFactory->lastSession()->lastFrameToken();
    emitProviderFailed(
        sessionFactory->lastSession(), failedToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("frame decode failed")));

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalRecoveryTest::
    providerTimedPlayAfterFrameFailureRestartsPlaybackRequest()
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

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken failedToken
        = sessionFactory->lastSession()->lastFrameToken();
    emitProviderFailed(
        sessionFactory->lastSession(), failedToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("frame decode failed")));

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken playbackToken
        = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QVERIFY(playbackToken != failedToken);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(viewportErrorString(item), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), playbackToken, &frame, 1, 100);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalRecoveryTest::providerFrameFailureAcceptsControlCommands()
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

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    emitProviderFailed(
        sessionFactory->lastSession(), frameToken, QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    const ImageViewportRevisionToken failedRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, -1).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const ImageViewportRevisionToken invalidCommandRevision
        = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", invalidCommandRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("frame decode failed")));

    const ImageViewportRevisionToken clearedCommandRevision
        = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), clearedCommandRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("frame decode failed")));
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*closeCount, 0);
}

void ImageViewportProviderTerminalRecoveryTest::
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderUnsupported(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest,
        QStringLiteral("unsupported codec"));
    drainQueuedProviderResults();

    const ImageViewportRevisionToken unsupportedRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    const ImageViewportRevisionToken unsupportedCommandRevision
        = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", unsupportedCommandRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(
        requestReasonValue(item), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), unsupportedRequestRevision);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("unsupported codec")));

    const ImageViewportRevisionToken clearedCommandRevision
        = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), clearedCommandRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(
        requestReasonValue(item), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), unsupportedRequestRevision);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("unsupported codec")));
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
}

void ImageViewportProviderTerminalRecoveryTest::
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
    item.setPresentationTarget(ImageViewportPresentationTarget(previousResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);

    QVERIFY(sessionFactory->lastSession());
    emitProviderUnsupported(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest,
        QStringLiteral("unsupported replacement metadata"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(
        requestReasonValue(item), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("unsupported replacement metadata")));

    const ImageViewportRevisionToken failedRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), failedRequestRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
}

void ImageViewportProviderTerminalRecoveryTest::providerFrameUnsupportedKeepsGenerationSeekable()
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

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    emitProviderUnsupported(sessionFactory->lastSession(), frameToken,
        ImageSequenceProviderUnsupportedCause::PayloadRejection,
        QStringLiteral("unsupported frame shape"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryFrameCount(item), 1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("unsupported frame shape")));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
}

void ImageViewportProviderTerminalRecoveryTest::
    providerFrameUnsupportedRetainsDisplayAndClearsOnSeek()
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

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken unsupportedToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    emitProviderUnsupported(sessionFactory->lastSession(), unsupportedToken,
        ImageSequenceProviderUnsupportedCause::PayloadRejection,
        QStringLiteral("unsupported frame shape"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("unsupported frame shape")));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalRecoveryTest::
    providerFrameUnsupportedKeepsGenerationPositionSeekable()
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

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken unsupportedToken
        = sessionFactory->lastSession()->lastFrameToken();
    emitProviderUnsupported(sessionFactory->lastSession(), unsupportedToken,
        ImageSequenceProviderUnsupportedCause::PayloadRejection,
        QStringLiteral("unsupported frame shape"));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("unsupported frame shape")));

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderTerminalRecoveryTest::providerFrameCancellationReportsProviderFailure()
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

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    emitProviderCancelled(
        sessionFactory->lastSession(), frameToken, QStringLiteral("cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("cancelled by provider")));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
}

void ImageViewportProviderTerminalRecoveryTest::
    providerFrameCancellationRetainsDisplayAndClearsOnSeek()
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

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken cancelledToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    emitProviderCancelled(
        sessionFactory->lastSession(), cancelledToken, QStringLiteral("cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("cancelled by provider")));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
}
QTEST_MAIN(ImageViewportProviderTerminalRecoveryTest)

#include "tst_imageviewport_provider_terminal_recovery.moc"
