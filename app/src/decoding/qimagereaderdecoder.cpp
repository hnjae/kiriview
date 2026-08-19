// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "qimagereaderdecoder.h"

#include "bufferedimagereader.h"
#include "decoding/imagerendering.h"
#include "decoding/qimagereaderdisplaysource.h"
#include "imageanimationrequest.h"
#include "imageanimationsourcecatalog.h"
#include "imagedecodeworkspace.h"
#include "location/sourcekey.h"
#include "staticimagedecode.h"

#include <QImage>
#include <memory>
#include <optional>
#include <utility>

namespace {
QString qtRasterFailureOperationName(kiriview::DecodedImageFailureOperation operation)
{
    switch (operation) {
    case kiriview::DecodedImageFailureOperation::Unknown:
        return QStringLiteral("unknown");
    case kiriview::DecodedImageFailureOperation::OpenStaticImageSource:
        return QStringLiteral("open static image source");
    case kiriview::DecodedImageFailureOperation::DecodeFirstDisplayImage:
        return QStringLiteral("decode first display image");
    case kiriview::DecodedImageFailureOperation::DecodeBlockingDisplayImage:
        return QStringLiteral("decode blocking display image");
    case kiriview::DecodedImageFailureOperation::DecodeRasterDisplayImage:
        return QStringLiteral("decode raster display image");
    case kiriview::DecodedImageFailureOperation::DecodeAnimationOpen:
        return QStringLiteral("decode animation open");
    case kiriview::DecodedImageFailureOperation::DecodeRawImage:
        return QStringLiteral("decode raw image");
    case kiriview::DecodedImageFailureOperation::DecodeHeifSequenceOpen:
        return QStringLiteral("decode HEIF sequence open");
    case kiriview::DecodedImageFailureOperation::DecodeHeifSequenceFrame:
        return QStringLiteral("decode HEIF sequence frame");
    }
    return QStringLiteral("unknown");
}

QString qtRasterFailureDiagnosticDetail(const QByteArray& format,
    kiriview::DecodedImageFailureOperation operation, const QString& backendError)
{
    return QStringLiteral("Qt image reader %1 failed for format %2: %3")
        .arg(qtRasterFailureOperationName(operation), QString::fromLatin1(format),
            backendError.isEmpty() ? QStringLiteral("<empty>") : backendError);
}

kiriview::DecodedImageResult failedQtRasterDecodedImageResult(
    kiriview::DecodedImageFailureOperation operation, const QByteArray& format,
    const QString& backendError,
    kiriview::DecodedImageFailureCause cause = kiriview::DecodedImageFailureCause::Unknown)
{
    return kiriview::failedDecodedImageResult(kiriview::DecodedImageFailure {
        kiriview::DecodedImageFailureRoute::QtRaster,
        operation,
        qtRasterFailureDiagnosticDetail(format, operation, backendError),
        kiriview::DecodedImageFailureSeverity::Error,
        false,
        cause,
    });
}

kiriview::DecodedImageResult failedQtRasterWorkspaceResult(const QByteArray& format)
{
    return failedQtRasterDecodedImageResult(
        kiriview::DecodedImageFailureOperation::DecodeAnimationOpen, format,
        kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
}

std::optional<qsizetype> qtReaderAnimationWorkspaceByteCount(QSize imageSize)
{
    return kiriview::qImageReaderGifTransientWorkspaceByteCount(imageSize);
}

std::optional<qsizetype> qtReaderAnimationOutputByteCount(QSize imageSize)
{
    return kiriview::checkedImageDecodeWorkspaceByteCount(imageSize, 4, 1);
}

void stampQtRasterFailure(kiriview::DecodedImageResult& result, const QByteArray& format)
{
    kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    if (failure == nullptr) {
        return;
    }
    failure->route = kiriview::DecodedImageFailureRoute::QtRaster;
    failure->diagnosticDetail
        = qtRasterFailureDiagnosticDetail(format, failure->operation, failure->diagnosticDetail);
}

kiriview::DecodedImageResult openedStaticImageResult(const QByteArray& data,
    const kiriview::ImageDecodeRequest& request, const QByteArray& format,
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget)
{
    kiriview::StaticImageDisplayDecodeDiagnostics diagnostics;
    std::shared_ptr<kiriview::StaticImageDisplaySource> source
        = kiriview::QImageReaderDisplaySource::open(data, format, &diagnostics);
    if (source == nullptr) {
        return failedQtRasterDecodedImageResult(
            kiriview::DecodedImageFailureOperation::OpenStaticImageSource, format,
            diagnostics.diagnosticDetail);
    }

    QString errorString;
    kiriview::DecodedImageResult result
        = kiriview::staticDecodedImageResult(std::move(source), request, &errorString,
            std::move(workspaceBudget), {}, kiriview::DecodedImageFailureRoute::QtRaster);
    stampQtRasterFailure(result, format);
    return result;
}

QString sourceIdentityForRequest(const kiriview::ImageDecodeRequest& request)
{
    return kiriview::sourceKeyForUrl(request.imageUrl()).identity;
}
}

namespace kiriview {
DecodedImageResult decodeQImageReaderImageData(const QByteArray& data,
    const ImageDecodeRequest& request, QtRasterFormat format,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    const QByteArray readerFormat = qtImageReaderFormat(format);
    if (format != QtRasterFormat::Gif) {
        return openedStaticImageResult(data, request, readerFormat, std::move(workspaceBudget));
    }

    ImageAnimationSourceCatalogResult catalog = readImageAnimationSourceCatalog(
        readerAnimationPlaybackRequest(data, readerFormat, {}, workspaceBudget));
    if (!catalog.has_value()) {
        if (catalog.error().cause
            == ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded) {
            return failedQtRasterWorkspaceResult(readerFormat);
        }
        return openedStaticImageResult(data, request, readerFormat, std::move(workspaceBudget));
    }

    const std::optional<qsizetype> workspaceByteCount
        = qtReaderAnimationWorkspaceByteCount(catalog->logicalSize);
    const std::optional<qsizetype> outputByteCount
        = qtReaderAnimationOutputByteCount(catalog->logicalSize);
    if (!workspaceByteCount.has_value() || !outputByteCount.has_value()) {
        return failedQtRasterWorkspaceResult(readerFormat);
    }
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    ImageDecodeWorkspaceLease workspaceLease
        = ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    if (!ImageDecodeWorkspaceDetail::tryReserve(workspaceLease, *workspaceByteCount)) {
        return failedQtRasterWorkspaceResult(readerFormat);
    }

    ImageDecodeWorkspaceLease outputLease = ImageDecodeWorkspaceDetail::startLeaseForOperation(
        *workspaceBudget, workspaceLease.reservedByteCount());
    if (!ImageDecodeWorkspaceDetail::tryReserve(outputLease, *outputByteCount)) {
        return failedQtRasterWorkspaceResult(readerFormat);
    }

    BufferedImageReader reader(data, readerFormat);
    if (!reader) {
        return failedQtRasterDecodedImageResult(DecodedImageFailureOperation::DecodeAnimationOpen,
            readerFormat, QStringLiteral("QImageReader could not be constructed"));
    }

    QImage image = reader.read();
    if (image.isNull()) {
        return failedQtRasterDecodedImageResult(
            DecodedImageFailureOperation::DecodeAnimationOpen, readerFormat, reader.errorString());
    }

    QImage firstFrame = displayReadyImage(image);
    if (firstFrame.isNull()) {
        return failedQtRasterWorkspaceResult(readerFormat);
    }
    if (catalog->logicalSize != firstFrame.size()) {
        const QString catalogFailure = QStringLiteral("animation source catalog size mismatch");
        return failedQtRasterDecodedImageResult(
            DecodedImageFailureOperation::DecodeAnimationOpen, readerFormat, catalogFailure);
    }
    return successfulDecodedImageResult(ReaderAnimationImage {
        outputLease.sharedHold(),
        std::move(firstFrame),
        {},
        {},
        data,
        readerFormat,
        std::move(*catalog),
        {},
        sourceIdentityForRequest(request),
        request.sourceRevision(),
    });
}
}
