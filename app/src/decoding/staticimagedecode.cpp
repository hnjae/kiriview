// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "staticimagedecode.h"

#include "decoding/imagedecoderequest.h"
#include "location/sourcekey.h"
#include "rendering/imagerendering.h"

#include <QImage>
#include <utility>

namespace {
QString errorStringValue(QString* errorString)
{
    return errorString == nullptr ? QString() : *errorString;
}

kiriview::DecodedImageResult failedStaticDecodedImageResult(
    kiriview::DecodedImageFailureOperation operation, QString* errorString,
    const kiriview::StaticImageDisplayDecodeDiagnostics* diagnostics = nullptr)
{
    QString message = diagnostics == nullptr ? QString() : diagnostics->userMessage;
    if (message.isEmpty()) {
        message = errorStringValue(errorString);
    }
    QString diagnosticDetail = diagnostics == nullptr ? QString() : diagnostics->diagnosticDetail;
    if (diagnosticDetail.isEmpty()) {
        diagnosticDetail = message;
    }
    if (errorString != nullptr && !message.isEmpty()) {
        *errorString = message;
    }
    return kiriview::failedDecodedImageResult(kiriview::DecodedImageFailure {
        message,
        kiriview::DecodedImageFailureRoute::Unknown,
        operation,
        diagnosticDetail,
        kiriview::DecodedImageFailureSeverity::Error,
        false,
    });
}

QString sourceIdentityForRequest(const kiriview::ImageDecodeRequest& request)
{
    return kiriview::sourceKeyForUrl(request.imageUrl()).identity;
}

kiriview::DisplayImageQuality displayQualityForImage(
    QSize originalSize, const QImage& image, bool firstDisplay)
{
    if (firstDisplay || image.size() != originalSize) {
        return kiriview::DisplayImageQuality::FirstDisplay;
    }
    return kiriview::DisplayImageQuality::Exact;
}

kiriview::StaticDisplayImagePayload staticDisplayPayload(
    std::shared_ptr<kiriview::StaticImageDisplaySource> source,
    const kiriview::ImageDecodeRequest& request, const QImage& image, bool firstDisplay)
{
    QImage displayImage = kiriview::displayReadyImage(image);
    const QSize originalSize = source == nullptr ? QSize() : source->imageSize();
    const kiriview::DisplayImageQuality quality
        = displayQualityForImage(originalSize, displayImage, firstDisplay);
    kiriview::StaticDisplayImagePayload payload {
        sourceIdentityForRequest(request),
        source == nullptr ? kiriview::StaticImageReaderTransform {}
                          : source->imageReaderTransform(),
        originalSize,
        std::move(displayImage),
        quality,
        {},
        std::move(source),
        kiriview::DisplayImagePreviewOrigin::None,
    };
    return payload;
}
}

namespace kiriview {
DecodedImageResult staticDecodedImageResult(std::shared_ptr<StaticImageDisplaySource> source,
    const ImageDecodeRequest& request, QString* errorString)
{
    if (source == nullptr) {
        return failedStaticDecodedImageResult(
            DecodedImageFailureOperation::OpenStaticImageSource, errorString);
    }

    StaticImageFirstDisplayDecodeResult firstDisplayResult
        = source->decodeFirstDisplayImage(request.firstDisplay());
    switch (firstDisplayResult.firstDisplay.status) {
    case FirstDisplayImageDecodeStatus::Ready:
        if (firstDisplayResult.firstDisplay.image.isNull()) {
            return failedStaticDecodedImageResult(
                DecodedImageFailureOperation::DecodeFirstDisplayImage, errorString,
                &firstDisplayResult.diagnostics);
        }
        return successfulDecodedImageResult(StaticDecodedImage {
            staticDisplayPayload(
                std::move(source), request, firstDisplayResult.firstDisplay.image, true),
            {},
        });
    case FirstDisplayImageDecodeStatus::NotImplemented:
        break;
    case FirstDisplayImageDecodeStatus::Error:
        return failedStaticDecodedImageResult(DecodedImageFailureOperation::DecodeFirstDisplayImage,
            errorString, &firstDisplayResult.diagnostics);
    }

    StaticImageDisplayDecodeResult previewResult
        = source->decodeBlockingDisplayImage(imageBlockingDisplayLongEdgeMax);
    if (previewResult.image.isNull()) {
        return failedStaticDecodedImageResult(
            DecodedImageFailureOperation::DecodeBlockingDisplayImage, errorString,
            &previewResult.diagnostics);
    }

    return successfulDecodedImageResult(StaticDecodedImage {
        staticDisplayPayload(std::move(source), request, previewResult.image, false),
        {},
    });
}
}
