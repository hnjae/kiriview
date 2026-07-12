#include "viewportengineassignmentoperations_p.h"

#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "viewportengineplaybackoperations_p.h"
#include "viewportengineprojection_p.h"

#include <limits>

namespace {
using namespace ImageViewportInternal;
std::size_t idx(ImageViewport::PageRole r)
{
    return r == ImageViewport::PageRole::Secondary ? 1U : 0U;
}
ImageViewport::DisplayStatus retained(const DisplayState& d)
{
    bool ok = (d.status == ImageViewport::DisplayStatus::Ready
                  || d.status == ImageViewport::DisplayStatus::Retained)
        && d.roles[0].displayedImageSize.isValid();
    return ok ? ImageViewport::DisplayStatus::Retained : ImageViewport::DisplayStatus::Empty;
}
void resetProvider(ProviderRoleState& p, ImageSequenceAuthoredAnimationFacts a = {})
{
    p.facts.metadataReady = false;
    p.facts.timedMetadata = false;
    p.facts.timedPlaybackSupport = false;
    p.facts.frameSeekSupport = false;
    p.facts.positionSeekSupport = false;
    p.facts.authoredAnimationFacts = a;
    p.facts.logicalSize = {};
    p.facts.timingIntervals = {};
    p.requests.activeMetadataToken = {};
    p.requests.activeFrameToken = {};
}
DisplayRequestTarget initial(const ImageSequenceSource& s)
{
    if (!s.facts.present || s.facts.provider || s.facts.frameCount <= 0)
        return {};
    return { 0, s.facts.timed ? sourceFrameStartPosition(s, 0) : -1,
        ProviderRequestTargetKind::Unknown };
}
void secondary(RequestState& r, DisplayRequestTarget t)
{
    auto& s = r.roles[1].activeRequest;
    s.identity = r.roles[0].activeRequest.identity;
    s.target = t;
    s.resolvedFrame
        = t.frame >= 0 ? ResolvedFrameIdentity { t.frame, t.position } : ResolvedFrameIdentity {};
    s.preparedPayloadId = r.roles[0].activeRequest.preparedPayloadId;
    if (t.frame >= 0)
        r.roles[1].latestNonPlaybackRequest = s;
}
void stage(RequestState& r, DisplayState& d)
{
    d.captureRenderFailureRetainedDisplay(r.roles[0].source.facts.present);
    d.roles[0].pendingRenderPayload.commitPending = true;
    d.beginPreparedPayloadIdentity(r.sequenceGeneration, r.roles[0].activeRequest);
    d.roles[0].pendingRenderPayload = FramePreparation::admitBuiltInFrame(
        r.roles[0].source, r.roles[0].activeRequest.target.frame, d.roles[0].pendingRenderPayload)
                                          .preparedPayload;
    if (r.roles[1].sequence && !r.roles[1].provider && r.roles[1].activeRequest.target.frame >= 0) {
        PreparedPayload p;
        p.commitPending = true;
        p.generation = r.sequenceGeneration;
        p.requestId = r.roles[0].activeRequest.identity.id;
        p.payloadId = ++d.nextPreparedPayloadId;
        r.roles[1].activeRequest.preparedPayloadId = p.payloadId;
        d.roles[1].pendingRenderPayload = FramePreparation::admitBuiltInFrame(
            r.roles[1].source, r.roles[1].activeRequest.target.frame, p)
                                              .preparedPayload;
    }
}
double zoom(const PresentationGeometry::State& s)
{
    QSizeF spread = PresentationGeometry::spreadSize(s);
    int rot = ((s.rotationDegrees % 360) + 360) % 360;
    if (rot == 90 || rot == 270)
        spread.transpose();
    QRectF c = PresentationGeometry::contentRect(s);
    if (c.isEmpty() || spread.width() <= 0 || spread.height() <= 0)
        return s.manualZoom * 100;
    return c.width() / spread.width() * s.devicePixelRatio * 100;
}
ViewportEnginePresentationTargetState targetState(
    const ImageViewportPresentationTarget& t, quint64 g)
{
    ViewportEnginePresentationTargetState s;
    if (t.isClear()) {
        s.presentationTarget = ImageViewportPresentationTarget::clear();
        s.generation = g;
        return s;
    }
    s.presentationTarget = t;
    s.generation = g;
    s.acceptedRoleSet = ImageViewportRoleSet(true, t.secondary() != nullptr);
    s.targetRoleSet = s.acceptedRoleSet;
    s.primaryRoleGeneration = g;
    s.secondaryRoleGeneration = t.secondary() ? g : 0;
    s.activeRole = ImageViewport::PageRole::Primary;
    s.activeRoleValid = true;
    return s;
}
}

ViewportProviderFrameTransportEffect ViewportEnginePresentationTargetAssignmentAccess::closeSession(
    ImageViewport::PageRole role)
{
    auto& p = m_roles[idx(role)].provider;
    ViewportEngineProviderSessionCloseAccess a(p.session, p.requests);
    return closeViewportEngineProviderSession(std::move(a));
}
ViewportEngineProviderSessionOpenEffect
ViewportEnginePresentationTargetAssignmentAccess::openSession(
    ImageViewport::PageRole role, const ImageViewportInternal::ImageSequenceSource& s, quint64 g)
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
    return reduceViewportEnginePresentationTargetTransition(std::move(input), std::move(view));
}

void ViewportEnginePresentationTargetAssignmentAccess::applyAutoplay()
{
    ViewportEngineAuthoredAutoplayAccess autoplay(m_request.roles[0].source,
        m_roles[0].provider.facts, m_request.roles[0].activeRequest, m_playback, m_request.status);
    reduceViewportEngineAuthoredAutoplay({}, std::move(autoplay));
}

ViewportEnginePresentationTargetAssignmentReduction
reduceViewportEnginePresentationTargetAssignment(ViewportEnginePresentationTargetAssignmentInput in,
    ViewportEnginePresentationTargetAssignmentAccess a)
{
    using namespace ImageViewportInternal;
    ViewportEnginePresentationTargetAssignmentReduction out;
    out.clear = in.presentationTarget.isClear();
    bool noop = out.clear && a.m_target.acceptedRoleSet == ImageViewportRoleSet();
    out.presentationTargetChanged = !noop;
    out.retainPreviousDisplay = in.transitionPolicy.displayTransition()
        == PresentationTargetTransitionPolicy::DisplayTransition::RetainPrevious;
    out.releaseDisplayedState = out.clear || !out.retainPreviousDisplay;
    out.resetDisplayRequests = out.stopPlayback = out.closeProviderSessions
        = out.presentationTargetChanged;
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
    QString oldE = a.m_request.errorString, oldW = a.m_request.warningString;
    out.providerEffects[0] = a.closeSession(ImageViewport::PageRole::Primary);
    out.providerEffects[1] = a.closeSession(ImageViewport::PageRole::Secondary);
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
    a.m_playback.resetRequestIdentity();
    a.m_display.nextPreparedPayloadId = 0;
    a.m_display.clearPendingRenderPayload();
    if (out.releaseDisplayedState) {
        a.m_display.clearDisplayedDisplay();
        a.m_display.clearRenderFailureRetainedDisplay();
    }
    a.m_request.errorString.clear();
    a.m_request.warningString.clear();
    a.m_playback.phase = ImageViewport::PlaybackPhase::Stopped;
    a.m_playback.stopWhenRequestReady = false;
    a.m_playback.providerStartPending = false;
    resetProvider(a.m_roles[0].provider,
        a.m_request.roles[0].source.facts.provider
            ? a.m_request.roles[0].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {});
    resetProvider(a.m_roles[1].provider,
        a.m_request.roles[1].source.facts.provider
            ? a.m_request.roles[1].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {});
    if (out.clear) {
        a.m_display.clearDisplayedDisplay();
        a.m_display.clearRenderFailureRetainedDisplay();
        a.m_request.status = ImageViewport::RequestStatus::NoRequest;
        a.m_request.reason = ImageViewport::RequestReason::NoRequest;
        a.m_display.status = ImageViewport::DisplayStatus::Empty;
    } else {
        auto st = initial(a.m_request.roles[1].source);
        if (a.m_request.roles[0].source.facts.provider) {
            auto& p = a.m_roles[0].provider;
            auto& f = a.m_request.roles[0].source.facts;
            DisplayRequestTarget t { -1, -1, ProviderRequestTargetKind::Unknown };
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
                t = { 0, p.facts.timedMetadata ? 0 : -1, ProviderRequestTargetKind::Frame };
            }
            a.m_request.beginDisplayRequest(DisplayRequestOrigin::Initial, t, true);
            a.m_playback.position = t.position;
            secondary(a.m_request, st);
            a.m_request.status = ImageViewport::RequestStatus::Loading;
            a.m_request.reason = ImageViewport::RequestReason::ProviderWaiting;
            a.m_display.status = retained(a.m_display);
            out.providerSessionOpenEffects[0] = a.openSession(ImageViewport::PageRole::Primary,
                a.m_request.roles[0].source, a.m_target.primaryRoleGeneration);
        } else if (a.m_request.roles[0].source.facts.present) {
            auto t = initial(a.m_request.roles[0].source);
            a.m_request.beginDisplayRequest(DisplayRequestOrigin::Initial, t, true);
            a.m_playback.position = t.position;
            secondary(a.m_request, st);
            stage(a.m_request, a.m_display);
            TargetSpreadWaitState w;
            w.requiresSecondary = a.m_request.roles[1].sequence != nullptr;
            if (in.geometry.itemBounds.isEmpty()) {
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
            a.m_request.status = ImageViewport::RequestStatus::Loading;
            a.m_request.reason = projectWaitReason(w);
            a.m_display.status = retained(a.m_display);
        }
        if (a.m_request.roles[1].provider) {
            a.m_request.status = ImageViewport::RequestStatus::Loading;
            a.m_request.reason = ImageViewport::RequestReason::ProviderWaiting;
            a.m_display.status = retained(a.m_display);
            out.providerSessionOpenEffects[1] = a.openSession(ImageViewport::PageRole::Secondary,
                a.m_request.roles[1].source, a.m_target.secondaryRoleGeneration);
        }
    }
    auto accepted = in.geometry;
    accepted.primaryPresent = a.m_request.roles[0].source.facts.present;
    accepted.primarySize = sourceLogicalSize(a.m_request.roles[0].source);
    accepted.secondarySize = sourceLogicalSize(a.m_request.roles[1].source);
    ViewportEnginePresentationTargetTransitionReduction pt;
    if (!out.clear) {
        pt = a.transition({ in.transitionPolicy.zoomTransition(),
            in.transitionPolicy.contentPositionTransition(),
            in.transitionPolicy.rotationTransition(), in.transitionPolicy.mirrorTransition(),
            in.transitionPolicy.fitModeTransition()
                    == PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit
                ? std::optional<ImageViewport::FitMode>(in.transitionPolicy.fitMode())
                : std::nullopt,
            in.transitionPolicy.spreadDirectionTransition()
                    == PresentationTargetTransitionPolicy::SpreadDirectionTransition::SetExplicit
                ? std::optional<ImageViewport::SpreadDirection>(
                      in.transitionPolicy.spreadDirection())
                : std::nullopt,
            in.transitionPolicy.pageGapTransition()
                    == PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit
                ? std::optional<double>(in.transitionPolicy.pageGap())
                : std::nullopt,
            accepted, PresentationGeometry::contentPosition(oldGeo), zoom(oldGeo),
            a.m_display.hasReadyDisplay(a.m_request.roles[0].source.facts.present) });
        if (pt.presentation)
            a.m_presentation = *pt.presentation;
    }
    a.applyAutoplay();
    out.changes = pt.changes;
    out.changes.requestState = out.changes.requestRevision = out.changes.displayState
        = out.changes.displayRevision = true;
    auto ng = projectViewportGeometryState(accepted, a.m_presentation);
    out.changes.geometryState
        = PresentationGeometry::contentRect(oldGeo) != PresentationGeometry::contentRect(ng)
        || PresentationGeometry::visibleImageRect(oldGeo)
            != PresentationGeometry::visibleImageRect(ng);
    out.changes.playbackPhase = oldPhase != a.m_playback.phase;
    out.changes.diagnostics = oldE != a.m_request.errorString || oldW != a.m_request.warningString;
    out.changes.scheduleUpdate = true;
    return out;
}
