#pragma once

#include "imageviewport.h"
#include "timingintervals_p.h"

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include <memory>

namespace ImageViewportInternal {

struct ViewportChangeSet
{
    bool requestState = false;
    bool displayState = false;
    bool geometryState = false;
    bool playbackPhase = false;
    bool diagnostics = false;
    bool presentation = false;
    bool sequence = false;
    bool looping = false;
    bool displayRevision = false;
    bool requestRevision = false;
    bool commandRevision = false;
    bool scheduleUpdate = false;
};

enum class ProviderRequestTargetKind {
    Unknown,
    Frame,
    Position,
    Playback,
};

enum class DisplayRequestOrigin {
    None,
    Initial,
    ExplicitSeek,
    Playback,
    MetadataBoundSelection,
    StopRestore,
};

struct DisplayRequestIdentity
{
    quint64 id = 0;
    DisplayRequestOrigin origin = DisplayRequestOrigin::None;
};

struct DisplayRequestTarget
{
    int frame = -1;
    int position = -1;
    ProviderRequestTargetKind providerTargetKind = ProviderRequestTargetKind::Unknown;
};

struct DisplayRequest
{
    DisplayRequestIdentity identity;
    DisplayRequestTarget target;
    ImageSequenceProviderRequestToken providerFrameToken;
    quint64 preparedPayloadId = 0;
};

struct DisplayRequestSnapshot
{
    quint64 generation = 0;
    DisplayRequest request;
};

struct PreparedPayloadIdentity
{
    quint64 generation = 0;
    quint64 requestId = 0;
    quint64 payloadId = 0;
};

struct PreparedPayload
{
    bool commitPending = false;
    quint64 generation = 0;
    quint64 requestId = 0;
    quint64 payloadId = 0;
    QImage image;
};

struct PresentationState
{
    ImageViewport::FillMode fillMode = ImageViewport::FillMode::Contain;
    ImageViewport::HorizontalAlignment horizontalAlignment
        = ImageViewport::HorizontalAlignment::AlignHCenter;
    ImageViewport::VerticalAlignment verticalAlignment
        = ImageViewport::VerticalAlignment::AlignVCenter;
    ImageViewport::BackgroundMode backgroundMode = ImageViewport::BackgroundMode::Transparent;
    QColor backgroundColor = Qt::transparent;
    double zoom = 1.0;
    QPointF pan;
    bool smoothing = true;
    bool mipmap = false;
    bool mirrorHorizontally = false;
    bool mirrorVertically = false;
};

struct DisplayState
{
    DisplayRequestSnapshot activeRequestSnapshot(quint64 sequenceGeneration,
        const DisplayRequest& activeRequest, int displayedPosition) const
    {
        DisplayRequestSnapshot snapshot = displayedRequest;
        snapshot.generation = sequenceGeneration;
        snapshot.request.target = activeRequest.target;
        snapshot.request.target.frame = activeRequest.target.frame;
        snapshot.request.target.position = displayedPosition;
        return snapshot;
    }

    void commitDisplayedRequestSnapshot(
        quint64 sequenceGeneration, const DisplayRequest& activeRequest, quint64 preparedPayloadId)
    {
        const auto displayedTarget = displayedRequest.request.target;
        displayedRequest.generation = sequenceGeneration;
        displayedRequest.request = activeRequest;
        displayedRequest.request.target = displayedTarget;
        displayedRequest.request.preparedPayloadId = preparedPayloadId;
    }

    void clearDisplayedDisplay()
    {
        displayedRequest = {};
        displayedImageSize = {};
        displayedImage = {};
    }

    void beginPreparedPayloadIdentity(quint64 sequenceGeneration, DisplayRequest& activeRequest)
    {
        pendingRenderPayload.generation = sequenceGeneration;
        pendingRenderPayload.requestId = activeRequest.identity.id;
        pendingRenderPayload.payloadId
            = activeRequest.identity.id == 0 ? 0 : ++nextPreparedPayloadId;
        activeRequest.preparedPayloadId = pendingRenderPayload.payloadId;
    }

    void commitPreparedPayloadIdentity(
        DisplayRequest& activeRequest, const PreparedPayload& preparedPayload)
    {
        pendingRenderPayload = preparedPayload;
        activeRequest.preparedPayloadId = preparedPayload.payloadId;
        if (preparedPayload.payloadId > nextPreparedPayloadId) {
            nextPreparedPayloadId = preparedPayload.payloadId;
        }
    }

    void clearPendingRenderPayload() { pendingRenderPayload = {}; }

    bool hasReadyDisplay(bool hasDisplayableSequence) const
    {
        return hasDisplayableSequence
            && (status == ImageViewport::DisplayStatus::Ready
                || status == ImageViewport::DisplayStatus::Retained)
            && displayedImageSize.isValid() && displayedImageSize.width() > 0.0
            && displayedImageSize.height() > 0.0;
    }

    void captureRenderFailureRetainedDisplay(bool hasDisplayableSequence)
    {
        if (!hasReadyDisplay(hasDisplayableSequence)) {
            clearRenderFailureRetainedDisplay();
            return;
        }

        renderFailureRetainedDisplayValid = true;
        renderFailureRetainedRequest = displayedRequest;
        renderFailureRetainedImageSize = displayedImageSize;
        renderFailureRetainedImage = displayedImage;
    }

    void clearRenderFailureRetainedDisplay()
    {
        renderFailureRetainedDisplayValid = false;
        renderFailureRetainedRequest = {};
        renderFailureRetainedImageSize = {};
        renderFailureRetainedImage = {};
    }

    ImageViewport::DisplayStatus status = ImageViewport::DisplayStatus::Empty;
    DisplayRequestSnapshot displayedRequest;
    QSizeF displayedImageSize;
    QImage displayedImage;
    quint64 nextPreparedPayloadId = 0;
    PreparedPayload pendingRenderPayload;
    DisplayRequestSnapshot renderFailureRetainedRequest;
    bool renderFailureRetainedDisplayValid = false;
    QSizeF renderFailureRetainedImageSize;
    QImage renderFailureRetainedImage;
    uint revision = 0;
};

struct RequestState
{
    void clearDisplayRequests()
    {
        nextRequestId = 0;
        activeRequest = {};
        latestNonPlaybackRequest = {};
        playbackPosition = -1;
    }

    void beginDisplayRequest(DisplayRequestOrigin origin, bool rememberAsLatestNonPlayback)
    {
        activeRequest.identity.id = ++nextRequestId;
        activeRequest.identity.origin = origin;
        activeRequest.providerFrameToken = {};
        activeRequest.preparedPayloadId = 0;
        if (rememberAsLatestNonPlayback) {
            latestNonPlaybackRequest.identity = activeRequest.identity;
        }
    }

    void beginDisplayRequest(
        DisplayRequestOrigin origin, DisplayRequestTarget target, bool rememberAsLatestNonPlayback)
    {
        beginDisplayRequest(origin, rememberAsLatestNonPlayback);
        activeRequest.target = target;
        if (rememberAsLatestNonPlayback) {
            latestNonPlaybackRequest.target = target;
        }
    }

    void setCommandDiagnostic(ImageViewport::CommandReason reason) { commandReason = reason; }

    bool clearCommandDiagnosticForAcceptedCommand()
    {
        if (commandReason == ImageViewport::CommandReason::NoCommand) {
            return false;
        }

        setCommandDiagnostic(ImageViewport::CommandReason::NoCommand);
        return true;
    }

    bool clearDiagnostics()
    {
        if (errorString.isEmpty() && warningString.isEmpty()) {
            return false;
        }

        errorString.clear();
        warningString.clear();
        return true;
    }

    QPointer<ImageSequence> sequence;
    std::shared_ptr<ImageSequence> sequenceOwner;
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::NoRequest;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::NoRequest;
    ImageViewport::CommandReason commandReason = ImageViewport::CommandReason::NoCommand;
    ImageViewport::PlaybackPhase playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    bool looping = false;
    bool stopPlaybackWhenRequestReady = false;
    bool providerPlaybackStartPending = false;
    DisplayRequest activeRequest;
    int playbackPosition = -1;
    DisplayRequest latestNonPlaybackRequest;
    quint64 sequenceGeneration = 0;
    quint64 nextRequestId = 0;
    uint requestRevision = 0;
    uint commandRevision = 0;
    QString errorString;
    QString warningString;
};

struct ProviderGenerationState
{
    QPointer<ImageSequenceProviderSession> session;
    quint64 sessionSerial = 0;
    quint64 nextRequestToken = 0;
    ImageSequenceProviderRequestToken activeMetadataToken;
    ImageSequenceProviderRequestToken activeFrameToken;
    quint64 activeFrameRequestId = 0;
    bool activeFrameFromPlayback = false;
    ProviderRequestTargetKind activeFrameTargetKind = ProviderRequestTargetKind::Unknown;
    bool queuedFrameRequest = false;
    quint64 queuedFrameGeneration = 0;
    quint64 queuedFrameRequestId = 0;
    int queuedFrame = -1;
    int queuedPosition = -1;
    bool queuedFrameFromPlayback = false;
    ProviderRequestTargetKind queuedFrameTargetKind = ProviderRequestTargetKind::Unknown;
    bool metadataReady = false;
    bool timedMetadata = false;
    bool timedPlaybackSupport = false;
    bool frameSeekSupport = false;
    bool positionSeekSupport = false;
    QSizeF logicalSize;
    TimingIntervals timingIntervals;
};

}
