// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_testhooks_p.h"
#include "viewportengine_p.h"
#include "viewportenginetestaccess_p.h"
#include "viewportplaybackcontract_p.h"

#include <QtCore/QScopedPointer>
#include <QtGui/QImage>
#include <QtTest/QTest>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

using namespace ImageViewportTestHooks;

class StubProviderSession final : public ImageSequenceProviderSession
{
    Q_OBJECT

public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void request(const ImageSequenceProviderRequest&) override { }
};

class StubProviderSessionFactory final
{
public:
    ImageSequenceProviderSession* createSession(QObject* parent)
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
        auto factory = std::make_shared<StubProviderSessionFactory>();
        return ImageSequenceProviderDescriptor(
            {}, ImageSequenceProviderThreadingContract::AffinityBound, [factory]() {
                return ImageSequenceProviderSessionFactoryResult::created(
                    factory->createSession(nullptr));
            });
    }
};

struct ProviderFrameQueueSetup
{
    ImageSequenceProviderRequestToken activeToken;
    quint64 activeRequestId = 0;
};

void activateProviderRequestForTest(ImageViewportInternal::ProviderRequestLedger& requests,
    const ImageViewportInternal::RequestState& request, ImageSequenceProviderRequestToken token,
    ImageSequenceProviderRequestKind kind,
    ImageViewportInternal::ProviderRequestOwnership ownership
    = ImageViewportInternal::ProviderRequestOwnership::DisplayRequest)
{
    const auto& active = request.roles[0].activeRequest;
    requests.activate({ token, kind, ImageViewportPageRole::Primary, request.sequenceGeneration,
        active.identity.id, ownership, active.target, active.resolvedFrame });
}

ImageViewportInternal::PreparedPayload preparedPayloadForTest(const QImage& image,
    QSizeF sourceLogicalSize = {}, quint64 generation = 1, quint64 requestId = 1,
    quint64 payloadId = 1);

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

    auto& sessionState
        = ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary);
    auto& providerRequests
        = ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary);
    sessionState.sessionActive = true;
    activateProviderRequestForTest(providerRequests, request, providerRequestTokenForTest(4),
        ImageSequenceProviderRequestKind::Playback);

    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::red);
    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::blue);

    auto& display = ViewportEngineTestAccess::display(engine);
    display.roles[0].pendingRenderPayload = { true, request.sequenceGeneration, 3, primaryImage };
    display.roles[1].pendingRenderPayload = { true, request.sequenceGeneration, 4, secondaryImage };
    display.status = ImageViewportDisplayStatus::Ready;
    display.roles[0].displayedRequest = display.activeRequestSnapshot(request.sequenceGeneration,
        request.roles[0].activeRequest, request.roles[0].activeRequest.target.position);
    display.roles[0].displayedPayload = preparedPayloadForTest(primaryImage, QSizeF(16.0, 8.0),
        request.sequenceGeneration, request.roles[0].activeRequest.identity.id, 2);

    return { providerRequests.frameToken(), request.roles[0].activeRequest.identity.id };
}

void seedCurrentProviderFrameQueue(ViewportEngine& engine)
{
    auto& request = ViewportEngineTestAccess::request(engine);
    auto& activeRequest = request.roles[0].activeRequest;
    auto& requests
        = ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary);
    auto& display = ViewportEngineTestAccess::display(engine);

    request.status = ImageViewportRequestStatus::Loading;
    request.reason = ImageViewportRequestReason::RequestQueued;
    display.roles[0].pendingRenderPayload = {};
    requests.retireFrame();
    requests.queuedFrame
        = ImageViewportInternal::QueuedProviderFrameRequest { request.sequenceGeneration,
              activeRequest.identity.id, activeRequest.target, activeRequest.resolvedFrame, true };
}

void verifyProviderFrameQueueCleared(const ImageViewportInternal::ProviderRequestLedger& requests)
{
    QVERIFY(!requests.queuedFrame.has_value());
}

ViewportProviderEvent providerTerminalEvent(ViewportEngine& engine,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderEventKind kind,
    ImageSequenceProviderUnsupportedCause cause, const QString& diagnostic)
{
    Q_UNUSED(diagnostic);
    ViewportProviderEvent event;
    event.kind = kind;
    event.role = ImageViewportPageRole::Primary;
    event.sessionSerial
        = ViewportEngineTestAccess::providerSession(engine, event.role).sessionSerial;
    event.generation = ViewportEngineTestAccess::request(engine).sequenceGeneration;
    event.token = token;
    event.unsupportedCause = cause;
    if (kind == ImageSequenceProviderEventKind::Failed) {
        event.providerFailureAvailable = true;
        event.providerCause = ImageSequenceProviderFailureCause::ProviderInternal;
    }
    return event;
}

void setViewport(ViewportEngine& engine, ViewportEngineViewportState viewport)
{
    engine.handleViewportChanged(viewport);
}

ImageViewportInternal::PreparedPayload preparedPayloadForTest(
    const QImage& image, QSizeF sourceLogicalSize, quint64 generation, quint64, quint64 payloadId)
{
    if (sourceLogicalSize.isEmpty())
        sourceLogicalSize = image.deviceIndependentSize();
    const QSizeF payloadRasterSize(image.size());
    const QSizeF sourceToPayloadScale = sourceLogicalSize.isEmpty()
        ? QSizeF {}
        : QSizeF(payloadRasterSize.width() / sourceLogicalSize.width(),
              payloadRasterSize.height() / sourceLogicalSize.height());
    return { false, generation, payloadId, image, sourceLogicalSize, payloadRasterSize,
        sourceToPayloadScale, image.sizeInBytes(), ImageViewportPayloadQuality::Exact,
        ImageViewportPayloadExactness::ExactForSource };
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
    void malformedPlaybackCommandKindIsRejectedBeforeStateMutation();
    void authoredAutoplayReducerUsesBoundedState();
    void playbackTickAdvancesBuiltInTargetInEngine();
    void defaultProviderStateMatchesEmptyGeneration();
    void providerStateOwnsTokensQueuesAndMetadataByRole();
    void providerDemandRestagingCancelsAndReissuesCurrentTarget();
    void providerDemandInvalidatesOverflowedPhysicalSize();
    void providerTerminalReducerRejectsStaleFrameToken();
    void providerTerminalReducerCommitsFrameFailureAtomically();
    void providerDispatchFailureIsGenerationTerminalAcrossDisplayRequests();
    void providerProtocolViolationClosesFrameGeneration();
    void providerProtocolViolationsClassifyActiveIngress_data();
    void providerProtocolViolationsClassifyActiveIngress();
    void providerEndOfSequenceStateViolationRecordsObservation();
    void providerTerminalReducerClosesMetadataGeneration();
    void providerFrameQueueFlushesOnlyCurrentLoadingRequest();
    void providerFrameQueueFlushRejectsStaleRequest();
    void defaultPresentationStateMatchesPublicDefaults();
    void geometryProjectionUsesEnginePresentationState();
    void coordinateQueryOwnsValidationAndMapping();
    void coordinateQueryUsesRetainedDisplayedPresentationWithoutMutation();
    void geometryProjectionRejectsFiniteOverflow();
    void geometryToleranceNormalizesAllEffectiveBounds();
    void devicePixelRatioChangeRevisesEffectiveFitZoom();
    void renderSnapshotUsesEnginePresentationAndPayloadState();
    void renderAttemptAuthorityRejectsStaleAndDuplicateFacts();
    void validPresentationTargetAssignmentAllocatesGenerationAndRoleSet();
    void mutatingIngressPublishesCoherentSnapshotBeforeReturn();
    void twoRoleAssignmentIsAcceptedAtomically();
    void providerAssignmentRegistersSessionIdentityBeforeHostOpen();
    void invalidPresentationTargetAssignmentMutatesOnlyCommandDiagnostics();
    void invalidTransitionPolicyMutatesOnlyCommandDiagnostics();
    void deferredTargetAnchorResolvesWhenItemGeometryArrives();
    void clearPresentationTargetAllocatesTransactionAndThenNoops();
    void presentationTargetAssignmentAdvancesCommandDiagnostic();
    void assignmentAppliesDisplayAndPlaybackTransitionPolicy();
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
    QCOMPARE(ViewportEngineTestAccess::commandDiagnostics(engine).reason,
        ImageViewportCommandReason::NoCommand);
    QCOMPARE(ViewportEngineTestAccess::commandDiagnostics(engine).revision.isValid(), true);
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
    request.status = ImageViewportRequestStatus::Ready;
    request.reason = ImageViewportRequestReason::Ready;
    request.requestRevision = 11;
    ViewportEngineTestAccess::publishedCommandRevision(engine) = 13;

    display.status = ImageViewportDisplayStatus::Ready;
    display.roles[0].displayedRequest = display.activeRequestSnapshot(
        request.sequenceGeneration, request.roles[0].activeRequest, -1);
    display.roles[0].displayedPayload = preparedPayloadForTest(
        QImage(16, 8, QImage::Format_ARGB32_Premultiplied), QSizeF(16.0, 8.0),
        request.sequenceGeneration, request.roles[0].activeRequest.identity.id);
    display.revision = 12;

    const ImageViewportStateSnapshot snapshot = engine.snapshot();
    QCOMPARE(snapshot.request().status(), ImageViewportRequestStatus::Ready);
    QCOMPARE(snapshot.request().reason(), ImageViewportRequestReason::Ready);
    QCOMPARE(snapshot.display().status(), ImageViewportDisplayStatus::Ready);
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

    QCOMPARE(display.status, ImageViewportDisplayStatus::Empty);
    QCOMPARE(display.roles[0].displayedRequest.generation, 0);
    QCOMPARE(display.roles[0].displayedRequest.request.identity.id, 0);
    QCOMPARE(display.roles[1].displayedRequest.generation, 0);
    QCOMPARE(display.roles[0].displayedPayload.hasPresentableContent(), false);
    QCOMPARE(display.roles[1].displayedPayload.hasPresentableContent(), false);
    QCOMPARE(display.nextPreparedPayloadId, 0);
    QCOMPARE(display.roles[0].pendingRenderPayload.commitPending, false);
    QCOMPARE(display.roles[0].pendingRenderPayload.identity().isValid(), false);
    QCOMPARE(display.roles[1].pendingRenderPayload.commitPending, false);
    QVERIFY(display.revision != 0);
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

    display.status = ImageViewportDisplayStatus::Ready;
    display.roles[0].displayedRequest = display.activeRequestSnapshot(request.sequenceGeneration,
        request.roles[0].activeRequest, request.roles[0].activeRequest.target.position);
    display.roles[0].displayedPayload = preparedPayloadForTest(
        QImage(16, 8, QImage::Format_ARGB32_Premultiplied), QSizeF(16.0, 8.0),
        request.sequenceGeneration, request.roles[0].activeRequest.identity.id);
    display.beginPreparedPayloadIdentity(
        request.sequenceGeneration, request.roles[0].activeRequest);
    display.roles[0].pendingRenderPayload.commitPending = true;
    display.roles[0].pendingRenderPayload.image = display.roles[0].displayedPayload.image;
    display.roles[1].pendingRenderPayload = { true, 12, 9, {} };
    display.revision = 42;

    const auto& observed = ViewportEngineTestAccess::display(engine);
    QCOMPARE(observed.status, ImageViewportDisplayStatus::Ready);
    QCOMPARE(observed.roles[0].displayedRequest.generation, 12);
    QCOMPARE(observed.roles[0].displayedRequest.request.target.frame, 2);
    QCOMPARE(observed.roles[0].displayedRequest.request.target.position, 100);
    QCOMPARE(observed.roles[0].displayedPayload.sourceLogicalSize, QSizeF(16.0, 8.0));
    QCOMPARE(observed.roles[0].pendingRenderPayload.commitPending, true);
    QCOMPARE(observed.roles[0].pendingRenderPayload.generation, 12);
    QCOMPARE(observed.roles[0].pendingRenderPayload.payloadId, 1);
    QCOMPARE(request.roles[0].activeRequest.preparedPayloadId, 1);
    QCOMPARE(observed.roles[1].pendingRenderPayload.payloadId, 9);
    QCOMPARE(observed.revision, 42);

    const auto committedPayload = display.roles[0].displayedPayload;
    display.status = ImageViewportDisplayStatus::Retained;
    QCOMPARE(display.roles[0].displayedRequest.generation, 12);
    QCOMPARE(display.roles[0].displayedPayload.identity().payloadId,
        committedPayload.identity().payloadId);
    QCOMPARE(display.roles[0].displayedPayload.image, committedPayload.image);
    QCOMPARE(display.roles[0].displayedPayload.sourceLogicalSize, QSizeF(16.0, 8.0));

    display.clearPendingRenderPayload();
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.commitPending,
        false);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[1].pendingRenderPayload.commitPending,
        false);
}

void ViewportEngineTest::defaultRequestStateMatchesPublicDefaults()
{
    ViewportEngine engine;
    ImageViewport item;
    const auto& request = ViewportEngineTestAccess::request(engine);

    QCOMPARE(request.status, ImageViewportRequestStatus::NoRequest);
    QCOMPARE(request.reason, ImageViewportRequestReason::NoRequest);
    QCOMPARE(
        ViewportEngineTestAccess::commandReason(engine), ImageViewportCommandReason::NoCommand);
    QCOMPARE(
        ViewportEngineTestAccess::playback(engine).phase, item.state().request().playbackPhase());
    QCOMPARE(
        ViewportEngineTestAccess::playback(engine).looping, item.state().presentation().looping());
    QCOMPARE(ViewportEngineTestAccess::playback(engine).stopWhenRequestReady, false);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).providerStartPending, false);
    QCOMPARE(request.roles[0].activeRequest.identity.id, 0);
    QCOMPARE(request.roles[1].activeRequest.identity.id, 0);
    QCOMPARE(request.roles[0].latestNonPlaybackRequest.identity.id, 0);
    QCOMPARE(request.roles[1].latestNonPlaybackRequest.identity.id, 0);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).position, -1);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).role, ImageViewportPageRole::Primary);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).loopIterationsCompleted, 0);
    QCOMPARE(request.sequenceGeneration, 0);
    QCOMPARE(request.nextRequestId, 0);
    QVERIFY(request.requestRevision != 0);
    QVERIFY(ViewportEngineTestAccess::publishedCommandRevision(engine) != 0);
}

void ViewportEngineTest::requestStateOwnsPlaybackDriverAndRequestIdentity()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);

    request.sequenceGeneration = 7;
    request.status = ImageViewportRequestStatus::Loading;
    request.reason = ImageViewportRequestReason::ProviderWaiting;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Waiting;
    ViewportEngineTestAccess::playback(engine).looping = true;
    ViewportEngineTestAccess::playback(engine).stopWhenRequestReady = true;
    ViewportEngineTestAccess::playback(engine).providerStartPending = true;
    ViewportEngineTestAccess::playback(engine).role = ImageViewportPageRole::Secondary;
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
    QCOMPARE(observed.status, ImageViewportRequestStatus::Loading);
    QCOMPARE(observed.reason, ImageViewportRequestReason::ProviderWaiting);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Waiting);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).looping, true);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).stopWhenRequestReady, true);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).providerStartPending, true);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).role, ImageViewportPageRole::Secondary);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).position, 125);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).loopIterationsCompleted, 2);
    QVERIFY(observed.roles[0].activeRequest.identity.id != 0);
    QCOMPARE(observed.roles[0].activeRequest.identity.origin,
        ImageViewportInternal::DisplayRequestOrigin::Playback);
    QCOMPARE(observed.roles[0].activeRequest.target.frame, 3);
    QCOMPARE(observed.roles[0].activeRequest.target.position, 120);
    QCOMPARE(
        observed.roles[1].activeRequest.identity.id, observed.roles[0].activeRequest.identity.id);
    QCOMPARE(observed.roles[1].activeRequest.identity.origin,
        observed.roles[0].activeRequest.identity.origin);
    QCOMPARE(observed.roles[1].activeRequest.target.frame, 4);
    QCOMPARE(observed.roles[1].latestNonPlaybackRequest.target.frame, 1);
}

void ViewportEngineTest::playbackScheduleStopsOutsideReadyPlayingState()
{
    ViewportEngine engine;

    const auto stopped = ViewportEngineTestAccess::playbackSchedule(engine);

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
    ViewportEngineTestAccess::playback(engine).role = ImageViewportPageRole::Primary;
    ViewportEngineTestAccess::playback(engine).position = 125;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Playing;
    request.status = ImageViewportRequestStatus::Ready;

    const auto effect = ViewportEngineTestAccess::playbackSchedule(engine);

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
    ViewportEngineTestAccess::playback(engine).role = ImageViewportPageRole::Secondary;
    ViewportEngineTestAccess::playback(engine).position = 40;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Playing;
    request.status = ImageViewportRequestStatus::Ready;
    auto& providerFacts
        = ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Secondary);
    providerFacts.metadataReady = true;
    providerFacts.timedMetadata = true;
    providerFacts.timingIntervals = TimingIntervals::fromFrameDurations({ 100, 250 });

    const auto effect = ViewportEngineTestAccess::playbackSchedule(engine);

    QCOMPARE(effect.action, ViewportPlaybackScheduleEffect::Action::ArmAfter);
    QCOMPARE(effect.delayMilliseconds, 60);
}

void ViewportEngineTest::playbackPauseCommandMutatesEngineAtomically()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    ViewportEngineTestAccess::playback(engine).role = ImageViewportPageRole::Primary;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Playing;

    const auto result = engine.applyPlaybackCommand(
        { { ViewportPlaybackCommand::Kind::Pause, ImageViewportPageRole::Primary } });

    QCOMPARE(result.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Paused);
    QCOMPARE(result.transition().playbackSchedule().action,
        ViewportPlaybackScheduleEffect::Action::Stop);

    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Waiting;
    const auto waiting = engine.applyPlaybackCommand(
        { { ViewportPlaybackCommand::Kind::Pause, ImageViewportPageRole::Primary } });
    QCOMPARE(waiting.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Paused);

    request.roles[1].source.facts.present = true;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Playing;
    const auto otherRole = engine.applyPlaybackCommand(
        { { ViewportPlaybackCommand::Kind::Pause, ImageViewportPageRole::Secondary } });
    QCOMPARE(otherRole.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Playing);

    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Stopped;
    const auto stopped = engine.applyPlaybackCommand(
        { { ViewportPlaybackCommand::Kind::Pause, ImageViewportPageRole::Primary } });
    QCOMPARE(stopped.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Stopped);
}

void ViewportEngineTest::malformedPlaybackCommandKindIsRejectedBeforeStateMutation()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.status = ImageViewportRequestStatus::Ready;
    request.reason = ImageViewportRequestReason::Ready;
    auto& playback = ViewportEngineTestAccess::playback(engine);
    playback.role = ImageViewportPageRole::Primary;
    playback.phase = ImageViewportPlaybackPhase::Playing;
    playback.position = 25;

    ViewportPlaybackCommand command;
    command.kind = static_cast<ViewportPlaybackCommand::Kind>(-1);
    command.role = ImageViewportPageRole::Primary;
    const auto result = engine.applyPlaybackCommand({ command });

    QCOMPARE(result.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(engine.snapshot().diagnostics().commandReason(),
        ImageViewportCommandReason::InvalidRequest);
    QCOMPARE(playback.phase, ImageViewportPlaybackPhase::Playing);
    QCOMPARE(playback.position, 25);
    QCOMPARE(request.status, ImageViewportRequestStatus::Ready);
    QCOMPARE(request.reason, ImageViewportRequestReason::Ready);
    QVERIFY(result.transition().providerTransport().isEmpty());
}

void ViewportEngineTest::authoredAutoplayReducerUsesBoundedState()
{
    ImageSequenceAuthoredAnimationFacts autoplay;
    autoplay.setAutoplay(true);

    {
        ViewportEngine engine;
        const auto result = ViewportEngineTestAccess::reduceAuthoredAutoplay(engine);
        QCOMPARE(result.armed, false);
        QCOMPARE(result.resolved, true);
        QCOMPARE(
            ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Stopped);
    }

    {
        ViewportEngine engine;
        auto& request = ViewportEngineTestAccess::request(engine);
        request.roles[0].source.facts.present = true;
        request.roles[0].source.facts.timed = true;
        request.roles[0].source.facts.timingIntervals
            = TimingIntervals::fromFrameDurations({ 100, 250 });
        request.roles[0].source.facts.authoredAnimationFacts = autoplay;
        request.roles[0].source.facts.authoredAnimationFactsAvailable = true;
        request.roles[0].activeRequest.target.frame = 1;
        request.status = ImageViewportRequestStatus::Ready;

        const auto result = ViewportEngineTestAccess::reduceAuthoredAutoplay(engine);
        QCOMPARE(result.armed, true);
        QCOMPARE(result.resolved, true);
        QCOMPARE(result.playbackPhaseChanged, true);
        QCOMPARE(ViewportEngineTestAccess::playback(engine).position, 100);
        QCOMPARE(
            ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Playing);
    }

    {
        ViewportEngine engine;
        auto& request = ViewportEngineTestAccess::request(engine);
        auto& source = request.roles[0].source;
        source.facts.present = true;
        source.facts.provider = true;
        source.facts.providerTimedPlaybackCapability
            = ImageViewportInternal::ImageSequenceProviderCapabilitySupport::KnownFalse;
        ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Primary)
            .authoredAnimationFacts
            = autoplay;
        ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Primary)
            .authoredAnimationFactsAvailable
            = true;
        request.status = ImageViewportRequestStatus::Loading;

        const auto result = ViewportEngineTestAccess::reduceAuthoredAutoplay(engine);
        QCOMPARE(result.armed, false);
        QCOMPARE(result.resolved, true);
        QCOMPARE(
            ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Stopped);
    }

    {
        ViewportEngine engine;
        auto& request = ViewportEngineTestAccess::request(engine);
        request.roles[0].source.facts.present = true;
        request.roles[0].source.facts.provider = true;
        auto& provider
            = ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Primary);
        provider.authoredAnimationFacts = autoplay;
        provider.authoredAnimationFactsAvailable = true;
        request.status = ImageViewportRequestStatus::Loading;

        const auto result = ViewportEngineTestAccess::reduceAuthoredAutoplay(engine);
        QCOMPARE(result.armed, false);
        QCOMPARE(result.resolved, false);
        QCOMPARE(request.roles[0].activeRequest.target.providerTargetKind,
            ImageViewportInternal::ProviderRequestTargetKind::Unknown);
        QCOMPARE(ViewportEngineTestAccess::playback(engine).providerStartPending, false);
        QCOMPARE(ViewportEngineTestAccess::playback(engine).position, -1);
        QCOMPARE(
            ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Stopped);
    }

    {
        ViewportEngine engine;
        auto& request = ViewportEngineTestAccess::request(engine);
        request.roles[0].source.facts.present = true;
        request.roles[0].source.facts.provider = true;
        request.roles[0].activeRequest.target.frame = 1;
        request.status = ImageViewportRequestStatus::Ready;
        auto& provider
            = ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Primary);
        provider.authoredAnimationFacts = autoplay;
        provider.authoredAnimationFactsAvailable = true;
        provider.metadataReady = true;
        provider.timedMetadata = true;
        provider.timedPlaybackSupport = true;
        provider.timingIntervals = TimingIntervals::fromFrameDurations({ 100, 250 });

        const auto result = ViewportEngineTestAccess::reduceAuthoredAutoplay(engine);
        QCOMPARE(result.armed, true);
        QCOMPARE(result.resolved, true);
        QCOMPARE(ViewportEngineTestAccess::playback(engine).position, 100);
        QCOMPARE(
            ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Playing);
    }
}

void ViewportEngineTest::playbackTickAdvancesBuiltInTargetInEngine()
{
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromTimedFrameList(&list));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    const auto assignment = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(sequence->sequence()), {} });
    QCOMPARE(assignment.outcome(), ImageViewportCommandOutcome::Accepted);
    auto& request = ViewportEngineTestAccess::request(engine);
    request.status = ImageViewportRequestStatus::Ready;
    request.reason = ImageViewportRequestReason::Ready;
    ViewportEngineTestAccess::playback(engine).role = ImageViewportPageRole::Primary;
    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Playing;
    ViewportEngineTestAccess::playback(engine).position = 0;

    setViewport(engine, { QRectF(0.0, 0.0, 100.0, 100.0), 1.0 });
    const auto result = engine.advancePlayback({ 100 });

    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.target.frame, 1);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.target.position, 100);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.origin,
        ImageViewportInternal::DisplayRequestOrigin::Playback);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Waiting);
    QCOMPARE(result.schedulesRenderUpdate(), true);
    QCOMPARE(result.playbackSchedule().action, ViewportPlaybackScheduleEffect::Action::Stop);
}

void ViewportEngineTest::defaultProviderStateMatchesEmptyGeneration()
{
    ViewportEngine engine;
    const auto& session
        = ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary);
    const auto& requests
        = ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary);
    const auto& facts
        = ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Primary);

    QCOMPARE(session.sessionActive, false);
    QCOMPARE(session.sessionSerial, 0);
    QCOMPARE(requests.nextRequestToken, 0);
    QVERIFY(!requests.metadataToken().isValid());
    QVERIFY(!requests.frameToken().isValid());
    QVERIFY(!requests.queuedFrame.has_value());
    QCOMPARE(facts.metadataReady, false);
    QCOMPARE(facts.timedMetadata, false);
    QCOMPARE(facts.timedPlaybackSupport, false);
    QCOMPARE(facts.frameSeekSupport, false);
    QCOMPARE(facts.positionSeekSupport, false);
    QCOMPARE(facts.logicalSize, QSizeF());
    QCOMPARE(facts.timingIntervals.isValid(), false);

    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Secondary)
                 .sessionSerial,
        0);
    QCOMPARE(ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Secondary)
                 .nextRequestToken,
        0);
    QCOMPARE(ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Secondary)
                 .metadataReady,
        false);
}

void ViewportEngineTest::providerStateOwnsTokensQueuesAndMetadataByRole()
{
    ViewportEngine engine;
    auto& session
        = ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary);
    auto& requests
        = ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary);
    auto& facts = ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Primary);

    session.sessionSerial = 11;
    requests.nextRequestToken = 3;
    requests.activate({ ImageViewportTestHooks::providerRequestTokenForTest(4),
        ImageSequenceProviderRequestKind::Metadata, ImageViewportPageRole::Primary, 7, 13,
        ImageViewportInternal::ProviderRequestOwnership::Metadata });
    requests.activate({ ImageViewportTestHooks::providerRequestTokenForTest(5),
        ImageSequenceProviderRequestKind::Playback, ImageViewportPageRole::Primary, 7, 13,
        ImageViewportInternal::ProviderRequestOwnership::DisplayRequest,
        { 2, 120, ImageViewportInternal::ProviderRequestTargetKind::Playback }, { 2, 120 } });
    requests.queuedFrame = ImageViewportInternal::QueuedProviderFrameRequest { 7, 13,
        { 2, 120, ImageViewportInternal::ProviderRequestTargetKind::Playback }, { 2, 120 }, true };
    facts.metadataReady = true;
    facts.timedMetadata = true;
    facts.timedPlaybackSupport = true;
    facts.frameSeekSupport = true;
    facts.positionSeekSupport = true;
    facts.authoredAnimationFacts = ImageSequenceAuthoredAnimationFacts::finiteLoop(3);
    facts.logicalSize = QSizeF(16.0, 8.0);
    facts.timingIntervals = TimingIntervals::fromFrameDurations({ 100, 250 });

    ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Secondary)
        .sessionSerial
        = 21;
    ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Secondary)
        .nextRequestToken
        = 9;
    auto& secondaryFacts
        = ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Secondary);
    secondaryFacts.metadataReady = true;
    secondaryFacts.logicalSize = QSizeF(4.0, 6.0);

    QCOMPARE(session.sessionSerial, 11);
    QCOMPARE(requests.nextRequestToken, 3);
    QCOMPARE(requests.metadataToken(), ImageViewportTestHooks::providerRequestTokenForTest(4));
    QCOMPARE(requests.frameToken(), ImageViewportTestHooks::providerRequestTokenForTest(5));
    QVERIFY(requests.queuedFrame.has_value());
    QCOMPARE(requests.queuedFrame->generation, 7);
    QCOMPARE(requests.queuedFrame->requestId, 13);
    QCOMPARE(requests.queuedFrame->target.frame, 2);
    QCOMPARE(requests.queuedFrame->target.position, 120);
    QCOMPARE(requests.queuedFrame->resolvedFrame.frame, 2);
    QCOMPARE(requests.queuedFrame->resolvedFrame.position, 120);
    QCOMPARE(requests.queuedFrame->fromPlayback, true);
    QCOMPARE(requests.queuedFrame->target.providerTargetKind,
        ImageViewportInternal::ProviderRequestTargetKind::Playback);
    QCOMPARE(facts.metadataReady, true);
    QCOMPARE(facts.timedMetadata, true);
    QCOMPARE(facts.timedPlaybackSupport, true);
    QCOMPARE(facts.frameSeekSupport, true);
    QCOMPARE(facts.positionSeekSupport, true);
    QCOMPARE(
        facts.authoredAnimationFacts.loopMode(), ImageSequenceAuthoredAnimationLoopMode::Finite);
    QCOMPARE(facts.authoredAnimationFacts.loopCount(), 3);
    QCOMPARE(facts.logicalSize, QSizeF(16.0, 8.0));
    QCOMPARE(facts.timingIntervals.frameCount(), 2);
    QCOMPARE(facts.timingIntervals.totalDuration(), 350);

    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Secondary)
                 .sessionSerial,
        21);
    QCOMPARE(ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Secondary)
                 .nextRequestToken,
        9);
    QCOMPARE(secondaryFacts.metadataReady, true);
    QCOMPARE(secondaryFacts.logicalSize, QSizeF(4.0, 6.0));
    QCOMPARE(facts.logicalSize, QSizeF(16.0, 8.0));
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
    ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary).sessionActive
        = true;
    auto& facts = ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Primary);
    facts.metadataReady = true;
    facts.logicalSize = QSizeF(16.0, 8.0);
    auto& requests
        = ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary);
    requests.nextRequestToken = 4;
    activateProviderRequestForTest(requests, request, providerRequestTokenForTest(4),
        ImageSequenceProviderRequestKind::Position);

    const auto transition
        = engine.handleViewportChanged({ QRectF(0.0, 0.0, 200.0, 100.0), 2.0, true });
    QCOMPARE(transition.providerTransport().size(), 2);
    const auto& cancel = transition.providerTransport()[0];
    const auto& send = transition.providerTransport()[1];

    QCOMPARE(cancel.request.tokens(),
        QVector<ImageSequenceProviderRequestToken> { providerRequestTokenForTest(4) });
    QCOMPARE(send.request.frame(), 2);
    QCOMPARE(send.request.requestedPosition(), 120);
    QCOMPARE(send.request.demand().targetDisplaySizePixels(), QSizeF(400.0, 200.0));
    QCOMPARE(send.request.demand().effectiveDevicePixelRatio(), 2.0);
    QVERIFY(send.request.demand().demandRevision().isValid());
    QCOMPARE(request.roles[0].activeRequest.demandRevision, send.request.demand().demandRevision());
    QCOMPARE(requests.frameToken(), send.request.token());
}

void ViewportEngineTest::providerDemandInvalidatesOverflowedPhysicalSize()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame }, { 0, -1 }, false);
    ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary).sessionActive
        = true;
    auto& facts = ViewportEngineTestAccess::providerFacts(engine, ImageViewportPageRole::Primary);
    facts.metadataReady = true;
    facts.logicalSize = QSizeF(16.0, 8.0);
    auto& requests
        = ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary);
    requests.nextRequestToken = 4;
    activateProviderRequestForTest(
        requests, request, providerRequestTokenForTest(4), ImageSequenceProviderRequestKind::Frame);

    const auto transition = engine.handleViewportChanged(
        { QRectF(0.0, 0.0, 200.0, 100.0), std::numeric_limits<double>::max(), true });

    QVERIFY(!transition.providerTransport().isEmpty());
    const ImageSequenceProviderDisplayDemand demand
        = transition.providerTransport().constLast().request.demand();
    QCOMPARE(demand.sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(demand.visibleSourceRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QVERIFY(!demand.targetDisplaySizePixels().isValid());
    QVERIFY(std::isfinite(demand.targetDisplaySizePixels().width()));
    QVERIFY(std::isfinite(demand.targetDisplaySizePixels().height()));
    QCOMPARE(demand.effectiveDevicePixelRatio(), std::numeric_limits<double>::max());
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
    ViewportEngineTestAccess::activateProviderSession(engine, ImageViewportPageRole::Primary);
    auto& requests
        = ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary);
    requests.nextRequestToken = 4;
    activateProviderRequestForTest(
        requests, request, providerRequestTokenForTest(4), ImageSequenceProviderRequestKind::Frame);

    ViewportProviderHostEvent hostEvent;
    hostEvent.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    hostEvent.role = ImageViewportPageRole::Primary;
    hostEvent.providerEvent = providerTerminalEvent(engine, providerRequestTokenForTest(3),
        ImageSequenceProviderEventKind::Failed,
        ImageSequenceProviderUnsupportedCause::PayloadRejection, QStringLiteral("stale"));
    const auto result = engine.handleProviderHostEvent(
        ViewportEngineProviderHostEventRequest::admit(std::move(hostEvent)));

    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
                 .sessionActive,
        true);
    QCOMPARE(request.status, ImageViewportRequestStatus::NoRequest);
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
    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Waiting;
    ViewportEngineTestAccess::activateProviderSession(engine, ImageViewportPageRole::Primary);
    activateProviderRequestForTest(
        ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary), request,
        providerRequestTokenForTest(3), ImageSequenceProviderRequestKind::Frame);

    ViewportProviderHostEvent hostEvent;
    hostEvent.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    hostEvent.role = ImageViewportPageRole::Primary;
    hostEvent.providerEvent = providerTerminalEvent(engine, providerRequestTokenForTest(3),
        ImageSequenceProviderEventKind::Failed,
        ImageSequenceProviderUnsupportedCause::PayloadRejection, QStringLiteral("frame failed"));
    const auto result = engine.handleProviderHostEvent(
        ViewportEngineProviderHostEventRequest::admit(std::move(hostEvent)));

    QCOMPARE(request.status, ImageViewportRequestStatus::Error);
    QCOMPARE(request.reason, ImageViewportRequestReason::ProviderFailure);
    QVERIFY(!request.errorString.isEmpty());
    QVERIFY(!request.errorString.text().contains(QStringLiteral("frame failed")));
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Stopped);
    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
                 .sessionActive,
        true);
    QVERIFY(!ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary)
            .frameToken()
            .isValid());
    QVERIFY(result.providerTransport().isEmpty());
}

void ViewportEngineTest::providerDispatchFailureIsGenerationTerminalAcrossDisplayRequests()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame }, false);
    ViewportEngineTestAccess::activateProviderSession(engine, ImageViewportPageRole::Primary);
    activateProviderRequestForTest(
        ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary), request,
        providerRequestTokenForTest(3), ImageSequenceProviderRequestKind::Frame);

    ViewportProviderHostEvent hostEvent;
    hostEvent.kind = ViewportProviderHostEvent::Kind::DispatchFailed;
    hostEvent.role = ImageViewportPageRole::Primary;
    hostEvent.token = providerRequestTokenForTest(3);
    const auto result = engine.handleProviderHostEvent(
        ViewportEngineProviderHostEventRequest::admit(std::move(hostEvent)));

    QCOMPARE(request.status, ImageViewportRequestStatus::Error);
    QCOMPARE(request.reason, ImageViewportRequestReason::ProviderFailure);
    QVERIFY(request.generationTerminal.primary.terminal);
    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
                 .sessionActive,
        false);
    QCOMPARE(result.providerTransport().size(), 1);
    QCOMPARE(
        result.providerTransport()[0].kind, ViewportProviderTransportCommand::Kind::CloseSession);

    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame }, false);
    QVERIFY(request.generationTerminal.sealed);
    QCOMPARE(request.generationTerminal.generation, request.sequenceGeneration);
    QVERIFY(request.generationTerminal.primary.terminal);
}

void ViewportEngineTest::providerProtocolViolationClosesFrameGeneration()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame }, false);
    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Waiting;
    ViewportEngineTestAccess::activateProviderSession(engine, ImageViewportPageRole::Primary);
    activateProviderRequestForTest(
        ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary), request,
        providerRequestTokenForTest(3), ImageSequenceProviderRequestKind::Frame);
    const quint64 requestId = request.roles[0].activeRequest.identity.id;
    const quint64 sessionSerial
        = ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
              .sessionSerial;

    ViewportProviderHostEvent hostEvent;
    hostEvent.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    hostEvent.role = ImageViewportPageRole::Primary;
    hostEvent.providerEvent = providerTerminalEvent(engine, providerRequestTokenForTest(3),
        ImageSequenceProviderEventKind::Unsupported,
        static_cast<ImageSequenceProviderUnsupportedCause>(-1),
        QStringLiteral("provider-supplied private detail"));
    const auto result = engine.handleProviderHostEvent(
        ViewportEngineProviderHostEventRequest::admit(std::move(hostEvent)));

    QCOMPARE(request.status, ImageViewportRequestStatus::Error);
    QCOMPARE(request.reason, ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(request.errorString.text(), QStringLiteral("provider protocol violation"));
    QVERIFY(request.generationTerminal.primary.terminal);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Stopped);
    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
                 .sessionActive,
        false);
    QCOMPARE(result.providerTransport().size(), 1);
    QCOMPARE(
        result.providerTransport()[0].kind, ViewportProviderTransportCommand::Kind::CloseSession);
    QCOMPARE(result.observations().size(), 1);
    const auto& observation = result.observations().constFirst();
    QCOMPARE(observation.subsystem, ImageViewportInternal::InternalObservationSubsystem::Engine);
    QCOMPARE(
        observation.category, ImageViewportInternal::InternalObservationCategory::AdmissionFailure);
    QCOMPARE(observation.cause,
        ImageViewportInternal::InternalObservationCause::ProviderProtocolEventShapeMismatch);
    QVERIFY(observation.identity.roleValid);
    QCOMPARE(observation.identity.role, ImageViewportPageRole::Primary);
    QCOMPARE(observation.identity.generation, quint64(7));
    QCOMPARE(observation.identity.sessionSerial, sessionSerial);
    QCOMPARE(observation.identity.requestId, requestId);
    QCOMPARE(observation.identity.providerToken, quint64(3));
}

void ViewportEngineTest::providerProtocolViolationsClassifyActiveIngress_data()
{
    QTest::addColumn<int>("violation");
    QTest::addColumn<int>("expectedCause");

    QTest::newRow("token")
        << 0 << int(ImageViewportInternal::InternalObservationCause::ProviderProtocolTokenMismatch);
    QTest::newRow("role")
        << 1 << int(ImageViewportInternal::InternalObservationCause::ProviderProtocolRoleMismatch);
    QTest::newRow("generation")
        << 2
        << int(ImageViewportInternal::InternalObservationCause::ProviderProtocolGenerationMismatch);
    QTest::newRow("event-kind")
        << 3
        << int(ImageViewportInternal::InternalObservationCause::ProviderProtocolEventKindMismatch);
    QTest::newRow("event-shape")
        << 4
        << int(ImageViewportInternal::InternalObservationCause::ProviderProtocolEventShapeMismatch);
}

void ViewportEngineTest::providerProtocolViolationsClassifyActiveIngress()
{
    QFETCH(int, violation);
    QFETCH(int, expectedCause);

    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame }, false);
    ViewportEngineTestAccess::activateProviderSession(engine, ImageViewportPageRole::Primary);
    auto& requests
        = ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary);
    activateProviderRequestForTest(
        requests, request, providerRequestTokenForTest(3), ImageSequenceProviderRequestKind::Frame);
    const quint64 requestId = request.roles[0].activeRequest.identity.id;
    const quint64 sessionSerial
        = ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
              .sessionSerial;

    ViewportProviderEvent event = providerTerminalEvent(engine, providerRequestTokenForTest(3),
        ImageSequenceProviderEventKind::Failed,
        ImageSequenceProviderUnsupportedCause::PayloadRejection, QStringLiteral("private detail"));
    switch (violation) {
    case 0:
        event.token = {};
        break;
    case 1:
        requests.active.first().role = ImageViewportPageRole::Secondary;
        break;
    case 2:
        --requests.active.first().generation;
        break;
    case 3:
        event.kind = ImageSequenceProviderEventKind::MetadataReady;
        break;
    case 4:
        event.kind = ImageSequenceProviderEventKind::Unsupported;
        event.unsupportedCause = static_cast<ImageSequenceProviderUnsupportedCause>(-1);
        break;
    default:
        Q_UNREACHABLE();
    }

    ViewportProviderHostEvent hostEvent;
    hostEvent.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    hostEvent.role = ImageViewportPageRole::Primary;
    hostEvent.providerEvent = event;
    const auto result = engine.handleProviderHostEvent(
        ViewportEngineProviderHostEventRequest::admit(std::move(hostEvent)));

    QCOMPARE(request.status, ImageViewportRequestStatus::Error);
    QCOMPARE(request.reason, ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
                 .sessionActive,
        false);
    QCOMPARE(result.providerTransport().size(), 1);
    QCOMPARE(result.observations().size(), 1);
    const auto& observation = result.observations().constFirst();
    QCOMPARE(observation.subsystem, ImageViewportInternal::InternalObservationSubsystem::Engine);
    QCOMPARE(
        observation.category, ImageViewportInternal::InternalObservationCategory::AdmissionFailure);
    QCOMPARE(int(observation.cause), expectedCause);
    QVERIFY(observation.identity.roleValid);
    QCOMPARE(observation.identity.role, ImageViewportPageRole::Primary);
    QCOMPARE(observation.identity.generation, quint64(7));
    QCOMPARE(observation.identity.sessionSerial, sessionSerial);
    QCOMPARE(observation.identity.requestId, requestId);
    QCOMPARE(observation.identity.providerToken, violation == 0 ? quint64(0) : quint64(3));
    QCOMPARE(observation.detail, int(event.kind));
}

void ViewportEngineTest::providerEndOfSequenceStateViolationRecordsObservation()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.provider = true;
    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
        { 1, 100, ImageViewportInternal::ProviderRequestTargetKind::Playback }, { 1, 100 }, false);
    ViewportEngineTestAccess::activateProviderSession(engine, ImageViewportPageRole::Primary);
    auto& requests
        = ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary);
    activateProviderRequestForTest(requests, request, providerRequestTokenForTest(3),
        ImageSequenceProviderRequestKind::Playback);
    const quint64 requestId = request.roles[0].activeRequest.identity.id;
    const quint64 sessionSerial
        = ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
              .sessionSerial;

    ViewportProviderHostEvent hostEvent;
    hostEvent.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    hostEvent.role = ImageViewportPageRole::Primary;
    hostEvent.providerEvent = providerTerminalEvent(engine, providerRequestTokenForTest(3),
        ImageSequenceProviderEventKind::EndOfSequence,
        ImageSequenceProviderUnsupportedCause::PayloadRejection, {});
    const auto result = engine.handleProviderHostEvent(
        ViewportEngineProviderHostEventRequest::admit(std::move(hostEvent)));

    QCOMPARE(request.status, ImageViewportRequestStatus::Error);
    QCOMPARE(request.reason, ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
                 .sessionActive,
        false);
    QCOMPARE(result.providerTransport().size(), 1);
    QCOMPARE(result.observations().size(), 1);
    const auto& observation = result.observations().constFirst();
    QCOMPARE(observation.subsystem, ImageViewportInternal::InternalObservationSubsystem::Engine);
    QCOMPARE(
        observation.category, ImageViewportInternal::InternalObservationCategory::AdmissionFailure);
    QCOMPARE(observation.cause,
        ImageViewportInternal::InternalObservationCause::ProviderProtocolEventStateMismatch);
    QVERIFY(observation.identity.roleValid);
    QCOMPARE(observation.identity.role, ImageViewportPageRole::Primary);
    QCOMPARE(observation.identity.generation, quint64(7));
    QCOMPARE(observation.identity.sessionSerial, sessionSerial);
    QCOMPARE(observation.identity.requestId, requestId);
    QCOMPARE(observation.identity.providerToken, quint64(3));
    QCOMPARE(observation.detail, int(ImageSequenceProviderEventKind::EndOfSequence));
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
    ViewportEngineTestAccess::activateProviderSession(engine, ImageViewportPageRole::Primary);
    activateProviderRequestForTest(
        ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary), request,
        providerRequestTokenForTest(5), ImageSequenceProviderRequestKind::Metadata,
        ImageViewportInternal::ProviderRequestOwnership::Metadata);

    ViewportProviderHostEvent hostEvent;
    hostEvent.kind = ViewportProviderHostEvent::Kind::ProviderEvent;
    hostEvent.role = ImageViewportPageRole::Primary;
    hostEvent.providerEvent = providerTerminalEvent(engine, providerRequestTokenForTest(5),
        ImageSequenceProviderEventKind::Unsupported,
        ImageSequenceProviderUnsupportedCause::UnsupportedRequest, QStringLiteral("unsupported"));
    const auto result = engine.handleProviderHostEvent(
        ViewportEngineProviderHostEventRequest::admit(std::move(hostEvent)));

    QCOMPARE(request.status, ImageViewportRequestStatus::Unsupported);
    QCOMPARE(request.reason, ImageViewportRequestReason::UnsupportedRequest);
    QVERIFY(request.generationTerminal.primary.terminal);
    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
                 .sessionActive,
        false);
    QCOMPARE(result.providerTransport().size(), 1);
    QCOMPARE(
        result.providerTransport()[0].kind, ViewportProviderTransportCommand::Kind::CloseSession);
    QVERIFY(!result.providerTransport()[0].sessionClose.metadataToken.isValid());
}

void ViewportEngineTest::providerFrameQueueFlushesOnlyCurrentLoadingRequest()
{
    ViewportEngine engine;
    StubProviderSession session;
    setUpCurrentProviderFrameQueueRequest(engine, session);
    seedCurrentProviderFrameQueue(engine);

    ViewportProviderHostEvent event;
    event.kind = ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest;
    event.role = ImageViewportPageRole::Primary;
    const auto result = engine.handleProviderHostEvent(
        ViewportEngineProviderHostEventRequest::admit(std::move(event)));

    QCOMPARE(result.providerTransport().size(), 1);
    QCOMPARE(result.providerTransport()[0].request.frame(), 4);
    QCOMPARE(
        result.providerTransport()[0].request.kind(), ImageSequenceProviderRequestKind::Playback);
    verifyProviderFrameQueueCleared(
        ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary));
}

void ViewportEngineTest::providerFrameQueueFlushRejectsStaleRequest()
{
    ViewportEngine engine;
    StubProviderSession session;
    setUpCurrentProviderFrameQueueRequest(engine, session);
    seedCurrentProviderFrameQueue(engine);
    ViewportEngineTestAccess::request(engine).roles[0].activeRequest.target.frame = 5;

    ViewportProviderHostEvent event;
    event.kind = ViewportProviderHostEvent::Kind::FlushQueuedFrameRequest;
    event.role = ImageViewportPageRole::Primary;
    const auto result = engine.handleProviderHostEvent(
        ViewportEngineProviderHostEventRequest::admit(std::move(event)));

    QVERIFY(result.providerTransport().isEmpty());
    verifyProviderFrameQueueCleared(
        ViewportEngineTestAccess::providerRequests(engine, ImageViewportPageRole::Primary));
}

void ViewportEngineTest::defaultPresentationStateMatchesPublicDefaults()
{
    ViewportEngine engine;
    ImageViewport item;

    const ImageViewportPresentationSnapshot presentation = item.state().presentation();
    const auto& enginePresentation = ViewportEngineTestAccess::presentation(engine);
    QCOMPARE(enginePresentation.fitMode, presentation.fitMode());
    QCOMPARE(enginePresentation.manualZoom * 100.0, presentation.manualZoomPercent());
    QCOMPARE(enginePresentation.rotationDegrees, presentation.rotationDegrees());
    QCOMPARE(enginePresentation.mirrorHorizontally, presentation.mirrorHorizontally());
    QCOMPARE(enginePresentation.mirrorVertically, presentation.mirrorVertically());
    QCOMPARE(enginePresentation.spreadDirection, presentation.spreadDirection());
    QCOMPARE(enginePresentation.pageGap, presentation.pageGap());
    QCOMPARE(enginePresentation.backgroundMode, presentation.backgroundMode());
    QCOMPARE(enginePresentation.backgroundColor, presentation.backgroundColor());
    QCOMPARE(enginePresentation.smoothing, presentation.smoothing());
    QCOMPARE(enginePresentation.mipmap, presentation.mipmap());
    QCOMPARE(enginePresentation.qualityPreference, ImageViewportQualityPreference::Default);
    QCOMPARE(enginePresentation.exactnessPreference, ImageViewportExactnessPreference::Default);
}

void ViewportEngineTest::geometryProjectionUsesEnginePresentationState()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.logicalSize = QSizeF(20, 10);
    request.roles[1].source.facts.present = true;
    request.roles[1].source.facts.logicalSize = QSizeF(8, 10);
    auto& display = ViewportEngineTestAccess::display(engine);
    display.status = ImageViewportDisplayStatus::Ready;
    display.roles[0].displayedPayload
        = preparedPayloadForTest(QImage(20, 10, QImage::Format_ARGB32_Premultiplied));
    display.roles[1].displayedPayload
        = preparedPayloadForTest(QImage(8, 10, QImage::Format_ARGB32_Premultiplied));
    ImageViewportPresentationCommand command;
    command.setFitMode(ImageViewportFitMode::Manual);
    command.setPageGap(4.0);
    command.setSpreadDirection(ImageViewportSpreadDirection::RightToLeft);
    command.setManualZoomPercent(200.0);
    command.setRotationDegrees(90);
    command.setMirrorHorizontally(true);
    setViewport(engine, { QRectF(0.0, 0.0, 100.0, 80.0), 2.0, true });
    QCOMPARE(engine.applyPresentationCommand({ command }).outcome(),
        ImageViewportCommandOutcome::Accepted);

    const PresentationGeometry::State geometry = ViewportEngineTestAccess::geometryState(engine);

    QCOMPARE(geometry.hasReadyDisplay, true);
    QCOMPARE(geometry.itemBounds, QRectF(0.0, 0.0, 100.0, 80.0));
    QCOMPARE(geometry.primaryImageSize, QSizeF(20.0, 10.0));
    QCOMPARE(geometry.secondaryImageSize, QSizeF(8.0, 10.0));
    QCOMPARE(geometry.pageGap, 4.0);
    QCOMPARE(geometry.spreadDirection, ImageViewportSpreadDirection::RightToLeft);
    QCOMPARE(geometry.fitMode, ImageViewportFitMode::Manual);
    QCOMPARE(geometry.rotationDegrees, 90);
    QCOMPARE(geometry.mirrorHorizontally, true);
    QCOMPARE(geometry.mirrorVertically, false);
    QCOMPARE(geometry.manualZoom, 2.0);
    QCOMPARE(geometry.devicePixelRatio, 2.0);
    QCOMPARE(geometry.contentPosition, QPointF());
    QCOMPARE(PresentationGeometry::spreadSize(geometry), QSizeF(32.0, 10.0));
}

void ViewportEngineTest::coordinateQueryOwnsValidationAndMapping()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[1].source.facts.present = true;
    auto& display = ViewportEngineTestAccess::display(engine);
    display.status = ImageViewportDisplayStatus::Ready;
    display.roles[0].displayedPayload
        = preparedPayloadForTest(QImage(10, 20, QImage::Format_ARGB32_Premultiplied));
    display.roles[1].displayedPayload
        = preparedPayloadForTest(QImage(30, 20, QImage::Format_ARGB32_Premultiplied));
    setViewport(engine, { QRectF(0.0, 0.0, 88.0, 44.0), 1.0, true });

    ImageViewportPresentationCommand command;
    command.setPageGap(4.0);
    QCOMPARE(engine.applyPresentationCommand({ command }).outcome(),
        ImageViewportCommandOutcome::Accepted);

    const ViewportEngineCoordinateQueryResult spread
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::Item,
            ImageViewportCoordinateSpace::DisplayedSpread, ViewportEngineCoordinateRoleKind::Null,
            ImageViewportPageRole::Primary, QPointF(22.0, 22.0) });
    QCOMPARE(spread.valid, true);
    QCOMPARE(spread.space, ImageViewportCoordinateSpace::DisplayedSpread);
    QCOMPARE(spread.role, std::nullopt);
    QCOMPARE(spread.point, QPointF(11.0, 10.0));

    const ViewportEngineCoordinateQueryResult primary
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::Item,
            ImageViewportCoordinateSpace::DisplayedPage, ViewportEngineCoordinateRoleKind::Value,
            ImageViewportPageRole::Primary, QPointF(19.0, 22.0) });
    QCOMPARE(primary.valid, true);
    QCOMPARE(primary.role, std::optional(ImageViewportPageRole::Primary));
    QCOMPARE(primary.point, QPointF(9.5, 10.0));

    const ViewportEngineCoordinateQueryResult gap
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::Item,
            ImageViewportCoordinateSpace::DisplayedPage, ViewportEngineCoordinateRoleKind::Value,
            ImageViewportPageRole::Primary, QPointF(22.0, 22.0) });
    QCOMPARE(gap.valid, false);
    QCOMPARE(gap.space, ImageViewportCoordinateSpace::DisplayedPage);
    QCOMPARE(gap.role, std::optional(ImageViewportPageRole::Primary));
    QCOMPARE(gap.point, QPointF());

    const ViewportEngineCoordinateQueryResult unnecessaryRole
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::Item,
            ImageViewportCoordinateSpace::DisplayedSpread, ViewportEngineCoordinateRoleKind::Value,
            ImageViewportPageRole::Primary, QPointF(22.0, 22.0) });
    QCOMPARE(unnecessaryRole.valid, false);
    QCOMPARE(unnecessaryRole.role, std::nullopt);

    const ViewportEngineCoordinateQueryResult invalidRole
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::DisplayedPage,
            ImageViewportCoordinateSpace::DisplayedSpread,
            ViewportEngineCoordinateRoleKind::Invalid, ImageViewportPageRole::Primary,
            QPointF(1.0, 1.0) });
    QCOMPARE(invalidRole.valid, false);
    QCOMPARE(invalidRole.role, std::nullopt);

    const ViewportEngineCoordinateQueryResult nonFinite
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::DisplayedSpread,
            ImageViewportCoordinateSpace::DisplayedSpread, ViewportEngineCoordinateRoleKind::Null,
            ImageViewportPageRole::Primary,
            QPointF(std::numeric_limits<double>::infinity(), 1.0) });
    QCOMPARE(nonFinite.valid, false);
    QCOMPARE(nonFinite.role, std::nullopt);

    const ViewportEngineCoordinateQueryResult missingRole
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::DisplayedPage,
            ImageViewportCoordinateSpace::DisplayedSpread, ViewportEngineCoordinateRoleKind::Null,
            ImageViewportPageRole::Primary, QPointF(1.0, 1.0) });
    QCOMPARE(missingRole.valid, false);
    QCOMPARE(missingRole.role, std::nullopt);

    const ViewportEngineCoordinateQueryResult excludedPageEdge
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::DisplayedPage,
            ImageViewportCoordinateSpace::DisplayedPage, ViewportEngineCoordinateRoleKind::Value,
            ImageViewportPageRole::Primary, QPointF(10.0, 10.0) });
    QCOMPARE(excludedPageEdge.valid, false);
    QCOMPARE(excludedPageEdge.role, std::optional(ImageViewportPageRole::Primary));

    const ViewportEngineCoordinateQueryResult invalidSpace
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::Item,
            static_cast<ImageViewportCoordinateSpace>(99), ViewportEngineCoordinateRoleKind::Null,
            ImageViewportPageRole::Primary, QPointF(1.0, 1.0) });
    QCOMPARE(invalidSpace.valid, false);
    QCOMPARE(invalidSpace.space, ImageViewportCoordinateSpace::Item);
    QCOMPARE(invalidSpace.role, std::nullopt);

    display.roles[1].displayedPayload = {};
    const ViewportEngineCoordinateQueryResult nonDisplayedRole
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::DisplayedPage,
            ImageViewportCoordinateSpace::DisplayedSpread, ViewportEngineCoordinateRoleKind::Value,
            ImageViewportPageRole::Secondary, QPointF(1.0, 1.0) });
    QCOMPARE(nonDisplayedRole.valid, false);
    QCOMPARE(nonDisplayedRole.role, std::optional(ImageViewportPageRole::Secondary));

    setViewport(engine, { QRectF(), 1.0, true });
    const ViewportEngineCoordinateQueryResult nonPositiveGeometry = engine.queryCoordinate(
        { ImageViewportCoordinateSpace::Item, ImageViewportCoordinateSpace::DisplayedSpread,
            ViewportEngineCoordinateRoleKind::Null, ImageViewportPageRole::Primary, QPointF() });
    QCOMPARE(nonPositiveGeometry.valid, false);
}

void ViewportEngineTest::coordinateQueryUsesRetainedDisplayedPresentationWithoutMutation()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    auto& display = ViewportEngineTestAccess::display(engine);
    display.status = ImageViewportDisplayStatus::Retained;
    display.roles[0].displayedPayload
        = preparedPayloadForTest(QImage(10, 20, QImage::Format_ARGB32_Premultiplied));
    display.displayedPresentation.rotationDegrees = 0;
    setViewport(engine, { QRectF(0.0, 0.0, 100.0, 100.0), 1.0, true });

    ImageViewportPresentationCommand command;
    command.setRotationDegrees(90);
    QCOMPARE(engine.applyPresentationCommand({ command }).outcome(),
        ImageViewportCommandOutcome::Accepted);

    const ImageViewportStateSnapshot before = engine.snapshot();
    const ViewportEngineCoordinateQueryResult result
        = engine.queryCoordinate({ ImageViewportCoordinateSpace::Item,
            ImageViewportCoordinateSpace::DisplayedSpread, ViewportEngineCoordinateRoleKind::Null,
            ImageViewportPageRole::Primary, QPointF(25.0, 0.0) });
    const ImageViewportStateSnapshot after = engine.snapshot();

    QCOMPARE(result.valid, true);
    QCOMPARE(result.point, QPointF());
    QCOMPARE(after.diagnostics(), before.diagnostics());
    QCOMPARE(after.revisions(), before.revisions());
}

void ViewportEngineTest::geometryProjectionRejectsFiniteOverflow()
{
    PresentationGeometry::State direct { true, QRectF(0.0, 0.0, 100.0, 100.0), QSizeF(16.0, 8.0),
        {}, 0.0, ImageViewportSpreadDirection::LeftToRight, ImageViewportFitMode::Manual, 0, false,
        false, 1.0, std::numeric_limits<double>::denorm_min(), {} };
    const QRectF directContent = PresentationGeometry::contentRect(direct);
    QVERIFY(directContent.isEmpty());
    QVERIFY(std::isfinite(directContent.x()));
    QVERIFY(std::isfinite(directContent.y()));
    QVERIFY(std::isfinite(directContent.width()));
    QVERIFY(std::isfinite(directContent.height()));
    QVERIFY(!PresentationGeometry::itemToSpread(direct, 50.0, 50.0).isValid());

    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    auto& display = ViewportEngineTestAccess::display(engine);
    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.logicalSize = QSizeF(16.0, 8.0);
    request.sequenceGeneration = 1;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame }, { 0, -1 }, true);
    request.status = ImageViewportRequestStatus::Ready;
    request.reason = ImageViewportRequestReason::Ready;
    display.status = ImageViewportDisplayStatus::Ready;
    display.roles[0].displayedRequest = display.activeRequestSnapshot(
        request.sequenceGeneration, request.roles[0].activeRequest, -1);
    display.roles[0].displayedPayload = preparedPayloadForTest(
        QImage(16, 8, QImage::Format_ARGB32_Premultiplied), QSizeF(16.0, 8.0),
        request.sequenceGeneration, request.roles[0].activeRequest.identity.id);

    setViewport(engine, { QRectF(0.0, 0.0, 100.0, 100.0), 1.0, true });
    ImageViewportPresentationCommand command;
    command.setFitMode(ImageViewportFitMode::Manual);
    QCOMPARE(engine.applyPresentationCommand({ command }).outcome(),
        ImageViewportCommandOutcome::Accepted);
    setViewport(engine,
        { QRectF(0.0, 0.0, 100.0, 100.0), std::numeric_limits<double>::denorm_min(), true });

    const ImageViewportStateSnapshot snapshot = engine.snapshot();
    QCOMPARE(snapshot.request().status(), ImageViewportRequestStatus::Ready);
    QCOMPARE(snapshot.display().status(), ImageViewportDisplayStatus::Ready);
    QCOMPARE(snapshot.presentation().zoomPercent(), 0.0);
    QVERIFY(snapshot.display().contentRect().isEmpty());
    QVERIFY(snapshot.primary().geometry().acceptedItemRect().isEmpty());
    QVERIFY(snapshot.primary().geometry().displayedItemRect().isEmpty());
}

void ViewportEngineTest::geometryToleranceNormalizesAllEffectiveBounds()
{
    PresentationGeometry::State withinTolerance { true, QRectF(0.0, 0.0, 200.0, 160.0),
        QSizeF(200.0005, 160.0005), {}, 0.0, ImageViewportSpreadDirection::LeftToRight,
        ImageViewportFitMode::Manual, 0, false, false, 1.0, 1.0, QPointF(0.0005, 0.0005) };

    QCOMPARE(PresentationGeometry::contentSize(withinTolerance), QSizeF(200.0005, 160.0005));
    QCOMPARE(PresentationGeometry::maximumContentPosition(withinTolerance), QPointF());
    QCOMPARE(PresentationGeometry::contentPosition(withinTolerance), QPointF());
    QCOMPARE(PresentationGeometry::horizontalPannable(withinTolerance), false);
    QCOMPARE(PresentationGeometry::verticalPannable(withinTolerance), false);
    QCOMPARE(PresentationGeometry::contentPositionForAnchoredSpreadPoint(
                 withinTolerance, QPointF(200.0005, 160.0005), QPointF()),
        QPointF());

    PresentationGeometry::State beyondTolerance = withinTolerance;
    beyondTolerance.primaryImageSize = QSizeF(200.0011, 160.0011);
    beyondTolerance.contentPosition = QPointF(1, 1);
    const QPointF maximum = PresentationGeometry::maximumContentPosition(beyondTolerance);
    QVERIFY(maximum.x() > 0.001);
    QVERIFY(maximum.y() > 0.001);
    QCOMPARE(PresentationGeometry::contentPosition(beyondTolerance), maximum);
    QCOMPARE(PresentationGeometry::horizontalPannable(beyondTolerance), true);
    QCOMPARE(PresentationGeometry::verticalPannable(beyondTolerance), true);
    QCOMPARE(PresentationGeometry::contentPositionForAnchoredSpreadPoint(
                 beyondTolerance, QPointF(200.0011, 160.0011), QPointF()),
        maximum);
}

void ViewportEngineTest::devicePixelRatioChangeRevisesEffectiveFitZoom()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    auto& display = ViewportEngineTestAccess::display(engine);

    request.roles[0].source.facts.present = true;
    request.roles[0].source.facts.logicalSize = QSizeF(16.0, 8.0);
    request.sequenceGeneration = 1;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Initial,
        { 0, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame }, { 0, -1 }, true);
    request.status = ImageViewportRequestStatus::Ready;
    request.reason = ImageViewportRequestReason::Ready;
    display.status = ImageViewportDisplayStatus::Ready;
    display.roles[0].displayedRequest = display.activeRequestSnapshot(
        request.sequenceGeneration, request.roles[0].activeRequest, -1);
    display.roles[0].displayedPayload = preparedPayloadForTest(
        QImage(16, 8, QImage::Format_ARGB32_Premultiplied), QSizeF(16.0, 8.0),
        request.sequenceGeneration, request.roles[0].activeRequest.identity.id);

    const ViewportEngineViewportState initial { QRectF(0.0, 0.0, 100.0, 100.0), 1.0, true };
    setViewport(engine, initial);
    const ImageViewportStateSnapshot before = engine.snapshot();
    QCOMPARE(before.presentation().zoomPercent(), 625.0);
    QCOMPARE(before.presentation().maximumManualZoomPercent(), 409600.0);

    engine.handleViewportChanged({ QRectF(0.0, 0.0, 100.0, 100.0), 2.0, true });
    const ImageViewportStateSnapshot after = engine.snapshot();

    QCOMPARE(after.presentation().zoomPercent(), 1250.0);
    QCOMPARE(after.presentation().maximumManualZoomPercent(), 819200.0);
    QVERIFY(after.revisions().presentation() != before.revisions().presentation());
}

void ViewportEngineTest::renderSnapshotUsesEnginePresentationAndPayloadState()
{
    ViewportEngine engine;
    auto& request = ViewportEngineTestAccess::request(engine);
    request.roles[0].source.facts.present = true;
    request.roles[1].source.facts.present = true;
    auto& display = ViewportEngineTestAccess::display(engine);
    display.status = ImageViewportDisplayStatus::Ready;
    display.roles[0].displayedPayload
        = preparedPayloadForTest(QImage(20, 10, QImage::Format_ARGB32_Premultiplied));
    display.roles[1].displayedPayload
        = preparedPayloadForTest(QImage(8, 10, QImage::Format_ARGB32_Premultiplied));
    ImageViewportPresentationCommand command;
    command.setBackgroundMode(ImageViewportBackgroundMode::SolidColor);
    command.setBackgroundColor(QColor(0x10, 0x20, 0x30));
    command.setRotationDegrees(90);
    command.setSmoothing(false);
    command.setMipmap(true);
    command.setMirrorHorizontally(true);
    command.setMirrorVertically(true);
    setViewport(engine, { QRectF(0.0, 0.0, 100.0, 80.0), 2.0, true });
    QCOMPARE(engine.applyPresentationCommand({ command }).outcome(),
        ImageViewportCommandOutcome::Accepted);

    request.sequenceGeneration = 7;
    request.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        ImageViewportInternal::DisplayRequestTarget {
            2, 100, ImageViewportInternal::ProviderRequestTargetKind::Frame },
        ImageViewportInternal::ResolvedFrameIdentity { 2, 100 }, true);

    QImage primaryImage(20, 10, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::red);
    QImage secondaryImage(8, 10, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::blue);

    display.roles[0].pendingRenderPayload = { true, request.sequenceGeneration, 3, primaryImage };
    display.roles[1].pendingRenderPayload = { true, request.sequenceGeneration, 4, secondaryImage };

    ViewportRenderSnapshotInput input;
    input.itemSize = QSizeF(100.0, 80.0);
    input.requiredRoleSet = ImageViewportRoleSet(true, true);
    input.preparedPayloads
        = { display.roles[0].pendingRenderPayload, display.roles[1].pendingRenderPayload };
    input.geometryState = ViewportEngineTestAccess::geometryState(engine);

    const ViewportRenderSnapshot snapshot = ViewportEngineTestAccess::renderSnapshot(engine, input);

    QCOMPARE(snapshot.itemSize, QSizeF(100.0, 80.0));
    QCOMPARE(snapshot.backgroundMode, ImageViewportBackgroundMode::SolidColor);
    QCOMPARE(snapshot.backgroundColor, QColor(0x10, 0x20, 0x30));
    QCOMPARE(snapshot.smoothing, false);
    QCOMPARE(snapshot.mipmap, true);

    QCOMPARE(snapshot.imageLayers.size(), 2);
    QCOMPARE(snapshot.imageLayers.at(0).role, ImageViewportPageRole::Primary);
    QCOMPARE(snapshot.imageLayers.at(0).preparedPayload.payloadId, 3);
    QCOMPARE(snapshot.imageLayers.at(0).targetRect,
        PresentationGeometry::pageItemRect(input.geometryState, ImageViewportPageRole::Primary)
            .intersected(input.geometryState.itemBounds));
    QCOMPARE(snapshot.imageLayers.at(0).sourceRect,
        PresentationGeometry::visiblePageRect(input.geometryState, ImageViewportPageRole::Primary));
    QCOMPARE(snapshot.imageLayers.at(0).rotationDegrees, 90);
    QCOMPARE(snapshot.imageLayers.at(0).mirrorHorizontally, true);
    QCOMPARE(snapshot.imageLayers.at(0).mirrorVertically, true);
    QCOMPARE(snapshot.imageLayers.at(1).role, ImageViewportPageRole::Secondary);
    QCOMPARE(snapshot.imageLayers.at(1).preparedPayload.payloadId, 4);
    QCOMPARE(snapshot.imageLayers.at(1).preparedPayload.image, secondaryImage);
    QCOMPARE(snapshot.imageLayers.at(1).rotationDegrees, 90);

    input.geometryState = { true, QRectF(0.0, 0.0, 10.0, 10.0), QSizeF(20.0, 10.0),
        QSizeF(8.0, 10.0), 0.0, ImageViewportSpreadDirection::LeftToRight,
        ImageViewportFitMode::Manual, 0, false, false, 1.0, 1.0, {} };
    const ViewportRenderSnapshot clipped = ViewportEngineTestAccess::renderSnapshot(engine, input);
    QCOMPARE(clipped.requiredRoleSet, ImageViewportRoleSet(true, true));
    QCOMPARE(clipped.imageLayers.size(), 2);
    QVERIFY(clipped.imageLayers.at(1).targetRect.isEmpty());
    QVERIFY(clipped.imageLayers.at(1).sourceRect.isEmpty());

    display.status = ImageViewportDisplayStatus::Retained;
    display.displayedPresentation.rotationDegrees = 0;
    input.useDisplayedPresentation = true;
    const ViewportRenderSnapshot retained = ViewportEngineTestAccess::renderSnapshot(engine, input);
    QCOMPARE(retained.imageLayers.at(0).rotationDegrees, 0);
    QCOMPARE(retained.imageLayers.at(1).rotationDegrees, 0);
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
    const auto result = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(sequence->sequence()), {} });
    const auto targetState = ViewportEngineTestAccess::presentationTargetState(engine);

    QCOMPARE(result.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(
        engine.snapshot().diagnostics().commandReason(), ImageViewportCommandReason::NoCommand);
    QVERIFY(engine.snapshot().revisions().command().isValid());
    QCOMPARE(targetState.presentationTarget.primary(), sequence->sequence());
    QCOMPARE(targetState.presentationTarget.secondary(), nullptr);
    QCOMPARE(targetState.acceptedRoleSet, ImageViewportRoleSet(true, false));
    QCOMPARE(targetState.targetRoleSet, ImageViewportRoleSet(true, false));
    QCOMPARE(targetState.generation, 1);
    QCOMPARE(targetState.primaryRoleGeneration, 1);
    QCOMPARE(targetState.secondaryRoleGeneration, 0);
    QCOMPARE(targetState.activeRoleValid, true);
    QCOMPARE(targetState.activeRole, ImageViewportPageRole::Primary);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].sequence, sequence->sequence());
    QCOMPARE(ViewportEngineTestAccess::request(engine).sequenceGeneration, 1);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.id, 1);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].activeRequest.target.frame, 0);
    QCOMPARE(ViewportEngineTestAccess::request(engine).status, ImageViewportRequestStatus::Loading);
    QCOMPARE(ViewportEngineTestAccess::request(engine).reason,
        ImageViewportRequestReason::RenderWaiting);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.commitPending,
        true);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.image.isNull(),
        false);
}

void ViewportEngineTest::mutatingIngressPublishesCoherentSnapshotBeforeReturn()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(image));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    const ImageViewportStateSnapshot before = engine.snapshot();
    const auto assigned = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(sequence->sequence()), {} });
    const ImageViewportStateSnapshot after = engine.snapshot();

    QCOMPARE(assigned.outcome(), ImageViewportCommandOutcome::Accepted);
    QVERIFY(after.revisions().request() != before.revisions().request());
    QVERIFY(after.revisions().command() != before.revisions().command());
    QVERIFY(after.revisions().snapshot() != before.revisions().snapshot());
    QCOMPARE(after.request().acceptedPresentationTargetGeneration().isValid(), true);
    QCOMPARE(after.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
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
    const auto result = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(primary->sequence(), secondary->sequence()), {} });
    const auto targetState = ViewportEngineTestAccess::presentationTargetState(engine);

    QCOMPARE(result.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(targetState.presentationTarget.primary(), primary->sequence());
    QCOMPARE(targetState.presentationTarget.secondary(), secondary->sequence());
    QCOMPARE(targetState.acceptedRoleSet, ImageViewportRoleSet(true, true));
    QCOMPARE(targetState.targetRoleSet, ImageViewportRoleSet(true, true));
    QCOMPARE(targetState.primaryRoleGeneration, targetState.generation);
    QCOMPARE(targetState.secondaryRoleGeneration, targetState.generation);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[1].sequence, secondary->sequence());
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[1].activeRequest.identity.id,
        ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.id);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[1].activeRequest.identity.origin,
        ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.origin);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[1].pendingRenderPayload.commitPending,
        true);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[1].pendingRenderPayload.image.isNull(),
        false);
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
    const auto& transport = result.transition().providerTransport();
    const auto open = std::find_if(transport.cbegin(), transport.cend(), [](const auto& command) {
        return command.kind == ViewportProviderTransportCommand::Kind::OpenSession;
    });

    QCOMPARE(result.outcome(), ImageViewportCommandOutcome::Accepted);
    QVERIFY(open != transport.cend());
    QCOMPARE(open->generation,
        ViewportEngineTestAccess::presentationTargetState(engine).primaryRoleGeneration);
    QVERIFY(open->sessionSerial != 0);
    QCOMPARE(ViewportEngineTestAccess::providerSession(engine, ImageViewportPageRole::Primary)
                 .sessionActive,
        true);
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
    QCOMPARE(
        engine
            .assignPresentationTarget({ ImageViewportPresentationTarget(primary->sequence()), {} })
            .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const ViewportEnginePresentationTargetState previousState
        = ViewportEngineTestAccess::presentationTargetState(engine);
    ImageViewportPresentationTarget secondaryOnly;
    secondaryOnly.setSecondary(secondary->sequence());

    const auto previousCommandRevision = engine.snapshot().revisions().command();
    const auto result = engine.assignPresentationTarget({ secondaryOnly, {} });

    QCOMPARE(result.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(engine.snapshot().diagnostics().commandReason(),
        ImageViewportCommandReason::InvalidRequest);
    QVERIFY(engine.snapshot().revisions().command() != previousCommandRevision);
    const auto& currentState = ViewportEngineTestAccess::presentationTargetState(engine);
    QCOMPARE(currentState.presentationTarget, previousState.presentationTarget);
    QCOMPARE(currentState.acceptedRoleSet, previousState.acceptedRoleSet);
    QCOMPARE(currentState.generation, previousState.generation);
    QCOMPARE(ViewportEngineTestAccess::commandDiagnostics(engine).reason,
        ImageViewportCommandReason::InvalidRequest);
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
    QCOMPARE(
        engine
            .assignPresentationTarget({ ImageViewportPresentationTarget(primary->sequence()), {} })
            .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const ViewportEnginePresentationTargetState previousState
        = ViewportEngineTestAccess::presentationTargetState(engine);
    PresentationTargetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(
        PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);

    const auto result = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(replacement->sequence()), invalidPolicy });

    QCOMPARE(result.outcome(), ImageViewportCommandOutcome::Invalid);
    const auto& currentState = ViewportEngineTestAccess::presentationTargetState(engine);
    QCOMPARE(currentState.presentationTarget, previousState.presentationTarget);
    QCOMPARE(currentState.acceptedRoleSet, previousState.acceptedRoleSet);
    QCOMPARE(currentState.generation, previousState.generation);
}

void ViewportEngineTest::deferredTargetAnchorResolvesWhenItemGeometryArrives()
{
    ImageSequenceFactory factory;
    QImage image(400, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(&frame));
    QVERIFY(sequence->sequence());

    PresentationTargetTransitionPolicy policy;
    policy.setFitModeTransition(PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit);
    policy.setFitMode(ImageViewportFitMode::Manual);
    policy.setContentPositionTransition(
        PresentationTargetTransitionPolicy::ContentPositionTransition::AnchorEnd);

    ViewportEngine engine;
    const auto assignment = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(sequence->sequence()), policy });
    QCOMPARE(assignment.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(ViewportEngineTestAccess::presentation(engine).contentPosition, QPointF());
    QVERIFY(ViewportEngineTestAccess::presentationTargetState(engine)
            .pendingPresentationTransition.isValid());

    const auto previousPresentationRevision = engine.snapshot().revisions().presentation();
    engine.handleViewportChanged({ QRectF(0.0, 0.0, 100.0, 100.0), 1.0, false });

    QCOMPARE(ViewportEngineTestAccess::presentation(engine).contentPosition, QPointF(300.0, 0.0));
    QCOMPARE(ViewportEngineTestAccess::presentationTargetState(engine)
                 .pendingPresentationTransition.isValid(),
        false);
    QVERIFY(engine.snapshot().revisions().presentation() != previousPresentationRevision);
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
    QCOMPARE(
        engine
            .assignPresentationTarget({ ImageViewportPresentationTarget(sequence->sequence()), {} })
            .outcome(),
        ImageViewportCommandOutcome::Accepted);

    const auto clearResult
        = engine.assignPresentationTarget({ ImageViewportPresentationTarget::clear(),
            PresentationTargetTransitionPolicy::defaultClear() });
    const auto clearState = ViewportEngineTestAccess::presentationTargetState(engine);

    QCOMPARE(clearResult.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(clearState.presentationTarget, ImageViewportPresentationTarget::clear());
    QCOMPARE(clearState.acceptedRoleSet, ImageViewportRoleSet(false, false));
    QCOMPARE(clearState.targetRoleSet, ImageViewportRoleSet(false, false));
    QCOMPARE(clearState.generation, 2);
    QCOMPARE(
        ViewportEngineTestAccess::request(engine).status, ImageViewportRequestStatus::NoRequest);
    QCOMPARE(ViewportEngineTestAccess::display(engine).status, ImageViewportDisplayStatus::Empty);
    QCOMPARE(ViewportEngineTestAccess::request(engine).roles[0].sequence, nullptr);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].displayedPayload.sourceLogicalSize,
        QSizeF());

    const ImageViewportStateSnapshot beforeNoop = engine.snapshot();
    const auto noopClear
        = engine.assignPresentationTarget({ ImageViewportPresentationTarget::clear(),
            PresentationTargetTransitionPolicy::defaultClear() });

    QCOMPARE(noopClear.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(ViewportEngineTestAccess::presentationTargetState(engine).generation,
        clearState.generation);
    QCOMPARE(engine.snapshot(), beforeNoop);
}

void ViewportEngineTest::presentationTargetAssignmentAdvancesCommandDiagnostic()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(&frame));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    ImageViewportPresentationTarget secondaryOnly;
    secondaryOnly.setSecondary(sequence->sequence());
    const auto rejected = engine.assignPresentationTarget({ secondaryOnly, {} });
    QCOMPARE(rejected.outcome(), ImageViewportCommandOutcome::Invalid);
    const auto rejectedRevision = engine.snapshot().revisions().command();

    const auto accepted = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(sequence->sequence()), {} });
    const auto acceptedRevision = engine.snapshot().revisions().command();

    QCOMPARE(accepted.outcome(), ImageViewportCommandOutcome::Accepted);
    QVERIFY(acceptedRevision != rejectedRevision);
    QCOMPARE(ViewportEngineTestAccess::commandDiagnostics(engine).reason,
        ImageViewportCommandReason::NoCommand);
    QCOMPARE(engine.snapshot().revisions().command(), acceptedRevision);
}

void ViewportEngineTest::assignmentAppliesDisplayAndPlaybackTransitionPolicy()
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
    ViewportEngineTestAccess::display(engine).status = ImageViewportDisplayStatus::Ready;
    ViewportEngineTestAccess::display(engine).roles[0].displayedPayload
        = preparedPayloadForTest(firstImage);
    ViewportEngineTestAccess::playback(engine).phase = ImageViewportPlaybackPhase::Playing;
    const auto retained = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(first->sequence()), {} });
    QCOMPARE(retained.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(
        ViewportEngineTestAccess::display(engine).status, ImageViewportDisplayStatus::Retained);
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Stopped);

    PresentationTargetTransitionPolicy policy;
    policy.setDisplayTransition(
        PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    const auto cleared = engine.assignPresentationTarget(
        { ImageViewportPresentationTarget(second->sequence()), policy });
    QCOMPARE(cleared.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(ViewportEngineTestAccess::display(engine).status, ImageViewportDisplayStatus::Empty);
    QCOMPARE(ViewportEngineTestAccess::display(engine).roles[0].displayedPayload.sourceLogicalSize,
        QSizeF());
    QCOMPARE(ViewportEngineTestAccess::playback(engine).phase, ImageViewportPlaybackPhase::Stopped);
}

void ViewportEngineTest::renderAttemptAuthorityRejectsStaleAndDuplicateFacts()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(&frame));
    QVERIFY(sequence->sequence());

    const ViewportEngineViewportState viewport { QRectF(0.0, 0.0, 100.0, 80.0), 1.0, true };
    ViewportEngine engine;
    setViewport(engine, viewport);
    ViewportEnginePresentationTargetAssignmentRequest assignment;
    assignment.presentationTarget = ImageViewportPresentationTarget(sequence->sequence());
    const auto assigned = engine.assignPresentationTarget(assignment);
    QCOMPARE(assigned.outcome(), ImageViewportCommandOutcome::Accepted);

    const auto hostFact = [](const ViewportRenderAttempt& attempt) {
        ViewportRenderHostFact fact;
        fact.outcome = ViewportRenderHostFact::Outcome::Committed;
        fact.acknowledgement.attempt = attempt.attempt;
        fact.acknowledgement.targetSpread = attempt.snapshot.targetSpread;
        fact.acknowledgement.presentation = attempt.snapshot.presentation;
        for (const auto& layer : attempt.snapshot.imageLayers) {
            fact.acknowledgement.rolePayloads.append(
                { layer.role, layer.preparedPayload.identity() });
        }
        fact.imagePresent = !fact.acknowledgement.rolePayloads.isEmpty();
        return fact;
    };

    const ViewportRenderAttempt staleAttempt = engine.beginRenderSynchronization();
    const ViewportRenderAttempt activeAttempt = engine.beginRenderSynchronization();
    QVERIFY(activeAttempt.attempt > staleAttempt.attempt);

    auto incompleteFact = hostFact(activeAttempt);
    incompleteFact.acknowledgement.rolePayloads.clear();
    const ImageViewportStateSnapshot beforeIncomplete = engine.snapshot();
    const auto incomplete = engine.handleRenderHostFact({ incompleteFact });
    QCOMPARE(incomplete.observations().size(), 1);
    QCOMPARE(incomplete.observations().constFirst().category,
        ImageViewportInternal::InternalObservationCategory::StaleDrop);
    QCOMPARE(engine.snapshot(), beforeIncomplete);

    auto wrongTargetSpread = hostFact(activeAttempt);
    ++wrongTargetSpread.acknowledgement.targetSpread.requestId;
    const auto targetMismatch = engine.handleRenderHostFact({ wrongTargetSpread });
    QCOMPARE(targetMismatch.observations().size(), 1);
    QCOMPARE(engine.snapshot().request().status(), ImageViewportRequestStatus::Loading);

    auto wrongPresentation = hostFact(activeAttempt);
    ++wrongPresentation.acknowledgement.presentation.revision;
    const auto presentationMismatch = engine.handleRenderHostFact({ wrongPresentation });
    QCOMPARE(presentationMismatch.observations().size(), 1);
    QCOMPARE(engine.snapshot().request().status(), ImageViewportRequestStatus::Loading);

    const auto stale = engine.handleRenderHostFact({ hostFact(staleAttempt) });
    QCOMPARE(stale.observations().size(), 1);
    QCOMPARE(stale.observations().constFirst().category,
        ImageViewportInternal::InternalObservationCategory::StaleDrop);
    QCOMPARE(engine.snapshot().request().status(), ImageViewportRequestStatus::Loading);

    engine.handleRenderHostFact({ hostFact(activeAttempt) });
    QCOMPARE(engine.snapshot().request().status(), ImageViewportRequestStatus::Ready);

    const ImageViewportStateSnapshot beforeDuplicate = engine.snapshot();
    const auto duplicate = engine.handleRenderHostFact({ hostFact(activeAttempt) });
    QCOMPARE(duplicate.observations().size(), 1);
    QCOMPARE(engine.snapshot(), beforeDuplicate);
}

QTEST_MAIN(ViewportEngineTest)

#include "tst_viewportengine.moc"
