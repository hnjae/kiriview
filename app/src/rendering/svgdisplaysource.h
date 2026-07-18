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
    FirstDisplayImageDecodeResult decodeFirstDisplayImage(
        const ImageFirstDisplayDecodeContext& context, QString* errorString) const override;
    bool supportsRasterDisplayRefinement() const override;
    QImage decodeRasterDisplayImage(const QSize& rasterSize, QString* errorString) const override;
    QImage decodeBlockingDisplayImage(int maximumLongEdge, QString* errorString) const override;
    qsizetype byteCost() const override;
    bool isResolutionIndependent() const override;

private:
    QByteArray m_data;
    QSize m_imageSize;
    Q_DISABLE_COPY(SvgDisplaySource)
};
}

#endif
