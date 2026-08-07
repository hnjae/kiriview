// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportintegrationruntime.h"

#include "async/imagecallback.h"
#include "rendering/imageviewportsequenceprovider.h"

#include <ImageViewport/imagesequence.h>

#include <QVariant>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
bool finitePoint(QPointF point) { return std::isfinite(point.x()) && std::isfinite(point.y()); }

bool accepted(ImageViewportCommandResult result)
{
    return result.outcome() == ImageViewportCommandOutcome::Accepted;
}

qreal normalizedScrollPosition(bool pannable, qreal position, qreal maximum)
{
    if (!pannable || !std::isfinite(position) || !std::isfinite(maximum) || maximum <= 0.0) {
        return 0.0;
    }
    return std::clamp(position / maximum, 0.0, 1.0);
}

qreal normalizedScrollPageSize(bool pannable, qreal contentSize, qreal maximum)
{
    if (!pannable || !std::isfinite(contentSize) || !std::isfinite(maximum) || contentSize <= 0.0) {
        return 1.0;
    }
    return std::clamp((contentSize - maximum) / contentSize, 0.0, 1.0);
}

QSize logicalSize(const ImageViewportRoleSnapshot& role)
{
    QSizeF size;
    if (role.metadata().available()) {
        size = role.metadata().sourceLogicalSize();
    } else if (role.display().sourceLogicalSize().isValid()) {
        size = role.display().sourceLogicalSize();
    } else {
        size = role.request().sourceLogicalSize();
    }
    return size.toSize();
}

bool completeAuthoritativeDisplayAvailable(const ImageViewportStateSnapshot& snapshot)
{
    const ImageViewportRoleSet displayedRoles = snapshot.display().displayedRoleSet();
    if (!displayedRoles.primary() && !displayedRoles.secondary()) {
        return false;
    }
    if (snapshot.display().phase() == ImageViewportDisplayPhase::PreviousActive) {
        return true;
    }
    return snapshot.display().phase() == ImageViewportDisplayPhase::CommittedActive
        && snapshot.request().status() == ImageViewportRequestStatus::Ready;
}

PresentationTargetTransitionPolicy transitionPolicy(
    const kiriview::ImageViewportIntegrationTarget& target)
{
    PresentationTargetTransitionPolicy policy;
    policy.setContentPositionTransition(target.anchorAtEnd
            ? PresentationTargetTransitionPolicy::ContentPositionTransition::AnchorEnd
            : PresentationTargetTransitionPolicy::ContentPositionTransition::AnchorStart);
    policy.setRotationTransition(PresentationTargetTransitionPolicy::RotationTransition::Reset);
    policy.setMirrorTransition(PresentationTargetTransitionPolicy::MirrorTransition::Reset);
    policy.setSpreadDirectionTransition(
        PresentationTargetTransitionPolicy::SpreadDirectionTransition::SetExplicit);
    policy.setSpreadDirection(target.rightToLeft ? ImageViewportSpreadDirection::RightToLeft
                                                 : ImageViewportSpreadDirection::LeftToRight);
    policy.setDisplayTransition(
        PresentationTargetTransitionPolicy::DisplayTransition::RetainPrevious);
    policy.setFailureTransition(
        PresentationTargetTransitionPolicy::FailureTransition::KeepFailedTarget);

    switch (target.transitionIntent) {
    case kiriview::ImageViewportTargetTransitionIntent::SameNavigationScope:
        policy.setZoomTransition(PresentationTargetTransitionPolicy::ZoomTransition::Preserve);
        return policy;
    case kiriview::ImageViewportTargetTransitionIntent::OutsideNavigationScope:
        policy.setZoomTransition(
            PresentationTargetTransitionPolicy::ZoomTransition::ResetToContain);
        return policy;
    case kiriview::ImageViewportTargetTransitionIntent::PresentationShapeChange:
        policy.setDisplayTransition(
            PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
        policy.setZoomTransition(PresentationTargetTransitionPolicy::ZoomTransition::Preserve);
        policy.setContentPositionTransition(
            PresentationTargetTransitionPolicy::ContentPositionTransition::Clamp);
        policy.setRotationTransition(
            PresentationTargetTransitionPolicy::RotationTransition::Preserve);
        policy.setMirrorTransition(PresentationTargetTransitionPolicy::MirrorTransition::Preserve);
        return policy;
    }
    return policy;
}

kiriview::ImageDocumentStatus documentStatus(ImageViewportRequestStatus status)
{
    switch (status) {
    case ImageViewportRequestStatus::NoRequest:
        return kiriview::ImageDocumentStatus::Null;
    case ImageViewportRequestStatus::Loading:
        return kiriview::ImageDocumentStatus::Loading;
    case ImageViewportRequestStatus::Ready:
        return kiriview::ImageDocumentStatus::Ready;
    case ImageViewportRequestStatus::Unsupported:
    case ImageViewportRequestStatus::Error:
        return kiriview::ImageDocumentStatus::Error;
    }
    return kiriview::ImageDocumentStatus::Error;
}
}

namespace kiriview {
struct ImageViewportIntegrationRuntime::TargetRecord
{
    ImageViewportIntegrationTarget target;
    std::shared_ptr<ImageViewportProviderResource> primaryResource;
    std::shared_ptr<ImageViewportProviderResource> secondaryResource;
    std::unique_ptr<ImageViewportSequenceProvider> primaryAdapter;
    std::unique_ptr<ImageViewportSequenceProvider> secondaryAdapter;
    std::unique_ptr<ImageSequenceFactoryResult> primaryFactoryResult;
    std::unique_ptr<ImageSequenceFactoryResult> secondaryFactoryResult;
    ImageViewportPresentationTargetGenerationToken acceptedGeneration;
    quint64 targetRevision = 0;
    quint64 attachmentRevision = 0;
};

struct ImageViewportIntegrationRuntime::SubmissionStamp
{
    quint64 targetRevision = 0;
    quint64 attachmentRevision = 0;
    QPointer<ImageViewport> viewport;
};

enum class ImageViewportIntegrationRuntime::SubmissionOutcome {
    Installed,
    FailedCurrent,
    Superseded,
};

bool ImageViewportIntegrationTarget::isValid() const
{
    const bool hasSecondaryUrl = !secondaryUrl.isEmpty();
    const bool hasSecondaryResource = bool(secondaryResource);
    const bool hasSecondarySession = secondarySessionId != 0;
    return sourceGeneration != 0 && !selectedSourceUrl.isEmpty() && bool(primaryResource)
        && hasSecondaryUrl == hasSecondaryResource && hasSecondaryUrl == hasSecondarySession;
}

ImageViewportIntegrationRuntime::ImageViewportIntegrationRuntime(Callbacks callbacks)
    : m_callbacks(std::move(callbacks))
{
}

ImageViewportIntegrationRuntime::~ImageViewportIntegrationRuntime()
{
    if (m_viewport != nullptr) {
        QObject::disconnect(m_stateConnection);
        QObject::disconnect(m_destroyedConnection);
        ImageViewport* viewport = m_viewport.data();
        m_viewport = nullptr;
        m_activeRecord = nullptr;
        std::vector<std::unique_ptr<TargetRecord>> retiredRecords = std::move(m_records);
        m_records.clear();
        viewport->clear();
        retiredRecords.clear();
    }
}

void ImageViewportIntegrationRuntime::attach(ImageViewport* viewport)
{
    if (m_viewport == viewport) {
        return;
    }
    const quint64 attachmentRevision = beginAttachmentRevision();
    const QPointer<ImageViewport> requestedViewport(viewport);
    if (m_viewport != nullptr) {
        invalidateAttachment(m_viewport.data());
    }
    if (m_attachmentRevision != attachmentRevision || requestedViewport == nullptr) {
        return;
    }

    m_viewport = requestedViewport;
    m_stateConnection = connect(requestedViewport, &ImageViewport::stateChanged, this,
        &ImageViewportIntegrationRuntime::handleStateChanged);
    m_destroyedConnection = connect(
        requestedViewport, &QObject::destroyed, this, [this, viewport, attachmentRevision]() {
            if (m_attachmentRevision != attachmentRevision || sender() != viewport) {
                return;
            }
            const quint64 destroyedRevision = beginAttachmentRevision();
            QObject::disconnect(m_stateConnection);
            QObject::disconnect(m_destroyedConnection);
            m_viewport = nullptr;
            m_activeRecord = nullptr;
            std::vector<std::unique_ptr<TargetRecord>> retiredRecords = std::move(m_records);
            m_records.clear();
            retiredRecords.clear();
            if (m_attachmentRevision != destroyedRevision || m_viewport != nullptr) {
                return;
            }
            ImageViewportIntegrationProjection projection = m_projection;
            projection.correlated = false;
            projection.completeAuthoritativeDisplayAvailable = false;
            publishProjection(std::move(projection));
        });
    if (m_target.has_value()) {
        static_cast<void>(submitCurrentTarget());
    } else {
        handleStateChanged();
    }
}

void ImageViewportIntegrationRuntime::detach(ImageViewport* viewport)
{
    if (viewport != nullptr && m_viewport == viewport) {
        static_cast<void>(beginAttachmentRevision());
        invalidateAttachment(viewport);
    }
}

bool ImageViewportIntegrationRuntime::submitTarget(ImageViewportIntegrationTarget target)
{
    if (!target.isValid()) {
        return false;
    }
    const quint64 targetRevision = beginTargetRevision();
    const quint64 attachmentRevision = m_attachmentRevision;
    std::optional<ImageViewportIntegrationTarget> retiredTarget;
    retiredTarget.swap(m_target);
    m_target.emplace(std::move(target));
    m_activeRecord = nullptr;
    retiredTarget.reset();
    if (m_targetRevision != targetRevision || m_attachmentRevision != attachmentRevision) {
        return true;
    }
    if (m_viewport == nullptr) {
        return true;
    }
    return submitCurrentTarget() != SubmissionOutcome::FailedCurrent;
}

bool ImageViewportIntegrationRuntime::resolvePrimaryTargetUrl(
    quint64 sourceGeneration, const QUrl& resolvedPrimaryUrl)
{
    if (sourceGeneration == 0 || resolvedPrimaryUrl.isEmpty() || !m_target.has_value()
        || m_target->sourceGeneration != sourceGeneration
        || !m_target->resolvedPrimaryUrl.isEmpty()) {
        return false;
    }
    if (m_activeRecord != nullptr) {
        if (!containsRecord(m_activeRecord) || m_activeRecord->targetRevision != m_targetRevision
            || m_activeRecord->attachmentRevision != m_attachmentRevision
            || m_activeRecord->target.sourceGeneration != sourceGeneration) {
            return false;
        }
    }

    m_target->resolvedPrimaryUrl = resolvedPrimaryUrl;
    if (m_activeRecord != nullptr) {
        m_activeRecord->target.resolvedPrimaryUrl = resolvedPrimaryUrl;
    }
    handleStateChanged();
    return true;
}

void ImageViewportIntegrationRuntime::clearTarget()
{
    const quint64 targetRevision = beginTargetRevision();
    const quint64 attachmentRevision = m_attachmentRevision;
    const QPointer<ImageViewport> viewport = m_viewport;
    std::optional<ImageViewportIntegrationTarget> retiredTarget;
    retiredTarget.swap(m_target);
    m_activeRecord = nullptr;
    retiredTarget.reset();
    if (m_targetRevision != targetRevision || m_attachmentRevision != attachmentRevision
        || m_viewport.data() != viewport.data()) {
        return;
    }
    if (viewport != nullptr) {
        viewport->clear();
    }
    if (m_targetRevision != targetRevision || m_attachmentRevision != attachmentRevision
        || m_viewport.data() != viewport.data()) {
        return;
    }
    std::vector<std::unique_ptr<TargetRecord>> retiredRecords = std::move(m_records);
    m_records.clear();
    retiredRecords.clear();
    if (m_targetRevision != targetRevision || m_attachmentRevision != attachmentRevision
        || m_viewport.data() != viewport.data()) {
        return;
    }
    publishProjection({});
}

void ImageViewportIntegrationRuntime::stopPlayback()
{
    const quint64 targetRevision = m_targetRevision;
    const quint64 attachmentRevision = m_attachmentRevision;
    const QPointer<ImageViewport> viewport = m_viewport;
    if (viewport == nullptr) {
        return;
    }
    viewport->stop(ImageViewportPageRole::Primary);
    if (m_targetRevision != targetRevision || m_attachmentRevision != attachmentRevision
        || m_viewport.data() != viewport.data()) {
        return;
    }
    viewport->stop(ImageViewportPageRole::Secondary);
}

const ImageViewportIntegrationProjection& ImageViewportIntegrationRuntime::projection() const
{
    return m_projection;
}

bool ImageViewportIntegrationRuntime::hasAuthoritativeDisplay() const
{
    if (m_viewport == nullptr) {
        return false;
    }
    return completeAuthoritativeDisplayAvailable(m_viewport->state());
}

std::optional<StaticDisplayImagePayload> ImageViewportIntegrationRuntime::displayedImage(
    ImageViewportPageRole role) const
{
    if (m_viewport == nullptr) {
        return std::nullopt;
    }
    const ImageViewportStateSnapshot snapshot = m_viewport->state();
    if (snapshot.display().displayedPresentationTargetGeneration()
        != m_projection.displayedTargetGeneration) {
        return std::nullopt;
    }
    const ImageViewportRoleSet displayedRoles = snapshot.display().displayedRoleSet();
    if ((role == ImageViewportPageRole::Primary && !displayedRoles.primary())
        || (role == ImageViewportPageRole::Secondary && !displayedRoles.secondary())) {
        return std::nullopt;
    }

    const TargetRecord* displayed
        = recordForGeneration(snapshot.display().displayedPresentationTargetGeneration());
    if (displayed == nullptr) {
        return std::nullopt;
    }
    const std::unique_ptr<ImageViewportSequenceProvider>& adapter
        = role == ImageViewportPageRole::Secondary ? displayed->secondaryAdapter
                                                   : displayed->primaryAdapter;
    const ImageViewportDemandRevisionToken demandRevision = role == ImageViewportPageRole::Secondary
        ? snapshot.secondary().display().demandRevision()
        : snapshot.primary().display().demandRevision();
    return adapter == nullptr ? std::nullopt : adapter->currentStillDisplayImage(demandRevision);
}

quint64 ImageViewportIntegrationRuntime::beginTargetRevision()
{
    if (m_targetRevision == std::numeric_limits<quint64>::max()) {
        qFatal("Image-viewport target revision exhausted");
    }
    return ++m_targetRevision;
}

quint64 ImageViewportIntegrationRuntime::beginAttachmentRevision()
{
    if (m_attachmentRevision == std::numeric_limits<quint64>::max()) {
        qFatal("Image-viewport attachment revision exhausted");
    }
    return ++m_attachmentRevision;
}

bool ImageViewportIntegrationRuntime::submissionIsCurrent(const SubmissionStamp& stamp) const
{
    return m_target.has_value() && stamp.targetRevision == m_targetRevision
        && stamp.attachmentRevision == m_attachmentRevision && stamp.viewport != nullptr
        && m_viewport.data() == stamp.viewport.data();
}

bool ImageViewportIntegrationRuntime::containsRecord(const TargetRecord* record) const
{
    return record != nullptr
        && std::ranges::any_of(m_records, [record](const std::unique_ptr<TargetRecord>& candidate) {
               return candidate.get() == record;
           });
}

void ImageViewportIntegrationRuntime::retireRecord(TargetRecord* record)
{
    const auto found
        = std::ranges::find_if(m_records, [record](const std::unique_ptr<TargetRecord>& candidate) {
              return candidate.get() == record;
          });
    if (found == m_records.end()) {
        return;
    }
    if (m_activeRecord == record) {
        m_activeRecord = nullptr;
    }
    std::unique_ptr<TargetRecord> retired = std::move(*found);
    m_records.erase(found);
    retired.reset();
}

ImageViewportIntegrationRuntime::SubmissionOutcome
ImageViewportIntegrationRuntime::submitCurrentTarget()
{
    if (m_viewport == nullptr || !m_target.has_value()) {
        return SubmissionOutcome::FailedCurrent;
    }

    const SubmissionStamp stamp {
        m_targetRevision,
        m_attachmentRevision,
        m_viewport,
    };
    auto record = std::make_unique<TargetRecord>();
    record->target = *m_target;
    record->targetRevision = stamp.targetRevision;
    record->attachmentRevision = stamp.attachmentRevision;
    const auto discardSuperseded = [&record]() {
        record.reset();
        return SubmissionOutcome::Superseded;
    };
    const auto discardFailedCurrent = [this, &record, &stamp]() {
        record.reset();
        return submissionIsCurrent(stamp) ? SubmissionOutcome::FailedCurrent
                                          : SubmissionOutcome::Superseded;
    };

    record->primaryResource = record->target.primaryResource();
    if (!submissionIsCurrent(stamp)) {
        return discardSuperseded();
    }
    record->target.resolvedPrimaryUrl = m_target->resolvedPrimaryUrl;
    if (record->primaryResource == nullptr) {
        return discardFailedCurrent();
    }
    record->primaryAdapter = std::make_unique<ImageViewportSequenceProvider>(
        record->primaryResource, record->target.primaryResource);
    ImageSequenceFactory factory;
    record->primaryFactoryResult.reset(factory.fromProvider(record->primaryAdapter.get()));
    if (!submissionIsCurrent(stamp)) {
        return discardSuperseded();
    }
    record->target.resolvedPrimaryUrl = m_target->resolvedPrimaryUrl;
    if (record->primaryFactoryResult == nullptr
        || record->primaryFactoryResult->outcome() != ImageSequenceFactoryOutcome::Created
        || record->primaryFactoryResult->sequence() == nullptr) {
        return discardFailedCurrent();
    }

    if (record->target.secondaryResource) {
        record->secondaryResource = record->target.secondaryResource();
        if (!submissionIsCurrent(stamp)) {
            return discardSuperseded();
        }
        record->target.resolvedPrimaryUrl = m_target->resolvedPrimaryUrl;
        if (record->secondaryResource == nullptr) {
            return discardFailedCurrent();
        }
        record->secondaryAdapter = std::make_unique<ImageViewportSequenceProvider>(
            record->secondaryResource, record->target.secondaryResource);
        record->secondaryFactoryResult.reset(factory.fromProvider(record->secondaryAdapter.get()));
        if (!submissionIsCurrent(stamp)) {
            return discardSuperseded();
        }
        record->target.resolvedPrimaryUrl = m_target->resolvedPrimaryUrl;
        if (record->secondaryFactoryResult == nullptr
            || record->secondaryFactoryResult->outcome() != ImageSequenceFactoryOutcome::Created
            || record->secondaryFactoryResult->sequence() == nullptr) {
            return discardFailedCurrent();
        }
    }

    TargetRecord* installed = record.get();
    m_records.push_back(std::move(record));
    m_activeRecord = installed;
    const ImageViewportPresentationTarget presentationTarget(
        installed->primaryFactoryResult->sequence(),
        installed->secondaryFactoryResult == nullptr
            ? nullptr
            : installed->secondaryFactoryResult->sequence());
    const ImageViewportCommandResult result = stamp.viewport->setPresentationTarget(
        presentationTarget, transitionPolicy(installed->target));
    if (!submissionIsCurrent(stamp) || !containsRecord(installed) || m_activeRecord != installed) {
        return SubmissionOutcome::Superseded;
    }
    if (!accepted(result)) {
        retireRecord(installed);
        return submissionIsCurrent(stamp) ? SubmissionOutcome::FailedCurrent
                                          : SubmissionOutcome::Superseded;
    }
    acceptSnapshot(stamp.viewport->state());
    return submissionIsCurrent(stamp) && containsRecord(installed) && m_activeRecord == installed
        ? SubmissionOutcome::Installed
        : SubmissionOutcome::Superseded;
}

void ImageViewportIntegrationRuntime::invalidateAttachment(ImageViewport* viewport)
{
    const quint64 attachmentRevision = m_attachmentRevision;
    QObject::disconnect(m_stateConnection);
    QObject::disconnect(m_destroyedConnection);
    m_viewport = nullptr;
    m_activeRecord = nullptr;
    std::vector<std::unique_ptr<TargetRecord>> retiredRecords = std::move(m_records);
    m_records.clear();
    viewport->clear();
    retiredRecords.clear();
    if (m_attachmentRevision != attachmentRevision || m_viewport != nullptr) {
        return;
    }
    ImageViewportIntegrationProjection projection = m_projection;
    projection.correlated = false;
    projection.completeAuthoritativeDisplayAvailable = false;
    publishProjection(std::move(projection));
}

void ImageViewportIntegrationRuntime::handleStateChanged()
{
    if (m_viewport != nullptr) {
        acceptSnapshot(m_viewport->state());
    }
}

void ImageViewportIntegrationRuntime::acceptSnapshot(const ImageViewportStateSnapshot& snapshot)
{
    if (m_activeRecord == nullptr || !m_target.has_value()) {
        return;
    }
    if (!containsRecord(m_activeRecord) || m_activeRecord->targetRevision != m_targetRevision
        || m_activeRecord->attachmentRevision != m_attachmentRevision) {
        return;
    }
    const ImageViewportPresentationTargetGenerationToken acceptedGeneration
        = snapshot.request().acceptedPresentationTargetGeneration();
    if (!acceptedGeneration.isValid()) {
        return;
    }
    if (!m_activeRecord->acceptedGeneration.isValid()) {
        const bool primaryMatches
            = snapshot.primary().sequence() == m_activeRecord->primaryFactoryResult->sequence();
        const bool secondaryMatches = m_activeRecord->secondaryFactoryResult == nullptr
            ? snapshot.secondary().sequence() == nullptr
            : snapshot.secondary().sequence() == m_activeRecord->secondaryFactoryResult->sequence();
        if (!primaryMatches || !secondaryMatches) {
            return;
        }
        m_activeRecord->acceptedGeneration = acceptedGeneration;
    }
    const ImageViewportFailureSnapshot componentFailure = snapshot.diagnostics().failure();
    if (acceptedGeneration != m_activeRecord->acceptedGeneration
        || m_activeRecord->target.sourceGeneration != m_target->sourceGeneration
        || m_activeRecord->targetRevision != m_targetRevision
        || m_activeRecord->attachmentRevision != m_attachmentRevision) {
        return;
    }

    TargetRecord* displayed
        = recordForGeneration(snapshot.display().displayedPresentationTargetGeneration());
    const bool completeDisplayAvailable = ::completeAuthoritativeDisplayAvailable(snapshot);
    const bool activeTargetHasCompleteDisplay
        = displayed == m_activeRecord && completeDisplayAvailable;
    const ImageViewportRequestStatus requestStatus = snapshot.request().status();
    const bool activeTargetFrameWait
        = requestStatus == ImageViewportRequestStatus::Loading && activeTargetHasCompleteDisplay;

    ImageViewportIntegrationProjection projection;
    projection.correlated = true;
    projection.sourceGeneration = m_activeRecord->target.sourceGeneration;
    projection.secondaryUrl = m_activeRecord->target.secondaryUrl;
    projection.secondarySessionId = m_activeRecord->target.secondarySessionId;
    projection.status
        = activeTargetFrameWait ? ImageDocumentStatus::Ready : documentStatus(requestStatus);
    projection.loading = projection.status == ImageDocumentStatus::Loading;
    projection.primaryImageSize = logicalSize(snapshot.primary());
    projection.secondaryImageSize = logicalSize(snapshot.secondary());
    projection.secondaryVisible = snapshot.display().displayedRoleSet().secondary();
    projection.fitMode = snapshot.presentation().fitMode();
    projection.zoomPercent = snapshot.presentation().zoomPercent();
    projection.preferredManualZoomPercent = snapshot.presentation().preferredManualZoomPercent();
    projection.minimumManualZoomPercent = snapshot.presentation().minimumManualZoomPercent();
    projection.maximumManualZoomPercent = snapshot.presentation().maximumManualZoomPercent();
    projection.manualZoomStepFactor = snapshot.presentation().manualZoomStepFactor();
    projection.rotationDegrees = snapshot.presentation().rotationDegrees();
    projection.horizontallyPannable = snapshot.display().horizontalPannable();
    projection.verticallyPannable = snapshot.display().verticalPannable();
    projection.viewportSize = m_viewport->size();
    projection.contentRect = snapshot.display().contentRect();
    projection.contentPosition = snapshot.display().contentPosition();
    projection.maximumContentPosition = snapshot.display().maximumContentPosition();
    projection.horizontalScrollPosition = normalizedScrollPosition(projection.horizontallyPannable,
        projection.contentPosition.x(), projection.maximumContentPosition.x());
    projection.horizontalScrollPageSize = normalizedScrollPageSize(projection.horizontallyPannable,
        snapshot.display().contentSize().width(), projection.maximumContentPosition.x());
    projection.verticalScrollPosition = normalizedScrollPosition(projection.verticallyPannable,
        projection.contentPosition.y(), projection.maximumContentPosition.y());
    projection.verticalScrollPageSize = normalizedScrollPageSize(projection.verticallyPannable,
        snapshot.display().contentSize().height(), projection.maximumContentPosition.y());
    projection.displayedTargetGeneration
        = snapshot.display().displayedPresentationTargetGeneration();
    projection.completeAuthoritativeDisplayAvailable = completeDisplayAvailable;

    if (displayed != nullptr) {
        if ((requestStatus == ImageViewportRequestStatus::Ready || activeTargetFrameWait)
            && displayed == m_activeRecord && !displayed->target.resolvedPrimaryUrl.isEmpty()) {
            projection.displayedUrl = displayed->target.resolvedPrimaryUrl;
        }
        if (requestStatus == ImageViewportRequestStatus::Ready) {
            const ImageViewportRoleSet displayedRoles = snapshot.display().displayedRoleSet();
            if (displayedRoles.primary() && displayed->primaryAdapter != nullptr) {
                displayed->primaryAdapter->acceptDisplayedStillDisplayImage(
                    ImageViewportPageRole::Primary, snapshot.primary().display().demandRevision());
            }
            if (displayedRoles.secondary() && displayed->secondaryAdapter != nullptr) {
                displayed->secondaryAdapter->acceptDisplayedStillDisplayImage(
                    ImageViewportPageRole::Secondary,
                    snapshot.secondary().display().demandRevision());
            }
        }
    }

    projection.failure = resolveFailure(*m_activeRecord, componentFailure);
    if (projection.failure.has_value()) {
        projection.errorString = projection.failure->userMessage;
    } else {
        projection.errorString = snapshot.diagnostics().errorString();
    }
    TargetRecord* correlatedRecord = m_activeRecord;
    pruneRecords(acceptedGeneration, snapshot.display().displayedPresentationTargetGeneration());
    if (m_activeRecord != correlatedRecord || !containsRecord(correlatedRecord)
        || !m_target.has_value() || m_target->sourceGeneration != projection.sourceGeneration
        || correlatedRecord->targetRevision != m_targetRevision
        || correlatedRecord->attachmentRevision != m_attachmentRevision) {
        return;
    }
    publishProjection(std::move(projection));
}

ImageViewportIntegrationRuntime::TargetRecord* ImageViewportIntegrationRuntime::recordForGeneration(
    ImageViewportPresentationTargetGenerationToken generation) const
{
    if (!generation.isValid()) {
        return nullptr;
    }
    const auto found = std::ranges::find_if(m_records.crbegin(), m_records.crend(),
        [generation](const std::unique_ptr<TargetRecord>& record) {
            return record->acceptedGeneration == generation;
        });
    return found == m_records.crend() ? nullptr : found->get();
}

void ImageViewportIntegrationRuntime::pruneRecords(
    ImageViewportPresentationTargetGenerationToken acceptedGeneration,
    ImageViewportPresentationTargetGenerationToken displayedGeneration)
{
    std::vector<std::unique_ptr<TargetRecord>> retained;
    std::vector<std::unique_ptr<TargetRecord>> retired;
    retained.reserve(m_records.size());
    retired.reserve(m_records.size());
    for (std::unique_ptr<TargetRecord>& record : m_records) {
        if (record.get() != m_activeRecord && record->acceptedGeneration != acceptedGeneration
            && record->acceptedGeneration != displayedGeneration) {
            retired.push_back(std::move(record));
        } else {
            retained.push_back(std::move(record));
        }
    }
    m_records = std::move(retained);
    retired.clear();
}

std::optional<ImageLoadFailure> ImageViewportIntegrationRuntime::resolveFailure(
    const TargetRecord& record, const ImageViewportFailureSnapshot& failure) const
{
    if (!failure.available() || !failure.providerFailureAvailable()
        || !failure.providerReference().isValid()) {
        return std::nullopt;
    }
    if (failure.role().isValid()
        && failure.role().value<ImageViewportPageRole>() == ImageViewportPageRole::Secondary) {
        return record.secondaryAdapter == nullptr
            ? std::nullopt
            : record.secondaryAdapter->resolveFailure(failure.providerReference());
    }
    return record.primaryAdapter->resolveFailure(failure.providerReference());
}

void ImageViewportIntegrationRuntime::publishProjection(
    ImageViewportIntegrationProjection projection)
{
    m_projection = std::move(projection);
    const ImageViewportIntegrationProjection published = m_projection;
    invokeIfSet(m_callbacks.projectionChanged, published);
}

bool ImageViewportIntegrationRuntime::submitPresentation(ImageViewportPresentationCommand command)
{
    return m_viewport != nullptr && m_projection.correlated
        && accepted(m_viewport->setPresentation(command));
}

bool ImageViewportIntegrationRuntime::resetView()
{
    return m_viewport != nullptr && m_projection.correlated && accepted(m_viewport->resetView());
}

bool ImageViewportIntegrationRuntime::setFitMode(ImageViewportFitMode fitMode)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(fitMode);
    return submitPresentation(command);
}

bool ImageViewportIntegrationRuntime::setPreferredManualZoomPercent(
    qreal percent, std::optional<QPointF> anchor)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(ImageViewportFitMode::Manual);
    command.setPreferredManualZoomPercent(percent);
    if (anchor.has_value()) {
        command.setZoomAnchor(*anchor);
    }
    return submitPresentation(command);
}

bool ImageViewportIntegrationRuntime::zoomBySteps(qreal steps, std::optional<QPointF> anchor)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(ImageViewportFitMode::Manual);
    command.setZoomStepDelta(steps);
    if (anchor.has_value()) {
        command.setZoomAnchor(*anchor);
    }
    return submitPresentation(command);
}

bool ImageViewportIntegrationRuntime::panBy(QPointF delta)
{
    if (!finitePoint(delta)) {
        return false;
    }
    ImageViewportPresentationCommand command;
    command.setPanDelta(delta);
    return submitPresentation(command);
}

bool ImageViewportIntegrationRuntime::setContentPosition(QPointF position)
{
    if (!finitePoint(position)) {
        return false;
    }
    ImageViewportPresentationCommand command;
    command.setContentPosition(position);
    return submitPresentation(command);
}

bool ImageViewportIntegrationRuntime::setRotationDegrees(int degrees)
{
    ImageViewportPresentationCommand command;
    command.setRotationDegrees(degrees);
    return submitPresentation(command);
}

bool ImageViewportIntegrationRuntime::setSpreadDirection(ImageViewportSpreadDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setSpreadDirection(direction);
    return submitPresentation(command);
}

bool ImageViewportIntegrationRuntime::submitHorizontalScrollPosition(qreal position)
{
    if (!m_projection.correlated || !m_projection.horizontallyPannable || !std::isfinite(position)
        || position < 0.0 || position > 1.0) {
        return false;
    }
    return setContentPosition(QPointF(
        position * m_projection.maximumContentPosition.x(), m_projection.contentPosition.y()));
}

bool ImageViewportIntegrationRuntime::submitVerticalScrollPosition(qreal position)
{
    if (!m_projection.correlated || !m_projection.verticallyPannable || !std::isfinite(position)
        || position < 0.0 || position > 1.0) {
        return false;
    }
    return setContentPosition(QPointF(
        m_projection.contentPosition.x(), position * m_projection.maximumContentPosition.y()));
}

ImageViewportCoordinateResult ImageViewportIntegrationRuntime::mapPoint(
    ImageViewportCoordinateInput input) const
{
    return m_viewport == nullptr || !m_projection.correlated
        ? ImageViewportCoordinateResult {}
        : m_viewport->mapPoint(std::move(input));
}

}
