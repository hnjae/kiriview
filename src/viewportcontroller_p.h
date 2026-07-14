#pragma once

#include "framepreparation_p.h"
#include "imageviewportstate_p.h"
#include "presentationgeometry_p.h"
#include "viewportcontrollerassignmentcontract_p.h"
#include "viewportcontrollertransition_p.h"
#include "viewportengine_p.h"
#include <ImageViewport/ImageViewport>

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QString>

#include <functional>

struct ControllerTransitionPolicy;
struct ViewportCommandResult;
struct ViewportPlaybackScheduleEffect;
struct ViewportPresentationCommandInput;
struct ViewportProviderFrameTransportEffect;
struct ViewportProviderHostEvent;
struct ViewportRenderAcknowledgement;
struct ViewportRenderSynchronization;
struct ViewportSequenceAssignment;

class ImageViewportPrivate;

class ViewportController
{
public:
    explicit ViewportController(std::function<QRectF()> captureItemBounds);

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    const ImageViewportInternal::PresentationState& presentationState() const;
    const ImageViewportInternal::DisplayState& displayState() const;
    const ImageViewportInternal::RequestState& requestState() const;
    const ImageViewportInternal::PlaybackState& playbackState() const;
    ViewportEngineCommandDiagnostics commandDiagnostics() const;
    quint64 publishedCommandRevision() const;
#endif
    ImageViewportStateSnapshot stateSnapshot(double devicePixelRatio = 1.0) const;
    PresentationGeometry::State geometryState(double devicePixelRatio = 1.0) const;
    PresentationGeometry::State geometryStateForItemBounds(
        const QRectF& itemBounds, double devicePixelRatio = 1.0) const;
    double minimumManualZoomPercent() const;
    double maximumManualZoomPercent(double devicePixelRatio = 1.0) const;
    double manualZoomStepFactor() const;
    double clampedManualZoomPercent(double percent, double devicePixelRatio = 1.0) const;
    double steppedManualZoomPercent(int stepCount, double devicePixelRatio = 1.0) const;

    ViewportCommandResult assignSequence(ViewportSequenceAssignment assignment);
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewportPageRole role);
    ViewportCommandResult clear();
    ViewportCommandResult play(ImageViewportPageRole role);
    ViewportCommandResult pause(ImageViewportPageRole role);
    ViewportCommandResult stop(ImageViewportPageRole role);
    ViewportCommandResult seek(ImageViewportPageRole role, int frame);
    ViewportCommandResult seekToPosition(ImageViewportPageRole role, int milliseconds);
    ViewportCommandResult applyPlaybackCommand(ViewportPlaybackCommand command);
    ViewportCommandResult setPresentation(const ViewportPresentationCommandInput& input);
    ViewportCommandResult setSpreadDirection(ImageViewportSpreadDirection direction);
    ViewportCommandResult setPageGap(double gap);
    ViewportCommandResult setFitMode(ImageViewportFitMode mode, QPointF anchor);
    ViewportCommandResult setZoomPercent(
        double percent, QPointF anchor, double devicePixelRatio = 1.0);
    ViewportCommandResult zoomByStep(int stepCount, QPointF anchor, double devicePixelRatio = 1.0);
    ViewportCommandResult panBy(QPointF delta);
    ViewportCommandResult panToStart();
    ViewportCommandResult panToEnd();
    ViewportCommandResult rotateClockwise(QPointF anchor);
    ViewportCommandResult rotateCounterClockwise(QPointF anchor);
    ViewportCommandResult setMirrorHorizontally(bool enabled, QPointF anchor);
    ViewportCommandResult setMirrorVertically(bool enabled, QPointF anchor);
    ViewportCommandResult resetView();
    ViewportControllerTransition handleProviderHostEvent(const ViewportProviderHostEvent& event);
    ViewportControllerTransition handleDevicePixelRatioChanged(double devicePixelRatio = 1.0);
    ViewportControllerTransition handleGeometryChanged(
        const QRectF& oldContentRect, const QRectF& oldVisibleImageRect);
    ViewportRenderSynchronization beginRenderSynchronization(double devicePixelRatio = 1.0);
    ViewportControllerTransition acknowledgeRenderCommit(
        const ViewportRenderAcknowledgement& acknowledgement, bool renderedImagePresent,
        const ViewportRenderSynchronization& synchronization);
    ViewportControllerTransition acknowledgeRenderFailure(
        const ViewportRenderAcknowledgement& acknowledgement);
    ViewportControllerTransition advancePlayback(int elapsedMilliseconds);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void setNextProviderRequestTokenForTest(quint64 token);
    void setNextProviderRequestTokenForTest(ImageViewportPageRole role, quint64 token);
    void setNextRevisionTokenForTest(quint64 token);
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
    quint64 secondaryPendingRenderPayloadIdForTest() const;
    ImageViewportInternal::RenderFailureDiagnostic
    lastAcceptedRenderFailureDiagnosticForTest() const;
#endif

private:
    friend class ImageViewportPrivate;
    ImageViewportInternal::ViewportChangeSet publishChanges(
        ImageViewportInternal::ViewportChangeSet changes);
    QRectF itemBounds() const;

    std::function<QRectF()> captureItemBounds;
    ViewportEngine engine;
};
