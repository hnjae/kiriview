// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_STATICIMAGE_H
#define KIRIVIEW_STATICIMAGE_H

#include "decoding/imagesourcedata.h"
#include "decoding/imagesourcerevision.h"
#include "displayimagequality.h"
#include "metadata/embeddedmetadata.h"

#include <QImage>
#include <QImageIOHandler>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <memory>
#include <optional>

namespace kiriview {
inline constexpr int imageBlockingDisplayLongEdgeMax = 2048;
inline constexpr qsizetype imageFullDecodeFallbackByteLimit = qsizetype { 512 } * 1024 * 1024;

struct ImageFirstDisplayDecodeContext
{
    QSize logicalViewportSize;

    [[nodiscard]] bool isValid() const { return !logicalViewportSize.isEmpty(); }
};

enum class FirstDisplayImageDecodeStatus {
    Ready,
    NotImplemented,
    Error,
};

struct FirstDisplayImageDecodeResult
{
    FirstDisplayImageDecodeStatus status = FirstDisplayImageDecodeStatus::NotImplemented;
    QImage image;
};

struct StaticImageDisplayDecodeDiagnostics
{
    QString userMessage;
    QString diagnosticDetail;
};

enum class StaticImageDisplayDecodeFailureCause {
    Decode,
    ResourceExhausted,
};

struct StaticImageDisplayDecodeResult
{
    QImage image;
    StaticImageDisplayDecodeDiagnostics diagnostics;
    StaticImageDisplayDecodeFailureCause failureCause
        = StaticImageDisplayDecodeFailureCause::Decode;
};

struct StaticImageFirstDisplayDecodeResult
{
    FirstDisplayImageDecodeResult firstDisplay;
    StaticImageDisplayDecodeDiagnostics diagnostics;
};

struct StaticImageReaderTransform
{
    QImageIOHandler::Transformations transformations = QImageIOHandler::TransformationNone;

    [[nodiscard]] bool hasTransform() const
    {
        return transformations != QImageIOHandler::TransformationNone;
    }
};

enum class StaticImageSourceDetailModel {
    FiniteRaster,
    ScalableRasterization,
};

class StaticImageDisplaySource
{
public:
    StaticImageDisplaySource() = default;
    virtual ~StaticImageDisplaySource() = default;

    [[nodiscard]] virtual QSize imageSize() const = 0;
    [[nodiscard]] virtual StaticImageSourceDetailModel detailModel() const;
    [[nodiscard]] virtual StaticImageFirstDisplayDecodeResult decodeFirstDisplayImage(
        const ImageFirstDisplayDecodeContext& context) const;
    [[nodiscard]] virtual bool supportsRasterDisplayRefinement() const;
    [[nodiscard]] virtual std::optional<qsizetype> rasterDisplayRefinementPeakByteCost(
        const QSize& rasterSize) const;
    [[nodiscard]] virtual StaticImageDisplayDecodeResult decodeRasterDisplayImage(
        const QSize& rasterSize) const;
    [[nodiscard]] virtual StaticImageDisplayDecodeResult decodeBlockingDisplayImage(
        int maximumLongEdge) const
        = 0;
    [[nodiscard]] virtual qsizetype byteCost() const = 0;
    [[nodiscard]] virtual StaticImageReaderTransform imageReaderTransform() const;
    Q_DISABLE_COPY_MOVE(StaticImageDisplaySource)
};

struct StaticDisplayImagePayload
{
    QString sourceIdentity;
    StaticImageReaderTransform imageReaderTransform;
    QSize originalSize;
    QImage image;
    DisplayImageQuality quality = DisplayImageQuality::Exact;
    EmbeddedMetadata embeddedMetadata;
    std::shared_ptr<StaticImageDisplaySource> refinementSource;
    DisplayImagePreviewOrigin previewOrigin = DisplayImagePreviewOrigin::None;
    StaticImageSourceDetailModel sourceDetailModel = StaticImageSourceDetailModel::FiniteRaster;
    ImageSourceRevision sourceRevision;
    DisplayImageRasterKind rasterKind = DisplayImageRasterKind::AuthoritativeStill;
    ImageSourceDataLease sourceDataLease;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isAuthoritative() const;
    [[nodiscard]] bool isProvisionalPreview() const;
    [[nodiscard]] qsizetype byteCost() const;
    [[nodiscard]] std::optional<qsizetype> byteCostWithinBudget(qsizetype byteBudget) const;
};
}

#endif
