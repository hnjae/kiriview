// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportproviderresource.h"

#include <QImageIOHandler>
#include <QSizeF>
#include <QTransform>
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

QSizeF sourceToPayloadScale(QSize sourceSize, QSize rasterSize)
{
    if (sourceSize.isEmpty() || rasterSize.isEmpty()) {
        return {};
    }
    return QSizeF(qreal(rasterSize.width()) / qreal(sourceSize.width()),
        qreal(rasterSize.height()) / qreal(sourceSize.height()));
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
    std::shared_ptr<ImageViewportFailureRegistry> failureRegistry,
    std::optional<StaticDisplayImagePayload> predecodedImage)
    : m_sourceGeneration(sourceGeneration)
    , m_locationIdentity(std::move(locationIdentity))
    , m_source(std::move(source))
    , m_displayStore(std::move(displayStore))
    , m_failureRegistry(failureRegistry == nullptr
              ? std::make_shared<ImageViewportFailureRegistry>()
              : std::move(failureRegistry))
    , m_predecodedImage(std::move(predecodedImage))
{
    Q_ASSERT(m_displayStore != nullptr);
}

ImageViewportProviderResource::~ImageViewportProviderResource() = default;

ImageSequenceProviderMetadata ImageViewportProviderResource::constructionMetadata() const
{
    if (m_source != nullptr) {
        const ImageSequenceProviderMetadata metadata = m_source->constructionMetadata();
        if (metadata.isSpecified()) {
            return metadata;
        }
    }
    if (m_predecodedImage.has_value() && m_predecodedImage->isValid()) {
        return ImageSequenceProviderMetadata::still(m_predecodedImage->originalSize);
    }
    return {};
}

void ImageViewportProviderResource::requestMetadata(
    const ImageViewportProviderWorkIdentity& identity, MetadataCompletion completion)
{
    if (!matchesResource(identity) || m_source == nullptr || !completion) {
        return;
    }
    const std::weak_ptr<ImageViewportProviderResource> resource = weak_from_this();
    m_source->requestMetadata(identity,
        [resource, completion = std::move(completion)](ImageViewportProviderWorkIdentity completed,
            ImageViewportProviderMetadataResult result) mutable {
            const std::shared_ptr<ImageViewportProviderResource> owner = resource.lock();
            if (owner != nullptr && owner->matchesResource(completed)) {
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

    const bool initialPayloadDemand
        = request.demand.currentPayloadQuality() == ImageViewportPayloadQuality::Unknown
        && request.demand.currentPayloadExactness() == ImageViewportPayloadExactness::Unknown;
    if (initialPayloadDemand && m_predecodedImage.has_value() && m_predecodedImage->isValid()) {
        StaticDisplayImagePayload predecoded = *m_predecodedImage;
        ImageSequenceProviderFrameEnvelope envelope = request.frame == 0
            ? ImageSequenceProviderFrameEnvelope::stillFrame()
            : ImageSequenceProviderFrameEnvelope {};
        envelope.setDemandRevision(identity.demandRevision);
        completion(identity,
            prepareFrame(identity,
                ImageViewportProviderFrameResult::ready(
                    std::move(predecoded), envelope, QString())));
        return;
    }

    if (m_source == nullptr) {
        ImageViewportProviderPreparedFrame result;
        result.failureCause = ImageSequenceProviderFailureCause::ProviderInternal;
        completion(identity, std::move(result));
        return;
    }

    const std::weak_ptr<ImageViewportProviderResource> resource = weak_from_this();
    m_source->requestFrame(identity, request,
        [resource, completion = std::move(completion)](
            const ImageViewportProviderWorkIdentity& completed,
            ImageViewportProviderFrameResult result) mutable {
            const std::shared_ptr<ImageViewportProviderResource> owner = resource.lock();
            if (owner == nullptr || !owner->matchesResource(completed)) {
                return;
            }
            completion(completed, owner->prepareFrame(completed, std::move(result)));
        });
}

void ImageViewportProviderResource::cancel(const QVector<ImageSequenceProviderRequestToken>& tokens)
{
    if (m_source != nullptr) {
        m_source->cancel(tokens);
    }
}

void ImageViewportProviderResource::close()
{
    if (m_source != nullptr) {
        m_source->close();
    }
}

std::optional<StaticDisplayImagePayload>
ImageViewportProviderResource::currentStillDisplayImage() const
{
    const QMutexLocker lock(&m_currentPayloadMutex);
    return m_currentStillDisplayImage;
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
    auto* frame = new ImageFrame(sourcePayload, entry->originalSize, entry->rasterSize,
        sourceToPayloadScale(entry->originalSize, entry->rasterSize), entry->byteCost,
        payloadQuality(entry->quality), payloadExactness(*entry), entry->image.hasAlphaChannel(),
        orientation, preparedFrame.formatIdentifier);
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

ImageViewportProviderPreparedFrame ImageViewportProviderResource::prepareFrame(
    const ImageViewportProviderWorkIdentity& identity, ImageViewportProviderFrameResult result)
{
    ImageViewportProviderPreparedFrame prepared;
    prepared.envelope = result.envelope;
    prepared.envelope.setDemandRevision(identity.demandRevision);
    prepared.formatIdentifier = std::move(result.formatIdentifier);
    prepared.failureCause = result.failureCause;
    prepared.failure = std::move(result.failure);
    if (!result.displayImage.has_value() || !result.displayImage->isValid()) {
        if (prepared.failureCause == ImageSequenceProviderFailureCause::Unavailable) {
            prepared.failureCause = ImageSequenceProviderFailureCause::ProviderInternal;
        }
        return prepared;
    }

    const StaticDisplayImagePayload& displayImage = *result.displayImage;
    const DisplayImageReuseKey reuseKey {
        m_locationIdentity,
        displayImage.sourceIdentity,
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
    if (result.envelope.isStillFrame()) {
        const QMutexLocker lock(&m_currentPayloadMutex);
        m_currentStillDisplayImage = displayImage;
    }
    return prepared;
}
}
