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
class ImageDecodeWorkspaceBudget;

std::optional<qsizetype> svgParserWorkspaceByteCost(qsizetype sourceByteCount);

class SvgDisplaySource final : public StaticImageDisplaySource
{
public:
    static std::shared_ptr<SvgDisplaySource> open(const QByteArray& data, QString* errorString,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
        bool* resourceExhausted = nullptr);

    SvgDisplaySource(QByteArray data, QSize imageSize);
    ~SvgDisplaySource() override = default;

    [[nodiscard]] QSize imageSize() const override;
    [[nodiscard]] StaticImageSourceDetailModel detailModel() const override;
    [[nodiscard]] std::optional<qsizetype> initialDisplayDecodePeakByteCost(
        const ImageFirstDisplayDecodeContext& context, int blockingMaximumLongEdge) const override;
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

private:
    QByteArray m_data;
    QSize m_imageSize;
    Q_DISABLE_COPY_MOVE(SvgDisplaySource)
};
}

#endif
