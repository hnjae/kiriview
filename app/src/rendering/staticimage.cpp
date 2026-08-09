// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "staticimage.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"

#include <algorithm>
#include <cstdint>
#include <limits>

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
StaticImageSourceDetailModel StaticImageDisplaySource::detailModel() const
{
    return StaticImageSourceDetailModel::FiniteRaster;
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
