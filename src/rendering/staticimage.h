// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_STATICIMAGE_H
#define KIRIVIEW_STATICIMAGE_H

#include "displayimagequality.h"
#include "metadata/embeddedmetadata.h"

#include <QImage>
#include <QImageIOHandler>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <memory>
#include <optional>
#include <vector>

namespace kiriview {
inline constexpr int imageBlockingDisplayLongEdgeMax = 2048;
inline constexpr qsizetype imageFullDecodeFallbackByteLimit = 512 * 1024 * 1024;

struct ImageFirstDisplayDecodeContext
{
    QSize physicalViewportSize;

    bool isValid() const { return !physicalViewportSize.isEmpty(); }
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
    qreal displayPixelsPerSourcePixel = 0.0;
};

enum class StaticImageDisplayDecodeOperation {
    FirstDisplayImage,
    RasterDisplayImage,
    BlockingDisplayImage,
};

enum class StaticImageDisplayDecodeFailureSeverity {
    Error,
};

struct StaticImageDisplayDecodeFailure
{
    StaticImageDisplayDecodeOperation operation
        = StaticImageDisplayDecodeOperation::RasterDisplayImage;
    QString userMessage;
    QString diagnosticDetail;
    StaticImageDisplayDecodeFailureSeverity severity
        = StaticImageDisplayDecodeFailureSeverity::Error;
    bool retryable = false;
};

struct StaticImageDisplayDecodeDiagnostics
{
    std::vector<StaticImageDisplayDecodeFailure> failures;

    QString userMessage() const;
    QString diagnosticDetail() const;
};

struct StaticImageDisplayDecodeResult
{
    QImage image;
    StaticImageDisplayDecodeDiagnostics diagnostics;
};

struct StaticImageFirstDisplayDecodeResult
{
    FirstDisplayImageDecodeResult firstDisplay;
    StaticImageDisplayDecodeDiagnostics diagnostics;
};

struct StaticImageReaderTransform
{
    QImageIOHandler::Transformations transformations = QImageIOHandler::TransformationNone;

    bool hasTransform() const { return transformations != QImageIOHandler::TransformationNone; }
};

class StaticImageDisplaySource
{
public:
    StaticImageDisplaySource() = default;

public:
    virtual ~StaticImageDisplaySource() = default;

    virtual QSize imageSize() const = 0;
    virtual StaticImageFirstDisplayDecodeResult decodeFirstDisplayImageWithDiagnostics(
        const ImageFirstDisplayDecodeContext& context) const;
    virtual FirstDisplayImageDecodeResult decodeFirstDisplayImage(
        const ImageFirstDisplayDecodeContext& context, QString* errorString) const;
    virtual bool supportsRasterDisplayRefinement() const;
    virtual StaticImageDisplayDecodeResult decodeRasterDisplayImageWithDiagnostics(
        const QSize& rasterSize) const;
    virtual QImage decodeRasterDisplayImage(const QSize& rasterSize, QString* errorString) const;
    virtual StaticImageDisplayDecodeResult decodeBlockingDisplayImageWithDiagnostics(
        int maximumLongEdge) const;
    virtual QImage decodeBlockingDisplayImage(int maximumLongEdge, QString* errorString) const = 0;
    virtual qsizetype byteCost() const = 0;
    virtual bool isResolutionIndependent() const;
    virtual StaticImageReaderTransform imageReaderTransform() const;
    Q_DISABLE_COPY(StaticImageDisplaySource)
};

struct StaticDisplayImagePayload
{
    QString sourceIdentity;
    StaticImageReaderTransform imageReaderTransform;
    QSize originalSize;
    QImage image;
    DisplayImageQuality quality = DisplayImageQuality::Exact;
    qreal displayPixelsPerSourcePixel = 0.0;
    EmbeddedMetadata embeddedMetadata;
    std::shared_ptr<StaticImageDisplaySource> refinementSource;
    DisplayImagePreviewOrigin previewOrigin = DisplayImagePreviewOrigin::None;

    bool isValid() const;
    qsizetype byteCost() const;
    std::optional<qsizetype> byteCostWithinBudget(qsizetype byteBudget) const;
};
}

#endif
