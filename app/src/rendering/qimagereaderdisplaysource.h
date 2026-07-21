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

    QImageReaderDisplaySource(
        QByteArray data, QByteArray format, QSize imageSize, StaticImageReaderTransform transform);

    QSize imageSize() const override;
    StaticImageFirstDisplayDecodeResult decodeFirstDisplayImage(
        const ImageFirstDisplayDecodeContext& context) const override;
    bool supportsRasterDisplayRefinement() const override;
    StaticImageDisplayDecodeResult decodeRasterDisplayImage(const QSize& rasterSize) const override;
    StaticImageDisplayDecodeResult decodeBlockingDisplayImage(int maximumLongEdge) const override;
    qsizetype byteCost() const override;
    StaticImageReaderTransform imageReaderTransform() const override;

private:
    bool supportsJpegScaledFirstDisplay() const;
    StaticImageDisplayDecodeResult readScaledDisplayImage(QSize scaledSize) const;
    QImage readScaledImage(QSize scaledSize, QString* errorString) const;

    QByteArray m_data;
    QByteArray m_format;
    QSize m_imageSize;
    StaticImageReaderTransform m_transform;
};
}

#endif
