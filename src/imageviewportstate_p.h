#pragma once

#include "imagesequencesource_p.h"
#include "imageviewport.h"
#include "renderfailurecause_p.h"
#include "timingintervals_p.h"

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include <array>
#include <memory>

namespace ImageViewportInternal {

struct RenderFailureDiagnostic
{
    bool valid = false;
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    quint64 generation = 0;
    quint64 requestId = 0;
    quint64 preparedPayloadId = 0;
    RenderFailureCause cause = RenderFailureCause::None;
};

struct ViewportChangeSet
{
    bool requestState = false;
    bool displayState = false;
    bool geometryState = false;
    bool playbackPhase = false;
    bool diagnostics = false;
    bool displayRevision = false;
    bool requestRevision = false;
    bool commandRevision = false;
    bool presentationRevision = false;
    quint64 commandRevisionValue = 0;
    bool scheduleUpdate = false;
    RenderFailureDiagnostic renderFailureDiagnostic;
};

enum class ProviderTransportOperation {
    None,
    Cancel,
    Close,
};

struct ProviderTransportDiagnostic
{
    bool valid = false;
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ProviderTransportOperation operation = ProviderTransportOperation::None;
    bool metadataTokenValid = false;
    quint64 metadataTokenValue = 0;
    bool frameTokenValid = false;
    quint64 frameTokenValue = 0;
    bool queued = false;
    bool pendingCleanup = false;
};

enum class ProviderRequestTargetKind {
    Unknown,
    Frame,
    Position,
    Playback,
};

enum class ProviderSchedulerOperation {
    None,
    FlushQueuedFrameRequest,
};

struct ProviderSchedulerDiagnostic
{
    bool valid = false;
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    quint64 generation = 0;
    quint64 activeRequestId = 0;
    quint64 queuedRequestId = 0;
    ProviderRequestTargetKind targetKind = ProviderRequestTargetKind::Unknown;
    ProviderSchedulerOperation operation = ProviderSchedulerOperation::None;
};

enum class DisplayRequestOrigin {
    None,
    Initial,
    ExplicitSeek,
    Playback,
    MetadataBoundSelection,
    StopRestore,
};

enum class FailureScope {
    None,
    Generation,
    DisplayRequest,
};

struct TargetSpreadRoleTerminalState
{
    bool terminal = false;
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::NoRequest;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::NoRequest;
    FailureScope failureScope = FailureScope::None;
    QString diagnostic;
};

struct TargetSpreadTerminalState
{
    void clear()
    {
        sealed = false;
        generation = 0;
        requestId = 0;
        primary = {};
        secondary = {};
    }

    bool sealed = false;
    quint64 generation = 0;
    quint64 requestId = 0;
    TargetSpreadRoleTerminalState primary;
    TargetSpreadRoleTerminalState secondary;
};

struct TargetSpreadRoleWaitState
{
    bool providerWaiting = false;
    bool requestQueued = false;
    bool uploadPending = false;
    bool renderWaiting = false;
};

struct TargetSpreadWaitState
{
    TargetSpreadRoleWaitState primary;
    TargetSpreadRoleWaitState secondary;
    bool requiresSecondary = false;
};

inline ImageViewport::RequestReason projectWaitReason(TargetSpreadWaitState waitState)
{
    const auto anyRequired = [&waitState](auto member) {
        return waitState.primary.*member
            || (waitState.requiresSecondary && waitState.secondary.*member);
    };

    if (anyRequired(&TargetSpreadRoleWaitState::providerWaiting)) {
        return ImageViewport::RequestReason::ProviderWaiting;
    }
    if (anyRequired(&TargetSpreadRoleWaitState::requestQueued)) {
        return ImageViewport::RequestReason::RequestQueued;
    }
    if (anyRequired(&TargetSpreadRoleWaitState::uploadPending)) {
        return ImageViewport::RequestReason::UploadPending;
    }
    if (anyRequired(&TargetSpreadRoleWaitState::renderWaiting)) {
        return ImageViewport::RequestReason::RenderWaiting;
    }
    return ImageViewport::RequestReason::NoRequest;
}

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

struct ResolvedFrameIdentity
{
    int frame = -1;
    int position = -1;

    bool isValid() const { return frame >= 0; }
};

struct DisplayRequest
{
    DisplayRequestIdentity identity;
    DisplayRequestTarget target;
    ResolvedFrameIdentity resolvedFrame;
    ImageSequenceProviderRequestToken providerFrameToken;
    ImageViewportDemandRevisionToken demandRevision;
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

    bool isValid() const { return generation != 0 && requestId != 0 && payloadId != 0; }
};

struct PreparedPayload
{
    bool commitPending = false;
    quint64 generation = 0;
    quint64 requestId = 0;
    quint64 payloadId = 0;
    QImage image;
    QSizeF sourceLogicalSize;
    QSizeF payloadRasterSize;
    QSizeF sourceToPayloadScale;
    ImageViewport::PayloadQuality quality = ImageViewport::PayloadQuality::Unknown;
    ImageViewport::PayloadExactness exactness = ImageViewport::PayloadExactness::Unknown;
    ImageViewportDemandRevisionToken demandRevision;

    PreparedPayloadIdentity identity() const { return { generation, requestId, payloadId }; }
};

struct PresentationState
{
    ImageViewport::FitMode fitMode = ImageViewport::FitMode::Contain;
    ImageViewport::SpreadDirection spreadDirection = ImageViewport::SpreadDirection::LeftToRight;
    ImageViewport::BackgroundMode backgroundMode = ImageViewport::BackgroundMode::Transparent;
    ImageViewport::QualityPreference qualityPreference = ImageViewport::QualityPreference::Default;
    ImageViewport::ExactnessPreference exactnessPreference
        = ImageViewport::ExactnessPreference::Default;
    QColor backgroundColor = Qt::transparent;
    double manualZoom = 1.0;
    double pageGap = 0.0;
    int rotationDegrees = 0;
    QPointF contentPosition;
    bool smoothing = true;
    bool mipmap = false;
    bool mirrorHorizontally = false;
    bool mirrorVertically = false;
};

struct DisplayState
{
    struct RoleState
    {
        DisplayRequestSnapshot displayedRequest;
        QSizeF displayedImageSize;
        QImage displayedImage;
        PreparedPayload displayedPayload;
        PreparedPayload pendingRenderPayload;
    };

    DisplayState() = default;
    DisplayState(const DisplayState&) = delete;
    DisplayState& operator=(const DisplayState&) = delete;
    DisplayState(DisplayState&&) = delete;
    DisplayState& operator=(DisplayState&&) = delete;

    DisplayRequestSnapshot activeRequestSnapshot(quint64 sequenceGeneration,
        const DisplayRequest& activeRequest, int displayedPosition) const
    {
        DisplayRequestSnapshot snapshot = displayedRequest;
        snapshot.generation = sequenceGeneration;
        snapshot.request.target = activeRequest.target;
        snapshot.request.target.frame = activeRequest.target.frame;
        snapshot.request.target.position = displayedPosition;
        snapshot.request.resolvedFrame = activeRequest.resolvedFrame;
        snapshot.request.resolvedFrame.position = displayedPosition;
        return snapshot;
    }

    void commitDisplayedRequestSnapshot(
        quint64 sequenceGeneration, const DisplayRequest& activeRequest, quint64 preparedPayloadId)
    {
        const auto displayedTarget = displayedRequest.request.target;
        const auto displayedResolvedFrame = displayedRequest.request.resolvedFrame;
        displayedRequest.generation = sequenceGeneration;
        displayedRequest.request = activeRequest;
        displayedRequest.request.target = displayedTarget;
        displayedRequest.request.resolvedFrame = displayedResolvedFrame;
        displayedRequest.request.preparedPayloadId = preparedPayloadId;
    }

    void clearDisplayedDisplay()
    {
        displayedRequest = {};
        secondaryDisplayedRequest = {};
        displayedImageSize = {};
        displayedImage = {};
        secondaryDisplayedImageSize = {};
        secondaryDisplayedImage = {};
        displayedPayload = {};
        secondaryDisplayedPayload = {};
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

    void clearPendingRenderPayload()
    {
        pendingRenderPayload = {};
        secondaryPendingRenderPayload = {};
    }

    bool pendingRenderPayloadMatches(const PreparedPayloadIdentity& identity) const
    {
        const PreparedPayloadIdentity pendingIdentity = pendingRenderPayload.identity();
        return pendingRenderPayload.commitPending && identity.isValid()
            && identity.generation == pendingIdentity.generation
            && identity.requestId == pendingIdentity.requestId
            && identity.payloadId == pendingIdentity.payloadId;
    }

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
    std::array<RoleState, 2> roles;
    DisplayRequestSnapshot& displayedRequest = roles[0].displayedRequest;
    DisplayRequestSnapshot& secondaryDisplayedRequest = roles[1].displayedRequest;
    QSizeF& displayedImageSize = roles[0].displayedImageSize;
    QSizeF& secondaryDisplayedImageSize = roles[1].displayedImageSize;
    QImage& displayedImage = roles[0].displayedImage;
    QImage& secondaryDisplayedImage = roles[1].displayedImage;
    PreparedPayload& displayedPayload = roles[0].displayedPayload;
    PreparedPayload& secondaryDisplayedPayload = roles[1].displayedPayload;
    quint64 nextPreparedPayloadId = 0;
    PreparedPayload& pendingRenderPayload = roles[0].pendingRenderPayload;
    PreparedPayload& secondaryPendingRenderPayload = roles[1].pendingRenderPayload;
    DisplayRequestSnapshot renderFailureRetainedRequest;
    bool renderFailureRetainedDisplayValid = false;
    QSizeF renderFailureRetainedImageSize;
    QImage renderFailureRetainedImage;
    PresentationState displayedPresentation;
    quint64 displayedPresentationRevision = 0;
    quint64 revision = 0;
};

struct RequestState
{
    struct RoleState
    {
        ImageSequenceSource source;
        QPointer<ImageSequence> sequence;
        bool provider = false;
        DisplayRequest activeRequest;
        DisplayRequest latestNonPlaybackRequest;
    };

    RequestState() = default;
    RequestState(const RequestState&) = delete;
    RequestState& operator=(const RequestState&) = delete;
    RequestState(RequestState&&) = delete;
    RequestState& operator=(RequestState&&) = delete;

    void clearDisplayRequests()
    {
        nextRequestId = 0;
        activeRequest = {};
        secondaryActiveRequest = {};
        latestNonPlaybackRequest = {};
        secondaryLatestNonPlaybackRequest = {};
        playbackPosition = -1;
        playbackRole = ImageViewport::PageRole::Primary;
        playbackLoopIterationsCompleted = 0;
        targetSpreadTerminal.clear();
        lastAcceptedRenderFailure = {};
    }

    void beginDisplayRequest(DisplayRequestOrigin origin, bool rememberAsLatestNonPlayback)
    {
        targetSpreadTerminal.clear();
        lastAcceptedRenderFailure = {};
        activeRequest.identity.id = ++nextRequestId;
        activeRequest.identity.origin = origin;
        activeRequest.providerFrameToken = {};
        activeRequest.demandRevision = {};
        activeRequest.preparedPayloadId = 0;
        if (rememberAsLatestNonPlayback) {
            latestNonPlaybackRequest.identity = activeRequest.identity;
        }
    }

    void beginDisplayRequest(
        DisplayRequestOrigin origin, DisplayRequestTarget target, bool rememberAsLatestNonPlayback)
    {
        beginDisplayRequest(
            origin, target, { target.frame, target.position }, rememberAsLatestNonPlayback);
    }

    void beginDisplayRequest(DisplayRequestOrigin origin, DisplayRequestTarget target,
        ResolvedFrameIdentity resolvedFrame, bool rememberAsLatestNonPlayback)
    {
        beginDisplayRequest(origin, rememberAsLatestNonPlayback);
        activeRequest.target = target;
        activeRequest.resolvedFrame = resolvedFrame;
        if (rememberAsLatestNonPlayback) {
            latestNonPlaybackRequest.target = target;
            latestNonPlaybackRequest.resolvedFrame = resolvedFrame;
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

    bool activeRequestMatchesProviderFrameToken(ImageSequenceProviderRequestToken token) const
    {
        return token.isValid() && token == activeRequest.providerFrameToken;
    }

    bool activeRequestOwnsPreparedPayload(const PreparedPayloadIdentity& identity) const
    {
        return identity.isValid() && identity.generation == sequenceGeneration
            && identity.requestId == activeRequest.identity.id
            && identity.payloadId == activeRequest.preparedPayloadId;
    }

    std::array<RoleState, 2> roles;
    ImageSequenceSource& sequenceSource = roles[0].source;
    ImageSequenceSource& secondarySequenceSource = roles[1].source;
    QPointer<ImageSequence>& sequence = roles[0].sequence;
    QPointer<ImageSequence>& secondarySequence = roles[1].sequence;
    bool& secondarySequenceIsProvider = roles[1].provider;
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::NoRequest;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::NoRequest;
    ImageViewport::CommandReason commandReason = ImageViewport::CommandReason::NoCommand;
    ImageViewport::PlaybackPhase playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    bool looping = false;
    bool stopPlaybackWhenRequestReady = false;
    bool providerPlaybackStartPending = false;
    DisplayRequest& activeRequest = roles[0].activeRequest;
    DisplayRequest& secondaryActiveRequest = roles[1].activeRequest;
    int playbackPosition = -1;
    ImageViewport::PageRole playbackRole = ImageViewport::PageRole::Primary;
    DisplayRequest& latestNonPlaybackRequest = roles[0].latestNonPlaybackRequest;
    DisplayRequest& secondaryLatestNonPlaybackRequest = roles[1].latestNonPlaybackRequest;
    int playbackLoopIterationsCompleted = 0;
    quint64 sequenceGeneration = 0;
    quint64 nextRequestId = 0;
    TargetSpreadTerminalState targetSpreadTerminal;
    RenderFailureDiagnostic lastAcceptedRenderFailure;
    quint64 requestRevision = 0;
    quint64 commandRevision = 0;
    QString errorString;
    QString warningString;
};

struct ProviderGenerationState
{
    bool sessionActive = false;
    quint64 sessionSerial = 0;
    quint64 nextRequestToken = 0;
    ImageSequenceProviderRequestToken activeMetadataToken;
    ImageSequenceProviderRequestToken activeFrameToken;
    bool queuedFrameRequest = false;
    quint64 queuedFrameGeneration = 0;
    quint64 queuedFrameRequestId = 0;
    int queuedFrame = -1;
    int queuedPosition = -1;
    ResolvedFrameIdentity queuedResolvedFrame;
    bool queuedFrameFromPlayback = false;
    ProviderRequestTargetKind queuedFrameTargetKind = ProviderRequestTargetKind::Unknown;
    bool metadataReady = false;
    bool timedMetadata = false;
    bool timedPlaybackSupport = false;
    bool frameSeekSupport = false;
    bool positionSeekSupport = false;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
    QSizeF logicalSize;
    TimingIntervals timingIntervals;
};

}
