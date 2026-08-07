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
class QImageReaderDisplaySource final : public StaticImageDisplaySource
{
public:
    static std::shared_ptr<QImageReaderDisplaySource> open(
        const QByteArray& data, const QByteArray& format, QString* errorString);

    QImageReaderDisplaySource(QByteArray data, QByteArray format, QSize imageSize,
        StaticImageReaderTransform transform, QSize readerImageSize = {},
        bool readerSupportsScaledSize = true,
        QImage::Format readerImageFormat = QImage::Format_RGBA8888_Premultiplied);

    [[nodiscard]] QSize imageSize() const override;
    [[nodiscard]] StaticImageFirstDisplayDecodeResult decodeFirstDisplayImage(
        const ImageFirstDisplayDecodeContext& context) const override;
    [[nodiscard]] bool supportsRasterDisplayRefinement() const override;
    [[nodiscard]] std::optional<qsizetype> rasterDisplayRefinementPeakByteCost(
        const QSize& rasterSize) const override;
    [[nodiscard]] StaticImageDisplayDecodeResult decodeRasterDisplayImage(
        const QSize& rasterSize) const override;
    [[nodiscard]] StaticImageDisplayDecodeResult decodeBlockingDisplayImage(
        int maximumLongEdge) const override;
    [[nodiscard]] qsizetype byteCost() const override;
    [[nodiscard]] StaticImageReaderTransform imageReaderTransform() const override;

private:
    [[nodiscard]] bool supportsJpegScaledFirstDisplay() const;
    [[nodiscard]] StaticImageDisplayDecodeResult readScaledDisplayImage(QSize scaledSize) const;
    QImage readScaledImage(
        QSize scaledSize, QString* errorString, bool* resourceExhausted = nullptr) const;

    QByteArray m_data;
    QByteArray m_format;
    QSize m_imageSize;
    StaticImageReaderTransform m_transform;
    QSize m_readerImageSize;
    bool m_readerSupportsScaledSize = true;
    QImage::Format m_readerImageFormat = QImage::Format_RGBA8888_Premultiplied;
};
}

#endif
