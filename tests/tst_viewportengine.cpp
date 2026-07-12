#include "imageviewport_testhooks_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportengine_p.h"
#include "viewportenginetestaccess_p.h"

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

class StubProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        return new StubProviderSession(parent);
    }
};

class StubProviderAdapter final : public ImageSequenceProviderAdapter
{
    Q_OBJECT

public:
    using ImageSequenceProviderAdapter::ImageSequenceProviderAdapter;

    ImageSequenceProviderDescriptor descriptor() const override
    {
        ImageSequenceProviderDescriptor descriptor;
        descriptor.setSessionFactory(std::make_shared<StubProviderSessionFactory>());
        return descriptor;
    }
};

struct ProviderFrameQueueSetup
{
    ImageSequenceProviderRequestToken activeToken;
    quint64 activeRequestId = 0;
};

ProviderFrameQueueSetup setUpCurrentProviderFrameQueueRequest(
    ViewportEngine& engine, ImageSequenceProviderSession&)
{
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
        ImageViewportInternal::DisplayRequestTarget {
            4, 120, ImageViewportInternal::ProviderRequestTargetKind::Playback },
        ImageViewportInternal::ResolvedFrameIdentity { 4, 120 }, false);

    auto& provider = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary);
    provider.sessionActive = true;
    provider.activeFrameToken = providerRequestTokenForTest(4);
    request.roles[0].activeRequest.providerFrameToken = provider.activeFrameToken;

    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::red);
    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::blue);

    auto& display = ViewportEngineTestAccess::display(engine);
    display.roles[0].pendingRenderPayload
        = { true, request.sequenceGeneration, request.roles[0].activeRequest.identity.id, 3, primaryImage };
    display.roles[1].pendingRenderPayload = { true, request.sequenceGeneration,
        request.roles[0].activeRequest.identity.id, 4, secondaryImage };
    display.status = ImageViewport::DisplayStatus::Ready;
    display.roles[0].displayedRequest = display.activeRequestSnapshot(
        request.sequenceGeneration, request.roles[0].activeRequest, request.roles[0].activeRequest.target.position);
    display.roles[0].displayedImageSize = QSizeF(16.0, 8.0);
    display.roles[0].displayedImage = primaryImage;
    display.roles[0].retainedDisplayValid = true;
    display.roles[0].retainedRequest = display.roles[0].displayedRequest;
    display.roles[0].retainedImageSize = display.roles[0].displayedImageSize;
    display.roles[0].retainedImage = display.roles[0].displayedImage;

    return { provider.activeFrameToken, request.roles[0].activeRequest.identity.id };
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
    void providerDemandRestagingCancelsAndReissuesCurrentTarget();
    void providerTerminalReducerRejectsStaleFrameToken();
    void providerTerminalReducerCommitsFrameFailureAtomically();
    void providerTerminalReducerClosesMetadataGeneration();
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
    void providerAssignmentRegistersSessionIdentityBeforeHostOpen();
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
    auto& request = ViewportEngineTestAccess::request(engine);
    auto& display = ViewportEngineTestAccess::display(engine);

    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.logicalSize = QSizeF(16.0, 8.0);
    request.roles[0].source.facts.frameCount = 1;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Unknown }, true);
    request.status = ImageViewport::RequestStatus::Ready;
    request.reason = ImageViewport::RequestReason::Ready;
    request.requestRevision = 11;
    ViewportEngineTestAccess::publishedCommandRevision(engine) = 13;

    display.status = ImageViewport::DisplayStatus::Ready;
    display.roles[0].displayedRequest
        = display.activeRequestSnapshot(request.sequenceGeneration, request.roles[0].activeRequest, -1);
    display.roles[0].displayedImageSize = QSizeF(16.0, 8.0);
    display.roles[0].displayedImage = QImage(16, 8, QImage::Format_ARGB32_Premultiplied);
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
    const auto& display = ViewportEngineTestAccess::display(engine);

    QCOMPARE(display.status, ImageViewport::DisplayStatus::Empty);
    QCOMPARE(display.roles[0].displayedRequest.generation, 0);
    QCOMPARE(display.roles[0].displayedRequest.request.identity.id, 0);
    QCOMPARE(display.roles[1].displayedRequest.generation, 0);
    QCOMPARE(display.roles[0].displayedImageSize, QSizeF());
    QCOMPARE(display.roles[0].displayedImage.isNull(), true);
    QCOMPARE(display.roles[1].displayedImageSize, QSizeF());
    QCOMPARE(display.roles[1].displayedImage.isNull(), true);
    QCOMPARE(display.nextPreparedPayloadId, 0);
    QCOMPARE(display.roles[0].pendingRenderPayload.commitPending, false);
    QCOMPARE(display.roles[0].pendingRenderPayload.identity().isValid(), false);
    QCOMPARE(display.roles[1].pendingRenderPayload.commitPending, false);
    QCOMPARE(display.roles[0].retainedDisplayValid, false);
    QCOMPARE(display.roles[0].retainedRequest.generation, 0);
    QCOMPARE(display.roles[0].retainedImageSize, QSizeF());
    QCOMPARE(display.roles[0].retainedImage.isNull(), true);
    QCOMPARE(display.revision, 0);
}

void ViewportEngineTest::displayStateOwnsRenderPayloadAndRetainedIdentity()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    auto& display = ViewportEngineTestAccess::display(engine);

    request.sequenceGeneration = 12;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        ImageViewportInternal::DisplayRequestTarget {
            2, 100, ImageViewportInternal::ProviderRequestTargetKind::Frame },
        ImageViewportInternal::ResolvedFrameIdentity { 2, 100 }, true);

    display.status = ImageViewport::DisplayStatus::Ready;
    display.roles[0].displayedRequest = display.activeRequestSnapshot(
        request.sequenceGeneration, request.roles[0].activeRequest, request.roles[0].activeRequest.target.position);
    display.roles[0].displayedImageSize = QSizeF(16.0, 8.0);
    display.roles[0].displayedImage = QImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    display.beginPreparedPayloadIdentity(request.sequenceGeneration, request.roles[0].activeRequest);
    display.roles[0].pendingRenderPayload.commitPending = true;
    display.roles[0].pendingRenderPayload.image = display.roles[0].displayedImage;
    display.roles[1].pendingRenderPayload = { true, 12, request.roles[0].activeRequest.identity.id, 9, {} };
    display.revision = 42;

    const auto& observed = ViewportEngineTestAccess::display(engine);
    QCOMPARE(observed.status, ImageViewport::DisplayStatus::Ready);
    QCOMPARE(observed.roles[0].displayedRequest.generation, 12);
    QCOMPARE(observed.roles[0].displayedRequest.request.target.frame, 2);
    QCOMPARE(observed.roles[0].displayedRequest.request.target.position, 100);
    QCOMPARE(observed.roles[0].displayedImageSize, QSizeF(16.0, 8.0));
    QCOMPARE(observed.roles[0].pendingRenderPayload.commitPending, true);
    QCOMPARE(observed.roles[0].pendingRenderPayload.generation, 12);
    QCOMPARE(observed.roles[0].pendingRenderPayload.requestId, request.roles[0].activeRequest.identity.id);
    QCOMPARE(observed.roles[0].pendingRenderPayload.payloadId, 1);
    QCOMPARE(request.roles[0].activeRequest.preparedPayloadId, 1);
    QCOMPARE(observed.roles[1].pendingRenderPayload.payloadId, 9);
    QCOMPARE(observed.revision, 42);

    display.captureRenderFailureRetainedDisplay(true);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].retainedDisplayValid, true);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].retainedRequest.generation, 12);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].retainedImageSize, QSizeF(16.0, 8.0));
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].retainedImage.isNull(), false);

    display.clearPendingRenderPayload();
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.commitPending, false);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[1].pendingRenderPayload.commitPending, false);
}

void ViewportEngineTest::defaultRequestStateMatchesPublicDefaults()
{
    ViewportEngine engine;
    ImageViewport item;
    const auto& request = ViewportEngineTestAccess::request(engine);

    QCOMPARE(request.status, ImageViewport::RequestStatus::NoRequest);
    QCOMPARE(request.reason, ImageViewport::RequestReason::NoRequest);
    QCOMPARE(ViewportEngineTestAccess::commandReason(engine), ImageViewport::CommandReason::NoCommand);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, item.state().request().playbackPhase());
    QCOMPARE(ViewportEngineTestAccess::playback(engine).looping, item.state().presentation().looping());
    QCOMPARE(ViewportEngineTestAccess::playback(engine).stopWhenRequestReady, false);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).providerStartPending, false);
    QCOMPARE(request.roles[0].activeRequest.identity.id, 0);
    QCOMPARE(request.roles[1].activeRequest.identity.id, 0);
    QCOMPARE(request.roles[0].latestNonPlaybackRequest.identity.id, 0);
    QCOMPARE(request.roles[1].latestNonPlaybackRequest.identity.id, 0);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).position, -1);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).role, ImageViewport::PageRole::Primary);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).loopIterationsCompleted, 0);
    QCOMPARE(request.sequenceGeneration, 0);
    QCOMPARE(request.nextRequestId, 0);
    QCOMPARE(request.requestRevision, 0);
    QCOMPARE(ViewportEngineTestAccess::publishedCommandRevision(engine), 0);
}

void ViewportEngineTest::requestStateOwnsPlaybackDriverAndRequestIdentity()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);

    request.sequenceGeneration = 7;
    request.status = ImageViewport::RequestStatus::Loading;
    request.reason = ImageViewport::RequestReason::ProviderWaiting;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewport::PlaybackPhase::Waiting;
    ViewportEngineTestAccess::playback(engine).looping = true;
    ViewportEngineTestAccess::playback(engine).stopWhenRequestReady = true;
    ViewportEngineTestAccess::playback(engine).providerStartPending = true;
    ViewportEngineTestAccess::playback(engine).role = ImageViewport::PageRole::Secondary;
    ViewportEngineTestAccess::playback(engine).position = 125;
    ViewportEngineTestAccess::playback(engine).loopIterationsCompleted = 2;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
        ImageViewportInternal::DisplayRequestTarget {
            3, 120, ImageViewportInternal::ProviderRequestTargetKind::Playback },
        false);
    request.roles[1].activeRequest.identity = request.roles[0].activeRequest.identity;
    request.roles[1].activeRequest.target
        = { 4, 160, ImageViewportInternal::ProviderRequestTargetKind::Playback };
    request.roles[1].latestNonPlaybackRequest.identity = request.roles[1].activeRequest.identity;
    request.roles[1].latestNonPlaybackRequest.target
        = { 1, 40, ImageViewportInternal::ProviderRequestTargetKind::Frame };

    const auto& observed = ViewportEngineTestAccess::request(engine);
    QCOMPARE(observed.sequenceGeneration, 7);
    QCOMPARE(observed.status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(observed.reason, ImageViewport::RequestReason::ProviderWaiting);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewport::PlaybackPhase::Waiting);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).looping, true);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).stopWhenRequestReady, true);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).providerStartPending, true);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).role, ImageViewport::PageRole::Secondary);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).position, 125);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).loopIterationsCompleted, 2);
    QVERIFY(observed.roles[0].activeRequest.identity.id != 0);
    QCOMPARE(observed.roles[0].activeRequest.identity.origin,
        ImageViewportInternal::DisplayRequestOrigin::Playback);
    QCOMPARE(observed.roles[0].activeRequest.target.frame, 3);
    QCOMPARE(observed.roles[0].activeRequest.target.position, 120);
    QCOMPARE(observed.roles[1].activeRequest.identity.id, observed.roles[0].activeRequest.identity.id);
    QCOMPARE(
        observed.roles[1].activeRequest.identity.origin, observed.roles[0].activeRequest.identity.origin);
    QCOMPARE(observed.roles[1].activeRequest.target.frame, 4);
    QCOMPARE(observed.roles[1].latestNonPlaybackRequest.target.frame, 1);
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
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.timed = true;
    request.roles[0].source.facts.frameCount = 2;
    request.roles[0].source.facts.totalDuration = 350;
    request.roles[0].source.facts.timingIntervals
        = TimingIntervals::fromFrameDurations({ 100, 250 });
    request.roles[0].activeRequest.target.frame = 1;
    ViewportEngineTestAccess::playback(engine).role = ImageViewport::PageRole::Primary;
    ViewportEngineTestAccess::playback(engine).position = 125;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewport::PlaybackPhase::Playing;
    request.status = ImageViewport::RequestStatus::Ready;

    const auto effect = engine.playbackScheduleEffect();

    QCOMPARE(effect.action, ViewportPlaybackScheduleEffect::Action::ArmAfter);
    QCOMPARE(effect.delayMilliseconds, 225);
}

void ViewportEngineTest::playbackScheduleUsesProviderFrameRemainderByRole()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[1].source.facts.present = true;
    request.roles[1].source.facts.provider = true;
    request.roles[1].activeRequest.target.frame = 0;
    ViewportEngineTestAccess::playback(engine).role = ImageViewport::PageRole::Secondary;
    ViewportEngineTestAccess::playback(engine).position = 40;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewport::PlaybackPhase::Playing;
    request.status = ImageViewport::RequestStatus::Ready;
    auto& provider = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Secondary);
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
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    ViewportEngineTestAccess::playback(engine).role = ImageViewport::PageRole::Primary;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewport::PlaybackPhase::Playing;

    const auto result = engine.applyPlaybackCommand(
        { { ViewportPlaybackCommand::Kind::Pause, ImageViewport::PageRole::Primary }, {} });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewport::PlaybackPhase::Paused);
    QCOMPARE(result.changes.playbackPhase, true);
    QCOMPARE(result.schedule.action, ViewportPlaybackScheduleEffect::Action::Stop);
}

void ViewportEngineTest::playbackTickAdvancesBuiltInTargetInEngine()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.sequenceGeneration = 7;
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.timed = true;
    request.roles[0].source.facts.frameCount = 2;
    request.roles[0].source.facts.totalDuration = 350;
    request.roles[0].source.facts.logicalSize = QSizeF(16.0, 8.0);
    request.roles[0].source.facts.timingIntervals
        = TimingIntervals::fromFrameDurations({ 100, 250 });
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, 0, ImageViewportInternal::ProviderRequestTargetKind::Unknown },
        { 0, 0 }, true);
    request.status = ImageViewport::RequestStatus::Ready;
    request.reason = ImageViewport::RequestReason::Ready;
    ViewportEngineTestAccess::playback(engine).role = ImageViewport::PageRole::Primary;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewport::PlaybackPhase::Playing;
    ViewportEngineTestAccess::playback(engine).position = 0;

    const auto result = engine.advancePlayback(
        { 100, { true, QRectF(0.0, 0.0, 100.0, 100.0), QSizeF(16.0, 8.0), {}, 1.0 } });

    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.target.frame, 1);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.target.position, 100);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.origin,
        ImageViewportInternal::DisplayRequestOrigin::Playback);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewport::PlaybackPhase::Waiting);
    QCOMPARE(result.changes.requestState, true);
    QCOMPARE(result.changes.scheduleUpdate, true);
    QCOMPARE(result.schedule.action, ViewportPlaybackScheduleEffect::Action::Stop);
}

void ViewportEngineTest::defaultProviderStateMatchesEmptyGeneration()
{
    ViewportEngine engine;
    const auto& provider = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary);
    const auto& secondaryProvider = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Secondary);

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
    auto& provider = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary);
    auto& secondaryProvider = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Secondary);

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

    const auto& observed = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary);
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

    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Secondary).sessionSerial, 21);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Secondary).nextRequestToken, 9);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Secondary).metadataReady, true);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Secondary).logicalSize, QSizeF(4.0, 6.0));
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).logicalSize, QSizeF(16.0, 8.0));
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
    QVERIFY(!ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).activeFrameToken.isValid());
    QVERIFY(!ViewportEngineTestAccess::request(engine).roles[0].activeRequest.providerFrameToken.isValid());
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).queuedFrameGeneration, 7);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).queuedFrameRequestId, setup.activeRequestId);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).queuedFrame, 4);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).queuedPosition, 120);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).queuedResolvedFrame.frame, 4);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).queuedResolvedFrame.position, 120);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).queuedFrameFromPlayback, true);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).queuedFrameTargetKind,
        ImageViewportInternal::ProviderRequestTargetKind::Playback);
    QCOMPARE(ViewportEngineTestAccess::request(engine).status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(ViewportEngineTestAccess::request(engine).reason, ImageViewport::RequestReason::RequestQueued);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.commitPending, false);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[1].pendingRenderPayload.commitPending, false);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].retainedDisplayValid, false);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].retainedRequest.generation, 0);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].retainedImageSize, QSizeF());
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].retainedImage.isNull(), true);
}

void ViewportEngineTest::providerDisplayDemandProjectsCanonicalEngineFacts()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.requestRevision = 11;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        { 2, 120, ImageViewportInternal::ProviderRequestTargetKind::Position },
        { 2, 100 }, false);
    ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).logicalSize = QSizeF(16.0, 8.0);
    ViewportEngineTestAccess::display(engine).roles[0].displayedPayload.quality = ImageViewport::PayloadQuality::Preview;
    ViewportEngineTestAccess::display(engine).roles[0].displayedPayload.exactness
        = ImageViewport::PayloadExactness::NotExact;
    ViewportEngineTestAccess::display(engine).roles[0].displayedPayload.payloadRasterSize = QSizeF(8.0, 4.0);
    ViewportEngineTestAccess::display(engine).roles[0].displayedPayload.sourceToPayloadScale = QSizeF(0.5, 0.5);

    ViewportEngine::GeometryInput geometry;
    geometry.primaryPresent = true;
    geometry.itemBounds = QRectF(0.0, 0.0, 100.0, 50.0);
    geometry.primarySize = QSizeF(16.0, 8.0);
    geometry.devicePixelRatio = 2.0;
    const ImageSequenceProviderDisplayDemand demand
        = engine.providerDisplayDemand(ImageViewport::PageRole::Primary, geometry);

    QVERIFY(demand.demandRevision().isValid());
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.demandRevision, demand.demandRevision());
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

void ViewportEngineTest::providerDemandRestagingCancelsAndReissuesCurrentTarget()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        { 2, 120, ImageViewportInternal::ProviderRequestTargetKind::Position }, { 2, 100 }, false);
    auto& provider = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary);
    provider.sessionActive = true;
    provider.metadataReady = true;
    provider.logicalSize = QSizeF(16.0, 8.0);
    provider.activeFrameToken = providerRequestTokenForTest(4);
    provider.nextRequestToken = 4;
    request.roles[0].activeRequest.providerFrameToken = provider.activeFrameToken;

    ViewportEngine::GeometryInput geometry { true, QRectF(0.0, 0.0, 100.0, 50.0),
        QSizeF(16.0, 8.0), {}, 1.0 };
    const auto oldDemand
        = engine.providerDisplayDemand(ImageViewport::PageRole::Primary, geometry).demandRevision();
    geometry.itemBounds = QRectF(0.0, 0.0, 200.0, 100.0);
    geometry.devicePixelRatio = 2.0;

    const auto effects = engine.restageProviderDemands(geometry);

    QCOMPARE(effects[0].cancelToken, providerRequestTokenForTest(4));
    QCOMPARE(effects[0].sendCommand, true);
    QCOMPARE(effects[0].command.frame, 2);
    QCOMPARE(effects[0].command.position, 120);
    QCOMPARE(effects[0].command.demand.targetDisplaySizePixels(), QSizeF(400.0, 200.0));
    QCOMPARE(effects[0].command.demand.effectiveDevicePixelRatio(), 2.0);
    QVERIFY(effects[0].command.demand.demandRevision() != oldDemand);
    QCOMPARE(request.roles[0].activeRequest.demandRevision,
        effects[0].command.demand.demandRevision());
    QCOMPARE(request.roles[0].activeRequest.providerFrameToken, effects[0].command.token);
}

void ViewportEngineTest::providerTerminalReducerRejectsStaleFrameToken()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame }, false);
    engine.activateProviderSession(ImageViewport::PageRole::Primary);
    ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).activeFrameToken = providerRequestTokenForTest(3);
    request.roles[0].activeRequest.providerFrameToken = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).activeFrameToken;

    const auto result = engine.reduceProviderTerminalEvent(ImageViewport::PageRole::Primary,
        { providerRequestTokenForTest(4), ViewportProviderTerminalEvent::Kind::Failure,
            ImageSequenceProviderSession::UnsupportedCause::PayloadRejection,
            QStringLiteral("stale"), false });

    QCOMPARE(result.changes.requestState, false);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).sessionActive, true);
    QCOMPARE(request.status, ImageViewport::RequestStatus::NoRequest);
}

void ViewportEngineTest::providerTerminalReducerCommitsFrameFailureAtomically()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame }, false);
    ViewportEngineTestAccess::playback(engine).phase = ImageViewport::PlaybackPhase::Waiting;
    engine.activateProviderSession(ImageViewport::PageRole::Primary);
    ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).activeFrameToken = providerRequestTokenForTest(3);
    request.roles[0].activeRequest.providerFrameToken = ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).activeFrameToken;

    const auto result = engine.reduceProviderTerminalEvent(ImageViewport::PageRole::Primary,
        { providerRequestTokenForTest(3), ViewportProviderTerminalEvent::Kind::Failure,
            ImageSequenceProviderSession::UnsupportedCause::PayloadRejection,
            QStringLiteral("frame failed"), false });

    QCOMPARE(result.changes.requestState, true);
    QCOMPARE(result.changes.playbackPhase, true);
    QCOMPARE(request.status, ImageViewport::RequestStatus::Error);
    QCOMPARE(request.reason, ImageViewport::RequestReason::ProviderFailure);
    QCOMPARE(request.errorString, QStringLiteral("frame failed"));
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewport::PlaybackPhase::Stopped);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).sessionActive, true);
    QVERIFY(!ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).activeFrameToken.isValid());
    QCOMPARE(result.providerFrameTransport.closeSession, false);
}

void ViewportEngineTest::providerTerminalReducerClosesMetadataGeneration()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 9;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { -1, -1, ImageViewportInternal::ProviderRequestTargetKind::Unknown }, false);
    engine.activateProviderSession(ImageViewport::PageRole::Primary);
    ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).activeMetadataToken = providerRequestTokenForTest(5);

    const auto result = engine.reduceProviderTerminalEvent(ImageViewport::PageRole::Primary,
        { providerRequestTokenForTest(5), ViewportProviderTerminalEvent::Kind::Unsupported,
            ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest,
            QStringLiteral("unsupported"), true });

    QCOMPARE(request.status, ImageViewport::RequestStatus::Unsupported);
    QCOMPARE(request.reason, ImageViewport::RequestReason::UnsupportedRequest);
    QCOMPARE(request.targetSpreadTerminal.primary.failureScope,
        ImageViewportInternal::FailureScope::Generation);
    QCOMPARE(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).sessionActive, false);
    QCOMPARE(result.providerFrameTransport.closeSession, true);
    QVERIFY(!result.providerFrameTransport.sessionClose.metadataToken.isValid());
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
    verifyProviderFrameQueueCleared(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary));
}

void ViewportEngineTest::providerFrameQueueFlushRejectsStaleRequest()
{
    ViewportEngine engine;
    StubProviderSession session;
    setUpCurrentProviderFrameQueueRequest(engine, session);
    engine.queueProviderFrameRequest({ ImageViewport::PageRole::Primary, 4,
        ImageViewportInternal::ProviderRequestTargetKind::Playback });
    ViewportEngineTestAccess::request(engine).roles[0].activeRequest.target.frame = 5;

    const auto result = engine.flushQueuedProviderFrameRequest(ImageViewport::PageRole::Primary);

    QCOMPARE(result.startRequest, false);
    verifyProviderFrameQueueCleared(ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary));
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
                {}, 0 })
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
                { true, QRectF(0.0, 0.0, 100.0, 80.0), QSizeF(20.0, 10.0), {}, 1.0 }, {}, 0 })
            .command.outcome,
        ImageViewport::CommandOutcome::Accepted);

    auto& request = ViewportEngineTestAccess::request(engine);
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        ImageViewportInternal::DisplayRequestTarget {
            2, 100, ImageViewportInternal::ProviderRequestTargetKind::Frame },
        ImageViewportInternal::ResolvedFrameIdentity { 2, 100 }, true);

    QImage primaryImage(20, 10, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::red);
    QImage secondaryImage(8, 10, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::blue);

    auto& display = ViewportEngineTestAccess::display(engine);
    display.roles[0].pendingRenderPayload
        = { true, request.sequenceGeneration, request.roles[0].activeRequest.identity.id, 3, primaryImage };
    display.roles[1].pendingRenderPayload = { true, request.sequenceGeneration,
        request.roles[0].activeRequest.identity.id, 4, secondaryImage };

    ViewportRenderSnapshotInput input;
    input.itemSize = QSizeF(100.0, 80.0);
    input.pendingTargetCommit = true;
    input.preparedPayload = display.roles[0].pendingRenderPayload;
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
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].sequence, sequence->sequence());
    QCOMPARE(ViewportEngineTestAccess::request(engine).sequenceGeneration, 1);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.id, 1);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.target.frame, 0);
    QCOMPARE(ViewportEngineTestAccess::request(engine).status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(ViewportEngineTestAccess::request(engine).reason, ImageViewport::RequestReason::RenderWaiting);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.commitPending, true);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.image.isNull(), false);
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
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[1].sequence, secondary->sequence());
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[1].activeRequest.identity.id,
        ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.id);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[1].activeRequest.identity.origin,
        ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.origin);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[1].pendingRenderPayload.commitPending, true);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[1].pendingRenderPayload.image.isNull(), false);
}

void ViewportEngineTest::providerAssignmentRegistersSessionIdentityBeforeHostOpen()
{
    ImageSequenceFactory factory;
    StubProviderAdapter adapter;
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromProvider(&adapter));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    const auto result = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(sequence->sequence()), {} });
    const auto binding = engine.providerSessionBinding(ImageViewport::PageRole::Primary);

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(result.openPrimaryProviderSession, true);
    QCOMPARE(binding.generation, result.presentationTargetState.primaryRoleGeneration);
    QVERIFY(binding.sessionSerial != 0);
    QCOMPARE(binding.sessionActive, true);
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
    QCOMPARE(ViewportEngineTestAccess::request(engine).status, ImageViewport::RequestStatus::NoRequest);
    QCOMPARE(ViewportEngineTestAccess::display(engine).status, ImageViewport::DisplayStatus::Empty);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].sequence, nullptr);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].displayedImageSize, QSizeF());

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
