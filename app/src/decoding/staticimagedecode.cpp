// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "staticimagedecode.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"
#include "decoding/imagedecoderequest.h"
#include "decoding/imagedecodeworkspace.h"
#include "decoding/imagerendering.h"
#include "location/sourcekey.h"

#include <QImage>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace {
QString errorStringValue(QString* errorString)
{
    return errorString == nullptr ? QString() : *errorString;
}

kiriview::DecodedImageResult failedStaticDecodedImageResult(
    kiriview::DecodedImageFailureRoute route, kiriview::DecodedImageFailureOperation operation,
    QString* errorString,
    const kiriview::StaticImageDisplayDecodeDiagnostics* diagnostics = nullptr,
    kiriview::DecodedImageFailureCause cause = kiriview::DecodedImageFailureCause::Unknown)
{
    QString diagnosticDetail = diagnostics == nullptr ? QString() : diagnostics->diagnosticDetail;
    if (diagnosticDetail.isEmpty()) {
        diagnosticDetail = errorStringValue(errorString);
    }
    if (errorString != nullptr && !diagnosticDetail.isEmpty()) {
        *errorString = diagnosticDetail;
    }
    return kiriview::failedDecodedImageResult(kiriview::DecodedImageFailure {
        route,
        operation,
        diagnosticDetail,
        kiriview::DecodedImageFailureSeverity::Error,
        false,
        cause,
    });
}

kiriview::DecodedImageResult failedStaticWorkspaceResult(kiriview::DecodedImageFailureRoute route,
    kiriview::DecodedImageFailureOperation operation, QString* errorString)
{
    kiriview::StaticImageDisplayDecodeDiagnostics diagnostics;
    diagnostics.diagnosticDetail = kiriview::imageDecodeWorkspaceResourceLimitDiagnostic();
    return failedStaticDecodedImageResult(route, operation, errorString, &diagnostics,
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
}

QString sourceIdentityForRequest(const kiriview::ImageDecodeRequest& request)
{
    return kiriview::sourceKeyForUrl(request.imageUrl()).identity;
}

kiriview::DisplayImageQuality displayQualityForImage(
    kiriview::StaticImageSourceDetailModel detailModel, QSize originalSize, const QImage& image,
    bool firstDisplay)
{
    if (firstDisplay || detailModel != kiriview::StaticImageSourceDetailModel::FiniteRaster
        || image.size() != originalSize) {
        return kiriview::DisplayImageQuality::FirstDisplay;
    }
    return kiriview::DisplayImageQuality::Exact;
}

std::optional<kiriview::StaticDisplayImagePayload> staticDisplayPayload(
    std::shared_ptr<kiriview::StaticImageDisplaySource> source,
    const kiriview::ImageDecodeRequest& request, QImage image, bool firstDisplay,
    kiriview::ImageDecodeWorkspaceLease producerLease,
    kiriview::DecodedImageFailureRoute decodeRoute)
{
    QImage displayImage = kiriview::displayReadyImage(image);
    image = {};
    const QSize originalSize = source == nullptr ? QSize() : source->imageSize();
    const kiriview::StaticImageSourceDetailModel detailModel = source == nullptr
        ? kiriview::StaticImageSourceDetailModel::FiniteRaster
        : source->detailModel();
    const kiriview::DisplayImageQuality quality
        = displayQualityForImage(detailModel, originalSize, displayImage, firstDisplay);
    const qsizetype sourceRasterByteCost = source == nullptr ? 0 : source->retainedRasterByteCost();
    if (sourceRasterByteCost > 0 && !source->hasRetainedRasterOutputWorkspace()) {
        return {};
    }
    const qsizetype retainedByteCost = kiriview::imageByteCost(displayImage);
    if (displayImage.isNull() || retainedByteCost <= 0
        || retainedByteCost == std::numeric_limits<qsizetype>::max()) {
        return {};
    }

    kiriview::ImageDecodeWorkspaceHold workspaceHold = producerLease.retainOnly(retainedByteCost);
    if (!workspaceHold.isManaged()) {
        return {};
    }
    QImage admittedImage
        = kiriview::imageRetainingDecodeWorkspace(std::move(displayImage), workspaceHold);
    if (admittedImage.isNull()) {
        return {};
    }
    kiriview::StaticDisplayImagePayload payload {
        sourceIdentityForRequest(request),
        source == nullptr ? kiriview::StaticImageReaderTransform {}
                          : source->imageReaderTransform(),
        originalSize,
        std::move(admittedImage),
        quality,
        {},
        {},
        {},
        std::move(source),
        kiriview::DisplayImagePreviewOrigin::None,
        detailModel,
        request.sourceRevision(),
        kiriview::DisplayImageRasterKind::AuthoritativeStill,
        decodeRoute,
    };
    return payload;
}
}

namespace kiriview {
DecodedImageResult staticDecodedImageResult(std::shared_ptr<StaticImageDisplaySource> source,
    const ImageDecodeRequest& request, QString* errorString,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    ImageDecodeWorkspaceLease producerLease, DecodedImageFailureRoute decodeRoute)
{
    if (source == nullptr) {
        return failedStaticDecodedImageResult(
            decodeRoute, DecodedImageFailureOperation::OpenStaticImageSource, errorString);
    }

    const std::optional<qsizetype> peakByteCost = source->initialDisplayDecodePeakByteCost(
        request.firstDisplay(), imageBlockingDisplayLongEdgeMax);
    if (!peakByteCost.has_value() || *peakByteCost <= 0
        || *peakByteCost == std::numeric_limits<qsizetype>::max()) {
        return failedStaticWorkspaceResult(
            decodeRoute, DecodedImageFailureOperation::DecodeBlockingDisplayImage, errorString);
    }
    if (!producerLease.isManaged()) {
        if (workspaceBudget == nullptr) {
            workspaceBudget = defaultImageDecodeWorkspaceBudget();
        }
        producerLease = ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    }
    const qsizetype alreadyReservedByteCount = producerLease.reservedByteCount();
    if (alreadyReservedByteCount < *peakByteCost
        && !ImageDecodeWorkspaceDetail::tryReserve(
            producerLease, *peakByteCost - alreadyReservedByteCount)) {
        return failedStaticWorkspaceResult(
            decodeRoute, DecodedImageFailureOperation::DecodeBlockingDisplayImage, errorString);
    }

    StaticImageFirstDisplayDecodeResult firstDisplayResult
        = source->decodeFirstDisplayImage(request.firstDisplay());
    switch (firstDisplayResult.firstDisplay.status) {
    case FirstDisplayImageDecodeStatus::Ready: {
        if (firstDisplayResult.firstDisplay.image.isNull()) {
            return failedStaticDecodedImageResult(decodeRoute,
                DecodedImageFailureOperation::DecodeFirstDisplayImage, errorString,
                &firstDisplayResult.diagnostics);
        }
        std::optional<StaticDisplayImagePayload> payload = staticDisplayPayload(std::move(source),
            request, std::move(firstDisplayResult.firstDisplay.image), true,
            std::move(producerLease), decodeRoute);
        return payload.has_value()
            ? successfulDecodedImageResult(StaticDecodedImage { std::move(*payload), {} })
            : failedStaticWorkspaceResult(
                  decodeRoute, DecodedImageFailureOperation::DecodeFirstDisplayImage, errorString);
    }
    case FirstDisplayImageDecodeStatus::NotImplemented:
        break;
    case FirstDisplayImageDecodeStatus::Error:
        return failedStaticDecodedImageResult(decodeRoute,
            DecodedImageFailureOperation::DecodeFirstDisplayImage, errorString,
            &firstDisplayResult.diagnostics,
            firstDisplayResult.failureCause
                    == StaticImageDisplayDecodeFailureCause::ResourceExhausted
                ? DecodedImageFailureCause::ResourceLimitExceeded
                : DecodedImageFailureCause::Unknown);
    }

    StaticImageDisplayDecodeResult previewResult
        = source->decodeBlockingDisplayImage(imageBlockingDisplayLongEdgeMax);
    if (previewResult.image.isNull()) {
        return failedStaticDecodedImageResult(decodeRoute,
            DecodedImageFailureOperation::DecodeBlockingDisplayImage, errorString,
            &previewResult.diagnostics,
            previewResult.failureCause == StaticImageDisplayDecodeFailureCause::ResourceExhausted
                ? DecodedImageFailureCause::ResourceLimitExceeded
                : DecodedImageFailureCause::Unknown);
    }

    std::optional<StaticDisplayImagePayload> payload = staticDisplayPayload(std::move(source),
        request, std::move(previewResult.image), false, std::move(producerLease), decodeRoute);
    return payload.has_value()
        ? successfulDecodedImageResult(StaticDecodedImage { std::move(*payload), {} })
        : failedStaticWorkspaceResult(
              decodeRoute, DecodedImageFailureOperation::DecodeBlockingDisplayImage, errorString);
}
}
