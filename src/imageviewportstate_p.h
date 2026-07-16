#pragma once

#include "imagesequencesource_p.h"
#include "renderfailurecause_p.h"
#include "timingintervals_p.h"
#include <ImageViewport/ImageViewport>

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include <array>
#include <memory>

namespace ImageViewportInternal {

struct RenderFailureDiagnostic
{
    bool valid = false;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    quint64 generation = 0;
    quint64 requestId = 0;
    quint64 preparedPayloadId = 0;
    RenderFailureCause cause = RenderFailureCause::None;
    quint64 renderAttempt = 0;
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
    bool targetPresentationRevision = false;
    bool adoptTargetPresentationRevision = false;
    quint64 commandRevisionValue = 0;
    bool scheduleUpdate = false;
    RenderFailureDiagnostic renderFailureDiagnostic;
};

enum class ProviderTransportOperation {
    None,
    Cancel,
    Close,
    Release,
    Destruction,
};

struct ProviderTransportDiagnostic
{
    bool valid = false;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ProviderTransportOperation operation = ProviderTransportOperation::None;
    bool metadataTokenValid = false;
    quint64 metadataTokenValue = 0;
    bool frameTokenValid = false;
    quint64 frameTokenValue = 0;
    bool queued = false;
    bool pendingCleanup = false;
    quint64 generation = 0;
    quint64 sessionSerial = 0;
    quint64 providerLeaseId = 0;
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
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
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
    ImageViewportRequestStatus status = ImageViewportRequestStatus::NoRequest;
    ImageViewportRequestReason reason = ImageViewportRequestReason::NoRequest;
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

inline ImageViewportRequestReason projectWaitReason(TargetSpreadWaitState waitState)
{
    const auto anyRequired = [&waitState](auto member) {
        return waitState.primary.*member
            || (waitState.requiresSecondary && waitState.secondary.*member);
    };

    if (anyRequired(&TargetSpreadRoleWaitState::providerWaiting)) {
        return ImageViewportRequestReason::ProviderWaiting;
    }
    if (anyRequired(&TargetSpreadRoleWaitState::requestQueued)) {
        return ImageViewportRequestReason::RequestQueued;
    }
    if (anyRequired(&TargetSpreadRoleWaitState::uploadPending)) {
        return ImageViewportRequestReason::UploadPending;
    }
    if (anyRequired(&TargetSpreadRoleWaitState::renderWaiting)) {
        return ImageViewportRequestReason::RenderWaiting;
    }
    return ImageViewportRequestReason::NoRequest;
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
    ImageViewportPayloadQuality quality = ImageViewportPayloadQuality::Unknown;
    ImageViewportPayloadExactness exactness = ImageViewportPayloadExactness::Unknown;
    ImageViewportDemandRevisionToken demandRevision;
    quint64 providerFrameLeaseId = 0;

    PreparedPayloadIdentity identity() const { return { generation, requestId, payloadId }; }
};

struct PresentationState
{
    ImageViewportFitMode fitMode = ImageViewportFitMode::Contain;
    ImageViewportSpreadDirection spreadDirection = ImageViewportSpreadDirection::LeftToRight;
    ImageViewportBackgroundMode backgroundMode = ImageViewportBackgroundMode::Transparent;
    ImageViewportQualityPreference qualityPreference = ImageViewportQualityPreference::Default;
    ImageViewportExactnessPreference exactnessPreference
        = ImageViewportExactnessPreference::Default;
    QColor backgroundColor = Qt::white;
    QColor checkerboardLightColor = Qt::white;
    QColor checkerboardDarkColor = QColor(220, 220, 220);
    double checkerboardCellSize = 8.0;
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
        bool retainedDisplayValid = false;
        DisplayRequestSnapshot retainedRequest;
        QSizeF retainedImageSize;
        QImage retainedImage;
    };

    DisplayState() = default;
    DisplayState(const DisplayState&) = delete;
    DisplayState& operator=(const DisplayState&) = delete;
    DisplayState(DisplayState&&) = delete;
    DisplayState& operator=(DisplayState&&) = delete;

    DisplayRequestSnapshot activeRequestSnapshot(quint64 sequenceGeneration,
        const DisplayRequest& activeRequest, int displayedPosition) const
    {
        DisplayRequestSnapshot snapshot = roles[0].displayedRequest;
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
        auto& displayedRequest = roles[0].displayedRequest;
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
        roles[0] = {};
        roles[1] = {};
    }

    void discardRetainedDisplay()
    {
        for (auto& role : roles) {
            role.displayedRequest = {};
            role.displayedImageSize = {};
            role.displayedImage = {};
            role.displayedPayload = {};
            role.retainedDisplayValid = false;
            role.retainedRequest = {};
            role.retainedImageSize = {};
            role.retainedImage = {};
        }
        status = ImageViewportDisplayStatus::Empty;
        displayedPresentation = {};
        displayedPresentationRevision = 0;
    }

    void beginPreparedPayloadIdentity(quint64 sequenceGeneration, DisplayRequest& activeRequest)
    {
        auto& pending = roles[0].pendingRenderPayload;
        pending.generation = sequenceGeneration;
        pending.requestId = activeRequest.identity.id;
        pending.payloadId = activeRequest.identity.id == 0 ? 0 : ++nextPreparedPayloadId;
        activeRequest.preparedPayloadId = pending.payloadId;
    }

    void commitPreparedPayloadIdentity(
        DisplayRequest& activeRequest, const PreparedPayload& preparedPayload)
    {
        roles[0].pendingRenderPayload = preparedPayload;
        activeRequest.preparedPayloadId = preparedPayload.payloadId;
        if (preparedPayload.payloadId > nextPreparedPayloadId) {
            nextPreparedPayloadId = preparedPayload.payloadId;
        }
    }

    void clearPendingRenderPayload()
    {
        roles[0].pendingRenderPayload = {};
        roles[1].pendingRenderPayload = {};
    }

    bool pendingRenderPayloadMatches(const PreparedPayloadIdentity& identity) const
    {
        const auto& pending = roles[0].pendingRenderPayload;
        const PreparedPayloadIdentity pendingIdentity = pending.identity();
        return pending.commitPending && identity.isValid()
            && identity.generation == pendingIdentity.generation
            && identity.requestId == pendingIdentity.requestId
            && identity.payloadId == pendingIdentity.payloadId;
    }

    bool hasReadyDisplay(bool hasDisplayableSequence) const
    {
        return hasDisplayableSequence
            && (status == ImageViewportDisplayStatus::Ready
                || status == ImageViewportDisplayStatus::Retained)
            && roles[0].displayedImageSize.isValid() && roles[0].displayedImageSize.width() > 0.0
            && roles[0].displayedImageSize.height() > 0.0;
    }

    void captureRenderFailureRetainedDisplay(bool hasDisplayableSequence)
    {
        if (!hasReadyDisplay(hasDisplayableSequence)) {
            clearRenderFailureRetainedDisplay();
            return;
        }

        for (auto& role : roles) {
            const bool displayed = role.displayedImageSize.isValid()
                && role.displayedImageSize.width() > 0.0 && role.displayedImageSize.height() > 0.0;
            role.retainedDisplayValid = displayed;
            role.retainedRequest = displayed ? role.displayedRequest : DisplayRequestSnapshot {};
            role.retainedImageSize = displayed ? role.displayedImageSize : QSizeF {};
            role.retainedImage = displayed ? role.displayedImage : QImage {};
        }
    }

    void clearRenderFailureRetainedDisplay()
    {
        for (auto& role : roles) {
            role.retainedDisplayValid = false;
            role.retainedRequest = {};
            role.retainedImageSize = {};
            role.retainedImage = {};
        }
    }

    ImageViewportDisplayStatus status = ImageViewportDisplayStatus::Empty;
    std::array<RoleState, 2> roles;
    quint64 nextPreparedPayloadId = 0;
    PresentationState displayedPresentation;
    quint64 displayedPresentationRevision = 0;
    quint64 revision = 0;
};

struct PlaybackState
{
    void resetRequestIdentity()
    {
        position = -1;
        role = ImageViewportPageRole::Primary;
        loopIterationsCompleted = 0;
    }

    ImageViewportPlaybackPhase phase = ImageViewportPlaybackPhase::Stopped;
    bool looping = false;
    bool stopWhenRequestReady = false;
    bool providerStartPending = false;
    int position = -1;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    int loopIterationsCompleted = 0;
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
        roles[0].activeRequest = {};
        roles[1].activeRequest = {};
        roles[0].latestNonPlaybackRequest = {};
        roles[1].latestNonPlaybackRequest = {};
        targetSpreadTerminal.clear();
        lastAcceptedRenderFailure = {};
    }

    void beginDisplayRequest(DisplayRequestOrigin origin, bool rememberAsLatestNonPlayback)
    {
        targetSpreadTerminal.clear();
        lastAcceptedRenderFailure = {};
        auto& activeRequest = roles[0].activeRequest;
        activeRequest.identity.id = ++nextRequestId;
        activeRequest.identity.origin = origin;
        activeRequest.providerFrameToken = {};
        activeRequest.demandRevision = {};
        activeRequest.preparedPayloadId = 0;
        if (rememberAsLatestNonPlayback) {
            roles[0].latestNonPlaybackRequest.identity = activeRequest.identity;
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
        roles[0].activeRequest.target = target;
        roles[0].activeRequest.resolvedFrame = resolvedFrame;
        if (rememberAsLatestNonPlayback) {
            roles[0].latestNonPlaybackRequest.target = target;
            roles[0].latestNonPlaybackRequest.resolvedFrame = resolvedFrame;
        }
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
        return token.isValid() && token == roles[0].activeRequest.providerFrameToken;
    }

    bool activeRequestOwnsPreparedPayload(const PreparedPayloadIdentity& identity) const
    {
        return identity.isValid() && identity.generation == sequenceGeneration
            && identity.requestId == roles[0].activeRequest.identity.id
            && identity.payloadId == roles[0].activeRequest.preparedPayloadId;
    }

    std::array<RoleState, 2> roles;
    ImageViewportRequestStatus status = ImageViewportRequestStatus::NoRequest;
    ImageViewportRequestReason reason = ImageViewportRequestReason::NoRequest;
    quint64 sequenceGeneration = 0;
    quint64 nextRequestId = 0;
    TargetSpreadTerminalState targetSpreadTerminal;
    RenderFailureDiagnostic lastAcceptedRenderFailure;
    quint64 requestRevision = 0;
    QString errorString;
    QString warningString;
};

struct ProviderSessionState
{
    bool sessionActive = false;
    quint64 sessionSerial = 0;
};

struct ProviderRequestState
{
    quint64 nextRequestToken = 0;
    ImageSequenceProviderRequestToken activeMetadataToken;
    ImageSequenceProviderRequestToken activeFrameToken;
    bool activeFrameRefinement = false;
    bool hasLastFrameDemand = false;
    ImageSequenceProviderDisplayDemand lastFrameDemand;
    bool queuedFrameRequest = false;
    quint64 queuedFrameGeneration = 0;
    quint64 queuedFrameRequestId = 0;
    int queuedFrame = -1;
    int queuedPosition = -1;
    ResolvedFrameIdentity queuedResolvedFrame;
    bool queuedFrameFromPlayback = false;
    ProviderRequestTargetKind queuedFrameTargetKind = ProviderRequestTargetKind::Unknown;
};

struct ProviderFactsState
{
    bool metadataReady = false;
    bool timedMetadata = false;
    bool timedPlaybackSupport = false;
    bool frameSeekSupport = false;
    bool positionSeekSupport = false;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
    QSizeF logicalSize;
    TimingIntervals timingIntervals;
};

struct ProviderRoleState
{
    ProviderSessionState session;
    ProviderRequestState requests;
    ProviderFactsState facts;
};

}
