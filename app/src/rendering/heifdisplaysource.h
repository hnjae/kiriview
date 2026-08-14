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
class ImageDecodeWorkspaceBudget;

class HeifDisplaySource final : public StaticImageDisplaySource
{
public:
    HeifDisplaySource(QByteArray data, QSize imageSize, std::optional<HeifTileGrid> tileGrid);
    ~HeifDisplaySource() override = default;

    [[nodiscard]] QSize imageSize() const override;
    [[nodiscard]] qsizetype byteCost() const override;
    [[nodiscard]] std::optional<qsizetype> initialDisplayDecodePeakByteCost(
        const ImageFirstDisplayDecodeContext& context, int blockingMaximumLongEdge) const override;
    [[nodiscard]] bool supportsRasterDisplayRefinement() const override;
    [[nodiscard]] std::optional<qsizetype> rasterDisplayRefinementPeakByteCost(
        const QSize& rasterSize) const override;
    [[nodiscard]] StaticImageDisplayDecodeResult decodeRasterDisplayImage(
        const QSize& rasterSize) const override;
    [[nodiscard]] StaticImageDisplayDecodeResult decodeBlockingDisplayImage(
        int maximumLongEdge) const override;

private:
    QImage decodeFullOrScaled(
        QSize targetSize, QString* errorString, bool* resourceExhausted = nullptr) const;
    QImage decodeGridRasterDisplayImage(
        QSize rasterSize, QString* errorString, bool* resourceExhausted = nullptr) const;

    QByteArray m_data;
    QSize m_imageSize;
    std::optional<HeifTileGrid> m_tileGrid;
    Q_DISABLE_COPY_MOVE(HeifDisplaySource)
};

std::shared_ptr<HeifDisplaySource> openHeifDisplaySource(const QByteArray& data,
    QString* errorString, std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
    qsizetype perOperationBaselineByteCount = 0, bool* resourceExhausted = nullptr);
}

#endif
