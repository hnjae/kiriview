// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_SVGDISPLAYSOURCE_H
#define KIRIVIEW_SVGDISPLAYSOURCE_H

#include "staticimage.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <memory>

namespace kiriview {
class SvgDisplaySource final : public StaticImageDisplaySource
{
public:
    static std::shared_ptr<SvgDisplaySource> open(const QByteArray& data, QString* errorString);

    SvgDisplaySource(QByteArray data, QSize imageSize);

    QSize imageSize() const override;
    StaticImageFirstDisplayDecodeResult decodeFirstDisplayImage(
        const ImageFirstDisplayDecodeContext& context) const override;
    bool supportsRasterDisplayRefinement() const override;
    StaticImageDisplayDecodeResult decodeRasterDisplayImage(const QSize& rasterSize) const override;
    StaticImageDisplayDecodeResult decodeBlockingDisplayImage(int maximumLongEdge) const override;
    qsizetype byteCost() const override;

private:
    QByteArray m_data;
    QSize m_imageSize;
    Q_DISABLE_COPY(SvgDisplaySource)
};
}

#endif
