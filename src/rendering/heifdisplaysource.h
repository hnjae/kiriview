// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_HEIFDISPLAYSOURCE_H
#define KIRIVIEW_HEIFDISPLAYSOURCE_H

#include "decoding/heiftilingplan.h"
#include "staticimage.h"

#include <QByteArray>
#include <QRect>
#include <QString>
#include <QtGlobal>
#include <memory>
#include <optional>

namespace kiriview {
class HeifDisplaySource final : public StaticImageDisplaySource
{
public:
    HeifDisplaySource(QByteArray data, QSize imageSize, std::optional<HeifTileGrid> tileGrid);

    QSize imageSize() const override;
    qsizetype byteCost() const override;
    bool supportsRasterDisplayRefinement() const override;
    QImage decodeRasterDisplayImage(const QSize& rasterSize, QString* errorString) const override;
    QImage decodeBlockingDisplayImage(int maximumLongEdge, QString* errorString) const override;

private:
    QImage decodeFullOrScaled(QSize targetSize, QString* errorString) const;
    QImage decodeGridRasterDisplayImage(QSize rasterSize, QString* errorString) const;

    QByteArray m_data;
    QSize m_imageSize;
    std::optional<HeifTileGrid> m_tileGrid;
    Q_DISABLE_COPY(HeifDisplaySource)
};

std::shared_ptr<HeifDisplaySource> openHeifDisplaySource(
    const QByteArray& data, QString* errorString);
}

#endif
