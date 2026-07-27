// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "staticimage.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"

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
    if (image.isNull() || !originalSize.isValid() || originalSize.isEmpty()) {
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
    return quality != DisplayImageQuality::Exact
        || (sourceDetailModel == StaticImageSourceDetailModel::FiniteRaster
            && image.size() == originalSize);
}

bool StaticDisplayImagePayload::isAuthoritative() const
{
    return isValid() && quality != DisplayImageQuality::ThumbnailPreview
        && previewOrigin == DisplayImagePreviewOrigin::None;
}

bool StaticDisplayImagePayload::isProvisionalPreview() const
{
    return isValid() && quality == DisplayImageQuality::ThumbnailPreview
        && previewOrigin != DisplayImagePreviewOrigin::None;
}

qsizetype StaticDisplayImagePayload::byteCost() const
{
    if (!isValid()) {
        return 0;
    }

    const qsizetype sourceCost = refinementSource == nullptr ? 0 : refinementSource->byteCost();
    return saturatedQtByteSum(sourceCost, imageByteCost(image));
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
