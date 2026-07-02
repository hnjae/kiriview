// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_QIMAGEREADERDISPLAYSOURCE_H
#define KIRIVIEW_QIMAGEREADERDISPLAYSOURCE_H

#include "staticimage.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>
#include <memory>

namespace kiriview {
using QImageReaderDisplayDecodeOperation = StaticImageDisplayDecodeOperation;
using QImageReaderDisplayDecodeFailureSeverity = StaticImageDisplayDecodeFailureSeverity;
using QImageReaderDisplayDecodeFailure = StaticImageDisplayDecodeFailure;
using QImageReaderDisplayDecodeDiagnostics = StaticImageDisplayDecodeDiagnostics;
using QImageReaderDisplayDecodeResult = StaticImageDisplayDecodeResult;
using QImageReaderFirstDisplayDecodeResult = StaticImageFirstDisplayDecodeResult;

class QImageReaderDisplaySource final : public StaticImageDisplaySource
{
public:
    static std::shared_ptr<QImageReaderDisplaySource> open(
        const QByteArray& data, const QByteArray& format, QString* errorString);

    QImageReaderDisplaySource(
        QByteArray data, QByteArray format, QSize imageSize, StaticImageReaderTransform transform);

    QSize imageSize() const override;
    QImageReaderFirstDisplayDecodeResult decodeFirstDisplayImageWithDiagnostics(
        const ImageFirstDisplayDecodeContext& context) const override;
    FirstDisplayImageDecodeResult decodeFirstDisplayImage(
        const ImageFirstDisplayDecodeContext& context, QString* errorString) const override;
    bool supportsRasterDisplayRefinement() const override;
    QImageReaderDisplayDecodeResult decodeRasterDisplayImageWithDiagnostics(
        const QSize& rasterSize) const override;
    QImage decodeRasterDisplayImage(const QSize& rasterSize, QString* errorString) const override;
    QImageReaderDisplayDecodeResult decodeBlockingDisplayImageWithDiagnostics(
        int maximumLongEdge) const override;
    QImage decodeBlockingDisplayImage(int maximumLongEdge, QString* errorString) const override;
    qsizetype byteCost() const override;
    StaticImageReaderTransform imageReaderTransform() const override;

private:
    bool supportsJpegScaledFirstDisplay() const;
    QImageReaderDisplayDecodeResult readScaledDisplayImage(
        QSize scaledSize, QImageReaderDisplayDecodeOperation operation) const;
    QImage readScaledImage(QSize scaledSize, QString* errorString) const;

    QByteArray m_data;
    QByteArray m_format;
    QSize m_imageSize;
    StaticImageReaderTransform m_transform;
};
}

#endif
