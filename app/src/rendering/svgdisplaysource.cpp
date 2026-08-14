// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "svgdisplaysource.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "decoding/imagedecodeworkspace.h"
#include "imagerendering.h"
#include "localization/imageerrortext.h"
#include "staticimagedisplaysourcehelpers_p.h"
#include "svgworkerlimits.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QRectF>
#include <QSize>
#include <QStringList>
#include <QtEndian>
#include <QtMath>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <sys/resource.h>
#include <unistd.h>
#include <utility>

namespace {
constexpr int svgWorkerStartTimeoutMilliseconds = 5'000;
constexpr int svgWorkerFinishTimeoutMilliseconds = 30'000;
constexpr int svgWorkerLimitSetupFailureExitCode = 125;

enum class SvgWorkerStatus {
    Success,
    DecodeError,
    ResourceExhausted,
    InternalError,
};

struct SvgWorkerResult
{
    SvgWorkerStatus status = SvgWorkerStatus::InternalError;
    QByteArray output;
};

QString svgWorkerExecutablePath()
{
    QString installedPath = QDir(QCoreApplication::applicationDirPath())
                                .filePath(QStringLiteral("kiriview-svg-worker"));
    if (QFileInfo::exists(installedPath)) {
        return installedPath;
    }
#ifdef KIRIVIEW_SVG_WORKER_BUILD_PATH
    return QStringLiteral(KIRIVIEW_SVG_WORKER_BUILD_PATH);
#else
    return installedPath;
#endif
}

SvgWorkerResult runSvgWorker(const QStringList& arguments, const QByteArray& data)
{
    QProcess process;
    process.setProgram(svgWorkerExecutablePath());
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setChildProcessModifier([] {
        const auto limit = static_cast<rlim_t>(kiriview::svgWorkerAddressSpaceByteLimit);
        const rlimit limits { limit, limit };
        if (::setrlimit(RLIMIT_AS, &limits) != 0) {
            ::_exit(svgWorkerLimitSetupFailureExitCode);
        }
    });
    process.start(QIODevice::ReadWrite);
    if (!process.waitForStarted(svgWorkerStartTimeoutMilliseconds)) {
        return {};
    }
    if (process.write(data) != static_cast<qint64>(data.size())) {
        process.kill();
        process.waitForFinished();
        return {};
    }
    process.closeWriteChannel();
    if (!process.waitForFinished(svgWorkerFinishTimeoutMilliseconds)) {
        process.kill();
        process.waitForFinished();
        return { SvgWorkerStatus::ResourceExhausted, {} };
    }
    if (process.exitStatus() != QProcess::NormalExit) {
        return { SvgWorkerStatus::ResourceExhausted, {} };
    }
    if (process.exitCode() == kiriview::svgWorkerDecodeErrorExitCode) {
        return { SvgWorkerStatus::DecodeError, {} };
    }
    if (process.exitCode() == kiriview::svgWorkerResourceExhaustedExitCode) {
        return { SvgWorkerStatus::ResourceExhausted, {} };
    }
    if (process.exitCode() != EXIT_SUCCESS) {
        return {};
    }
    return { SvgWorkerStatus::Success, process.readAllStandardOutput() };
}

std::optional<QSize> svgIntrinsicSize(const QByteArray& data, bool* resourceExhausted)
{
    const SvgWorkerResult result = runSvgWorker({ QStringLiteral("intrinsic") }, data);
    if (result.status == SvgWorkerStatus::ResourceExhausted) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        return std::nullopt;
    }
    if (result.status != SvgWorkerStatus::Success || result.output.size() != 8) {
        return std::nullopt;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- fixed worker protocol.
    const auto* encoded = reinterpret_cast<const uchar*>(result.output.constData());
    const int width = qFromBigEndian<qint32>(encoded);
    const int height = qFromBigEndian<qint32>(encoded + 4);
    const QSize size(width, height);
    if (size.isEmpty()) {
        return std::nullopt;
    }
    return size;
}

QImage imageFromPremultipliedRgbaBytes(const QByteArray& bytes, QSize size, bool* resourceExhausted)
{
    if (bytes.isEmpty() || size.isEmpty()) {
        return {};
    }

    const std::uint64_t expectedSize
        = static_cast<std::uint64_t>(size.width()) * static_cast<std::uint64_t>(size.height()) * 4;
    if (expectedSize > static_cast<std::uint64_t>(std::numeric_limits<qsizetype>::max())
        || bytes.size() != static_cast<qsizetype>(expectedSize)) {
        return {};
    }

    QImage image = kiriview::copiedImageFromBytes(bytes, size,
        static_cast<qsizetype>(size.width()) * 4, QImage::Format_RGBA8888_Premultiplied);
    if (image.isNull() && resourceExhausted != nullptr) {
        *resourceExhausted = true;
    }
    return image;
}

QByteArray renderSvgImageBytes(
    const QByteArray& data, QSize size, bool* resourceExhausted = nullptr)
{
    if (size.isEmpty()) {
        return {};
    }

    const SvgWorkerResult result = runSvgWorker(
        { QStringLiteral("render"), QString::number(size.width()), QString::number(size.height()) },
        data);
    if (result.status == SvgWorkerStatus::ResourceExhausted && resourceExhausted != nullptr) {
        *resourceExhausted = true;
    }
    return result.status == SvgWorkerStatus::Success ? result.output : QByteArray {};
}

QImage renderSvgImage(const QByteArray& data, QSize size, bool* resourceExhausted = nullptr)
{
    if (resourceExhausted != nullptr) {
        *resourceExhausted = false;
    }
    return imageFromPremultipliedRgbaBytes(
        renderSvgImageBytes(data, size, resourceExhausted), size, resourceExhausted);
}

QSize svgFirstDisplayPreviewSize(QSize imageSize, QSize logicalViewportSize)
{
    if (imageSize.isEmpty() || logicalViewportSize.isEmpty()) {
        return {};
    }

    const QRectF targetRect = kiriview::imageTargetRect(imageSize, QSizeF(logicalViewportSize));
    if (targetRect.isEmpty()) {
        return {};
    }

    return QSize {
        std::clamp(qCeil(targetRect.width()), 1, logicalViewportSize.width()),
        std::clamp(qCeil(targetRect.height()), 1, logicalViewportSize.height()),
    };
}
}

namespace kiriview {
std::optional<qsizetype> svgParserWorkspaceByteCost(qsizetype sourceByteCount)
{
    if (sourceByteCount < 0) {
        return std::nullopt;
    }
    const qsizetype peakByteCost
        = saturatedQtByteSum(svgWorkerAddressSpaceByteLimit, sourceByteCount);
    return peakByteCost == std::numeric_limits<qsizetype>::max()
        ? std::nullopt
        : std::optional<qsizetype>(peakByteCost);
}

std::shared_ptr<SvgDisplaySource> SvgDisplaySource::open(const QByteArray& data,
    QString* errorString, std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    bool* resourceExhausted)
{
    if (resourceExhausted != nullptr) {
        *resourceExhausted = false;
    }
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    const std::optional<qsizetype> parserByteCost = svgParserWorkspaceByteCost(data.size());
    ImageDecodeWorkspaceLease parserWorkspace
        = ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    if (!parserByteCost.has_value()
        || !ImageDecodeWorkspaceDetail::tryReserve(parserWorkspace, *parserByteCost)) {
        if (resourceExhausted != nullptr) {
            *resourceExhausted = true;
        }
        setStaticImageDisplaySourceError(
            errorString, imageErrorText(ImageErrorTextId::ReadImageData));
        return {};
    }

    const std::optional<QSize> intrinsicSize = svgIntrinsicSize(data, resourceExhausted);
    if (!intrinsicSize.has_value()) {
        if (resourceExhausted != nullptr && *resourceExhausted) {
            setStaticImageDisplaySourceError(
                errorString, imageErrorText(ImageErrorTextId::ReadImageData));
        }
        return {};
    }

    return std::make_shared<SvgDisplaySource>(data, *intrinsicSize);
}

SvgDisplaySource::SvgDisplaySource(QByteArray data, QSize imageSize)
    : m_data(std::move(data))
    , m_imageSize(imageSize)
{
}

QSize SvgDisplaySource::imageSize() const { return m_imageSize; }

StaticImageSourceDetailModel SvgDisplaySource::detailModel() const
{
    return StaticImageSourceDetailModel::ScalableRasterization;
}

std::optional<qsizetype> SvgDisplaySource::initialDisplayDecodePeakByteCost(
    const ImageFirstDisplayDecodeContext& context, int blockingMaximumLongEdge) const
{
    if (!context.isValid()) {
        return rasterDisplayRefinementPeakByteCost(
            boundedPreviewSize(m_imageSize, blockingMaximumLongEdge));
    }

    const QSize firstDisplaySize
        = svgFirstDisplayPreviewSize(m_imageSize, context.logicalViewportSize);
    if (firstDisplaySize.isEmpty()) {
        return rasterDisplayRefinementPeakByteCost(
            boundedPreviewSize(m_imageSize, blockingMaximumLongEdge));
    }
    return rasterDisplayRefinementPeakByteCost(firstDisplaySize);
}

StaticImageFirstDisplayDecodeResult SvgDisplaySource::decodeFirstDisplayImage(
    const ImageFirstDisplayDecodeContext& context) const
{
    if (!context.isValid()) {
        return {};
    }

    const QSize previewSize = svgFirstDisplayPreviewSize(m_imageSize, context.logicalViewportSize);
    if (previewSize.isEmpty()) {
        return {};
    }

    bool resourceExhausted = false;
    QImage preview = renderSvgImage(m_data, previewSize, &resourceExhausted);
    if (preview.isNull()) {
        const QString message = imageErrorText(ImageErrorTextId::RenderSvgImage);
        return { { FirstDisplayImageDecodeStatus::Error, {} }, { message, message },
            resourceExhausted ? StaticImageDisplayDecodeFailureCause::ResourceExhausted
                              : StaticImageDisplayDecodeFailureCause::Decode };
    }

    return { { FirstDisplayImageDecodeStatus::Ready, std::move(preview) }, {} };
}

bool SvgDisplaySource::supportsRasterDisplayRefinement() const { return true; }

std::optional<qsizetype> SvgDisplaySource::rasterDisplayRefinementPeakByteCost(
    const QSize& rasterSize) const
{
    const qsizetype outputByteCost = estimatedRgbaByteCost(rasterSize);
    const std::optional<qsizetype> parserByteCost = svgParserWorkspaceByteCost(m_data.size());
    if (outputByteCost <= 0 || !parserByteCost.has_value()) {
        return std::nullopt;
    }
    const qsizetype peakByteCost
        = saturatedQtByteSum(*parserByteCost, saturatedQtByteProduct(outputByteCost, 2));
    return peakByteCost == std::numeric_limits<qsizetype>::max()
        ? std::nullopt
        : std::optional<qsizetype>(peakByteCost);
}

StaticImageDisplayDecodeResult SvgDisplaySource::decodeRasterDisplayImage(
    const QSize& rasterSize) const
{
    bool resourceExhausted = false;
    const QImage image = renderSvgImage(m_data, rasterSize, &resourceExhausted);
    if (image.isNull()) {
        const QString message = imageErrorText(ImageErrorTextId::RenderSvgImage);
        return { {}, { message, message },
            resourceExhausted ? StaticImageDisplayDecodeFailureCause::ResourceExhausted
                              : StaticImageDisplayDecodeFailureCause::Decode };
    }
    return { image, {} };
}

StaticImageDisplayDecodeResult SvgDisplaySource::decodeBlockingDisplayImage(
    int maximumLongEdge) const
{
    const QSize previewSize = boundedPreviewSize(m_imageSize, maximumLongEdge);
    bool resourceExhausted = false;
    const QImage preview = renderSvgImage(m_data, previewSize, &resourceExhausted);
    if (preview.isNull()) {
        const QString message = imageErrorText(ImageErrorTextId::RenderSvgImage);
        return { {}, { message, message },
            resourceExhausted ? StaticImageDisplayDecodeFailureCause::ResourceExhausted
                              : StaticImageDisplayDecodeFailureCause::Decode };
    }
    return { preview, {} };
}

qsizetype SvgDisplaySource::byteCost() const { return m_data.size(); }
}
