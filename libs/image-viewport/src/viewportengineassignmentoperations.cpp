// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    auto& p = m_mutation.roles[idx(role)].provider;
    ViewportEngineProviderSessionCloseAccess a(p.session, p.requests);
    auto effect = closeViewportEngineProviderSession(a);
    auto mutation = a.takeMutation();
    p.session = mutation.session;
    p.requests = std::move(mutation.requests);
    return effect;
}
ViewportEngineProviderSessionOpenEffect
ViewportEnginePresentationTargetAssignmentAccess::openSession(
    ImageViewportPageRole role, const ImageViewportInternal::ImageSequenceSource& s, quint64 g)
{
    auto& p = m_mutation.roles[idx(role)].provider;
    ViewportEngineProviderSessionOpenAccess a(s, p.session);
    auto effect = beginViewportEngineProviderSession({ role, g }, a);
    p.session = a.takeSession();
    return effect;
}

ViewportEnginePresentationTargetTransitionReduction
ViewportEnginePresentationTargetAssignmentAccess::transition(
    ViewportEnginePresentationTargetTransitionInput input)
{
    ViewportEnginePresentationTargetTransitionStateView view(m_mutation.presentation);
    return reduceViewportEnginePresentationTargetTransition(input, std::move(view));
}

void ViewportEnginePresentationTargetAssignmentAccess::applyAutoplay()
{
    ViewportEngineAuthoredAutoplayAccess autoplay(m_mutation.request,
        { m_mutation.roles[0].provider.facts, m_mutation.roles[1].provider.facts },
        m_mutation.playback);
    reduceViewportEngineAuthoredAutoplay({}, autoplay);
    m_mutation.playback = autoplay.takeMutation().playback;
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
    const DisplayRequest previousPrimaryRequest = a.m_mutation.request.roles[0].activeRequest;
    const DisplayRequest previousSecondaryRequest = a.m_mutation.request.roles[1].activeRequest;
    const PlaybackState previousPlayback = a.m_mutation.playback;
    bool noop = out.clear && a.m_mutation.target.acceptedRoleSet == ImageViewportRoleSet();
    out.presentationTargetChanged = !noop;
    out.retainPreviousDisplay = in.transitionPolicy.displayTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::DisplayTransition::RetainPrevious;
    out.releaseDisplayedState = out.clear || !out.retainPreviousDisplay;
    out.resetDisplayRequests = out.closeProviderSessions = out.presentationTargetChanged;
    out.stopPlayback = out.presentationTargetChanged && !refinement;
    if (out.presentationTargetChanged) {
        if (a.m_mutation.nextTargetGeneration == std::numeric_limits<quint64>::max())
            qFatal("ImageViewport presentation target generation exhausted");
        a.m_mutation.target
            = targetState(in.presentationTarget, ++a.m_mutation.nextTargetGeneration);
    }
    out.presentationTargetState = a.m_mutation.target;
    if (!out.presentationTargetChanged) {
        out.mutation = std::move(a.m_mutation);
        return out;
    }
    auto oldGeo = projectViewportGeometryState(in.geometry, a.m_mutation.presentation);
    const double oldMaximumManualZoom
        = projectViewportMaximumManualZoomPercent(in.geometry, a.m_mutation.presentation);
    const std::array<ImageViewportPlaybackPhase, 2> oldPhases {
        a.m_mutation.playback.roles[0].phase,
        a.m_mutation.playback.roles[1].phase,
    };
    const PublicDiagnosticText oldError = a.m_mutation.request.errorString;
    const bool oldWarning = a.m_mutation.display.hasActiveRenderQualityFallback(
        a.m_mutation.request.sequenceGeneration, a.m_mutation.presentation);
    out.providerEffects[0] = a.closeSession(ImageViewportPageRole::Primary);
    out.providerEffects[1] = a.closeSession(ImageViewportPageRole::Secondary);
    auto primary = std::move(in.primarySource), secondarySource = std::move(in.secondarySource);
    if (!out.clear && !primary.sequence)
        primary = factorySequenceSource(in.presentationTarget.primary());
    if (!out.clear && !secondarySource.sequence)
        secondarySource = factorySequenceSource(in.presentationTarget.secondary());
    a.m_mutation.request.roles[0].source = std::move(primary);
    a.m_mutation.request.roles[0].sequence = a.m_mutation.request.roles[0].source.sequence;
    a.m_mutation.request.roles[1].source = std::move(secondarySource);
    a.m_mutation.request.roles[1].sequence = a.m_mutation.request.roles[1].source.sequence;
    a.m_mutation.request.roles[1].provider = a.m_mutation.request.roles[1].source.facts.provider;
    a.m_mutation.request.sequenceGeneration = a.m_mutation.target.generation;
    a.m_mutation.request.clearDisplayRequests();
    if (!refinement) {
        a.m_mutation.playback.resetRequestIdentity();
    }
    a.m_mutation.display.nextPreparedPayloadId = 0;
    a.m_mutation.display.clearPendingRenderPayload();
    if (out.releaseDisplayedState) {
        a.m_mutation.display.clearDisplayedDisplay();
    }
    a.m_mutation.request.errorString.clear();
    a.m_mutation.display.renderQualityFallback.clear();
    if (refinement) {
        a.m_mutation.playback = previousPlayback;
        for (auto& rolePlayback : a.m_mutation.playback.roles) {
            if (rolePlayback.phase == ImageViewportPlaybackPhase::Playing) {
                rolePlayback.phase = ImageViewportPlaybackPhase::Waiting;
            }
            rolePlayback.providerStartPending = false;
        }
    } else {
        for (auto& rolePlayback : a.m_mutation.playback.roles) {
            rolePlayback.phase = ImageViewportPlaybackPhase::Stopped;
            rolePlayback.stopWhenRequestReady = false;
            rolePlayback.providerStartPending = false;
        }
    }
    resetProvider(a.m_mutation.roles[0].provider,
        a.m_mutation.request.roles[0].source.facts.provider
            ? a.m_mutation.request.roles[0].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {},
        a.m_mutation.request.roles[0].source.facts.provider
            && a.m_mutation.request.roles[0].source.facts.authoredAnimationFactsAvailable);
    resetProvider(a.m_mutation.roles[1].provider,
        a.m_mutation.request.roles[1].source.facts.provider
            ? a.m_mutation.request.roles[1].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {},
        a.m_mutation.request.roles[1].source.facts.provider
            && a.m_mutation.request.roles[1].source.facts.authoredAnimationFactsAvailable);
    if (out.clear) {
        for (auto& rolePlayback : a.m_mutation.playback.roles) {
            rolePlayback.authoredAutoplayArbitration = AuthoredAutoplayArbitrationState::Resolved;
        }
        a.m_mutation.display.clearDisplayedDisplay();
        a.m_mutation.request.status = ImageViewportRequestStatus::NoRequest;
        a.m_mutation.request.reason = ImageViewportRequestReason::NoRequest;
        a.m_mutation.display.status = ImageViewportDisplayStatus::Empty;
    } else {
        const auto primaryTarget = refinement ? previousPrimaryRequest.target
                                              : initial(a.m_mutation.request.roles[0].source);
        const auto primaryResolved = refinement
            ? previousPrimaryRequest.resolvedFrame
            : ResolvedFrameIdentity { primaryTarget.frame, primaryTarget.position };
        const auto secondaryTarget = refinement ? previousSecondaryRequest.target
                                                : initial(a.m_mutation.request.roles[1].source);
        const auto secondaryResolved = refinement
            ? previousSecondaryRequest.resolvedFrame
            : ResolvedFrameIdentity { secondaryTarget.frame, secondaryTarget.position };
        if (a.m_mutation.request.roles[0].source.facts.provider) {
            auto& p = a.m_mutation.roles[0].provider;
            auto& f = a.m_mutation.request.roles[0].source.facts;
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
            a.m_mutation.request.beginDisplayRequest(DisplayRequestOrigin::Initial, t,
                refinement ? primaryResolved : ResolvedFrameIdentity { t.frame, t.position }, true);
            a.m_mutation.playback.roles[0].position
                = refinement ? previousPlayback.roles[0].position : t.position;
            secondary(a.m_mutation.request, secondaryTarget, secondaryResolved);
            a.m_mutation.request.status = ImageViewportRequestStatus::Loading;
            a.m_mutation.request.reason = ImageViewportRequestReason::ProviderWaiting;
            a.m_mutation.display.status = retained(a.m_mutation.display);
            out.providerSessionOpenEffects[0] = a.openSession(ImageViewportPageRole::Primary,
                a.m_mutation.request.roles[0].source, a.m_mutation.target.primaryRoleGeneration);
        } else if (a.m_mutation.request.roles[0].source.facts.present) {
            a.m_mutation.request.beginDisplayRequest(
                DisplayRequestOrigin::Initial, primaryTarget, primaryResolved, true);
            a.m_mutation.playback.roles[0].position
                = refinement ? previousPlayback.roles[0].position : primaryTarget.position;
            secondary(a.m_mutation.request, secondaryTarget, secondaryResolved);
            const auto admission = stage(a.m_mutation.request, a.m_mutation.display,
                a.m_mutation.playback, a.m_mutation.presentation.exactnessPreference);
            if (admission.accepted) {
                TargetSpreadWaitState w;
                w.requiresSecondary = a.m_mutation.request.roles[1].sequence != nullptr;
                if (!in.geometry.renderAvailable || in.geometry.itemBounds.isEmpty()) {
                    w.primary.renderWaiting = true;
                    if (w.requiresSecondary && !a.m_mutation.request.roles[1].provider)
                        w.secondary.renderWaiting = true;
                } else {
                    w.primary.uploadPending = true;
                    if (w.requiresSecondary && !a.m_mutation.request.roles[1].provider)
                        w.secondary.uploadPending = true;
                }
                if (a.m_mutation.request.roles[1].provider)
                    w.secondary.providerWaiting = true;
                a.m_mutation.request.status = ImageViewportRequestStatus::Loading;
                a.m_mutation.request.reason = projectWaitReason(w);
                a.m_mutation.display.status = retained(a.m_mutation.display);
            }
        }
        if (a.m_mutation.request.roles[1].provider
            && viewportEngineRoleCanRefineCurrentTerminal(
                a.m_mutation.request, ImageViewportPageRole::Secondary)) {
            if (!viewportEngineHasCurrentTerminal(a.m_mutation.request)) {
                a.m_mutation.request.status = ImageViewportRequestStatus::Loading;
                a.m_mutation.request.reason = ImageViewportRequestReason::ProviderWaiting;
                a.m_mutation.display.status = retained(a.m_mutation.display);
            }
            out.providerSessionOpenEffects[1] = a.openSession(ImageViewportPageRole::Secondary,
                a.m_mutation.request.roles[1].source, a.m_mutation.target.secondaryRoleGeneration);
        }
        if (a.m_mutation.request.roles[1].source.facts.present) {
            a.m_mutation.playback.roles[1].position
                = refinement ? previousPlayback.roles[1].position : secondaryTarget.position;
        }
    }
    auto accepted = in.geometry;
    accepted.primaryPresent = a.m_mutation.request.roles[0].source.facts.present;
    accepted.primarySize = sourceLogicalSize(a.m_mutation.request.roles[0].source);
    accepted.secondarySize = sourceLogicalSize(a.m_mutation.request.roles[1].source);
    const auto positiveSize
        = [](QSizeF size) { return size.isValid() && size.width() > 0.0 && size.height() > 0.0; };
    const bool resolveContentPosition = !accepted.itemBounds.isEmpty()
        && positiveSize(accepted.primarySize)
        && (!a.m_mutation.target.acceptedRoleSet.secondary()
            || positiveSize(accepted.secondarySize));
    const QPointF previousContentPosition = a.m_mutation.presentation.contentPosition;
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
            a.m_mutation.display.hasReadyDisplay(
                a.m_mutation.request.roles[0].source.facts.present) });
        if (pt.presentation)
            a.m_mutation.presentation = *pt.presentation;
        if (!resolveContentPosition) {
            a.m_mutation.target.pendingPresentationTransition = { a.m_mutation.target.generation,
                in.transitionPolicy.contentPositionTransition(), previousContentPosition };
        }
    }
    if (!refinement) {
        a.applyAutoplay();
    }
    out.changes = pt.changes;
    out.changes.requestState = out.changes.requestRevision = out.changes.displayState
        = out.changes.displayRevision = true;
    auto ng = projectViewportGeometryState(accepted, a.m_mutation.presentation);
    const double newMaximumManualZoom
        = projectViewportMaximumManualZoomPercent(accepted, a.m_mutation.presentation);
    out.changes.presentationRevision
        = out.changes.presentationRevision || oldMaximumManualZoom != newMaximumManualZoom;
    out.changes.geometryState
        = PresentationGeometry::contentRect(oldGeo) != PresentationGeometry::contentRect(ng)
        || PresentationGeometry::visibleImageRect(oldGeo)
            != PresentationGeometry::visibleImageRect(ng);
    out.changes.playbackPhase = oldPhases[0] != a.m_mutation.playback.roles[0].phase
        || oldPhases[1] != a.m_mutation.playback.roles[1].phase;
    const bool newWarning = a.m_mutation.display.hasActiveRenderQualityFallback(
        a.m_mutation.request.sequenceGeneration, a.m_mutation.presentation);
    out.changes.diagnostics
        = oldError != a.m_mutation.request.errorString || oldWarning != newWarning;
    out.changes.scheduleUpdate = true;
    out.mutation = std::move(a.m_mutation);
    return out;
}
