/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imagesequencesource_p.h"
#include "imageviewporttoken_p.h"
#include "publicdiagnostic_p.h"
#include "renderfailurecause_p.h"
#include "timingintervals_p.h"
#include <ImageViewport/imagesequenceprovider.h>
#include <ImageViewport/imageviewportstate.h>

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include <array>
#include <memory>
#include <optional>

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

inline ImageSequenceProviderRequestKind providerRequestKind(ProviderRequestTargetKind kind)
{
    switch (kind) {
    case ProviderRequestTargetKind::Position:
        return ImageSequenceProviderRequestKind::Position;
    case ProviderRequestTargetKind::Playback:
        return ImageSequenceProviderRequestKind::Playback;
    case ProviderRequestTargetKind::Unknown:
    case ProviderRequestTargetKind::Frame:
        return ImageSequenceProviderRequestKind::Frame;
    }
    return ImageSequenceProviderRequestKind::Frame;
}

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

struct TargetSpreadRoleTerminalState
{
    bool terminal = false;
    ImageViewportRequestStatus status = ImageViewportRequestStatus::NoRequest;
    ImageViewportRequestReason reason = ImageViewportRequestReason::NoRequest;
    PublicDiagnosticText diagnostic;
    bool providerFailureAvailable = false;
    ImageSequenceProviderFailureCause providerCause
        = ImageSequenceProviderFailureCause::Unavailable;
    ImageSequenceProviderFailureReference providerReference;
    quint64 providerFailureLeaseId = 0;
};

struct GenerationTerminalState
{
    void clear()
    {
        sealed = false;
        generation = 0;
        primary = {};
        secondary = {};
    }

    bool sealed = false;
    quint64 generation = 0;
    TargetSpreadRoleTerminalState primary;
    TargetSpreadRoleTerminalState secondary;
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
    ImageViewportDemandRevisionToken demandRevision;
    quint64 preparedPayloadId = 0;
};

struct DisplayRequestSnapshot
{
    quint64 generation = 0;
    DisplayRequest request;
};

struct TargetSpreadIdentity
{
    quint64 generation = 0;
    quint64 requestId = 0;

    bool isValid() const { return generation != 0 && requestId != 0; }
};

struct RenderPresentationIdentity
{
    quint64 revision = 0;

    bool isValid() const { return revision != 0; }
};

struct PreparedPayloadIdentity
{
    quint64 generation = 0;
    quint64 payloadId = 0;

    bool isValid() const { return generation != 0 && payloadId != 0; }
};

struct PreparedPayload
{
    bool commitPending = false;
    quint64 generation = 0;
    quint64 payloadId = 0;
    QImage image;
    QSizeF sourceLogicalSize;
    QSizeF payloadRasterSize;
    QSizeF sourceToPayloadScale;
    qint64 payloadByteSize = 0;
    ImageViewportPayloadQuality quality = ImageViewportPayloadQuality::Unknown;
    ImageViewportPayloadExactness exactness = ImageViewportPayloadExactness::Unknown;
    bool hasAlpha = false;
    ImageFrame::OrientationPolicy orientationPolicy = ImageFrame::OrientationPolicy::Identity;
    QString formatIdentifier;
    bool roleValid = false;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ResolvedFrameIdentity resolvedFrame;
    int frameDuration = -1;
    ImageViewportDemandRevisionToken demandRevision;
    quint64 providerFrameLeaseId = 0;

    PreparedPayloadIdentity identity() const { return { generation, payloadId }; }
    bool hasPresentableContent() const
    {
        return identity().isValid() && !image.isNull() && sourceLogicalSize.isValid()
            && sourceLogicalSize.width() > 0.0 && sourceLogicalSize.height() > 0.0;
    }
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
    double preferredManualZoom = 1.0;
    double pageGap = 0.0;
    int rotationDegrees = 0;
    QPointF contentPosition;
    bool smoothing = true;
    bool mipmap = false;
    bool mirrorHorizontally = false;
    bool mirrorVertically = false;
};

struct PayloadAllocationState
{
    quint64 nextGeneration = 0;
    quint64 generation = 0;
    std::array<qint64, 2> roleBudgets { -1, -1 };
};

struct RenderQualityFallbackState
{
    bool activeFor(quint64 acceptedGeneration, const PresentationState& presentation) const
    {
        return generation != 0 && generation == acceptedGeneration
            && ((smoothingUnavailable && presentation.smoothing)
                || (mipmapUnavailable && presentation.mipmap));
    }

    void assign(quint64 ownerGeneration, bool smoothingFallback, bool mipmapFallback)
    {
        if (ownerGeneration == 0 || (!smoothingFallback && !mipmapFallback)) {
            clear();
            return;
        }
        generation = ownerGeneration;
        smoothingUnavailable = smoothingFallback;
        mipmapUnavailable = mipmapFallback;
    }

    void reconcile(const PresentationState& presentation)
    {
        smoothingUnavailable = smoothingUnavailable && presentation.smoothing;
        mipmapUnavailable = mipmapUnavailable && presentation.mipmap;
        if (!smoothingUnavailable && !mipmapUnavailable) {
            clear();
        }
    }

    void clear()
    {
        generation = 0;
        smoothingUnavailable = false;
        mipmapUnavailable = false;
    }

    quint64 generation = 0;
    bool smoothingUnavailable = false;
    bool mipmapUnavailable = false;
};

struct DisplayState
{
    struct RoleState
    {
        DisplayRequestSnapshot displayedRequest;
        PreparedPayload displayedPayload;
        PreparedPayload pendingRenderPayload;
    };

    DisplayState() = default;
    DisplayState(const DisplayState&) = default;
    DisplayState& operator=(const DisplayState&) = default;
    DisplayState(DisplayState&&) noexcept = default;
    DisplayState& operator=(DisplayState&&) noexcept = default;
    ~DisplayState() = default;

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
        renderQualityFallback.clear();
    }

    void discardRetainedDisplay()
    {
        for (auto& role : roles) {
            role.displayedRequest = {};
            role.displayedPayload = {};
        }
        status = ImageViewportDisplayStatus::Empty;
        displayedPresentation = {};
        displayedPresentationRevision = 0;
        renderQualityFallback.clear();
    }

    void beginPreparedPayloadIdentity(quint64 sequenceGeneration, DisplayRequest& activeRequest)
    {
        auto& pending = roles[0].pendingRenderPayload;
        pending.generation = sequenceGeneration;
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

    bool pendingRenderPayloadMatches(PreparedPayloadIdentity identity) const
    {
        const auto& pending = roles[0].pendingRenderPayload;
        const PreparedPayloadIdentity pendingIdentity = pending.identity();
        return pending.commitPending && identity.isValid()
            && identity.generation == pendingIdentity.generation
            && identity.payloadId == pendingIdentity.payloadId;
    }

    bool hasReadyDisplay(bool hasDisplayableSequence) const
    {
        return hasDisplayableSequence
            && (status == ImageViewportDisplayStatus::Ready
                || status == ImageViewportDisplayStatus::Retained)
            && roles[0].displayedPayload.hasPresentableContent();
    }

    bool hasActiveRenderQualityFallback(
        quint64 acceptedGeneration, const PresentationState& presentation) const
    {
        return status != ImageViewportDisplayStatus::Empty
            && roles[0].displayedRequest.generation == acceptedGeneration
            && roles[0].displayedPayload.hasPresentableContent()
            && renderQualityFallback.activeFor(acceptedGeneration, presentation);
    }

    ImageViewportDisplayStatus status = ImageViewportDisplayStatus::Empty;
    std::array<RoleState, 2> roles;
    quint64 nextPreparedPayloadId = 0;
    PayloadAllocationState payloadAllocation;
    RenderQualityFallbackState renderQualityFallback;
    PresentationState displayedPresentation;
    quint64 displayedPresentationRevision = 0;
    quint64 revision = 0;
};

enum class AuthoredAutoplayArbitrationState {
    Pending,
    Resolved,
    Suppressed,
};

struct RolePlaybackState
{
    void resetRequestIdentity()
    {
        position = -1;
        loopIterationsCompleted = 0;
        activeScheduleIdentity = 0;
        authoredAutoplayArbitration = AuthoredAutoplayArbitrationState::Pending;
    }

    ImageViewportPlaybackPhase phase = ImageViewportPlaybackPhase::Stopped;
    bool stopWhenRequestReady = false;
    bool providerStartPending = false;
    int position = -1;
    int loopIterationsCompleted = 0;
    quint64 nextScheduleIdentity = 0;
    quint64 activeScheduleIdentity = 0;
    AuthoredAutoplayArbitrationState authoredAutoplayArbitration
        = AuthoredAutoplayArbitrationState::Pending;
};

struct PlaybackState
{
    RolePlaybackState& forRole(ImageViewportPageRole role)
    {
        return roles[role == ImageViewportPageRole::Secondary ? 1U : 0U];
    }

    const RolePlaybackState& forRole(ImageViewportPageRole role) const
    {
        return roles[role == ImageViewportPageRole::Secondary ? 1U : 0U];
    }

    void resetRequestIdentity()
    {
        for (auto& role : roles) {
            role.resetRequestIdentity();
        }
    }

    std::array<RolePlaybackState, 2> roles;
    bool looping = false;
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
    RequestState(const RequestState&) = default;
    RequestState& operator=(const RequestState&) = default;
    RequestState(RequestState&&) noexcept = default;
    RequestState& operator=(RequestState&&) noexcept = default;
    ~RequestState() = default;

    void clearDisplayRequests()
    {
        nextRequestId = 0;
        roles[0].activeRequest = {};
        roles[1].activeRequest = {};
        roles[0].latestNonPlaybackRequest = {};
        roles[1].latestNonPlaybackRequest = {};
        generationTerminal.clear();
        targetSpreadTerminal.clear();
        lastAcceptedRenderFailure = {};
    }

    void beginDisplayRequest(DisplayRequestOrigin origin, bool rememberAsLatestNonPlayback)
    {
        targetSpreadTerminal.clear();
        lastAcceptedRenderFailure = {};
        auto& activeRequest = roles[0].activeRequest;
        activeRequest.identity = { ++nextRequestId, origin };
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

    void beginRoleDisplayRequest(ImageViewportPageRole role, DisplayRequestOrigin origin,
        DisplayRequestTarget target, ResolvedFrameIdentity resolvedFrame,
        bool rememberAsLatestNonPlayback)
    {
        const DisplayRequest previousPrimary = roles[0].activeRequest;
        if (role == ImageViewportPageRole::Primary) {
            beginDisplayRequest(origin, target, resolvedFrame, rememberAsLatestNonPlayback);
            roles[1].activeRequest.identity = roles[0].activeRequest.identity;
            return;
        }

        beginDisplayRequest(origin, previousPrimary.target, previousPrimary.resolvedFrame, false);
        const DisplayRequestIdentity spreadIdentity = roles[0].activeRequest.identity;
        roles[0].activeRequest = previousPrimary;
        roles[0].activeRequest.identity = spreadIdentity;
        auto& secondary = roles[1].activeRequest;
        secondary.identity = spreadIdentity;
        secondary.target = target;
        secondary.resolvedFrame = resolvedFrame;
        secondary.demandRevision = {};
        secondary.preparedPayloadId = 0;
        if (rememberAsLatestNonPlayback) {
            roles[1].latestNonPlaybackRequest = secondary;
        }
    }

    bool clearError()
    {
        if (errorString.isEmpty()) {
            return false;
        }

        errorString.clear();
        return true;
    }

    bool activeRequestOwnsPreparedPayload(
        ImageViewportPageRole role, PreparedPayloadIdentity identity) const
    {
        const std::size_t index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
        return identity.isValid() && identity.generation == sequenceGeneration
            && identity.payloadId == roles[index].activeRequest.preparedPayloadId;
    }

    TargetSpreadIdentity activeTargetSpreadIdentity() const
    {
        return { sequenceGeneration, roles[0].activeRequest.identity.id };
    }

    std::array<RoleState, 2> roles;
    ImageViewportRequestStatus status = ImageViewportRequestStatus::NoRequest;
    ImageViewportRequestReason reason = ImageViewportRequestReason::NoRequest;
    quint64 sequenceGeneration = 0;
    quint64 nextRequestId = 0;
    GenerationTerminalState generationTerminal;
    TargetSpreadTerminalState targetSpreadTerminal;
    RenderFailureDiagnostic lastAcceptedRenderFailure;
    quint64 requestRevision = 0;
    PublicDiagnosticText errorString;
};

struct ProviderSessionState
{
    bool sessionActive = false;
    quint64 sessionSerial = 0;
};

enum class ProviderRequestOwnership {
    Metadata,
    DisplayRequest,
    Refinement,
};

struct ProviderRequestRecord
{
    ImageSequenceProviderRequestToken token;
    ImageSequenceProviderRequestKind kind = ImageSequenceProviderRequestKind::Metadata;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    quint64 generation = 0;
    quint64 requestId = 0;
    ProviderRequestOwnership ownership = ProviderRequestOwnership::Metadata;
    DisplayRequestTarget target;
    ResolvedFrameIdentity resolvedFrame;
    std::optional<ImageSequenceProviderDisplayDemand> demand;

    bool isMetadata() const { return kind == ImageSequenceProviderRequestKind::Metadata; }
    bool isFrameWork() const
    {
        return kind == ImageSequenceProviderRequestKind::Frame
            || kind == ImageSequenceProviderRequestKind::Position
            || kind == ImageSequenceProviderRequestKind::Playback;
    }
    bool isRefinement() const { return ownership == ProviderRequestOwnership::Refinement; }
};

struct QueuedProviderFrameRequest
{
    quint64 generation = 0;
    quint64 requestId = 0;
    DisplayRequestTarget target;
    ResolvedFrameIdentity resolvedFrame;
    bool fromPlayback = false;
};

enum class ProviderRequestTokenAdmissionKind {
    Active,
    Retired,
    Mismatch,
};

struct ProviderRequestTokenAdmission
{
    ProviderRequestTokenAdmissionKind kind = ProviderRequestTokenAdmissionKind::Retired;
    const ProviderRequestRecord* record = nullptr;
};

struct ProviderRequestLedger
{
    const ProviderRequestRecord* find(ImageSequenceProviderRequestToken token) const
    {
        for (const auto& record : active) {
            if (token.isValid() && record.token == token) {
                return &record;
            }
        }
        return nullptr;
    }

    ProviderRequestRecord* find(ImageSequenceProviderRequestToken token)
    {
        for (auto& record : active) {
            if (token.isValid() && record.token == token) {
                return &record;
            }
        }
        return nullptr;
    }

    ProviderRequestTokenAdmission admit(ImageSequenceProviderRequestToken token) const
    {
        if (const auto* record = find(token)) {
            return { ProviderRequestTokenAdmissionKind::Active, record };
        }
        const quint64 value = ProviderRequestTokenPrivateAccess::value(token);
        if (active.isEmpty() || (token.isValid() && value <= nextRequestToken)) {
            return { ProviderRequestTokenAdmissionKind::Retired, nullptr };
        }
        return { ProviderRequestTokenAdmissionKind::Mismatch, nullptr };
    }

    const ProviderRequestRecord* metadataRequest() const
    {
        for (const auto& record : active) {
            if (record.isMetadata()) {
                return &record;
            }
        }
        return nullptr;
    }

    const ProviderRequestRecord* frameRequest() const
    {
        for (const auto& record : active) {
            if (record.isFrameWork()) {
                return &record;
            }
        }
        return nullptr;
    }

    ImageSequenceProviderRequestToken metadataToken() const
    {
        const auto* record = metadataRequest();
        return record ? record->token : ImageSequenceProviderRequestToken {};
    }

    ImageSequenceProviderRequestToken frameToken() const
    {
        const auto* record = frameRequest();
        return record ? record->token : ImageSequenceProviderRequestToken {};
    }

    void activate(ProviderRequestRecord record)
    {
        if (!record.token.isValid() || find(record.token)
            || (record.isMetadata() ? metadataRequest() != nullptr : frameRequest() != nullptr)) {
            qFatal("ImageViewport provider request ledger invariant violated");
        }
        active.append(std::move(record));
    }

    std::optional<ProviderRequestRecord> retire(ImageSequenceProviderRequestToken token)
    {
        for (qsizetype index = 0; index < active.size(); ++index) {
            if (token.isValid() && active.at(index).token == token) {
                return active.takeAt(index);
            }
        }
        return std::nullopt;
    }

    std::optional<ProviderRequestRecord> retireMetadata() { return retire(metadataToken()); }

    std::optional<ProviderRequestRecord> retireFrame() { return retire(frameToken()); }

    void queue(QueuedProviderFrameRequest request) { queuedFrame = request; }

    void clearQueue() { queuedFrame.reset(); }

    void clearWork()
    {
        active.clear();
        clearQueue();
        lastIssuedFrameDemand.reset();
    }

    void resetSession()
    {
        clearWork();
        nextRequestToken = 0;
    }

    quint64 nextRequestToken = 0;
    QVector<ProviderRequestRecord> active;
    std::optional<ImageSequenceProviderDisplayDemand> lastIssuedFrameDemand;
    std::optional<QueuedProviderFrameRequest> queuedFrame;
};

struct ProviderFactsState
{
    bool metadataReady = false;
    bool timedMetadata = false;
    bool timedPlaybackSupport = false;
    bool frameSeekSupport = false;
    bool positionSeekSupport = false;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
    bool authoredAnimationFactsAvailable = false;
    QSizeF logicalSize;
    TimingIntervals timingIntervals;
};

struct ProviderRoleState
{
    ProviderSessionState session;
    ProviderRequestLedger requests;
    ProviderFactsState facts;
};

}
