#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

class ImageViewportProviderRequestsTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderRequestsTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void providerRequestTokensAreUniqueWithinSession();
    void providerFrameSeekUsesFrameRequest();
    void providerFrameSeekBeforeMetadataResolvesAfterMetadata();
    void providerStalePreMetadataSeekIgnoresStaleRejection();
    void providerStillMetadataRevisesAcceptedSeekObservations();
    void providerInvalidPreMetadataSeekCanStartPlaybackAfterMetadata();
    void providerPositionSeekBeforeMetadataResolvesAfterMetadata();
    void providerPositionSeekUsesPositionRequest();
    void providerTotalDurationPositionSeekUsesPositionRequest();
    void providerPreMetadataPositionSeekResolvesToPositionRequest();
    void providerTotalDurationSeekBeforeMetadataResolvesFinalFrame();
    void providerPositionSeekBeforeStillMetadataKeepsGenerationSeekable();
    void providerPlaybackBeforeStillMetadataKeepsGenerationSeekable();
    void providerFrameSeekQueuesBehindActiveFrameRequest();
    void waitProjectionRevisionChangesOnlyWhenPublicReasonChanges();
    void providerTimedSameFrameSeekRetiresActiveRequest();
    void providerQueuedFrameRequestSchedulerFailureReportsProviderFailure();
    void secondaryProviderQueuedFrameRequestSchedulerFailureReportsProviderFailure();
    void providerTimedFrameSeekRequestsSelectedFrame();
    void providerTimedFrameSeekWithoutDiagnosticsDoesNotNotify();
    void providerTimedFrameCommitPreservesGeometryObservations();
    void providerTimedFrameSeekCancelsStaleRequest();
    void providerTimedPositionSeekRequestsResolvedFrame();
    void secondaryProviderFrameSeekBeforeMetadataResolvesAfterMetadata();
    void secondaryProviderPositionSeekBeforeMetadataResolvesAfterMetadata();
    void secondaryProviderFrameSeekUsesFrameRequest();
    void secondaryProviderPositionSeekRequestsResolvedFrame();
    void secondaryProviderFrameSeekRetainsDisplayedSpreadUntilCommit();
    void secondaryProviderPositionSeekRetainsDisplayedSpreadUntilCommit();
    void secondaryProviderInvalidAndUnsupportedSeekCommandsPreserveRequest();
    void secondaryProviderFrameSeekIgnoresStaleFrameResult();
};

void ImageViewportProviderRequestsTest::providerRequestTokensAreUniqueWithinSession()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    QVERIFY(metadataToken.isValid());
    QCOMPARE(*metadataRequestCount, 1);

    emitProviderMetadataReady(sessionFactory->lastSession(), metadataToken,
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    const ImageSequenceProviderRequestToken initialFrameToken
        = sessionFactory->lastSession()->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());
    QVERIFY(initialFrameToken != metadataToken);
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken seekFrameToken
        = sessionFactory->lastSession()->lastFrameToken();

    QVERIFY(seekFrameToken.isValid());
    QVERIFY(seekFrameToken != metadataToken);
    QVERIFY(seekFrameToken != initialFrameToken);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*cancelRequestCount, 1);
}

void ImageViewportProviderRequestsTest::providerFrameSeekUsesFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<ImageSequenceProviderRequestToken>(),
        positionRequestCount, lastPositionFrame, lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*positionRequestCount, 0);
    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(*positionRequestCount, 0);
    QCOMPARE(*lastPositionFrame, -1);
    QCOMPARE(*lastRequestedPosition, -1);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
}

void ImageViewportProviderRequestsTest::providerFrameSeekBeforeMetadataResolvesAfterMetadata()
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

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    const quint64 preMetadataRequestId = activeRequestIdForTest(item);
    QVERIFY(preMetadataRequestId > 0);
    const ImageViewportRevisionToken preMetadataRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QVERIFY(activeRequestIdForTest(item) > preMetadataRequestId);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);
    verifyRevisionChanged(item, "requestRevision", preMetadataRequestRevision);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportProviderRequestsTest::providerStalePreMetadataSeekIgnoresStaleRejection()
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

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 3).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), -1);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderRequestsTest::providerStillMetadataRevisesAcceptedSeekObservations()
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

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), -1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewport::CapabilitySupport::Unavailable);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewport::CapabilitySupport::Unavailable);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewport::CapabilitySupport::Unavailable);
    const ImageViewportRevisionToken preMetadataRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 0);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewport::CapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewport::CapabilitySupport::False);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewport::CapabilitySupport::False);
    verifyRevisionChanged(item, "requestRevision", preMetadataRequestRevision);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportProviderRequestsTest::
    providerInvalidPreMetadataSeekCanStartPlaybackAfterMetadata()
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 3).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(primaryRequestedFrame(item), 3);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "InvalidRequest"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(*frameRequestCount, 0);

    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 0);
    QCOMPARE(*lastPlaybackPosition, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
}

void ImageViewportProviderRequestsTest::providerPositionSeekBeforeMetadataResolvesAfterMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<ImageSequenceProviderRequestToken>(),
        positionRequestCount, lastPositionFrame, lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Primary, 349).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), 349);
    const ImageViewportRevisionToken preMetadataRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 349);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 349);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);
    verifyRevisionChanged(item, "requestRevision", preMetadataRequestRevision);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportProviderRequestsTest::providerPositionSeekUsesPositionRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<ImageSequenceProviderRequestToken>(),
        positionRequestCount, lastPositionFrame, lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*positionRequestCount, 0);
    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Primary, 125).outcome(), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 125);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 125);
}

void ImageViewportProviderRequestsTest::providerTotalDurationPositionSeekUsesPositionRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<ImageSequenceProviderRequestToken>(),
        positionRequestCount, lastPositionFrame, lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Primary, 350).outcome(), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 350);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);
}

void ImageViewportProviderRequestsTest::providerPreMetadataPositionSeekResolvesToPositionRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<ImageSequenceProviderRequestToken>(),
        positionRequestCount, lastPositionFrame, lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Primary, 125).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*positionRequestCount, 0);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 125);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 125);
}

void ImageViewportProviderRequestsTest::providerTotalDurationSeekBeforeMetadataResolvesFinalFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<ImageSequenceProviderRequestToken>(),
        positionRequestCount, lastPositionFrame, lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Primary, 350).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 350);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastPositionToken(), &frame, 1, 100);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderRequestsTest::
    providerPositionSeekBeforeStillMetadataKeepsGenerationSeekable()
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

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Primary, 10).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), 10);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(
        requestReasonValue(item), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), 10);
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewport::CapabilitySupport::False);

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
}

void ImageViewportProviderRequestsTest::providerPlaybackBeforeStillMetadataKeepsGenerationSeekable()
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

    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(*frameRequestCount, 0);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(
        requestReasonValue(item), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewport::CapabilitySupport::False);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewport::CapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewport::CapabilitySupport::False);

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), -1);
}

void ImageViewportProviderRequestsTest::providerFrameSeekQueuesBehindActiveFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto sessionFactory
        = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
            frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount, lastCancelledToken);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    useSynchronousProviderQueueFlushSchedulerForTest(item);
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    CountingProviderSession* session = sessionFactory->lastSession();
    const ImageSequenceProviderRequestToken initialFrameToken = session->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    const ImageViewportRevisionToken providerWaitingRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledToken, initialFrameToken);
    const ImageSequenceProviderRequestToken seekFrameToken = session->lastFrameToken();
    QVERIFY(seekFrameToken.isValid());
    QVERIFY(seekFrameToken != initialFrameToken);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    verifyRevisionChanged(item, "requestRevision", providerWaitingRevision);
    QCOMPARE(stateSpy.count(), 2);

    QImage staleImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    staleImage.fill(Qt::transparent);
    ImageFrame staleFrame(staleImage);
    const ImageViewportRevisionToken activeRevision
        = revisionTokenProperty(item, "requestRevision");
    emitTimedProviderFrameReady(session, initialFrameToken, &staleFrame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), activeRevision);
    QCOMPARE(stateSpy.count(), 2);
}

void ImageViewportProviderRequestsTest::waitProjectionRevisionChangesOnlyWhenPublicReasonChanges()
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
    CountingProviderAdapter primaryAdapter(
        primarySessionFactory, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
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
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QVERIFY(primarySessionFactory->lastSession());
    QVERIFY(secondarySessionFactory->lastSession());
    const ImageSequenceProviderRequestToken primaryFrameToken
        = primarySessionFactory->lastSession()->lastFrameToken();
    const ImageSequenceProviderRequestToken secondaryMetadataToken
        = secondarySessionFactory->lastSession()->lastMetadataToken();
    QVERIFY(primaryFrameToken.isValid());
    QVERIFY(secondaryMetadataToken.isValid());

    const ImageViewportRevisionToken providerWaitingRevision
        = revisionTokenProperty(item, "requestRevision");

    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    emitProviderFrameReady(primarySessionFactory->lastSession(), primaryFrameToken, &primaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), providerWaitingRevision);

    emitProviderMetadataReady(secondarySessionFactory->lastSession(), secondaryMetadataToken,
        ImageSequenceProviderMetadata::still(QSizeF(8.0, 16.0)));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken secondaryFrameToken
        = secondarySessionFactory->lastSession()->lastFrameToken();
    QVERIFY(secondaryFrameToken.isValid());

    const ImageViewportRevisionToken secondaryProviderWaitingRevision
        = revisionTokenProperty(item, "requestRevision");
    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    emitProviderFrameReady(
        secondarySessionFactory->lastSession(), secondaryFrameToken, &secondaryFrame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "UploadPending"));
    verifyRevisionChanged(item, "requestRevision", secondaryProviderWaitingRevision);
}

void ImageViewportProviderRequestsTest::providerTimedSameFrameSeekRetiresActiveRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto sessionFactory
        = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
            frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount, lastCancelledToken);
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

    CountingProviderSession* session = sessionFactory->lastSession();
    const ImageSequenceProviderRequestToken initialFrameToken = session->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());
    const quint64 initialRequestId = activeRequestIdForTest(item);
    QVERIFY(initialRequestId > 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    const ImageViewportRevisionToken providerWaitingRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledToken, initialFrameToken);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RequestQueued"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    verifyRevisionChanged(item, "requestRevision", providerWaitingRevision);
    QCOMPARE(stateSpy.count(), 1);

    const ImageViewportRevisionToken queuedRevision
        = revisionTokenProperty(item, "requestRevision");
    drainQueuedProviderResults();

    const ImageSequenceProviderRequestToken activeFrameToken = session->lastFrameToken();
    QVERIFY(activeFrameToken.isValid());
    QVERIFY(activeFrameToken != initialFrameToken);
    const quint64 activeRequestId = activeRequestIdForTest(item);
    QVERIFY(activeRequestId > initialRequestId);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    verifyRevisionChanged(item, "requestRevision", queuedRevision);
    QCOMPARE(stateSpy.count(), 2);

    QImage staleImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    staleImage.fill(Qt::transparent);
    ImageFrame staleFrame(staleImage);
    const ImageViewportRevisionToken activeRevision
        = revisionTokenProperty(item, "requestRevision");
    emitTimedProviderFrameReady(session, initialFrameToken, &staleFrame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
    QCOMPARE(activeRequestIdForTest(item), activeRequestId);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), activeRevision);
    QCOMPARE(stateSpy.count(), 2);

    QImage activeImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    activeImage.fill(Qt::black);
    ImageFrame activeFrame(activeImage);
    emitTimedProviderFrameReady(session, activeFrameToken, &activeFrame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QVERIFY(hasPendingRenderCommitForTest(item));

    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderRequestsTest::
    providerQueuedFrameRequestSchedulerFailureReportsProviderFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto sessionFactory
        = std::make_shared<CountingProviderSessionFactory>(sessionCount, metadataRequestCount,
            frameRequestCount, lastRequestedFrame, closeCount, std::shared_ptr<int>(),
            std::shared_ptr<int>(), std::shared_ptr<int>(), cancelRequestCount, lastCancelledToken);
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

    CountingProviderSession* session = sessionFactory->lastSession();
    const ImageSequenceProviderRequestToken initialFrameToken = session->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    const ImageViewportRevisionToken providerWaitingRevision
        = revisionTokenProperty(item, "requestRevision");
    failNextProviderQueueFlushSchedulingForTest(item, ImageViewport::PageRole::Primary);

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledToken, initialFrameToken);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider")));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("failed")));
    verifyRevisionChanged(item, "requestRevision", providerWaitingRevision);

    const ProviderSchedulerDiagnosticForTest diagnostic
        = lastProviderSchedulerDiagnosticForTest(item);
    QVERIFY(diagnostic.valid);
    QCOMPARE(diagnostic.role, ImageViewport::PageRole::Primary);
    QCOMPARE(diagnostic.activeRequestId, activeRequestIdForTest(item));
    QCOMPARE(diagnostic.queuedRequestId, activeRequestIdForTest(item));
    QCOMPARE(diagnostic.targetKind, ImageViewportInternal::ProviderRequestTargetKind::Frame);
    QCOMPARE(diagnostic.operation, ProviderSchedulerOperationForTest::FlushQueuedFrameRequest);
}

void ImageViewportProviderRequestsTest::
    secondaryProviderQueuedFrameRequestSchedulerFailureReportsProviderFailure()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryCancelRequestCount = std::make_shared<int>(0);
    const auto secondaryLastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto secondarySessionFactory
        = std::make_shared<CountingProviderSessionFactory>(secondarySessionCount,
            secondaryMetadataRequestCount, secondaryFrameRequestCount, secondaryLastRequestedFrame,
            secondaryCloseCount, std::shared_ptr<int>(), std::shared_ptr<int>(),
            std::shared_ptr<int>(), secondaryCancelRequestCount, secondaryLastCancelledToken);
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
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(8.0, 16.0), { 100, 250 }));
    drainQueuedProviderResults();

    CountingProviderSession* session = secondarySessionFactory->lastSession();
    const ImageSequenceProviderRequestToken initialFrameToken = session->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());
    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(*secondaryLastRequestedFrame, 0);

    const ImageViewportRevisionToken providerWaitingRevision
        = revisionTokenProperty(item, "requestRevision");
    failNextProviderQueueFlushSchedulingForTest(item, ImageViewport::PageRole::Secondary);

    QCOMPARE(item.seek(ImageViewport::PageRole::Secondary, 0).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*secondaryCancelRequestCount, 1);
    QCOMPARE(*secondaryLastCancelledToken, initialFrameToken);
    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(secondaryRequestedFrame(item), 0);
    QCOMPARE(secondaryRequestedPosition(item), 0);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider")));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("failed")));
    verifyRevisionChanged(item, "requestRevision", providerWaitingRevision);

    const ProviderSchedulerDiagnosticForTest diagnostic
        = lastProviderSchedulerDiagnosticForTest(item);
    QVERIFY(diagnostic.valid);
    QCOMPARE(diagnostic.role, ImageViewport::PageRole::Secondary);
    QCOMPARE(diagnostic.activeRequestId, activeRequestIdForTest(item));
    QCOMPARE(diagnostic.queuedRequestId, activeRequestIdForTest(item));
    QCOMPARE(diagnostic.targetKind, ImageViewportInternal::ProviderRequestTargetKind::Frame);
    QCOMPARE(diagnostic.operation, ProviderSchedulerOperationForTest::FlushQueuedFrameRequest);
}

void ImageViewportProviderRequestsTest::providerTimedFrameSeekRequestsSelectedFrame()
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
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    const ImageViewportRevisionToken readyDisplayRevision
        = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
}

void ImageViewportProviderRequestsTest::providerTimedFrameSeekWithoutDiagnosticsDoesNotNotify()
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

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
}

void ImageViewportProviderRequestsTest::providerTimedFrameCommitPreservesGeometryObservations()
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

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &firstFrame, 0, 0);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);

    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame secondFrame(secondImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &secondFrame, 1, 100);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));
}

void ImageViewportProviderRequestsTest::providerTimedFrameSeekCancelsStaleRequest()
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
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken firstSeekToken
        = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*cancelRequestCount, 1);
    QVERIFY(sessionFactory->lastSession()->lastCancelledToken() == firstSeekToken);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), firstSeekToken, &frame, 1, 100);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    emitProviderUnsupported(sessionFactory->lastSession(), firstSeekToken,
        ImageSequenceProviderUnsupportedCause::PayloadRejection,
        QStringLiteral("stale request unsupported late"));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(viewportErrorString(item), QString());

    emitProviderCancelled(sessionFactory->lastSession(), firstSeekToken,
        QStringLiteral("stale request cleanup complete"));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(viewportErrorString(item), QString());

    emitProviderFailed(
        sessionFactory->lastSession(), firstSeekToken, QStringLiteral("stale request failed late"));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderRequestsTest::providerTimedPositionSeekRequestsResolvedFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<ImageSequenceProviderRequestToken>(),
        positionRequestCount, lastPositionFrame, lastRequestedPosition);
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
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Primary, 349).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*positionRequestCount, 1);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 349);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 349);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));

    emitTimedProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastPositionToken(), &frame, 1, 100);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Primary, 350).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*positionRequestCount, 2);
    QCOMPARE(*lastPositionFrame, 1);
    QCOMPARE(*lastRequestedPosition, 350);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);

    emitTimedProviderFrameReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastPositionToken(), &frame, 1, 100);
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
    QCOMPARE(primaryRequestedPosition(item), 350);
}

void ImageViewportProviderRequestsTest::
    secondaryProviderFrameSeekBeforeMetadataResolvesAfterMetadata()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(ImageViewport::PageRole::Secondary, 1).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), -1);

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(*secondaryLastRequestedFrame, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
    QCOMPARE(secondaryFrameCount(item), 2);
    QCOMPARE(secondaryTotalDuration(item), 350);
}

void ImageViewportProviderRequestsTest::
    secondaryProviderPositionSeekBeforeMetadataResolvesAfterMetadata()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryPositionRequestCount = std::make_shared<int>(0);
    const auto secondaryLastPositionFrame = std::make_shared<int>(-1);
    const auto secondaryLastRequestedPosition = std::make_shared<int>(-1);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount, std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<ImageSequenceProviderRequestToken>(), secondaryPositionRequestCount,
        secondaryLastPositionFrame, secondaryLastRequestedPosition);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 349).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryPositionRequestCount, 0);
    QCOMPARE(secondaryRequestedFrame(item), -1);
    QCOMPARE(secondaryRequestedPosition(item), 349);

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryPositionRequestCount, 1);
    QCOMPARE(*secondaryLastPositionFrame, 1);
    QCOMPARE(*secondaryLastRequestedPosition, 349);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 349);
    QCOMPARE(secondaryFrameCount(item), 2);
    QCOMPARE(secondaryTotalDuration(item), 350);
}

void ImageViewportProviderRequestsTest::secondaryProviderFrameSeekUsesFrameRequest()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryPositionRequestCount = std::make_shared<int>(0);
    const auto secondaryLastPositionFrame = std::make_shared<int>(-1);
    const auto secondaryLastRequestedPosition = std::make_shared<int>(-1);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount, std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<ImageSequenceProviderRequestToken>(), secondaryPositionRequestCount,
        secondaryLastPositionFrame, secondaryLastRequestedPosition);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(*secondaryLastRequestedFrame, 0);

    QCOMPARE(item.seek(ImageViewport::PageRole::Secondary, 1).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 2);
    QCOMPARE(*secondaryLastRequestedFrame, 1);
    QCOMPARE(*secondaryPositionRequestCount, 0);
    QCOMPARE(*secondaryLastPositionFrame, -1);
    QCOMPARE(*secondaryLastRequestedPosition, -1);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
}

void ImageViewportProviderRequestsTest::secondaryProviderPositionSeekRequestsResolvedFrame()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryPositionRequestCount = std::make_shared<int>(0);
    const auto secondaryLastPositionFrame = std::make_shared<int>(-1);
    const auto secondaryLastRequestedPosition = std::make_shared<int>(-1);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount, std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<ImageSequenceProviderRequestToken>(), secondaryPositionRequestCount,
        secondaryLastPositionFrame, secondaryLastRequestedPosition);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 350).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(*secondaryLastRequestedFrame, 0);
    QCOMPARE(*secondaryPositionRequestCount, 1);
    QCOMPARE(*secondaryLastPositionFrame, 1);
    QCOMPARE(*secondaryLastRequestedPosition, 350);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 350);
}

void ImageViewportProviderRequestsTest::
    secondaryProviderFrameSeekRetainsDisplayedSpreadUntilCommit()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
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
    useSynchronousProviderEventDeliveryForTest(item);
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(8.0, 16.0), { 100, 250 }));
    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    emitTimedProviderFrameReady(secondarySessionFactory->lastSession(), &secondaryFrame, 0, 0);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(secondaryDisplayedFrame(item), 0);

    QCOMPARE(item.seek(ImageViewport::PageRole::Secondary, 1).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*secondaryFrameRequestCount, 2);
    QCOMPARE(*secondaryLastRequestedFrame, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
    QCOMPARE(secondaryDisplayedFrame(item), 0);

    emitTimedProviderFrameReady(secondarySessionFactory->lastSession(), &secondaryFrame, 1, 100);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(secondaryDisplayedFrame(item), 0);

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(secondaryDisplayedFrame(item), 1);
    QCOMPARE(secondaryDisplayedPosition(item), 100);
}

void ImageViewportProviderRequestsTest::
    secondaryProviderPositionSeekRetainsDisplayedSpreadUntilCommit()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryPositionRequestCount = std::make_shared<int>(0);
    const auto secondaryLastPositionFrame = std::make_shared<int>(-1);
    const auto secondaryLastRequestedPosition = std::make_shared<int>(-1);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount, std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<ImageSequenceProviderRequestToken>(), secondaryPositionRequestCount,
        secondaryLastPositionFrame, secondaryLastRequestedPosition);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    useSynchronousProviderEventDeliveryForTest(item);
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(8.0, 16.0), { 100, 250 }));
    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    emitTimedProviderFrameReady(secondarySessionFactory->lastSession(), &secondaryFrame, 0, 0);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(secondaryDisplayedFrame(item), 0);

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 350).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*secondaryPositionRequestCount, 1);
    QCOMPARE(*secondaryLastPositionFrame, 1);
    QCOMPARE(*secondaryLastRequestedPosition, 350);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 350);
    QCOMPARE(secondaryDisplayedFrame(item), 0);

    emitTimedProviderFrameReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastPositionToken(), &secondaryFrame, 1, 100);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(secondaryDisplayedFrame(item), 0);

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(secondaryDisplayedFrame(item), 1);
    QCOMPARE(secondaryDisplayedPosition(item), 100);
}

void ImageViewportProviderRequestsTest::
    secondaryProviderInvalidAndUnsupportedSeekCommandsPreserveRequest()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryPositionRequestCount = std::make_shared<int>(0);
    const auto secondaryLastPositionFrame = std::make_shared<int>(-1);
    const auto secondaryLastRequestedPosition = std::make_shared<int>(-1);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount, std::shared_ptr<int>(),
        std::shared_ptr<int>(), std::shared_ptr<int>(), std::shared_ptr<int>(),
        std::shared_ptr<ImageSequenceProviderRequestToken>(), secondaryPositionRequestCount,
        secondaryLastPositionFrame, secondaryLastRequestedPosition);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(item.seek(ImageViewport::PageRole::Secondary, 1).outcome(),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*secondaryFrameRequestCount, 1);

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 0).outcome(),
        ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*secondaryPositionRequestCount, 0);
    QCOMPARE(*secondaryLastPositionFrame, -1);
    QCOMPARE(*secondaryLastRequestedPosition, -1);
    QCOMPARE(secondaryRequestedFrame(item), 0);
    QCOMPARE(secondaryRequestedPosition(item), -1);
}

void ImageViewportProviderRequestsTest::secondaryProviderFrameSeekIgnoresStaleFrameResult()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    const auto secondaryCancelRequestCount = std::make_shared<int>(0);
    const auto secondaryLastCancelledToken = std::make_shared<ImageSequenceProviderRequestToken>();
    auto secondarySessionFactory
        = std::make_shared<CountingProviderSessionFactory>(secondarySessionCount,
            secondaryMetadataRequestCount, secondaryFrameRequestCount, secondaryLastRequestedFrame,
            secondaryCloseCount, std::shared_ptr<int>(), std::shared_ptr<int>(),
            std::shared_ptr<int>(), secondaryCancelRequestCount, secondaryLastCancelledToken);
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
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();
    CountingProviderSession* session = secondarySessionFactory->lastSession();
    const ImageSequenceProviderRequestToken initialFrameToken = session->lastFrameToken();
    QVERIFY(initialFrameToken.isValid());

    QCOMPARE(item.seek(ImageViewport::PageRole::Secondary, 1).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*secondaryCancelRequestCount, 1);
    QCOMPARE(*secondaryLastCancelledToken, initialFrameToken);
    QCOMPARE(*secondaryFrameRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RequestQueued"));
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);

    drainQueuedProviderResults();
    QCOMPARE(*secondaryFrameRequestCount, 2);
    QCOMPARE(*secondaryLastRequestedFrame, 1);

    QImage staleImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    staleImage.fill(Qt::transparent);
    ImageFrame staleFrame(staleImage);
    emitTimedProviderFrameReady(session, initialFrameToken, &staleFrame, 0, 0);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
    QCOMPARE(secondaryDisplayedFrame(item), -1);
    QVERIFY(viewportErrorString(item).isEmpty());
}

QTEST_MAIN(ImageViewportProviderRequestsTest)

#include "tst_imageviewport_provider_requests.moc"
