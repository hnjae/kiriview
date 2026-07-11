#include "imageviewport_testhooks_p.h"
#include "viewportcontrollerplaybackcontract_p.h"
#include "viewportengine_p.h"

#include <QtCore/QScopedPointer>
#include <QtGui/QImage>
#include <QtTest/QTest>

namespace {

using namespace ImageViewportTestHooks;

class StubProviderSession final : public ImageSequenceProviderSession
{
    Q_OBJECT

public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void request(const ImageSequenceProviderRequest&) override { }
};

struct ProviderFrameQueueSetup
{
    ImageSequenceProviderRequestToken activeToken;
    quint64 activeRequestId = 0;
};

ProviderFrameQueueSetup setUpCurrentProviderFrameQueueRequest(
    ViewportEngine& engine, ImageSequenceProviderSession&)
{
    auto& request = engine.requestState();
    request.sequenceSource.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
        ImageViewportInternal::DisplayRequestTarget {
            4, 120, ImageViewportInternal::ProviderRequestTargetKind::Playback },
        ImageViewportInternal::ResolvedFrameIdentity { 4, 120 }, false);

    auto& provider = engine.providerState();
    provider.sessionActive = true;
    provider.activeFrameToken = providerRequestTokenForTest(4);
    request.activeRequest.providerFrameToken = provider.activeFrameToken;

    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::red);
    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::blue);

    auto& display = engine.displayState();
    display.pendingRenderPayload
        = { true, request.sequenceGeneration, request.activeRequest.identity.id, 3, primaryImage };
    display.secondaryPendingRenderPayload = { true, request.sequenceGeneration,
        request.activeRequest.identity.id, 4, secondaryImage };
    display.status = ImageViewport::DisplayStatus::Ready;
    display.displayedRequest = display.activeRequestSnapshot(
        request.sequenceGeneration, request.activeRequest, request.activeRequest.target.position);
    display.displayedImageSize = QSizeF(16.0, 8.0);
    display.displayedImage = primaryImage;
    display.renderFailureRetainedDisplayValid = true;
    display.renderFailureRetainedRequest = display.displayedRequest;
    display.renderFailureRetainedImageSize = display.displayedImageSize;
    display.renderFailureRetainedImage = display.displayedImage;

    return { provider.activeFrameToken, request.activeRequest.identity.id };
}

void verifyProviderFrameQueueCleared(const ImageViewportInternal::ProviderGenerationState& provider)
{
    QCOMPARE(provider.queuedFrameRequest, false);
    QCOMPARE(provider.queuedFrameGeneration, 0);
    QCOMPARE(provider.queuedFrameRequestId, 0);
    QCOMPARE(provider.queuedFrame, -1);
    QCOMPARE(provider.queuedPosition, -1);
    QCOMPARE(provider.queuedResolvedFrame.isValid(), false);
    QCOMPARE(provider.queuedFrameFromPlayback, false);
    QCOMPARE(
        provider.queuedFrameTargetKind, ImageViewportInternal::ProviderRequestTargetKind::Unknown);
}

} // namespace

class ViewportEngineTest : public QObject
{
    Q_OBJECT

public:
    explicit ViewportEngineTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void defaultSnapshotMatchesPublicDefaultProjection();
    void snapshotProjectsCanonicalEngineState();
    void defaultDisplayStateMatchesEmptyRenderState();
    void displayStateOwnsRenderPayloadAndRetainedIdentity();
    void defaultRequestStateMatchesPublicDefaults();
    void requestStateOwnsPlaybackDriverAndRequestIdentity();
    void playbackScheduleStopsOutsideReadyPlayingState();
    void playbackScheduleUsesBuiltInFrameRemainder();
    void playbackScheduleUsesProviderFrameRemainderByRole();
    void playbackPauseCommandMutatesEngineAtomically();
    void playbackTickAdvancesBuiltInTargetInEngine();
    void defaultProviderStateMatchesEmptyGeneration();
    void providerStateOwnsTokensQueuesAndMetadataByRole();
    void providerDisplayDemandProjectsCanonicalEngineFacts();
    void providerFrameQueueStoresCurrentRequestIdentity();
    void providerFrameQueueFlushesOnlyCurrentLoadingRequest();
    void providerFrameQueueFlushRejectsStaleRequest();
    void invalidCommandUpdatesOnlyCommandDiagnostics();
    void malformedEnumRejectionMatchesInvalidCommand();
    void clearFromEmptyIsAcceptedNoop();
    void presentationNoopValidatesEnumShape();
    void defaultPresentationStateMatchesPublicDefaults();
    void geometryProjectionUsesEnginePresentationState();
    void renderSnapshotUsesEnginePresentationAndPayloadState();
    void validPresentationTargetAssignmentAllocatesGenerationAndRoleSet();
    void twoRoleAssignmentIsAcceptedAtomically();
    void invalidPresentationTargetAssignmentMutatesOnlyCommandDiagnostics();
    void invalidTransitionPolicyMutatesOnlyCommandDiagnostics();
    void clearPresentationTargetAllocatesTransactionAndThenNoops();
    void presentationTargetAssignmentPreservesPreviousCommandDiagnostic();
    void assignmentEffectFlagsFollowTransitionPolicy();
};

void ViewportEngineTest::defaultSnapshotMatchesPublicDefaultProjection()
{
    ViewportEngine engine;
    ImageViewport item;
    const ImageViewportStateSnapshot engineSnapshot = engine.snapshot();
    const ImageViewportStateSnapshot itemSnapshot = item.state();

    QCOMPARE(engineSnapshot.request(), itemSnapshot.request());
    QCOMPARE(engineSnapshot.display().status(), itemSnapshot.display().status());
    QCOMPARE(engineSnapshot.display().phase(), itemSnapshot.display().phase());
    QCOMPARE(
        engineSnapshot.display().displayedRoleSet(), itemSnapshot.display().displayedRoleSet());
    QCOMPARE(engineSnapshot.display().targetRoleSet(), itemSnapshot.display().targetRoleSet());
    QCOMPARE(engineSnapshot.display().belongsToAcceptedPresentationTarget(),
        itemSnapshot.display().belongsToAcceptedPresentationTarget());
    QCOMPARE(engineSnapshot.display().retained(), itemSnapshot.display().retained());
    QCOMPARE(engineSnapshot.display().displayedPresentationRevision().isValid(), false);
    QCOMPARE(engineSnapshot.display().targetPresentationRevision().isValid(), false);
    QCOMPARE(engineSnapshot.primary().present(), itemSnapshot.primary().present());
    QCOMPARE(engineSnapshot.primary().sequence(), itemSnapshot.primary().sequence());
    QCOMPARE(engineSnapshot.secondary().present(), itemSnapshot.secondary().present());
    QCOMPARE(engineSnapshot.secondary().sequence(), itemSnapshot.secondary().sequence());
    QCOMPARE(engineSnapshot.diagnostics(), itemSnapshot.diagnostics());
    QCOMPARE(engineSnapshot.revisions(), itemSnapshot.revisions());
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(engine.commandDiagnostics().revision.isValid(), false);
}

void ViewportEngineTest::snapshotProjectsCanonicalEngineState()
{
    ViewportEngine engine;
    auto& request = engine.requestState();
    auto& display = engine.displayState();

    request.sequenceSource.facts.present = true;
    request.sequenceSource.facts.logicalSize = QSizeF(16.0, 8.0);
    request.sequenceSource.facts.frameCount = 1;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Unknown }, true);
    request.status = ImageViewport::RequestStatus::Ready;
    request.reason = ImageViewport::RequestReason::Ready;
    request.requestRevision = 11;
    request.commandRevision = 13;

    display.status = ImageViewport::DisplayStatus::Ready;
    display.displayedRequest
        = display.activeRequestSnapshot(request.sequenceGeneration, request.activeRequest, -1);
    display.displayedImageSize = QSizeF(16.0, 8.0);
    display.displayedImage = QImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    display.revision = 12;

    const ImageViewportStateSnapshot snapshot = engine.snapshot();
    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::Ready);
    QCOMPARE(snapshot.request().reason(), ImageViewport::RequestReason::Ready);
    QCOMPARE(snapshot.display().status(), ImageViewport::DisplayStatus::Ready);
    QCOMPARE(snapshot.primary().present(), true);
    QCOMPARE(snapshot.primary().request().frame(), 0);
    QCOMPARE(snapshot.primary().display().frame(), 0);
    QCOMPARE(snapshot.revisions().request().isValid(), true);
    QCOMPARE(snapshot.revisions().display().isValid(), true);
    QCOMPARE(snapshot.revisions().command().isValid(), true);
}

void ViewportEngineTest::defaultDisplayStateMatchesEmptyRenderState()
{
    ViewportEngine engine;
    const auto& display = engine.displayState();

    QCOMPARE(display.status, ImageViewport::DisplayStatus::Empty);
    QCOMPARE(display.displayedRequest.generation, 0);
    QCOMPARE(display.displayedRequest.request.identity.id, 0);
    QCOMPARE(display.secondaryDisplayedRequest.generation, 0);
    QCOMPARE(display.displayedImageSize, QSizeF());
    QCOMPARE(display.displayedImage.isNull(), true);
    QCOMPARE(display.secondaryDisplayedImageSize, QSizeF());
    QCOMPARE(display.secondaryDisplayedImage.isNull(), true);
    QCOMPARE(display.nextPreparedPayloadId, 0);
    QCOMPARE(display.pendingRenderPayload.commitPending, false);
    QCOMPARE(display.pendingRenderPayload.identity().isValid(), false);
    QCOMPARE(display.secondaryPendingRenderPayload.commitPending, false);
    QCOMPARE(display.renderFailureRetainedDisplayValid, false);
    QCOMPARE(display.renderFailureRetainedRequest.generation, 0);
    QCOMPARE(display.renderFailureRetainedImageSize, QSizeF());
    QCOMPARE(display.renderFailureRetainedImage.isNull(), true);
    QCOMPARE(display.revision, 0);
}

void ViewportEngineTest::displayStateOwnsRenderPayloadAndRetainedIdentity()
{
    ViewportEngine engine;
    auto& request = engine.requestState();
    auto& display = engine.displayState();

    request.sequenceGeneration = 12;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        ImageViewportInternal::DisplayRequestTarget {
            2, 100, ImageViewportInternal::ProviderRequestTargetKind::Frame },
        ImageViewportInternal::ResolvedFrameIdentity { 2, 100 }, true);

    display.status = ImageViewport::DisplayStatus::Ready;
    display.displayedRequest = display.activeRequestSnapshot(
        request.sequenceGeneration, request.activeRequest, request.activeRequest.target.position);
    display.displayedImageSize = QSizeF(16.0, 8.0);
    display.displayedImage = QImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    display.beginPreparedPayloadIdentity(request.sequenceGeneration, request.activeRequest);
    display.pendingRenderPayload.commitPending = true;
    display.pendingRenderPayload.image = display.displayedImage;
    display.secondaryPendingRenderPayload = { true, 12, request.activeRequest.identity.id, 9, {} };
    display.revision = 42;

    const auto& observed = engine.displayState();
    QCOMPARE(observed.status, ImageViewport::DisplayStatus::Ready);
    QCOMPARE(observed.displayedRequest.generation, 12);
    QCOMPARE(observed.displayedRequest.request.target.frame, 2);
    QCOMPARE(observed.displayedRequest.request.target.position, 100);
    QCOMPARE(observed.displayedImageSize, QSizeF(16.0, 8.0));
    QCOMPARE(observed.pendingRenderPayload.commitPending, true);
    QCOMPARE(observed.pendingRenderPayload.generation, 12);
    QCOMPARE(observed.pendingRenderPayload.requestId, request.activeRequest.identity.id);
    QCOMPARE(observed.pendingRenderPayload.payloadId, 1);
    QCOMPARE(request.activeRequest.preparedPayloadId, 1);
    QCOMPARE(observed.secondaryPendingRenderPayload.payloadId, 9);
    QCOMPARE(observed.revision, 42);

    display.captureRenderFailureRetainedDisplay(true);
    QCOMPARE(engine.displayState().renderFailureRetainedDisplayValid, true);
    QCOMPARE(engine.displayState().renderFailureRetainedRequest.generation, 12);
    QCOMPARE(engine.displayState().renderFailureRetainedImageSize, QSizeF(16.0, 8.0));
    QCOMPARE(engine.displayState().renderFailureRetainedImage.isNull(), false);

    display.clearPendingRenderPayload();
    QCOMPARE(engine.displayState().pendingRenderPayload.commitPending, false);
    QCOMPARE(engine.displayState().secondaryPendingRenderPayload.commitPending, false);
}

void ViewportEngineTest::defaultRequestStateMatchesPublicDefaults()
{
    ViewportEngine engine;
    ImageViewport item;
    const auto& request = engine.requestState();

    QCOMPARE(request.status, ImageViewport::RequestStatus::NoRequest);
    QCOMPARE(request.reason, ImageViewport::RequestReason::NoRequest);
    QCOMPARE(request.commandReason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(request.playbackPhase, item.state().request().playbackPhase());
    QCOMPARE(request.looping, item.state().presentation().looping());
    QCOMPARE(request.stopPlaybackWhenRequestReady, false);
    QCOMPARE(request.providerPlaybackStartPending, false);
    QCOMPARE(request.activeRequest.identity.id, 0);
    QCOMPARE(request.secondaryActiveRequest.identity.id, 0);
    QCOMPARE(request.latestNonPlaybackRequest.identity.id, 0);
    QCOMPARE(request.secondaryLatestNonPlaybackRequest.identity.id, 0);
    QCOMPARE(request.playbackPosition, -1);
    QCOMPARE(request.playbackRole, ImageViewport::PageRole::Primary);
    QCOMPARE(request.playbackLoopIterationsCompleted, 0);
    QCOMPARE(request.sequenceGeneration, 0);
    QCOMPARE(request.nextRequestId, 0);
    QCOMPARE(request.requestRevision, 0);
    QCOMPARE(request.commandRevision, 0);
}

void ViewportEngineTest::requestStateOwnsPlaybackDriverAndRequestIdentity()
{
    ViewportEngine engine;
    auto& request = engine.requestState();

    request.sequenceGeneration = 7;
    request.status = ImageViewport::RequestStatus::Loading;
    request.reason = ImageViewport::RequestReason::ProviderWaiting;
    request.playbackPhase = ImageViewport::PlaybackPhase::Waiting;
    request.looping = true;
    request.stopPlaybackWhenRequestReady = true;
    request.providerPlaybackStartPending = true;
    request.playbackRole = ImageViewport::PageRole::Secondary;
    request.playbackPosition = 125;
    request.playbackLoopIterationsCompleted = 2;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
        ImageViewportInternal::DisplayRequestTarget {
            3, 120, ImageViewportInternal::ProviderRequestTargetKind::Playback },
        false);
    request.secondaryActiveRequest.identity = request.activeRequest.identity;
    request.secondaryActiveRequest.target
        = { 4, 160, ImageViewportInternal::ProviderRequestTargetKind::Playback };
    request.secondaryLatestNonPlaybackRequest.identity = request.secondaryActiveRequest.identity;
    request.secondaryLatestNonPlaybackRequest.target
        = { 1, 40, ImageViewportInternal::ProviderRequestTargetKind::Frame };

    const auto& observed = engine.requestState();
    QCOMPARE(observed.sequenceGeneration, 7);
    QCOMPARE(observed.status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(observed.reason, ImageViewport::RequestReason::ProviderWaiting);
    QCOMPARE(observed.playbackPhase, ImageViewport::PlaybackPhase::Waiting);
    QCOMPARE(observed.looping, true);
    QCOMPARE(observed.stopPlaybackWhenRequestReady, true);
    QCOMPARE(observed.providerPlaybackStartPending, true);
    QCOMPARE(observed.playbackRole, ImageViewport::PageRole::Secondary);
    QCOMPARE(observed.playbackPosition, 125);
    QCOMPARE(observed.playbackLoopIterationsCompleted, 2);
    QVERIFY(observed.activeRequest.identity.id != 0);
    QCOMPARE(observed.activeRequest.identity.origin,
        ImageViewportInternal::DisplayRequestOrigin::Playback);
    QCOMPARE(observed.activeRequest.target.frame, 3);
    QCOMPARE(observed.activeRequest.target.position, 120);
    QCOMPARE(observed.secondaryActiveRequest.identity.id, observed.activeRequest.identity.id);
    QCOMPARE(
        observed.secondaryActiveRequest.identity.origin, observed.activeRequest.identity.origin);
    QCOMPARE(observed.secondaryActiveRequest.target.frame, 4);
    QCOMPARE(observed.secondaryLatestNonPlaybackRequest.target.frame, 1);
}

void ViewportEngineTest::playbackScheduleStopsOutsideReadyPlayingState()
{
    ViewportEngine engine;

    const auto stopped = engine.playbackScheduleEffect();

    QCOMPARE(stopped.action, ViewportPlaybackScheduleEffect::Action::Stop);
    QCOMPARE(stopped.delayMilliseconds, -1);
}

void ViewportEngineTest::playbackScheduleUsesBuiltInFrameRemainder()
{
    ViewportEngine engine;
    auto& request = engine.requestState();
    request.sequenceSource.facts.present = true;
    request.sequenceSource.facts.timed = true;
    request.sequenceSource.facts.frameCount = 2;
    request.sequenceSource.facts.totalDuration = 350;
    request.sequenceSource.facts.timingIntervals
        = TimingIntervals::fromFrameDurations({ 100, 250 });
    request.activeRequest.target.frame = 1;
    request.playbackRole = ImageViewport::PageRole::Primary;
    request.playbackPosition = 125;
    request.playbackPhase = ImageViewport::PlaybackPhase::Playing;
    request.status = ImageViewport::RequestStatus::Ready;

    const auto effect = engine.playbackScheduleEffect();

    QCOMPARE(effect.action, ViewportPlaybackScheduleEffect::Action::ArmAfter);
    QCOMPARE(effect.delayMilliseconds, 225);
}

void ViewportEngineTest::playbackScheduleUsesProviderFrameRemainderByRole()
{
    ViewportEngine engine;
    auto& request = engine.requestState();
    request.secondarySequenceSource.facts.present = true;
    request.secondarySequenceSource.facts.provider = true;
    request.secondaryActiveRequest.target.frame = 0;
    request.playbackRole = ImageViewport::PageRole::Secondary;
    request.playbackPosition = 40;
    request.playbackPhase = ImageViewport::PlaybackPhase::Playing;
    request.status = ImageViewport::RequestStatus::Ready;
    auto& provider = engine.secondaryProviderState();
    provider.metadataReady = true;
    provider.timedMetadata = true;
    provider.timingIntervals = TimingIntervals::fromFrameDurations({ 100, 250 });

    const auto effect = engine.playbackScheduleEffect();

    QCOMPARE(effect.action, ViewportPlaybackScheduleEffect::Action::ArmAfter);
    QCOMPARE(effect.delayMilliseconds, 60);
}

void ViewportEngineTest::playbackPauseCommandMutatesEngineAtomically()
{
    ViewportEngine engine;
    auto& request = engine.requestState();
    request.sequenceSource.facts.present = true;
    request.playbackRole = ImageViewport::PageRole::Primary;
    request.playbackPhase = ImageViewport::PlaybackPhase::Playing;

    const auto result = engine.applyPlaybackCommand(
        { { ViewportPlaybackCommand::Kind::Pause, ImageViewport::PageRole::Primary }, {} });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(engine.requestState().playbackPhase, ImageViewport::PlaybackPhase::Paused);
    QCOMPARE(result.changes.playbackPhase, true);
    QCOMPARE(result.schedule.action, ViewportPlaybackScheduleEffect::Action::Stop);
}

void ViewportEngineTest::playbackTickAdvancesBuiltInTargetInEngine()
{
    ViewportEngine engine;
    auto& request = engine.requestState();
    request.sequenceGeneration = 7;
    request.sequenceSource.facts.present = true;
    request.sequenceSource.facts.timed = true;
    request.sequenceSource.facts.frameCount = 2;
    request.sequenceSource.facts.totalDuration = 350;
    request.sequenceSource.facts.logicalSize = QSizeF(16.0, 8.0);
    request.sequenceSource.facts.timingIntervals
        = TimingIntervals::fromFrameDurations({ 100, 250 });
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, 0, ImageViewportInternal::ProviderRequestTargetKind::Unknown },
        { 0, 0 }, true);
    request.status = ImageViewport::RequestStatus::Ready;
    request.reason = ImageViewport::RequestReason::Ready;
    request.playbackRole = ImageViewport::PageRole::Primary;
    request.playbackPhase = ImageViewport::PlaybackPhase::Playing;
    request.playbackPosition = 0;

    const auto result = engine.advancePlayback(
        { 100, { true, QRectF(0.0, 0.0, 100.0, 100.0), QSizeF(16.0, 8.0), {}, 1.0 } });

    QCOMPARE(engine.requestState().activeRequest.target.frame, 1);
    QCOMPARE(engine.requestState().activeRequest.target.position, 100);
    QCOMPARE(engine.requestState().activeRequest.identity.origin,
        ImageViewportInternal::DisplayRequestOrigin::Playback);
    QCOMPARE(engine.requestState().playbackPhase, ImageViewport::PlaybackPhase::Waiting);
    QCOMPARE(result.changes.requestState, true);
    QCOMPARE(result.changes.scheduleUpdate, true);
    QCOMPARE(result.schedule.action, ViewportPlaybackScheduleEffect::Action::Stop);
}

void ViewportEngineTest::defaultProviderStateMatchesEmptyGeneration()
{
    ViewportEngine engine;
    const auto& provider = engine.providerState();
    const auto& secondaryProvider = engine.secondaryProviderState();

    QCOMPARE(provider.sessionActive, false);
    QCOMPARE(provider.sessionSerial, 0);
    QCOMPARE(provider.nextRequestToken, 0);
    QVERIFY(!provider.activeMetadataToken.isValid());
    QVERIFY(!provider.activeFrameToken.isValid());
    QCOMPARE(provider.queuedFrameRequest, false);
    QCOMPARE(provider.queuedFrameGeneration, 0);
    QCOMPARE(provider.queuedFrameRequestId, 0);
    QCOMPARE(provider.queuedFrame, -1);
    QCOMPARE(provider.queuedPosition, -1);
    QCOMPARE(provider.queuedResolvedFrame.isValid(), false);
    QCOMPARE(provider.queuedFrameFromPlayback, false);
    QCOMPARE(
        provider.queuedFrameTargetKind, ImageViewportInternal::ProviderRequestTargetKind::Unknown);
    QCOMPARE(provider.metadataReady, false);
    QCOMPARE(provider.timedMetadata, false);
    QCOMPARE(provider.timedPlaybackSupport, false);
    QCOMPARE(provider.frameSeekSupport, false);
    QCOMPARE(provider.positionSeekSupport, false);
    QCOMPARE(provider.logicalSize, QSizeF());
    QCOMPARE(provider.timingIntervals.isValid(), false);

    QCOMPARE(secondaryProvider.sessionSerial, 0);
    QCOMPARE(secondaryProvider.nextRequestToken, 0);
    QCOMPARE(secondaryProvider.metadataReady, false);
}

void ViewportEngineTest::providerStateOwnsTokensQueuesAndMetadataByRole()
{
    ViewportEngine engine;
    auto& provider = engine.providerState();
    auto& secondaryProvider = engine.secondaryProviderState();

    provider.sessionSerial = 11;
    provider.nextRequestToken = 3;
    provider.activeMetadataToken = ImageViewportTestHooks::providerRequestTokenForTest(4);
    provider.activeFrameToken = ImageViewportTestHooks::providerRequestTokenForTest(5);
    provider.queuedFrameRequest = true;
    provider.queuedFrameGeneration = 7;
    provider.queuedFrameRequestId = 13;
    provider.queuedFrame = 2;
    provider.queuedPosition = 120;
    provider.queuedResolvedFrame = { 2, 120 };
    provider.queuedFrameFromPlayback = true;
    provider.queuedFrameTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Playback;
    provider.metadataReady = true;
    provider.timedMetadata = true;
    provider.timedPlaybackSupport = true;
    provider.frameSeekSupport = true;
    provider.positionSeekSupport = true;
    provider.authoredAnimationFacts = ImageSequenceAuthoredAnimationFacts::finiteLoop(3);
    provider.logicalSize = QSizeF(16.0, 8.0);
    provider.timingIntervals = TimingIntervals::fromFrameDurations({ 100, 250 });

    secondaryProvider.sessionSerial = 21;
    secondaryProvider.nextRequestToken = 9;
    secondaryProvider.metadataReady = true;
    secondaryProvider.logicalSize = QSizeF(4.0, 6.0);

    const auto& observed = engine.providerState();
    QCOMPARE(observed.sessionSerial, 11);
    QCOMPARE(observed.nextRequestToken, 3);
    QCOMPARE(observed.activeMetadataToken, ImageViewportTestHooks::providerRequestTokenForTest(4));
    QCOMPARE(observed.activeFrameToken, ImageViewportTestHooks::providerRequestTokenForTest(5));
    QCOMPARE(observed.queuedFrameRequest, true);
    QCOMPARE(observed.queuedFrameGeneration, 7);
    QCOMPARE(observed.queuedFrameRequestId, 13);
    QCOMPARE(observed.queuedFrame, 2);
    QCOMPARE(observed.queuedPosition, 120);
    QCOMPARE(observed.queuedResolvedFrame.frame, 2);
    QCOMPARE(observed.queuedResolvedFrame.position, 120);
    QCOMPARE(observed.queuedFrameFromPlayback, true);
    QCOMPARE(
        observed.queuedFrameTargetKind, ImageViewportInternal::ProviderRequestTargetKind::Playback);
    QCOMPARE(observed.metadataReady, true);
    QCOMPARE(observed.timedMetadata, true);
    QCOMPARE(observed.timedPlaybackSupport, true);
    QCOMPARE(observed.frameSeekSupport, true);
    QCOMPARE(observed.positionSeekSupport, true);
    QCOMPARE(observed.authoredAnimationFacts.loopMode(),
        ImageSequenceAuthoredAnimationFacts::LoopMode::Finite);
    QCOMPARE(observed.authoredAnimationFacts.loopCount(), 3);
    QCOMPARE(observed.logicalSize, QSizeF(16.0, 8.0));
    QCOMPARE(observed.timingIntervals.frameCount(), 2);
    QCOMPARE(observed.timingIntervals.totalDuration(), 350);

    QCOMPARE(engine.secondaryProviderState().sessionSerial, 21);
    QCOMPARE(engine.secondaryProviderState().nextRequestToken, 9);
    QCOMPARE(engine.secondaryProviderState().metadataReady, true);
    QCOMPARE(engine.secondaryProviderState().logicalSize, QSizeF(4.0, 6.0));
    QCOMPARE(engine.providerState().logicalSize, QSizeF(16.0, 8.0));
}

void ViewportEngineTest::providerFrameQueueStoresCurrentRequestIdentity()
{
    ViewportEngine engine;
    StubProviderSession session;
    const ProviderFrameQueueSetup setup = setUpCurrentProviderFrameQueueRequest(engine, session);

    const auto result = engine.queueProviderFrameRequest({ ImageViewport::PageRole::Primary, 4,
        ImageViewportInternal::ProviderRequestTargetKind::Playback });

    QCOMPARE(result.deferredFlush, true);
    QCOMPARE(result.cancelToken, setup.activeToken);
    QVERIFY(!engine.providerState().activeFrameToken.isValid());
    QVERIFY(!engine.requestState().activeRequest.providerFrameToken.isValid());
    QCOMPARE(engine.providerState().queuedFrameGeneration, 7);
    QCOMPARE(engine.providerState().queuedFrameRequestId, setup.activeRequestId);
    QCOMPARE(engine.providerState().queuedFrame, 4);
    QCOMPARE(engine.providerState().queuedPosition, 120);
    QCOMPARE(engine.providerState().queuedResolvedFrame.frame, 4);
    QCOMPARE(engine.providerState().queuedResolvedFrame.position, 120);
    QCOMPARE(engine.providerState().queuedFrameFromPlayback, true);
    QCOMPARE(engine.providerState().queuedFrameTargetKind,
        ImageViewportInternal::ProviderRequestTargetKind::Playback);
    QCOMPARE(engine.requestState().status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(engine.requestState().reason, ImageViewport::RequestReason::RequestQueued);
    QCOMPARE(engine.displayState().pendingRenderPayload.commitPending, false);
    QCOMPARE(engine.displayState().secondaryPendingRenderPayload.commitPending, false);
    QCOMPARE(engine.displayState().renderFailureRetainedDisplayValid, false);
    QCOMPARE(engine.displayState().renderFailureRetainedRequest.generation, 0);
    QCOMPARE(engine.displayState().renderFailureRetainedImageSize, QSizeF());
    QCOMPARE(engine.displayState().renderFailureRetainedImage.isNull(), true);
}

void ViewportEngineTest::providerDisplayDemandProjectsCanonicalEngineFacts()
{
    ViewportEngine engine;
    auto& request = engine.requestState();
    request.sequenceSource.facts.present = true;
    request.sequenceSource.facts.provider = true;
    request.sequenceGeneration = 7;
    request.requestRevision = 11;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        { 2, 120, ImageViewportInternal::ProviderRequestTargetKind::Position },
        { 2, 100 }, false);
    engine.providerState().logicalSize = QSizeF(16.0, 8.0);
    engine.displayState().displayedPayload.quality = ImageViewport::PayloadQuality::Preview;
    engine.displayState().displayedPayload.exactness
        = ImageViewport::PayloadExactness::NotExact;
    engine.displayState().displayedPayload.payloadRasterSize = QSizeF(8.0, 4.0);
    engine.displayState().displayedPayload.sourceToPayloadScale = QSizeF(0.5, 0.5);

    ViewportEngine::GeometryInput geometry;
    geometry.primaryPresent = true;
    geometry.itemBounds = QRectF(0.0, 0.0, 100.0, 50.0);
    geometry.primarySize = QSizeF(16.0, 8.0);
    geometry.devicePixelRatio = 2.0;
    const ImageSequenceProviderDisplayDemand demand
        = engine.providerDisplayDemand(ImageViewport::PageRole::Primary, geometry);

    QVERIFY(demand.demandRevision().isValid());
    QCOMPARE(engine.requestState().activeRequest.demandRevision, demand.demandRevision());
    QVERIFY(demand.requestRevision().isValid());
    QCOMPARE(demand.role(), ImageViewport::PageRole::Primary);
    QCOMPARE(demand.resolvedFrame(), 2);
    QCOMPARE(demand.requestedPosition(), 120);
    QCOMPARE(demand.sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(demand.visibleSourceRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(demand.targetDisplaySizePixels(), QSizeF(200.0, 100.0));
    QCOMPARE(demand.effectiveDevicePixelRatio(), 2.0);
    QCOMPARE(demand.maximumTextureSize(), -1);
    QCOMPARE(demand.maximumPayloadBytes(), ImageSequenceLimits::maximumPayloadBytesPerFrame());
    QCOMPARE(demand.displayByteBudget(), -1);
    QCOMPARE(demand.currentPayloadQuality(), ImageViewport::PayloadQuality::Preview);
    QCOMPARE(demand.currentPayloadExactness(), ImageViewport::PayloadExactness::NotExact);
    QCOMPARE(demand.currentPayloadRasterSize(), QSizeF(8.0, 4.0));
    QCOMPARE(demand.currentSourceToPayloadScale(), QSizeF(0.5, 0.5));
}

void ViewportEngineTest::providerFrameQueueFlushesOnlyCurrentLoadingRequest()
{
    ViewportEngine engine;
    StubProviderSession session;
    setUpCurrentProviderFrameQueueRequest(engine, session);
    engine.queueProviderFrameRequest({ ImageViewport::PageRole::Primary, 4,
        ImageViewportInternal::ProviderRequestTargetKind::Playback });

    const auto result = engine.flushQueuedProviderFrameRequest(ImageViewport::PageRole::Primary);

    QCOMPARE(result.startRequest, true);
    QCOMPARE(result.frame, 4);
    QCOMPARE(result.targetKind, ImageViewportInternal::ProviderRequestTargetKind::Playback);
    verifyProviderFrameQueueCleared(engine.providerState());
}

void ViewportEngineTest::providerFrameQueueFlushRejectsStaleRequest()
{
    ViewportEngine engine;
    StubProviderSession session;
    setUpCurrentProviderFrameQueueRequest(engine, session);
    engine.queueProviderFrameRequest({ ImageViewport::PageRole::Primary, 4,
        ImageViewportInternal::ProviderRequestTargetKind::Playback });
    engine.requestState().activeRequest.target.frame = 5;

    const auto result = engine.flushQueuedProviderFrameRequest(ImageViewport::PageRole::Primary);

    QCOMPARE(result.startRequest, false);
    verifyProviderFrameQueueCleared(engine.providerState());
}

void ViewportEngineTest::invalidCommandUpdatesOnlyCommandDiagnostics()
{
    ViewportEngine engine;
    const ImageViewportStateSnapshot snapshot = engine.snapshot();

    const ViewportEngine::CommandResult result = engine.rejectInvalidCommand();

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(result.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(result.commandRevisionChanged, true);
    QVERIFY(result.commandRevision.isValid());
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(engine.commandDiagnostics().revision, result.commandRevision);
    const ImageViewportStateSnapshot rejectedSnapshot = engine.snapshot();
    QCOMPARE(rejectedSnapshot.request(), snapshot.request());
    QCOMPARE(rejectedSnapshot.display(), snapshot.display());
    QCOMPARE(rejectedSnapshot.presentation(), snapshot.presentation());
    QCOMPARE(rejectedSnapshot.diagnostics().commandReason(),
        ImageViewport::CommandReason::InvalidRequest);
    QVERIFY(rejectedSnapshot.revisions().command().isValid());
    QVERIFY(rejectedSnapshot.revisions().snapshot().isValid());
}

void ViewportEngineTest::malformedEnumRejectionMatchesInvalidCommand()
{
    ViewportEngine engine;

    const ViewportEngine::CommandResult result = engine.rejectMalformedEnumCommand();

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(result.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(result.commandRevisionChanged, true);
    QVERIFY(result.commandRevision.isValid());
}

void ViewportEngineTest::clearFromEmptyIsAcceptedNoop()
{
    ViewportEngine engine;
    const ImageViewportStateSnapshot snapshot = engine.snapshot();

    const ViewportEngine::CommandResult result = engine.clearFromEmpty();

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(result.reason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(result.commandRevisionChanged, false);
    QCOMPARE(result.commandRevision.isValid(), false);
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(engine.snapshot(), snapshot);
}

void ViewportEngineTest::presentationNoopValidatesEnumShape()
{
    ViewportEngine engine;

    const ViewportEngine::CommandResult accepted
        = engine.validatePresentationNoop(ImageViewport::FitMode::Contain);
    QCOMPARE(accepted.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(accepted.commandRevisionChanged, false);

    const ViewportEngine::CommandResult rejected
        = engine.validatePresentationNoop(static_cast<ImageViewport::FitMode>(-1));
    QCOMPARE(rejected.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(rejected.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(rejected.commandRevisionChanged, true);
    QVERIFY(rejected.commandRevision.isValid());
}

void ViewportEngineTest::defaultPresentationStateMatchesPublicDefaults()
{
    ViewportEngine engine;
    ImageViewport item;

    const ImageViewportPresentationSnapshot presentation = item.state().presentation();
    QCOMPARE(engine.presentationState().fitMode, presentation.fitMode());
    QCOMPARE(engine.presentationState().manualZoom * 100.0, presentation.zoomPercent());
    QCOMPARE(engine.presentationState().rotationDegrees, presentation.rotationDegrees());
    QCOMPARE(engine.presentationState().mirrorHorizontally, presentation.mirrorHorizontally());
    QCOMPARE(engine.presentationState().mirrorVertically, presentation.mirrorVertically());
    QCOMPARE(engine.presentationState().spreadDirection, presentation.spreadDirection());
    QCOMPARE(engine.presentationState().pageGap, presentation.pageGap());
    QCOMPARE(engine.presentationState().backgroundMode, presentation.backgroundMode());
    QCOMPARE(engine.presentationState().backgroundColor, presentation.backgroundColor());
    QCOMPARE(engine.presentationState().smoothing, presentation.smoothing());
    QCOMPARE(engine.presentationState().mipmap, presentation.mipmap());
    QCOMPARE(
        engine.presentationState().qualityPreference, ImageViewport::QualityPreference::Default);
    QCOMPARE(engine.presentationState().exactnessPreference,
        ImageViewport::ExactnessPreference::Default);
}

void ViewportEngineTest::geometryProjectionUsesEnginePresentationState()
{
    ViewportEngine engine;
    ImageViewportPresentationCommand command;
    command.setPageGap(4.0);
    command.setSpreadDirection(ImageViewport::SpreadDirection::RightToLeft);
    command.setManualZoomPercent(200.0);
    command.setRotationDegrees(90);
    command.setMirrorHorizontally(true);
    QCOMPARE(
        engine
            .applyPresentationCommand({ command,
                { true, QRectF(0.0, 0.0, 100.0, 80.0), QSizeF(20.0, 10.0), QSizeF(8.0, 10.0), 2.0 },
                {}, true })
            .command.outcome,
        ImageViewport::CommandOutcome::Accepted);

    const PresentationGeometry::State geometry = engine.geometryState(
        { true, QRectF(0.0, 0.0, 100.0, 80.0), QSizeF(20.0, 10.0), QSizeF(8.0, 10.0), 2.0 });

    QCOMPARE(geometry.hasReadyDisplay, true);
    QCOMPARE(geometry.itemBounds, QRectF(0.0, 0.0, 100.0, 80.0));
    QCOMPARE(geometry.primaryImageSize, QSizeF(20.0, 10.0));
    QCOMPARE(geometry.secondaryImageSize, QSizeF(8.0, 10.0));
    QCOMPARE(geometry.pageGap, 4.0);
    QCOMPARE(geometry.spreadDirection, ImageViewport::SpreadDirection::RightToLeft);
    QCOMPARE(geometry.fitMode, ImageViewport::FitMode::Manual);
    QCOMPARE(geometry.rotationDegrees, 90);
    QCOMPARE(geometry.mirrorHorizontally, true);
    QCOMPARE(geometry.mirrorVertically, false);
    QCOMPARE(geometry.manualZoom, 2.0);
    QCOMPARE(geometry.devicePixelRatio, 2.0);
    QCOMPARE(geometry.contentPosition, QPointF());
    QCOMPARE(PresentationGeometry::spreadSize(geometry), QSizeF(32.0, 10.0));
}

void ViewportEngineTest::renderSnapshotUsesEnginePresentationAndPayloadState()
{
    ViewportEngine engine;
    ImageViewportPresentationCommand command;
    command.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    command.setBackgroundColor(QColor(0x10, 0x20, 0x30));
    command.setRotationDegrees(90);
    command.setSmoothing(false);
    command.setMipmap(true);
    command.setMirrorHorizontally(true);
    command.setMirrorVertically(true);
    QCOMPARE(
        engine
            .applyPresentationCommand({ command,
                { true, QRectF(0.0, 0.0, 100.0, 80.0), QSizeF(20.0, 10.0), {}, 1.0 }, {}, true })
            .command.outcome,
        ImageViewport::CommandOutcome::Accepted);

    auto& request = engine.requestState();
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        ImageViewportInternal::DisplayRequestTarget {
            2, 100, ImageViewportInternal::ProviderRequestTargetKind::Frame },
        ImageViewportInternal::ResolvedFrameIdentity { 2, 100 }, true);

    QImage primaryImage(20, 10, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::red);
    QImage secondaryImage(8, 10, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::blue);

    auto& display = engine.displayState();
    display.pendingRenderPayload
        = { true, request.sequenceGeneration, request.activeRequest.identity.id, 3, primaryImage };
    display.secondaryPendingRenderPayload = { true, request.sequenceGeneration,
        request.activeRequest.identity.id, 4, secondaryImage };

    ViewportRenderSnapshotInput input;
    input.itemSize = QSizeF(100.0, 80.0);
    input.pendingTargetCommit = true;
    input.preparedPayload = display.pendingRenderPayload;
    input.geometryState = engine.geometryState(
        { true, QRectF(0.0, 0.0, 100.0, 80.0), QSizeF(20.0, 10.0), QSizeF(8.0, 10.0), 2.0 });

    const ViewportRenderSnapshot snapshot = engine.renderSnapshot(input);

    QCOMPARE(snapshot.itemSize, QSizeF(100.0, 80.0));
    QCOMPARE(snapshot.backgroundMode, ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(snapshot.backgroundColor, QColor(0x10, 0x20, 0x30));
    QCOMPARE(snapshot.rotationDegrees, 90);
    QCOMPARE(snapshot.smoothing, false);
    QCOMPARE(snapshot.mipmap, true);
    QCOMPARE(snapshot.mirrorHorizontally, true);
    QCOMPARE(snapshot.mirrorVertically, true);
    QCOMPARE(snapshot.preparedPayload.payloadId, 3);
    QCOMPARE(snapshot.targetRect,
        PresentationGeometry::pageItemRect(input.geometryState, ImageViewport::PageRole::Primary)
            .intersected(input.geometryState.itemBounds));
    QCOMPARE(snapshot.sourceRect,
        PresentationGeometry::visiblePageRect(
            input.geometryState, ImageViewport::PageRole::Primary));

    QCOMPARE(snapshot.imageLayers.size(), 2);
    QCOMPARE(snapshot.imageLayers.at(0).role, ImageViewport::PageRole::Primary);
    QCOMPARE(snapshot.imageLayers.at(0).preparedPayload.payloadId, 3);
    QCOMPARE(snapshot.imageLayers.at(0).targetRect, snapshot.targetRect);
    QCOMPARE(snapshot.imageLayers.at(0).sourceRect, snapshot.sourceRect);
    QCOMPARE(snapshot.imageLayers.at(0).rotationDegrees, 90);
    QCOMPARE(snapshot.imageLayers.at(0).mirrorHorizontally, true);
    QCOMPARE(snapshot.imageLayers.at(0).mirrorVertically, true);
    QCOMPARE(snapshot.imageLayers.at(1).role, ImageViewport::PageRole::Secondary);
    QCOMPARE(snapshot.imageLayers.at(1).preparedPayload.payloadId, 4);
    QCOMPARE(snapshot.imageLayers.at(1).preparedPayload.image, secondaryImage);
    QCOMPARE(snapshot.imageLayers.at(1).rotationDegrees, 90);
}

void ViewportEngineTest::validPresentationTargetAssignmentAllocatesGenerationAndRoleSet()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(&frame));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    const ViewportEngine::PresentationTargetAssignmentResult result
        = engine.assignPresentationTarget(
            { ImageViewportPresentationTarget(sequence->sequence()), {} });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(result.command.reason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(result.command.commandRevisionChanged, false);
    QCOMPARE(result.presentationTargetChanged, true);
    QCOMPARE(result.clear, false);
    QCOMPARE(result.presentationTargetState.presentationTarget.primary(), sequence->sequence());
    QCOMPARE(result.presentationTargetState.presentationTarget.secondary(), nullptr);
    QCOMPARE(result.presentationTargetState.acceptedRoleSet, ImageViewportRoleSet(true, false));
    QCOMPARE(result.presentationTargetState.targetRoleSet, ImageViewportRoleSet(true, false));
    QCOMPARE(result.presentationTargetState.generation, 1);
    QCOMPARE(result.presentationTargetState.primaryRoleGeneration, 1);
    QCOMPARE(result.presentationTargetState.secondaryRoleGeneration, 0);
    QCOMPARE(result.presentationTargetState.activeRoleValid, true);
    QCOMPARE(result.presentationTargetState.activeRole, ImageViewport::PageRole::Primary);
    QCOMPARE(
        engine.presentationTargetState().generation, result.presentationTargetState.generation);
}

void ViewportEngineTest::twoRoleAssignmentIsAcceptedAtomically()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromFrame(&primaryFrame));
    QVERIFY(primary->sequence());
    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondary(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondary->sequence());

    ViewportEngine engine;
    const ViewportEngine::PresentationTargetAssignmentResult result
        = engine.assignPresentationTarget(
            { ImageViewportPresentationTarget(primary->sequence(), secondary->sequence()), {} });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(result.presentationTargetState.presentationTarget.primary(), primary->sequence());
    QCOMPARE(result.presentationTargetState.presentationTarget.secondary(), secondary->sequence());
    QCOMPARE(result.presentationTargetState.acceptedRoleSet, ImageViewportRoleSet(true, true));
    QCOMPARE(result.presentationTargetState.targetRoleSet, ImageViewportRoleSet(true, true));
    QCOMPARE(result.presentationTargetState.primaryRoleGeneration,
        result.presentationTargetState.generation);
    QCOMPARE(result.presentationTargetState.secondaryRoleGeneration,
        result.presentationTargetState.generation);
}

void ViewportEngineTest::invalidPresentationTargetAssignmentMutatesOnlyCommandDiagnostics()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
    ImageFrame secondaryFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondary(factory.fromFrame(&secondaryFrame));
    QVERIFY(primary->sequence());
    QVERIFY(secondary->sequence());

    ViewportEngine engine;
    QVERIFY(engine
            .assignPresentationTarget({ ImageViewportPresentationTarget(primary->sequence()), {} })
            .presentationTargetChanged);
    const ViewportEngine::PresentationTargetState previousState = engine.presentationTargetState();
    ImageViewportPresentationTarget secondaryOnly;
    secondaryOnly.setSecondary(secondary->sequence());

    const ViewportEngine::PresentationTargetAssignmentResult result
        = engine.assignPresentationTarget({ secondaryOnly, {} });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(result.command.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(result.command.commandRevisionChanged, true);
    QVERIFY(result.command.commandRevision.isValid());
    QCOMPARE(result.presentationTargetChanged, false);
    QCOMPARE(engine.presentationTargetState().presentationTarget, previousState.presentationTarget);
    QCOMPARE(engine.presentationTargetState().acceptedRoleSet, previousState.acceptedRoleSet);
    QCOMPARE(engine.presentationTargetState().generation, previousState.generation);
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::InvalidRequest);
}

void ViewportEngineTest::invalidTransitionPolicyMutatesOnlyCommandDiagnostics()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
    ImageFrame replacementFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> replacement(factory.fromFrame(&replacementFrame));
    QVERIFY(primary->sequence());
    QVERIFY(replacement->sequence());

    ViewportEngine engine;
    QVERIFY(engine
            .assignPresentationTarget({ ImageViewportPresentationTarget(primary->sequence()), {} })
            .presentationTargetChanged);
    const ViewportEngine::PresentationTargetState previousState = engine.presentationTargetState();
    PresentationTargetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(
        PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);

    const ViewportEngine::PresentationTargetAssignmentResult result
        = engine.assignPresentationTarget(
            { ImageViewportPresentationTarget(replacement->sequence()), invalidPolicy });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(result.presentationTargetChanged, false);
    QCOMPARE(engine.presentationTargetState().presentationTarget, previousState.presentationTarget);
    QCOMPARE(engine.presentationTargetState().acceptedRoleSet, previousState.acceptedRoleSet);
    QCOMPARE(engine.presentationTargetState().generation, previousState.generation);
}

void ViewportEngineTest::clearPresentationTargetAllocatesTransactionAndThenNoops()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(&frame));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    QVERIFY(engine
            .assignPresentationTarget({ ImageViewportPresentationTarget(sequence->sequence()), {} })
            .presentationTargetChanged);

    const ViewportEngine::PresentationTargetAssignmentResult clearResult
        = engine.assignPresentationTarget({ ImageViewportPresentationTarget::clear(), {} });

    QCOMPARE(clearResult.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(clearResult.clear, true);
    QCOMPARE(clearResult.presentationTargetChanged, true);
    QCOMPARE(clearResult.presentationTargetState.presentationTarget,
        ImageViewportPresentationTarget::clear());
    QCOMPARE(
        clearResult.presentationTargetState.acceptedRoleSet, ImageViewportRoleSet(false, false));
    QCOMPARE(clearResult.presentationTargetState.targetRoleSet, ImageViewportRoleSet(false, false));
    QCOMPARE(clearResult.presentationTargetState.generation, 2);
    QCOMPARE(clearResult.releaseDisplayedState, true);
    QCOMPARE(clearResult.resetDisplayRequests, true);
    QCOMPARE(clearResult.closeProviderSessions, true);

    const ViewportEngine::PresentationTargetAssignmentResult noopClear
        = engine.assignPresentationTarget({ ImageViewportPresentationTarget::clear(), {} });

    QCOMPARE(noopClear.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(noopClear.presentationTargetChanged, false);
    QCOMPARE(noopClear.presentationTargetState.generation,
        clearResult.presentationTargetState.generation);
    QCOMPARE(noopClear.resetDisplayRequests, false);
    QCOMPARE(noopClear.closeProviderSessions, false);
}

void ViewportEngineTest::presentationTargetAssignmentPreservesPreviousCommandDiagnostic()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(&frame));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    QVERIFY(engine.rejectInvalidCommand().commandRevisionChanged);
    const RevisionToken rejectedRevision = engine.commandDiagnostics().revision;

    const ViewportEngine::PresentationTargetAssignmentResult accepted
        = engine.assignPresentationTarget(
            { ImageViewportPresentationTarget(sequence->sequence()), {} });

    QCOMPARE(accepted.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(accepted.command.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(accepted.command.commandRevisionChanged, false);
    QCOMPARE(accepted.command.commandRevision, rejectedRevision);
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(engine.commandDiagnostics().revision, rejectedRevision);
}

void ViewportEngineTest::assignmentEffectFlagsFollowTransitionPolicy()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    QScopedPointer<ImageSequenceFactoryResult> first(factory.fromFrame(&firstFrame));
    QVERIFY(first->sequence());
    QImage secondImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame secondFrame(secondImage);
    QScopedPointer<ImageSequenceFactoryResult> second(factory.fromFrame(&secondFrame));
    QVERIFY(second->sequence());

    ViewportEngine engine;
    const ViewportEngine::PresentationTargetAssignmentResult retained
        = engine.assignPresentationTarget(
            { ImageViewportPresentationTarget(first->sequence()), {} });
    QCOMPARE(retained.retainPreviousDisplay, true);
    QCOMPARE(retained.releaseDisplayedState, false);
    QCOMPARE(retained.resetDisplayRequests, true);
    QCOMPARE(retained.stopPlayback, true);

    PresentationTargetTransitionPolicy policy;
    policy.setDisplayTransition(
        PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    const ViewportEngine::PresentationTargetAssignmentResult cleared
        = engine.assignPresentationTarget(
            { ImageViewportPresentationTarget(second->sequence()), policy });
    QCOMPARE(cleared.retainPreviousDisplay, false);
    QCOMPARE(cleared.releaseDisplayedState, true);
    QCOMPARE(cleared.resetDisplayRequests, true);
    QCOMPARE(cleared.stopPlayback, true);
}

QTEST_MAIN(ViewportEngineTest)

#include "tst_viewportengine.moc"
