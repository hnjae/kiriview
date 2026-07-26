// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportintegrationruntime.h"

#include "async/imagecallback.h"
#include "rendering/imageviewportsequenceprovider.h"

#include <ImageViewport/imagesequence.h>

#include <QVariant>
#include <algorithm>
#include <cmath>
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
};

bool ImageViewportIntegrationTarget::isValid() const
{
    return sourceGeneration != 0 && !primaryUrl.isEmpty() && bool(primaryResource)
        && (secondaryUrl.isEmpty() == !bool(secondaryResource));
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
        ImageViewport* viewport = m_viewport;
        m_viewport = nullptr;
        m_activeRecord = nullptr;
        viewport->clear();
    }
}

void ImageViewportIntegrationRuntime::attach(ImageViewport* viewport)
{
    if (m_viewport == viewport) {
        return;
    }
    if (m_viewport != nullptr) {
        invalidateAttachment(m_viewport);
    }
    if (viewport == nullptr) {
        return;
    }

    m_viewport = viewport;
    m_stateConnection = connect(viewport, &ImageViewport::stateChanged, this,
        &ImageViewportIntegrationRuntime::handleStateChanged);
    m_destroyedConnection = connect(viewport, &QObject::destroyed, this, [this]() {
        QObject::disconnect(m_stateConnection);
        m_viewport = nullptr;
        m_activeRecord = nullptr;
        ImageViewportIntegrationProjection projection = m_projection;
        projection.correlated = false;
        publishProjection(std::move(projection));
    });
    if (m_target.has_value()) {
        submitCurrentTarget();
    } else {
        handleStateChanged();
    }
}

void ImageViewportIntegrationRuntime::detach(ImageViewport* viewport)
{
    if (viewport != nullptr && m_viewport == viewport) {
        invalidateAttachment(viewport);
    }
}

bool ImageViewportIntegrationRuntime::submitTarget(ImageViewportIntegrationTarget target)
{
    if (!target.isValid()) {
        return false;
    }
    m_target = std::move(target);
    m_activeRecord = nullptr;
    return m_viewport == nullptr || submitCurrentTarget();
}

void ImageViewportIntegrationRuntime::clearTarget()
{
    m_target.reset();
    m_activeRecord = nullptr;
    if (m_viewport != nullptr) {
        m_viewport->clear();
    }
    publishProjection({});
}

void ImageViewportIntegrationRuntime::stopPlayback()
{
    if (m_viewport == nullptr) {
        return;
    }
    m_viewport->stop(ImageViewportPageRole::Primary);
    m_viewport->stop(ImageViewportPageRole::Secondary);
}

const ImageViewportIntegrationProjection& ImageViewportIntegrationRuntime::projection() const
{
    return m_projection;
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
    const std::shared_ptr<ImageViewportProviderResource>& resource
        = role == ImageViewportPageRole::Secondary ? displayed->secondaryResource
                                                   : displayed->primaryResource;
    const ImageViewportDemandRevisionToken demandRevision = role == ImageViewportPageRole::Secondary
        ? snapshot.secondary().display().demandRevision()
        : snapshot.primary().display().demandRevision();
    return resource == nullptr ? std::nullopt : resource->currentStillDisplayImage(demandRevision);
}

bool ImageViewportIntegrationRuntime::submitCurrentTarget()
{
    if (m_viewport == nullptr || !m_target.has_value()) {
        return false;
    }

    auto record = std::make_unique<TargetRecord>();
    record->target = *m_target;
    record->primaryResource = record->target.primaryResource();
    if (record->primaryResource == nullptr) {
        return false;
    }
    record->primaryAdapter
        = std::make_unique<ImageViewportSequenceProvider>(record->primaryResource);
    ImageSequenceFactory factory;
    record->primaryFactoryResult.reset(factory.fromProvider(record->primaryAdapter.get()));
    if (record->primaryFactoryResult == nullptr
        || record->primaryFactoryResult->outcome() != ImageSequenceFactoryOutcome::Created
        || record->primaryFactoryResult->sequence() == nullptr) {
        return false;
    }

    if (record->target.secondaryResource) {
        record->secondaryResource = record->target.secondaryResource();
        if (record->secondaryResource == nullptr) {
            return false;
        }
        record->secondaryAdapter
            = std::make_unique<ImageViewportSequenceProvider>(record->secondaryResource);
        record->secondaryFactoryResult.reset(factory.fromProvider(record->secondaryAdapter.get()));
        if (record->secondaryFactoryResult == nullptr
            || record->secondaryFactoryResult->outcome() != ImageSequenceFactoryOutcome::Created
            || record->secondaryFactoryResult->sequence() == nullptr) {
            return false;
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
    const ImageViewportCommandResult result = m_viewport->setPresentationTarget(
        presentationTarget, transitionPolicy(installed->target));
    if (!accepted(result)) {
        m_activeRecord = nullptr;
        return false;
    }
    acceptSnapshot(m_viewport->state());
    return true;
}

void ImageViewportIntegrationRuntime::invalidateAttachment(ImageViewport* viewport)
{
    QObject::disconnect(m_stateConnection);
    QObject::disconnect(m_destroyedConnection);
    m_viewport = nullptr;
    m_activeRecord = nullptr;
    viewport->clear();
    ImageViewportIntegrationProjection projection = m_projection;
    projection.correlated = false;
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
        || m_activeRecord->target.sourceGeneration != m_target->sourceGeneration) {
        return;
    }

    ImageViewportIntegrationProjection projection;
    projection.correlated = true;
    projection.sourceGeneration = m_activeRecord->target.sourceGeneration;
    projection.secondaryUrl = m_activeRecord->target.secondaryUrl;
    projection.status = documentStatus(snapshot.request().status());
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

    TargetRecord* displayed
        = recordForGeneration(snapshot.display().displayedPresentationTargetGeneration());
    if (displayed != nullptr) {
        projection.displayedUrl = displayed->target.primaryUrl;
        if (snapshot.request().status() == ImageViewportRequestStatus::Ready) {
            const ImageViewportRoleSet displayedRoles = snapshot.display().displayedRoleSet();
            if (displayedRoles.primary() && displayed->primaryResource != nullptr) {
                displayed->primaryResource->acceptDisplayedStillDisplayImage(
                    ImageViewportPageRole::Primary, snapshot.primary().display().demandRevision());
            }
            if (displayedRoles.secondary() && displayed->secondaryResource != nullptr) {
                displayed->secondaryResource->acceptDisplayedStillDisplayImage(
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
    if (m_activeRecord != correlatedRecord || !m_target.has_value()
        || m_target->sourceGeneration != projection.sourceGeneration) {
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
    std::erase_if(m_records,
        [this, acceptedGeneration, displayedGeneration](
            const std::unique_ptr<TargetRecord>& record) {
            return record.get() != m_activeRecord
                && record->acceptedGeneration != acceptedGeneration
                && record->acceptedGeneration != displayedGeneration;
        });
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
        return record.secondaryResource == nullptr
            ? std::nullopt
            : record.secondaryResource->failureRegistry()->resolve(failure.providerReference());
    }
    return record.primaryResource->failureRegistry()->resolve(failure.providerReference());
}

void ImageViewportIntegrationRuntime::publishProjection(
    ImageViewportIntegrationProjection projection)
{
    m_projection = std::move(projection);
    invokeIfSet(m_callbacks.projectionChanged, m_projection);
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
