// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "staticimage.h"

#include "cache/imagebyteaccounting.h"
#include "cache/imagebytecost.h"

namespace kiriview {
namespace {
    void appendDisplayDecodeFailure(StaticImageDisplayDecodeDiagnostics* diagnostics,
        StaticImageDisplayDecodeOperation operation, const QString& userMessage,
        const QString& diagnosticDetail)
    {
        if (diagnostics == nullptr) {
            return;
        }

        diagnostics->failures.push_back(StaticImageDisplayDecodeFailure {
            operation,
            userMessage,
            diagnosticDetail.isEmpty() ? userMessage : diagnosticDetail,
        });
    }
}

QString StaticImageDisplayDecodeDiagnostics::userMessage() const
{
    for (auto failure = failures.crbegin(); failure != failures.crend(); ++failure) {
        if (!failure->userMessage.isEmpty()) {
            return failure->userMessage;
        }
    }
    return {};
}

QString StaticImageDisplayDecodeDiagnostics::diagnosticDetail() const
{
    for (auto failure = failures.crbegin(); failure != failures.crend(); ++failure) {
        if (!failure->diagnosticDetail.isEmpty()) {
            return failure->diagnosticDetail;
        }
    }
    return {};
}

StaticImageFirstDisplayDecodeResult
StaticImageDisplaySource::decodeFirstDisplayImageWithDiagnostics(
    const ImageFirstDisplayDecodeContext& context) const
{
    QString errorString;
    StaticImageFirstDisplayDecodeResult result;
    result.firstDisplay = decodeFirstDisplayImage(context, &errorString);
    if (result.firstDisplay.status == FirstDisplayImageDecodeStatus::Error) {
        appendDisplayDecodeFailure(&result.diagnostics,
            StaticImageDisplayDecodeOperation::FirstDisplayImage, errorString, errorString);
    }
    return result;
}

FirstDisplayImageDecodeResult StaticImageDisplaySource::decodeFirstDisplayImage(
    const ImageFirstDisplayDecodeContext& context, QString* errorString) const
{
    Q_UNUSED(context);
    Q_UNUSED(errorString);
    return {};
}

bool StaticImageDisplaySource::supportsRasterDisplayRefinement() const { return false; }

StaticImageDisplayDecodeResult StaticImageDisplaySource::decodeRasterDisplayImageWithDiagnostics(
    const QSize& rasterSize) const
{
    if (rasterSize.isEmpty()) {
        return {};
    }

    QString errorString;
    StaticImageDisplayDecodeResult result;
    result.image = decodeRasterDisplayImage(rasterSize, &errorString);
    if (result.image.isNull() && !errorString.isEmpty()) {
        appendDisplayDecodeFailure(&result.diagnostics,
            StaticImageDisplayDecodeOperation::RasterDisplayImage, errorString, errorString);
    }
    return result;
}

QImage StaticImageDisplaySource::decodeRasterDisplayImage(
    const QSize& rasterSize, QString* errorString) const
{
    Q_UNUSED(rasterSize);
    Q_UNUSED(errorString);
    return {};
}

StaticImageDisplayDecodeResult StaticImageDisplaySource::decodeBlockingDisplayImageWithDiagnostics(
    int maximumLongEdge) const
{
    QString errorString;
    StaticImageDisplayDecodeResult result;
    result.image = decodeBlockingDisplayImage(maximumLongEdge, &errorString);
    if (result.image.isNull() && !errorString.isEmpty()) {
        appendDisplayDecodeFailure(&result.diagnostics,
            StaticImageDisplayDecodeOperation::BlockingDisplayImage, errorString, errorString);
    }
    return result;
}

bool StaticImageDisplaySource::isResolutionIndependent() const { return false; }

StaticImageReaderTransform StaticImageDisplaySource::imageReaderTransform() const { return {}; }

bool StaticDisplayImagePayload::isValid() const
{
    return !image.isNull() && originalSize.isValid() && !originalSize.isEmpty();
}

qsizetype StaticDisplayImagePayload::byteCost() const
{
    if (!isValid()) {
        return 0;
    }

    const qsizetype sourceCost = refinementSource == nullptr ? 0 : refinementSource->byteCost();
    return saturatedQtByteSum(sourceCost, imageByteCost(image));
}

std::optional<qsizetype> StaticDisplayImagePayload::byteCostWithinBudget(qsizetype byteBudget) const
{
    const qsizetype cost = byteCost();
    if (cost <= 0 || cost > byteBudget) {
        return std::nullopt;
    }

    return cost;
}
}
