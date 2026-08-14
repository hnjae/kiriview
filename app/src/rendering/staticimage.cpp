// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "staticimage.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "staticimagedisplaysourcehelpers_p.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
qsizetype stringStorageByteCost(const QString& value)
{
    const qsizetype retainedCodeUnits = std::max(value.size(), value.capacity());
    if (retainedCodeUnits <= 0) {
        return 0;
    }
    return kiriview::saturatedQtByteProduct(
        kiriview::saturatedQtByteSum(retainedCodeUnits, 1), sizeof(QChar));
}

qsizetype embeddedMetadataStorageByteCost(const kiriview::EmbeddedMetadata& metadata)
{
    qsizetype byteCost = 0;
    for (const QString* field : {
             &metadata.cameraMake,
             &metadata.cameraModel,
             &metadata.taken,
             &metadata.location,
             &metadata.lens,
             &metadata.exposure,
             &metadata.iso,
             &metadata.focalLength,
             &metadata.software,
             &metadata.duration,
             &metadata.frameSize,
         }) {
        byteCost = kiriview::saturatedQtByteSum(byteCost, stringStorageByteCost(*field));
    }

    const std::size_t maximumCountedCapacity
        = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max());
    const std::size_t rowCapacity
        = std::min(metadata.advancedRows.capacity(), maximumCountedCapacity);
    byteCost = kiriview::saturatedQtByteSum(byteCost,
        kiriview::saturatedQtByteProduct(
            static_cast<std::int64_t>(rowCapacity), sizeof(kiriview::EmbeddedMetadataRow)));
    for (const kiriview::EmbeddedMetadataRow& row : metadata.advancedRows) {
        byteCost = kiriview::saturatedQtByteSum(byteCost, stringStorageByteCost(row.label));
        byteCost = kiriview::saturatedQtByteSum(byteCost, stringStorageByteCost(row.value));
    }
    return byteCost;
}
}

namespace kiriview {
StaticDisplayImagePayload::StaticDisplayImagePayload(QString sourceIdentity,
    StaticImageReaderTransform imageReaderTransform, QSize originalSize, QImage image,
    DisplayImageQuality quality, EmbeddedMetadata embeddedMetadata,
    ImageSourceDataLease sourceDataLease, ImageDecodeWorkspaceHold inputWorkspaceHold,
    std::shared_ptr<StaticImageDisplaySource> refinementSource,
    DisplayImagePreviewOrigin previewOrigin, StaticImageSourceDetailModel sourceDetailModel,
    ImageSourceRevision sourceRevision, DisplayImageRasterKind rasterKind)
    : sourceIdentity(std::move(sourceIdentity))
    , imageReaderTransform(imageReaderTransform)
    , originalSize(originalSize)
    , image(std::move(image))
    , quality(quality)
    , embeddedMetadata(std::move(embeddedMetadata))
    , sourceDataLease(std::move(sourceDataLease))
    , inputWorkspaceHold(std::move(inputWorkspaceHold))
    , refinementSource(std::move(refinementSource))
    , previewOrigin(previewOrigin)
    , sourceDetailModel(sourceDetailModel)
    , sourceRevision(std::move(sourceRevision))
    , rasterKind(rasterKind)
{
}

StaticDisplayImagePayload& StaticDisplayImagePayload::operator=(
    const StaticDisplayImagePayload& other)
{
    if (this != &other) {
        StaticDisplayImagePayload replacement(other);
        swap(*this, replacement);
    }
    return *this;
}

StaticDisplayImagePayload& StaticDisplayImagePayload::operator=(
    StaticDisplayImagePayload&& other) noexcept
{
    if (this != &other) {
        StaticDisplayImagePayload replacement(std::move(other));
        swap(*this, replacement);
    }
    return *this;
}

void swap(StaticDisplayImagePayload& left, StaticDisplayImagePayload& right) noexcept
{
    using std::swap;
    swap(left.sourceIdentity, right.sourceIdentity);
    swap(left.imageReaderTransform, right.imageReaderTransform);
    swap(left.originalSize, right.originalSize);
    swap(left.image, right.image);
    swap(left.quality, right.quality);
    swap(left.embeddedMetadata, right.embeddedMetadata);
    swap(left.sourceDataLease, right.sourceDataLease);
    swap(left.inputWorkspaceHold, right.inputWorkspaceHold);
    swap(left.refinementSource, right.refinementSource);
    swap(left.previewOrigin, right.previewOrigin);
    swap(left.sourceDetailModel, right.sourceDetailModel);
    swap(left.sourceRevision, right.sourceRevision);
    swap(left.rasterKind, right.rasterKind);
}

StaticImageSourceDetailModel StaticImageDisplaySource::detailModel() const
{
    return StaticImageSourceDetailModel::FiniteRaster;
}

std::optional<qsizetype> StaticImageDisplaySource::initialDisplayDecodePeakByteCost(
    const ImageFirstDisplayDecodeContext& context, int blockingMaximumLongEdge) const
{
    Q_UNUSED(context);
    return rasterDisplayRefinementPeakByteCost(
        boundedPreviewSize(imageSize(), blockingMaximumLongEdge));
}

StaticImageFirstDisplayDecodeResult StaticImageDisplaySource::decodeFirstDisplayImage(
    const ImageFirstDisplayDecodeContext& context) const
{
    Q_UNUSED(context);
    return {};
}

bool StaticImageDisplaySource::supportsRasterDisplayRefinement() const { return false; }

std::optional<qsizetype> StaticImageDisplaySource::rasterDisplayRefinementPeakByteCost(
    const QSize& rasterSize) const
{
    Q_UNUSED(rasterSize);
    return std::nullopt;
}

StaticImageDisplayDecodeResult StaticImageDisplaySource::decodeRasterDisplayImage(
    const QSize& rasterSize) const
{
    if (rasterSize.isEmpty()) {
        return {};
    }

    return {};
}

StaticImageReaderTransform StaticImageDisplaySource::imageReaderTransform() const { return {}; }

qsizetype StaticImageDisplaySource::retainedRasterByteCost() const { return 0; }

void StaticImageDisplaySource::retainRasterOutputWorkspace(ImageDecodeWorkspaceHold hold)
{
    m_rasterOutputWorkspaceHold = std::move(hold);
}

bool StaticImageDisplaySource::hasRetainedRasterOutputWorkspace() const
{
    return m_rasterOutputWorkspaceHold.isManaged();
}

bool StaticDisplayImagePayload::isValid() const
{
    if (sourceIdentity.isEmpty() || !sourceRevision.isValid() || image.isNull()
        || !originalSize.isValid() || originalSize.isEmpty()) {
        return false;
    }
    if (refinementSource != nullptr) {
        const StaticImageReaderTransform sourceTransform = refinementSource->imageReaderTransform();
        if (refinementSource->imageSize() != originalSize
            || refinementSource->detailModel() != sourceDetailModel
            || sourceTransform.transformations != imageReaderTransform.transformations) {
            return false;
        }
    }
    const bool previewRaster = quality == DisplayImageQuality::ThumbnailPreview
        && previewOrigin != DisplayImagePreviewOrigin::None;
    if (rasterKind == DisplayImageRasterKind::ProvisionalPreview && !previewRaster) {
        return false;
    }
    if ((rasterKind == DisplayImageRasterKind::TimedFrame
            || rasterKind == DisplayImageRasterKind::Refinement)
        && previewRaster) {
        return false;
    }
    return quality != DisplayImageQuality::Exact
        || (sourceDetailModel == StaticImageSourceDetailModel::FiniteRaster
            && image.size() == originalSize);
}

bool StaticDisplayImagePayload::isAuthoritative() const
{
    return isValid()
        && (rasterKind == DisplayImageRasterKind::AuthoritativeStill
            || rasterKind == DisplayImageRasterKind::Refinement)
        && quality != DisplayImageQuality::ThumbnailPreview
        && previewOrigin == DisplayImagePreviewOrigin::None;
}

bool StaticDisplayImagePayload::isProvisionalPreview() const
{
    return isValid() && rasterKind == DisplayImageRasterKind::ProvisionalPreview
        && quality == DisplayImageQuality::ThumbnailPreview
        && previewOrigin != DisplayImagePreviewOrigin::None;
}

qsizetype StaticDisplayImagePayload::retainedRasterByteCost() const
{
    const qsizetype sourceRasterByteCost
        = refinementSource == nullptr ? 0 : refinementSource->retainedRasterByteCost();
    return saturatedQtByteSum(inputWorkspaceHold.reservedByteCount(),
        saturatedQtByteSum(sourceRasterByteCost, imageByteCost(image)));
}

qsizetype StaticDisplayImagePayload::byteCost() const
{
    if (!isValid()) {
        return 0;
    }

    const qsizetype sourceCost = refinementSource == nullptr ? 0 : refinementSource->byteCost();
    return saturatedQtByteSum(saturatedQtByteSum(sourceCost, imageByteCost(image)),
        embeddedMetadataStorageByteCost(embeddedMetadata));
}

std::optional<qsizetype> StaticDisplayImagePayload::byteCostWithinBudget(qsizetype byteBudget) const
{
    const qsizetype cost = byteCost();
    if (cost <= 0 || cost > byteBudget) {
        return std::nullopt;
    }

    return cost;
}
}
