// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportproviderresource.h"

#include <QImageIOHandler>
#include <QTransform>
#include <algorithm>
#include <ranges>
#include <utility>

namespace {
ImageViewportPayloadQuality payloadQuality(kiriview::DisplayImageQuality quality)
{
    switch (quality) {
    case kiriview::DisplayImageQuality::ThumbnailPreview:
        return ImageViewportPayloadQuality::Preview;
    case kiriview::DisplayImageQuality::FirstDisplay:
        return ImageViewportPayloadQuality::FirstDisplay;
    case kiriview::DisplayImageQuality::BoundedDetail:
        return ImageViewportPayloadQuality::BoundedDetail;
    case kiriview::DisplayImageQuality::Exact:
        return ImageViewportPayloadQuality::Exact;
    }
    return ImageViewportPayloadQuality::Unknown;
}

ImageViewportPayloadExactness payloadExactness(const kiriview::DisplayImageStoreEntry& entry)
{
    return entry.quality == kiriview::DisplayImageQuality::Exact
            && entry.originalSize == entry.rasterSize
        ? ImageViewportPayloadExactness::ExactForSource
        : ImageViewportPayloadExactness::NotExact;
}

ImageFrame::OrientationPolicy orientationPolicy(QImageIOHandler::Transformations transformations)
{
    switch (static_cast<QImageIOHandler::Transformation>(transformations.toInt())) {
    case QImageIOHandler::TransformationNone:
        return ImageFrame::OrientationPolicy::Identity;
    case QImageIOHandler::TransformationMirror:
        return ImageFrame::OrientationPolicy::MirrorHorizontally;
    case QImageIOHandler::TransformationFlip:
        return ImageFrame::OrientationPolicy::MirrorVertically;
    case QImageIOHandler::TransformationRotate180:
        return ImageFrame::OrientationPolicy::Rotate180;
    case QImageIOHandler::TransformationRotate90:
        return ImageFrame::OrientationPolicy::Rotate90;
    case QImageIOHandler::TransformationMirrorAndRotate90:
        return ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90;
    case QImageIOHandler::TransformationFlipAndRotate90:
        return ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90;
    case QImageIOHandler::TransformationRotate270:
        return ImageFrame::OrientationPolicy::Rotate270;
    }
    return ImageFrame::OrientationPolicy::Identity;
}

kiriview::DisplayedPageRole displayedPageRole(ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? kiriview::DisplayedPageRole::Secondary
                                                    : kiriview::DisplayedPageRole::Primary;
}

std::optional<kiriview::DisplayImageRasterIdentity> rasterIdentityFor(
    const kiriview::StaticDisplayImagePayload& displayImage,
    const ImageSequenceProviderFrameEnvelope& envelope,
    kiriview::ImageViewportProviderFrameStage stage)
{
    if (envelope.isTimedFrame()) {
        if (stage == kiriview::ImageViewportProviderFrameStage::Provisional
            || displayImage.rasterKind != kiriview::DisplayImageRasterKind::TimedFrame) {
            return std::nullopt;
        }
        kiriview::DisplayImageRasterIdentity identity
            = kiriview::DisplayImageRasterIdentity::timedFrame(envelope.frame());
        return identity.isValid() ? std::optional<kiriview::DisplayImageRasterIdentity>(identity)
                                  : std::nullopt;
    }
    if (!envelope.isStillFrame()) {
        return std::nullopt;
    }
    if (stage == kiriview::ImageViewportProviderFrameStage::Provisional) {
        return displayImage.rasterKind == kiriview::DisplayImageRasterKind::ProvisionalPreview
            ? std::optional<kiriview::DisplayImageRasterIdentity>(
                  kiriview::DisplayImageRasterIdentity::provisionalPreview())
            : std::nullopt;
    }
    switch (displayImage.rasterKind) {
    case kiriview::DisplayImageRasterKind::AuthoritativeStill:
        return kiriview::DisplayImageRasterIdentity::authoritativeStill();
    case kiriview::DisplayImageRasterKind::Refinement:
        return kiriview::DisplayImageRasterIdentity::refinement();
    case kiriview::DisplayImageRasterKind::ProvisionalPreview:
    case kiriview::DisplayImageRasterKind::TimedFrame:
        return std::nullopt;
    }
    return std::nullopt;
}

QImage sourcePayloadForOrientation(
    const QImage& normalizedImage, ImageFrame::OrientationPolicy orientation)
{
    switch (orientation) {
    case ImageFrame::OrientationPolicy::Identity:
        return normalizedImage;
    case ImageFrame::OrientationPolicy::MirrorHorizontally:
        return normalizedImage.flipped(Qt::Horizontal);
    case ImageFrame::OrientationPolicy::MirrorVertically:
        return normalizedImage.flipped(Qt::Vertical);
    case ImageFrame::OrientationPolicy::Rotate180:
        return normalizedImage.transformed(QTransform().rotate(180));
    case ImageFrame::OrientationPolicy::Rotate90:
        return normalizedImage.transformed(QTransform().rotate(270));
    case ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90:
        return normalizedImage.transformed(QTransform().rotate(270)).flipped(Qt::Horizontal);
    case ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90:
        return normalizedImage.transformed(QTransform().rotate(270)).flipped(Qt::Vertical);
    case ImageFrame::OrientationPolicy::Rotate270:
        return normalizedImage.transformed(QTransform().rotate(90));
    }
    return normalizedImage;
}
}

namespace kiriview {
bool operator==(
    const ImageViewportProviderWorkIdentity& left, const ImageViewportProviderWorkIdentity& right)
{
    return left.sourceGeneration == right.sourceGeneration && left.role == right.role
        && left.requestToken == right.requestToken && left.demandRevision == right.demandRevision
        && left.locationIdentity == right.locationIdentity;
}

ImageViewportProviderMetadataResult ImageViewportProviderMetadataResult::ready(
    ImageSequenceProviderMetadata metadata)
{
    ImageViewportProviderMetadataResult result;
    result.metadata = std::move(metadata);
    return result;
}

ImageViewportProviderMetadataResult ImageViewportProviderMetadataResult::failed(
    ImageSequenceProviderFailureCause cause, ImageLoadFailure failure)
{
    ImageViewportProviderMetadataResult result;
    result.failureCause = cause;
    result.failure = std::move(failure);
    return result;
}

ImageViewportProviderFrameResult ImageViewportProviderFrameResult::ready(
    StaticDisplayImagePayload displayImage, ImageSequenceProviderFrameEnvelope envelope,
    QString formatIdentifier)
{
    ImageViewportProviderFrameResult result;
    result.displayImage = std::move(displayImage);
    result.envelope = envelope;
    result.formatIdentifier = std::move(formatIdentifier);
    return result;
}

ImageViewportProviderFrameResult ImageViewportProviderFrameResult::provisional(
    StaticDisplayImagePayload displayImage, ImageSequenceProviderFrameEnvelope envelope,
    QString formatIdentifier)
{
    ImageViewportProviderFrameResult result;
    result.displayImage = std::move(displayImage);
    result.envelope = envelope;
    result.formatIdentifier = std::move(formatIdentifier);
    result.stage = ImageViewportProviderFrameStage::Provisional;
    return result;
}

ImageViewportProviderFrameResult ImageViewportProviderFrameResult::unsupported(
    ImageSequenceProviderUnsupportedCause cause)
{
    ImageViewportProviderFrameResult result;
    result.unsupportedCause = cause;
    return result;
}

ImageViewportProviderFrameResult ImageViewportProviderFrameResult::failed(
    ImageSequenceProviderFailureCause cause, ImageLoadFailure failure)
{
    ImageViewportProviderFrameResult result;
    result.failureCause = cause;
    result.failure = std::move(failure);
    return result;
}

ImageViewportProviderResource::ImageViewportProviderResource(quint64 sourceGeneration,
    QString locationIdentity, std::shared_ptr<ImageViewportProviderSource> source,
    std::shared_ptr<DisplayImageStore> displayStore,
    std::shared_ptr<ImageViewportFailureRegistry> failureRegistry)
    : m_sourceGeneration(sourceGeneration)
    , m_locationIdentity(std::move(locationIdentity))
    , m_displayLocationIdentity(m_locationIdentity)
    , m_source(std::move(source))
    , m_displayStore(std::move(displayStore))
    , m_failureRegistry(failureRegistry == nullptr
              ? std::make_shared<ImageViewportFailureRegistry>()
              : std::move(failureRegistry))
{
    Q_ASSERT(m_displayStore != nullptr);
}

ImageViewportProviderResource::~ImageViewportProviderResource() = default;

bool ImageViewportProviderResource::bindDisplayLocationIdentity(QString locationIdentity)
{
    if (locationIdentity.isEmpty()) {
        return false;
    }

    const QMutexLocker lock(&m_stateMutex);
    if (m_closed || m_displayLocationIdentityBound || m_payloadPreparationStarted) {
        return false;
    }
    m_displayLocationIdentity = std::move(locationIdentity);
    m_displayLocationIdentityBound = true;
    return true;
}

ImageSequenceProviderMetadata ImageViewportProviderResource::constructionMetadata() const
{
    return m_source == nullptr ? ImageSequenceProviderMetadata {}
                               : m_source->constructionMetadata();
}

void ImageViewportProviderResource::requestMetadata(
    const ImageViewportProviderWorkIdentity& identity, MetadataCompletion completion)
{
    if (!matchesResource(identity) || m_source == nullptr || !completion) {
        return;
    }
    {
        const QMutexLocker lock(&m_stateMutex);
        if (m_closed) {
            return;
        }
        if (std::ranges::contains(m_activeMetadataWork, identity)) {
            return;
        }
        m_activeMetadataWork.push_back(identity);
    }
    const std::weak_ptr<ImageViewportProviderResource> resource = weak_from_this();
    m_source->requestMetadata(identity,
        [resource, completion = std::move(completion)](ImageViewportProviderWorkIdentity completed,
            ImageViewportProviderMetadataResult result) mutable {
            const std::shared_ptr<ImageViewportProviderResource> owner = resource.lock();
            if (owner != nullptr && owner->matchesResource(completed)
                && owner->finalizeMetadata(completed)) {
                completion(std::move(completed), std::move(result));
            }
        });
}

void ImageViewportProviderResource::requestFrame(const ImageViewportProviderWorkIdentity& identity,
    ImageViewportProviderFrameRequest request, FrameCompletion completion)
{
    if (!matchesResource(identity) || !completion) {
        return;
    }
    {
        const QMutexLocker lock(&m_stateMutex);
        if (m_closed) {
            return;
        }
        std::erase_if(
            m_activeFrameWork, [&identity](const ImageViewportProviderWorkIdentity& active) {
                return active.role == identity.role && active != identity;
            });
        std::erase_if(m_authoritativeFrameCandidates,
            [&identity](const AuthoritativeFrameCandidate& candidate) {
                return candidate.identity.role == identity.role;
            });
        if (m_authoritativeStillDisplayImageCandidate.has_value()
            && m_authoritativeStillDisplayImageCandidate->identity.role == identity.role) {
            m_authoritativeStillDisplayImageCandidate.reset();
        }
        if (!std::ranges::contains(m_activeFrameWork, identity)) {
            m_activeFrameWork.push_back(identity);
        }
    }

    if (m_source == nullptr) {
        ImageViewportProviderPreparedFrame result;
        result.failureCause = ImageSequenceProviderFailureCause::ProviderInternal;
        if (finalizePreparedFrame(identity, result)) {
            completion(identity, std::move(result));
        }
        return;
    }

    const qint64 displayStoreEntryBudget = m_displayStore->byteBudget();
    request.maximumStoreEntryBytes = request.maximumStoreEntryBytes < 0
        ? displayStoreEntryBudget
        : std::min(request.maximumStoreEntryBytes, displayStoreEntryBudget);
    const std::weak_ptr<ImageViewportProviderResource> resource = weak_from_this();
    m_source->requestFrame(identity, request,
        [resource, completion = std::move(completion)](
            const ImageViewportProviderWorkIdentity& completed,
            ImageViewportProviderFrameResult result) mutable {
            const std::shared_ptr<ImageViewportProviderResource> owner = resource.lock();
            if (owner == nullptr || !owner->matchesResource(completed)) {
                return;
            }
            ImageViewportProviderPreparedFrame prepared
                = owner->prepareFrame(completed, std::move(result));
            if (owner->finalizePreparedFrame(completed, prepared)) {
                completion(completed, std::move(prepared));
            }
        });
}

void ImageViewportProviderResource::cancel(const QVector<ImageSequenceProviderRequestToken>& tokens)
{
    {
        const QMutexLocker lock(&m_stateMutex);
        std::erase_if(
            m_activeMetadataWork, [&tokens](const ImageViewportProviderWorkIdentity& identity) {
                return std::ranges::contains(tokens, identity.requestToken);
            });
        std::erase_if(
            m_activeFrameWork, [&tokens](const ImageViewportProviderWorkIdentity& identity) {
                return std::ranges::contains(tokens, identity.requestToken);
            });
        std::erase_if(m_authoritativeFrameCandidates,
            [&tokens](const AuthoritativeFrameCandidate& candidate) {
                return std::ranges::contains(tokens, candidate.identity.requestToken);
            });
        if (m_authoritativeStillDisplayImageCandidate.has_value()
            && std::ranges::contains(
                tokens, m_authoritativeStillDisplayImageCandidate->identity.requestToken)) {
            m_authoritativeStillDisplayImageCandidate.reset();
        }
    }
    if (m_source != nullptr) {
        m_source->cancel(tokens);
    }
}

void ImageViewportProviderResource::close()
{
    {
        const QMutexLocker lock(&m_stateMutex);
        if (m_closed) {
            return;
        }
        m_closed = true;
        m_activeMetadataWork.clear();
        m_activeFrameWork.clear();
        m_authoritativeFrameCandidates.clear();
        m_authoritativeStillDisplayImageCandidate.reset();
    }
    if (m_source != nullptr) {
        m_source->close();
    }
}

std::optional<StaticDisplayImagePayload> ImageViewportProviderResource::currentStillDisplayImage(
    ImageViewportDemandRevisionToken demandRevision) const
{
    const QMutexLocker lock(&m_stateMutex);
    if (!m_currentStillDisplayImage.has_value()
        || m_currentStillDisplayImage->identity.demandRevision != demandRevision) {
        return std::nullopt;
    }
    const std::optional<DisplayImageStoreEntry> entry
        = m_displayStore->entry(m_currentStillDisplayImage->storeEntryId);
    if (!entry.has_value()) {
        return std::nullopt;
    }

    StaticDisplayImagePayload displayImage = m_currentStillDisplayImage->displayImage;
    displayImage.image = entry->image;
    return displayImage.isValid()
        ? std::optional<StaticDisplayImagePayload>(std::move(displayImage))
        : std::nullopt;
}

bool ImageViewportProviderResource::acceptAuthoritativeStillDisplayImage(
    const ImageViewportProviderWorkIdentity& identity,
    const ImageViewportProviderPreparedFrame& preparedFrame)
{
    if (!matchesResource(identity) || preparedFrame.isProvisional() || !preparedFrame.isReady()
        || !preparedFrame.envelope.isStillFrame()
        || preparedFrame.envelope.demandRevision() != identity.demandRevision
        || !preparedFrame.authoritativeStillDisplayImage.has_value()
        || !preparedFrame.authoritativeStillDisplayImage->isValid()) {
        return false;
    }

    const QMutexLocker lock(&m_stateMutex);
    const auto candidate = std::ranges::find_if(m_authoritativeFrameCandidates,
        [&identity, &preparedFrame](const AuthoritativeFrameCandidate& current) {
            return current.identity == identity
                && current.storeEntryId == preparedFrame.storeEntryId;
        });
    if (m_closed || candidate == m_authoritativeFrameCandidates.end()) {
        return false;
    }

    m_authoritativeFrameCandidates.erase(candidate);
    StaticDisplayImagePayload displayImage = *preparedFrame.authoritativeStillDisplayImage;
    displayImage.image = {};
    m_authoritativeStillDisplayImageCandidate = AuthoritativeStillDisplayImage { identity,
        preparedFrame.storeEntryId, std::move(displayImage) };
    return true;
}

bool ImageViewportProviderResource::acceptDisplayedStillDisplayImage(
    ImageViewportPageRole role, ImageViewportDemandRevisionToken demandRevision)
{
    const QMutexLocker lock(&m_stateMutex);
    if (m_currentStillDisplayImage.has_value() && m_currentStillDisplayImage->identity.role == role
        && m_currentStillDisplayImage->identity.demandRevision == demandRevision) {
        return true;
    }
    if (!m_authoritativeStillDisplayImageCandidate.has_value()
        || m_authoritativeStillDisplayImageCandidate->identity.role != role
        || m_authoritativeStillDisplayImageCandidate->identity.demandRevision != demandRevision) {
        return false;
    }

    m_currentStillDisplayImage = std::move(m_authoritativeStillDisplayImageCandidate);
    m_authoritativeStillDisplayImageCandidate.reset();
    return true;
}

ImageSequenceProviderFrameHandle* ImageViewportProviderResource::acquireFrameHandle(
    const ImageViewportProviderPreparedFrame& preparedFrame)
{
    if (!preparedFrame.isReady()
        || !m_displayStore->acquireFrameLease(preparedFrame.storeEntryId)) {
        return nullptr;
    }

    const std::optional<DisplayImageStoreEntry> entry
        = m_displayStore->entry(preparedFrame.storeEntryId);
    if (!entry.has_value()) {
        m_displayStore->releaseFrameLease(preparedFrame.storeEntryId);
        return nullptr;
    }

    const ImageFrame::OrientationPolicy orientation
        = orientationPolicy(entry->imageReaderTransformations);
    const QImage sourcePayload = sourcePayloadForOrientation(entry->image, orientation);
    auto* frame = new ImageFrame(sourcePayload, entry->originalSize, entry->byteCost,
        payloadQuality(entry->quality), payloadExactness(*entry), orientation,
        preparedFrame.formatIdentifier);
    const std::shared_ptr<DisplayImageStore> store = m_displayStore;
    const QString entryId = preparedFrame.storeEntryId;
    return new ImageSequenceProviderFrameHandle(frame, [store, entryId](ImageFrame* releasedFrame) {
        delete releasedFrame;
        store->releaseFrameLease(entryId);
    });
}

ImageSequenceProviderFailure ImageViewportProviderResource::failure(
    ImageSequenceProviderFailureCause cause, std::optional<ImageLoadFailure> failure)
{
    if (!failure.has_value()) {
        return ImageSequenceProviderFailure(cause);
    }
    return ImageSequenceProviderFailure(
        cause, m_failureRegistry->registerFailure(std::move(*failure)));
}

bool ImageViewportProviderResource::matchesResource(
    const ImageViewportProviderWorkIdentity& identity) const
{
    return identity.sourceGeneration == m_sourceGeneration
        && identity.locationIdentity == m_locationIdentity;
}

bool ImageViewportProviderResource::finalizeMetadata(
    const ImageViewportProviderWorkIdentity& identity)
{
    const QMutexLocker lock(&m_stateMutex);
    const auto active = std::ranges::find(m_activeMetadataWork, identity);
    if (m_closed || active == m_activeMetadataWork.end()) {
        return false;
    }
    m_activeMetadataWork.erase(active);
    return true;
}

bool ImageViewportProviderResource::finalizePreparedFrame(
    const ImageViewportProviderWorkIdentity& identity,
    const ImageViewportProviderPreparedFrame& preparedFrame)
{
    const QMutexLocker lock(&m_stateMutex);
    const auto active = std::ranges::find(m_activeFrameWork, identity);
    if (m_closed || active == m_activeFrameWork.end()) {
        return false;
    }
    if (preparedFrame.isProvisional()) {
        return true;
    }

    m_activeFrameWork.erase(active);
    std::erase_if(
        m_authoritativeFrameCandidates, [&identity](const AuthoritativeFrameCandidate& candidate) {
            return candidate.identity.role == identity.role;
        });
    if (preparedFrame.isReady() && preparedFrame.authoritativeStillDisplayImage.has_value()
        && preparedFrame.envelope.isStillFrame()) {
        m_authoritativeFrameCandidates.push_back(
            AuthoritativeFrameCandidate { identity, preparedFrame.storeEntryId });
    }
    return true;
}

ImageViewportProviderPreparedFrame ImageViewportProviderResource::prepareFrame(
    const ImageViewportProviderWorkIdentity& identity, ImageViewportProviderFrameResult result)
{
    const QString displayLocationIdentity = displayLocationIdentityForPayloadPreparation();
    ImageViewportProviderPreparedFrame prepared;
    prepared.stage = result.stage;
    prepared.envelope = result.envelope;
    prepared.envelope.setDemandRevision(identity.demandRevision);
    prepared.formatIdentifier = std::move(result.formatIdentifier);
    prepared.failureCause = result.failureCause;
    prepared.failure = std::move(result.failure);
    prepared.unsupportedCause = result.unsupportedCause;
    if (prepared.unsupportedCause.has_value()) {
        return prepared;
    }
    if (!result.displayImage.has_value() || !result.displayImage->isValid()) {
        if (!prepared.unsupportedCause.has_value()
            && prepared.failureCause == ImageSequenceProviderFailureCause::Unavailable) {
            prepared.failureCause = ImageSequenceProviderFailureCause::ProviderInternal;
        }
        return prepared;
    }

    const StaticDisplayImagePayload& displayImage = *result.displayImage;
    const std::optional<DisplayImageRasterIdentity> rasterIdentity
        = rasterIdentityFor(displayImage, prepared.envelope, prepared.stage);
    if (!rasterIdentity.has_value()) {
        prepared.failureCause = ImageSequenceProviderFailureCause::ProviderInternal;
        return prepared;
    }
    const DisplayImageReuseKey reuseKey {
        displayLocationIdentity,
        displayImage.sourceIdentity,
        displayImage.sourceRevision,
        *rasterIdentity,
        displayImage.imageReaderTransform.transformations,
        displayImage.originalSize,
        displayImage.image.size(),
        displayImage.quality,
        displayImage.previewOrigin,
        displayedPageRole(identity.role),
    };
    prepared.storeEntryId = m_displayStore->acquireReusable(
        DisplayImageEntry {
            displayImage.image,
            displayImage.originalSize,
            displayImage.image.size(),
            displayImage.quality,
            DisplayImageRetentionPriority::Visible,
        },
        reuseKey);
    if (prepared.storeEntryId.isEmpty()) {
        prepared.failureCause = ImageSequenceProviderFailureCause::ResourceExhausted;
        return prepared;
    }
    if (!prepared.isProvisional() && prepared.envelope.isStillFrame()) {
        prepared.authoritativeStillDisplayImage = displayImage;
    }
    return prepared;
}

QString ImageViewportProviderResource::displayLocationIdentityForPayloadPreparation()
{
    const QMutexLocker lock(&m_stateMutex);
    m_payloadPreparationStarted = true;
    return m_displayLocationIdentity;
}
}
