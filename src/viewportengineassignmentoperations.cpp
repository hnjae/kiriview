#include "viewportengineassignmentoperations_p.h"

#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "viewportenginebuiltinframeoperations_p.h"
#include "viewportengineplaybackoperations_p.h"
#include "viewportengineprojection_p.h"
#include "viewportenginetargetspreadterminaloperations_p.h"

#include <limits>

namespace {
using namespace ImageViewportInternal;

struct AuthoritativeRoleContract
{
    bool authoritative = false;
    bool timed = false;
    QSizeF logicalSize;
    TimingIntervals timing;
    ImageSequenceAuthoredAnimationFacts animation;
    bool animationAvailable = false;
};

bool animationsEqual(
    ImageSequenceAuthoredAnimationFacts lhs, ImageSequenceAuthoredAnimationFacts rhs)
{
    return lhs.autoplay() == rhs.autoplay() && lhs.loopMode() == rhs.loopMode()
        && lhs.loopCount() == rhs.loopCount();
}

bool timingsEqual(const TimingIntervals& lhs, const TimingIntervals& rhs)
{
    if (lhs.isValid() != rhs.isValid()) {
        return false;
    }
    if (!lhs.isValid()) {
        return true;
    }
    if (lhs.frameCount() != rhs.frameCount()) {
        return false;
    }
    for (int frame = 0; frame < lhs.frameCount(); ++frame) {
        if (lhs.frameDuration(frame) != rhs.frameDuration(frame)) {
            return false;
        }
    }
    return true;
}

AuthoritativeRoleContract incomingContract(const ImageSequenceSource& source)
{
    AuthoritativeRoleContract result;
    if (!source.facts.present) {
        return result;
    }
    result.animation = source.facts.authoredAnimationFacts;
    result.animationAvailable = source.facts.authoredAnimationFactsAvailable;
    if (!source.facts.provider) {
        result.authoritative = true;
        result.timed = source.facts.timed;
        result.logicalSize = source.facts.logicalSize;
        result.timing = source.facts.timingIntervals;
        return result;
    }
    if (!source.facts.hasCompleteProviderKnownMetadata) {
        return result;
    }
    result.authoritative = true;
    result.timed = source.facts.providerKnownFacts.isTimedFrameList();
    result.logicalSize = source.facts.providerKnownLogicalSize;
    result.timing = source.facts.providerKnownTimingIntervals;
    return result;
}

AuthoritativeRoleContract currentContract(
    const RequestState::RoleState& role, const ProviderFactsState& provider)
{
    if (!role.source.facts.provider) {
        return incomingContract(role.source);
    }
    if (!provider.metadataReady) {
        return {};
    }
    return { true, provider.timedMetadata, provider.logicalSize, provider.timingIntervals,
        provider.authoredAnimationFacts, provider.authoredAnimationFactsAvailable };
}

bool contractsEqual(const AuthoritativeRoleContract& lhs, const AuthoritativeRoleContract& rhs)
{
    return lhs.authoritative && rhs.authoritative && lhs.timed == rhs.timed
        && lhs.logicalSize == rhs.logicalSize && timingsEqual(lhs.timing, rhs.timing)
        && lhs.animationAvailable == rhs.animationAvailable
        && (!lhs.animationAvailable || animationsEqual(lhs.animation, rhs.animation));
}

bool targetFitsContract(const DisplayRequest& request, const AuthoritativeRoleContract& contract)
{
    if (!contract.authoritative || request.identity.id == 0 || request.target.frame < 0
        || request.resolvedFrame.frame != request.target.frame) {
        return false;
    }
    const int frameCount = contract.timed ? contract.timing.frameCount() : 1;
    if (request.target.frame >= frameCount) {
        return false;
    }
    if (!contract.timed) {
        return request.target.frame == 0 && request.target.position == -1
            && request.resolvedFrame.position == -1;
    }
    const int resolvedPosition = contract.timing.frameStartPosition(request.target.frame);
    if (request.resolvedFrame.position != resolvedPosition) {
        return false;
    }
    if (request.target.providerTargetKind == ProviderRequestTargetKind::Position) {
        return contract.timing.frameIndexForPosition(request.target.position)
            == request.target.frame;
    }
    return request.target.position == resolvedPosition;
}

bool canonicalClearPolicy(const ViewportEnginePresentationTargetTransitionPolicy& policy)
{
    return policy.displayTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad
        && policy.zoomTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::ZoomTransition::Preserve
        && policy.contentPositionTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::Clamp
        && policy.rotationTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::RotationTransition::Preserve
        && policy.mirrorTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::MirrorTransition::Preserve
        && policy.fitModeTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::FitModeTransition::Preserve
        && policy.spreadDirectionTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::SpreadDirectionTransition::Preserve
        && policy.pageGapTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::PageGapTransition::Preserve
        && policy.replacementIntent()
        == ViewportEnginePresentationTargetTransitionPolicy::ReplacementIntent::NewTarget;
}

bool presentationPreservingRefinementPolicy(
    const ViewportEnginePresentationTargetTransitionPolicy& policy)
{
    return policy.zoomTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::ZoomTransition::Preserve
        && policy.contentPositionTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::Clamp
        && policy.rotationTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::RotationTransition::Preserve
        && policy.mirrorTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::MirrorTransition::Preserve
        && policy.fitModeTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::FitModeTransition::Preserve
        && policy.spreadDirectionTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::SpreadDirectionTransition::Preserve
        && policy.pageGapTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::PageGapTransition::Preserve;
}

std::size_t idx(ImageViewportPageRole r) { return r == ImageViewportPageRole::Secondary ? 1U : 0U; }
ImageViewportDisplayStatus retained(const DisplayState& d)
{
    bool ok = (d.status == ImageViewportDisplayStatus::Ready
                  || d.status == ImageViewportDisplayStatus::Retained)
        && d.roles[0].displayedPayload.hasPresentableContent();
    return ok ? ImageViewportDisplayStatus::Retained : ImageViewportDisplayStatus::Empty;
}
void resetProvider(ProviderRoleState& p, ImageSequenceAuthoredAnimationFacts a = {},
    bool authoredAnimationFactsAvailable = false)
{
    p.facts.metadataReady = false;
    p.facts.timedMetadata = false;
    p.facts.timedPlaybackSupport = false;
    p.facts.frameSeekSupport = false;
    p.facts.positionSeekSupport = false;
    p.facts.authoredAnimationFacts = a;
    p.facts.authoredAnimationFactsAvailable = authoredAnimationFactsAvailable;
    p.facts.logicalSize = {};
    p.facts.timingIntervals = {};
    p.requests.clearWork();
}
DisplayRequestTarget initial(const ImageSequenceSource& s)
{
    if (!s.facts.present || s.facts.provider || s.facts.frameCount <= 0)
        return {};
    return { 0, s.facts.timed ? sourceFrameStartPosition(s, 0) : -1,
        ProviderRequestTargetKind::Unknown };
}
void secondary(RequestState& r, DisplayRequestTarget t, ResolvedFrameIdentity resolved = {})
{
    auto& s = r.roles[1].activeRequest;
    s.identity = r.roles[0].activeRequest.identity;
    s.target = t;
    s.resolvedFrame = resolved.frame >= 0 ? resolved
        : t.frame >= 0                    ? ResolvedFrameIdentity { t.frame, t.position }
                                          : ResolvedFrameIdentity {};
    s.preparedPayloadId = r.roles[0].activeRequest.preparedPayloadId;
    if (t.frame >= 0)
        r.roles[1].latestNonPlaybackRequest = s;
}
ViewportEngineBuiltInFrameStageResult stage(RequestState& r, DisplayState& d,
    PlaybackState& playback, ImageViewportExactnessPreference exactnessPreference)
{
    return stageViewportEngineBuiltInTargetSpread(r, d, exactnessPreference, &playback);
}
ViewportEnginePresentationTargetState targetState(
    const ViewportEnginePresentationTarget& t, quint64 g)
{
    ViewportEnginePresentationTargetState s;
    if (t.isClear()) {
        s.presentationTarget = ViewportEnginePresentationTarget::clear();
        s.generation = g;
        return s;
    }
    s.presentationTarget = t;
    s.generation = g;
    s.acceptedRoleSet = ImageViewportRoleSet(true, t.secondary() != nullptr);
    s.targetRoleSet = s.acceptedRoleSet;
    s.primaryRoleGeneration = g;
    s.secondaryRoleGeneration = t.secondary() ? g : 0;
    s.activeRole = ImageViewportPageRole::Primary;
    s.activeRoleValid = true;
    return s;
}
}

bool validateViewportEnginePresentationTargetAssignment(
    const ViewportEnginePresentationTargetAssignmentInput& input,
    const ViewportEnginePresentationTargetState& currentTarget, const RequestState& currentRequest,
    const std::array<ViewportEngineRoleState, 2>& currentRoles)
{
    if (!input.presentationTarget.isValid() || !input.transitionPolicy.isValid()) {
        return false;
    }
    if (input.presentationTarget.isClear()) {
        return canonicalClearPolicy(input.transitionPolicy);
    }
    const bool incomingSecondary = input.presentationTarget.secondary() != nullptr;
    if (input.primarySource.sequence != input.presentationTarget.primary()
        || input.secondarySource.sequence != input.presentationTarget.secondary()
        || !input.primarySource.facts.present
        || input.secondarySource.facts.present != incomingSecondary) {
        return false;
    }
    if (input.transitionPolicy.replacementIntent()
        != ViewportEnginePresentationTargetTransitionPolicy::ReplacementIntent::
            SameTargetRefinement) {
        return true;
    }
    if (!presentationPreservingRefinementPolicy(input.transitionPolicy)
        || currentTarget.presentationTarget.isClear()
        || currentTarget.acceptedRoleSet != ImageViewportRoleSet(true, incomingSecondary)) {
        return false;
    }
    for (std::size_t index = 0; index < (incomingSecondary ? 2U : 1U); ++index) {
        const auto current
            = currentContract(currentRequest.roles[index], currentRoles[index].provider.facts);
        const auto incoming
            = incomingContract(index == 0 ? input.primarySource : input.secondarySource);
        if (!contractsEqual(current, incoming)
            || !targetFitsContract(currentRequest.roles[index].activeRequest, incoming)) {
            return false;
        }
    }
    return true;
}

ViewportProviderFrameTransportEffect ViewportEnginePresentationTargetAssignmentAccess::closeSession(
    ImageViewportPageRole role)
{
    auto& p = m_roles[idx(role)].provider;
    ViewportEngineProviderSessionCloseAccess a(p.session, p.requests);
    return closeViewportEngineProviderSession(std::move(a));
}
ViewportEngineProviderSessionOpenEffect
ViewportEnginePresentationTargetAssignmentAccess::openSession(
    ImageViewportPageRole role, const ImageViewportInternal::ImageSequenceSource& s, quint64 g)
{
    auto& p = m_roles[idx(role)].provider;
    ViewportEngineProviderSessionOpenAccess a(s, p.session);
    return beginViewportEngineProviderSession({ role, g }, std::move(a));
}

ViewportEnginePresentationTargetTransitionReduction
ViewportEnginePresentationTargetAssignmentAccess::transition(
    ViewportEnginePresentationTargetTransitionInput input)
{
    ViewportEnginePresentationTargetTransitionStateView view(m_presentation);
    return reduceViewportEnginePresentationTargetTransition(input, std::move(view));
}

void ViewportEnginePresentationTargetAssignmentAccess::applyAutoplay()
{
    ViewportEngineAuthoredAutoplayAccess autoplay(
        m_request, { m_roles[0].provider.facts, m_roles[1].provider.facts }, m_playback);
    reduceViewportEngineAuthoredAutoplay({}, std::move(autoplay));
}

ViewportEnginePresentationTargetAssignmentReduction
reduceViewportEnginePresentationTargetAssignment(ViewportEnginePresentationTargetAssignmentInput in,
    ViewportEnginePresentationTargetAssignmentAccess a)
{
    using namespace ImageViewportInternal;
    ViewportEnginePresentationTargetAssignmentReduction out;
    out.clear = in.presentationTarget.isClear();
    const bool refinement = !out.clear
        && in.transitionPolicy.replacementIntent()
            == ViewportEnginePresentationTargetTransitionPolicy::ReplacementIntent::
                SameTargetRefinement;
    const DisplayRequest previousPrimaryRequest = a.m_request.roles[0].activeRequest;
    const DisplayRequest previousSecondaryRequest = a.m_request.roles[1].activeRequest;
    const PlaybackState previousPlayback = a.m_playback;
    bool noop = out.clear && a.m_target.acceptedRoleSet == ImageViewportRoleSet();
    out.presentationTargetChanged = !noop;
    out.retainPreviousDisplay = in.transitionPolicy.displayTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::DisplayTransition::RetainPrevious;
    out.releaseDisplayedState = out.clear || !out.retainPreviousDisplay;
    out.resetDisplayRequests = out.closeProviderSessions = out.presentationTargetChanged;
    out.stopPlayback = out.presentationTargetChanged && !refinement;
    if (out.presentationTargetChanged) {
        if (a.m_nextTargetGeneration == std::numeric_limits<quint64>::max())
            qFatal("ImageViewport presentation target generation exhausted");
        a.m_target = targetState(in.presentationTarget, ++a.m_nextTargetGeneration);
    }
    out.presentationTargetState = a.m_target;
    if (!out.presentationTargetChanged)
        return out;
    auto oldGeo = projectViewportGeometryState(in.geometry, a.m_presentation);
    auto oldPhase = a.m_playback.phase;
    const PublicDiagnosticText oldError = a.m_request.errorString;
    const bool oldWarning = a.m_display.hasActiveRenderQualityFallback(
        a.m_request.sequenceGeneration, a.m_presentation);
    out.providerEffects[0] = a.closeSession(ImageViewportPageRole::Primary);
    out.providerEffects[1] = a.closeSession(ImageViewportPageRole::Secondary);
    auto primary = std::move(in.primarySource), secondarySource = std::move(in.secondarySource);
    if (!out.clear && !primary.sequence)
        primary = factorySequenceSource(in.presentationTarget.primary());
    if (!out.clear && !secondarySource.sequence)
        secondarySource = factorySequenceSource(in.presentationTarget.secondary());
    a.m_request.roles[0].source = std::move(primary);
    a.m_request.roles[0].sequence = a.m_request.roles[0].source.sequence;
    a.m_request.roles[1].source = std::move(secondarySource);
    a.m_request.roles[1].sequence = a.m_request.roles[1].source.sequence;
    a.m_request.roles[1].provider = a.m_request.roles[1].source.facts.provider;
    a.m_request.sequenceGeneration = a.m_target.generation;
    a.m_request.clearDisplayRequests();
    if (!refinement) {
        a.m_playback.resetRequestIdentity();
    }
    a.m_display.nextPreparedPayloadId = 0;
    a.m_display.clearPendingRenderPayload();
    if (out.releaseDisplayedState) {
        a.m_display.clearDisplayedDisplay();
    }
    a.m_request.errorString.clear();
    a.m_display.renderQualityFallback.clear();
    if (refinement) {
        a.m_playback = previousPlayback;
        if (a.m_playback.phase == ImageViewportPlaybackPhase::Playing) {
            a.m_playback.phase = ImageViewportPlaybackPhase::Waiting;
        }
        a.m_playback.providerStartPending = false;
    } else {
        a.m_playback.phase = ImageViewportPlaybackPhase::Stopped;
        a.m_playback.stopWhenRequestReady = false;
        a.m_playback.providerStartPending = false;
    }
    resetProvider(a.m_roles[0].provider,
        a.m_request.roles[0].source.facts.provider
            ? a.m_request.roles[0].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {},
        a.m_request.roles[0].source.facts.provider
            && a.m_request.roles[0].source.facts.authoredAnimationFactsAvailable);
    resetProvider(a.m_roles[1].provider,
        a.m_request.roles[1].source.facts.provider
            ? a.m_request.roles[1].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {},
        a.m_request.roles[1].source.facts.provider
            && a.m_request.roles[1].source.facts.authoredAnimationFactsAvailable);
    if (out.clear) {
        a.m_playback.authoredAutoplayArbitration = AuthoredAutoplayArbitrationState::Resolved;
        a.m_display.clearDisplayedDisplay();
        a.m_request.status = ImageViewportRequestStatus::NoRequest;
        a.m_request.reason = ImageViewportRequestReason::NoRequest;
        a.m_display.status = ImageViewportDisplayStatus::Empty;
    } else {
        const auto primaryTarget
            = refinement ? previousPrimaryRequest.target : initial(a.m_request.roles[0].source);
        const auto primaryResolved = refinement
            ? previousPrimaryRequest.resolvedFrame
            : ResolvedFrameIdentity { primaryTarget.frame, primaryTarget.position };
        const auto secondaryTarget
            = refinement ? previousSecondaryRequest.target : initial(a.m_request.roles[1].source);
        const auto secondaryResolved = refinement
            ? previousSecondaryRequest.resolvedFrame
            : ResolvedFrameIdentity { secondaryTarget.frame, secondaryTarget.position };
        if (a.m_request.roles[0].source.facts.provider) {
            auto& p = a.m_roles[0].provider;
            auto& f = a.m_request.roles[0].source.facts;
            DisplayRequestTarget t = primaryTarget;
            if (f.hasCompleteProviderKnownMetadata) {
                p.facts.metadataReady = true;
                p.facts.timedMetadata = f.providerKnownFacts.isTimedFrameList();
                p.facts.timedPlaybackSupport = providerResolvedCapability(
                    f.providerTimedPlaybackCapability, p.facts.timedMetadata);
                p.facts.frameSeekSupport
                    = providerResolvedCapability(f.providerFrameSeekCapability, true);
                p.facts.positionSeekSupport = providerResolvedCapability(
                    f.providerPositionSeekCapability, p.facts.timedMetadata);
                p.facts.logicalSize = f.providerKnownLogicalSize;
                p.facts.timingIntervals = f.providerKnownTimingIntervals;
                if (!refinement) {
                    t = { 0, p.facts.timedMetadata ? 0 : -1, ProviderRequestTargetKind::Frame };
                }
            }
            a.m_request.beginDisplayRequest(DisplayRequestOrigin::Initial, t,
                refinement ? primaryResolved : ResolvedFrameIdentity { t.frame, t.position }, true);
            a.m_playback.position = refinement ? previousPlayback.position : t.position;
            secondary(a.m_request, secondaryTarget, secondaryResolved);
            a.m_request.status = ImageViewportRequestStatus::Loading;
            a.m_request.reason = ImageViewportRequestReason::ProviderWaiting;
            a.m_display.status = retained(a.m_display);
            out.providerSessionOpenEffects[0] = a.openSession(ImageViewportPageRole::Primary,
                a.m_request.roles[0].source, a.m_target.primaryRoleGeneration);
        } else if (a.m_request.roles[0].source.facts.present) {
            a.m_request.beginDisplayRequest(
                DisplayRequestOrigin::Initial, primaryTarget, primaryResolved, true);
            a.m_playback.position = refinement ? previousPlayback.position : primaryTarget.position;
            secondary(a.m_request, secondaryTarget, secondaryResolved);
            const auto admission = stage(
                a.m_request, a.m_display, a.m_playback, a.m_presentation.exactnessPreference);
            if (admission.accepted) {
                TargetSpreadWaitState w;
                w.requiresSecondary = a.m_request.roles[1].sequence != nullptr;
                if (!in.geometry.renderAvailable || in.geometry.itemBounds.isEmpty()) {
                    w.primary.renderWaiting = true;
                    if (w.requiresSecondary && !a.m_request.roles[1].provider)
                        w.secondary.renderWaiting = true;
                } else {
                    w.primary.uploadPending = true;
                    if (w.requiresSecondary && !a.m_request.roles[1].provider)
                        w.secondary.uploadPending = true;
                }
                if (a.m_request.roles[1].provider)
                    w.secondary.providerWaiting = true;
                a.m_request.status = ImageViewportRequestStatus::Loading;
                a.m_request.reason = projectWaitReason(w);
                a.m_display.status = retained(a.m_display);
            }
        }
        if (a.m_request.roles[1].provider
            && viewportEngineRoleCanRefineCurrentTerminal(
                a.m_request, ImageViewportPageRole::Secondary)) {
            if (!viewportEngineHasCurrentTerminal(a.m_request)) {
                a.m_request.status = ImageViewportRequestStatus::Loading;
                a.m_request.reason = ImageViewportRequestReason::ProviderWaiting;
                a.m_display.status = retained(a.m_display);
            }
            out.providerSessionOpenEffects[1] = a.openSession(ImageViewportPageRole::Secondary,
                a.m_request.roles[1].source, a.m_target.secondaryRoleGeneration);
        }
    }
    auto accepted = in.geometry;
    accepted.primaryPresent = a.m_request.roles[0].source.facts.present;
    accepted.primarySize = sourceLogicalSize(a.m_request.roles[0].source);
    accepted.secondarySize = sourceLogicalSize(a.m_request.roles[1].source);
    const auto positiveSize
        = [](QSizeF size) { return size.isValid() && size.width() > 0.0 && size.height() > 0.0; };
    const bool resolveContentPosition = !accepted.itemBounds.isEmpty()
        && positiveSize(accepted.primarySize)
        && (!a.m_target.acceptedRoleSet.secondary() || positiveSize(accepted.secondarySize));
    const QPointF previousContentPosition = a.m_presentation.contentPosition;
    ViewportEnginePresentationTargetTransitionReduction pt;
    if (!out.clear) {
        pt = a.transition({ in.transitionPolicy.zoomTransition(),
            in.transitionPolicy.contentPositionTransition(),
            in.transitionPolicy.rotationTransition(), in.transitionPolicy.mirrorTransition(),
            in.transitionPolicy.fitModeTransition()
                    == ViewportEnginePresentationTargetTransitionPolicy::FitModeTransition::
                        SetExplicit
                ? std::optional<ImageViewportFitMode>(in.transitionPolicy.fitMode())
                : std::nullopt,
            in.transitionPolicy.spreadDirectionTransition()
                    == ViewportEnginePresentationTargetTransitionPolicy::SpreadDirectionTransition::
                        SetExplicit
                ? std::optional<ImageViewportSpreadDirection>(in.transitionPolicy.spreadDirection())
                : std::nullopt,
            in.transitionPolicy.pageGapTransition()
                    == ViewportEnginePresentationTargetTransitionPolicy::PageGapTransition::
                        SetExplicit
                ? std::optional<double>(in.transitionPolicy.pageGap())
                : std::nullopt,
            accepted, previousContentPosition, resolveContentPosition,
            a.m_display.hasReadyDisplay(a.m_request.roles[0].source.facts.present) });
        if (pt.presentation)
            a.m_presentation = *pt.presentation;
        if (!resolveContentPosition) {
            a.m_target.pendingPresentationTransition = { a.m_target.generation,
                in.transitionPolicy.contentPositionTransition(), previousContentPosition };
        }
    }
    if (!refinement) {
        a.applyAutoplay();
    }
    out.changes = pt.changes;
    out.changes.requestState = out.changes.requestRevision = out.changes.displayState
        = out.changes.displayRevision = true;
    auto ng = projectViewportGeometryState(accepted, a.m_presentation);
    out.changes.geometryState
        = PresentationGeometry::contentRect(oldGeo) != PresentationGeometry::contentRect(ng)
        || PresentationGeometry::visibleImageRect(oldGeo)
            != PresentationGeometry::visibleImageRect(ng);
    out.changes.playbackPhase = oldPhase != a.m_playback.phase;
    const bool newWarning = a.m_display.hasActiveRenderQualityFallback(
        a.m_request.sequenceGeneration, a.m_presentation);
    out.changes.diagnostics = oldError != a.m_request.errorString || oldWarning != newWarning;
    out.changes.scheduleUpdate = true;
    return out;
}
