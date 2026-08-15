// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_SVGDISPLAYSOURCE_H
#define KIRIVIEW_SVGDISPLAYSOURCE_H

#include "staticimage.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <memory>

namespace kiriview {
class ImageDecodeWorkspaceBudget;

std::optional<qsizetype> svgParserWorkspaceByteCost(qsizetype sourceByteCount);

enum class SvgWorkerProcessOutcome {
    StartFailed,
    WriteFailed,
    TimedOut,
    Crashed,
    Exited,
};

struct SvgWorkerProcessRequest
{
    QString program;
    QStringList arguments;
    QByteArray input;
    int startTimeoutMilliseconds = 0;
    int finishTimeoutMilliseconds = 0;
    qsizetype addressSpaceByteLimit = 0;
};

struct SvgWorkerProcessResult
{
    SvgWorkerProcessOutcome outcome = SvgWorkerProcessOutcome::StartFailed;
    int exitCode = -1;
    QByteArray output;
};

class SvgWorkerProcessExecutor
{
public:
    SvgWorkerProcessExecutor() = default;
    virtual ~SvgWorkerProcessExecutor() = default;
    [[nodiscard]] virtual SvgWorkerProcessResult execute(
        const SvgWorkerProcessRequest& request) const
        = 0;
    Q_DISABLE_COPY_MOVE(SvgWorkerProcessExecutor)
};

class SvgDisplaySource final : public StaticImageDisplaySource
{
public:
    static std::shared_ptr<SvgDisplaySource> open(const QByteArray& data, QString* errorString,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
        bool* resourceExhausted = nullptr,
        std::shared_ptr<const SvgWorkerProcessExecutor> processExecutor = {});

    SvgDisplaySource(QByteArray data, QSize imageSize,
        std::shared_ptr<const SvgWorkerProcessExecutor> processExecutor = {});
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
    const std::shared_ptr<const SvgWorkerProcessExecutor> m_processExecutor;
    Q_DISABLE_COPY_MOVE(SvgDisplaySource)
};
}

#endif
