// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedecodepipeline.h"

#include "apnganimationreader.h"
#include "avifcompatibility.h"
#include "bufferedimagereader.h"
#include "cache/imagebyteaccounting.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "heifcontainer.h"
#include "heifdecoder.h"
#include "imageanimationrequest.h"
#include "imageanimationsourcecatalog.h"
#include "imagedecodelogging.h"
#include "imagedecodeworkspace.h"
#include "jxlanimationreader.h"
#include "localization/imageerrortext.h"
#include "location/sourcekey.h"
#include "metadata/embeddedmetadata.h"
#include "qimagereaderdecoder.h"
#include "rawdecoder.h"
#include "rendering/heifdisplaysource.h"
#include "rendering/imagerendering.h"
#include "rendering/qimagereaderdisplaysource.h"
#include "rendering/svgdisplaysource.h"
#include "staticimagedecode.h"
#include "webpanimationreader.h"

#include <QDebug>
#include <QString>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <utility>
#include <variant>

Q_LOGGING_CATEGORY(kiriviewDecodeLog, "org.hnjae.kiriview.decode", QtWarningMsg)

namespace {
constexpr qsizetype embeddedMetadataWorkspaceReservation = qsizetype { 64 } * 1024 * 1024;

qsizetype decodedImageWorkspaceByteCost(const kiriview::DecodedImage& image)
{
    return std::visit(
        [](const auto& decoded) -> qsizetype {
            const qsizetype inputByteCost = [&decoded]() -> qsizetype {
                if constexpr (requires { decoded.inputWorkspaceHold; }) {
                    return decoded.inputWorkspaceHold.reservedByteCount();
                } else if constexpr (requires { decoded.displayImage.inputWorkspaceHold; }) {
                    return decoded.displayImage.inputWorkspaceHold.reservedByteCount();
                }
                return 0;
            }();
            if constexpr (requires { decoded.firstFrameWorkspaceHold; }) {
                return kiriview::saturatedQtByteSum(
                    inputByteCost, decoded.firstFrameWorkspaceHold.reservedByteCount());
            } else if constexpr (requires { decoded.displayImage.retainedRasterByteCost(); }) {
                return decoded.displayImage.retainedRasterByteCost();
            }
            return inputByteCost;
        },
        image);
}

void retainSourceDataLease(
    kiriview::DecodedImageResult& result, kiriview::ImageSourceDataLease lease)
{
    kiriview::DecodedImage* image = kiriview::decodedImageResultImage(result);
    if (image == nullptr) {
        return;
    }
    std::visit(
        [lease = std::move(lease)](auto& decoded) mutable {
            using Image = std::decay_t<decltype(decoded)>;
            if constexpr (std::is_same_v<Image, kiriview::StaticDecodedImage>) {
                decoded.displayImage.sourceDataLease = std::move(lease);
            } else {
                decoded.sourceDataLease = std::move(lease);
            }
        },
        *image);
}

const char* imageInputKindName(kiriview::ImageInputKind kind)
{
    switch (kind) {
    case kiriview::ImageInputKind::Unknown:
        return "Unknown";
    case kiriview::ImageInputKind::Svg:
        return "Svg";
    case kiriview::ImageInputKind::Apng:
        return "Apng";
    case kiriview::ImageInputKind::HeifFamily:
        return "HeifFamily";
    case kiriview::ImageInputKind::Raw:
        return "Raw";
    case kiriview::ImageInputKind::QtRaster:
        return "QtRaster";
    }

    return "Unknown";
}

const char* imageDecodeHandlerKindName(kiriview::ImageDecodeHandlerKind kind)
{
    switch (kind) {
    case kiriview::ImageDecodeHandlerKind::None:
        return "None";
    case kiriview::ImageDecodeHandlerKind::Svg:
        return "Svg";
    case kiriview::ImageDecodeHandlerKind::Apng:
        return "Apng";
    case kiriview::ImageDecodeHandlerKind::HeifFamily:
        return "HeifFamily";
    case kiriview::ImageDecodeHandlerKind::Raw:
        return "Raw";
    case kiriview::ImageDecodeHandlerKind::QtRaster:
        return "QtRaster";
    }

    return "None";
}

kiriview::DecodedImageFailureRoute decodedFailureRouteForHandlerKind(
    kiriview::ImageDecodeHandlerKind kind)
{
    switch (kind) {
    case kiriview::ImageDecodeHandlerKind::Svg:
        return kiriview::DecodedImageFailureRoute::Svg;
    case kiriview::ImageDecodeHandlerKind::Apng:
        return kiriview::DecodedImageFailureRoute::Apng;
    case kiriview::ImageDecodeHandlerKind::HeifFamily:
        return kiriview::DecodedImageFailureRoute::HeifFamily;
    case kiriview::ImageDecodeHandlerKind::Raw:
        return kiriview::DecodedImageFailureRoute::Raw;
    case kiriview::ImageDecodeHandlerKind::QtRaster:
        return kiriview::DecodedImageFailureRoute::QtRaster;
    case kiriview::ImageDecodeHandlerKind::None:
        return kiriview::DecodedImageFailureRoute::Unknown;
    }

    return kiriview::DecodedImageFailureRoute::Unknown;
}

const char* imageDecodeDataSourceName(kiriview::ImageDecodeDataSource source)
{
    switch (source) {
    case kiriview::ImageDecodeDataSource::Original:
        return "Original";
    case kiriview::ImageDecodeDataSource::AvifCompatible:
        return "AvifCompatible";
    }

    return "Original";
}

const char* qtRasterFormatName(kiriview::QtRasterFormat format)
{
    switch (format) {
    case kiriview::QtRasterFormat::None:
        return "None";
    case kiriview::QtRasterFormat::Png:
        return "Png";
    case kiriview::QtRasterFormat::Jpeg:
        return "Jpeg";
    case kiriview::QtRasterFormat::Gif:
        return "Gif";
    case kiriview::QtRasterFormat::Webp:
        return "Webp";
    case kiriview::QtRasterFormat::Bmp:
        return "Bmp";
    case kiriview::QtRasterFormat::Tiff:
        return "Tiff";
    case kiriview::QtRasterFormat::Jxl:
        return "Jxl";
    case kiriview::QtRasterFormat::Jp2:
        return "Jp2";
    }

    return "None";
}

kiriview::ImageDecodeCompatibleDataTransform::Result avifCompatibleImageData(const QByteArray& data)
{
    kiriview::AvifCompatibleData compatibleData = kiriview::avifDataWithCompatibilityFixes(data);
    return {
        std::move(compatibleData.data),
        compatibleData.storage == kiriview::AvifCompatibleDataStorage::OwnedReplacement
            ? kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement
            : kiriview::ImageDecodeCompatibleDataTransform::Storage::Original,
    };
}

class ImageDecodeRouterByteInputs
{
public:
    ImageDecodeRouterByteInputs(const QByteArray& originalData,
        const kiriview::ImageDecodeCompatibleDataTransform& compatibleDataTransform,
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget)
        : m_originalData(originalData)
        , m_compatibleDataTransform(compatibleDataTransform)
        , m_workspaceBudget(std::move(workspaceBudget))
    {
    }

    const QByteArray* dataFor(kiriview::ImageDecodeDataSource dataSource)
    {
        switch (dataSource) {
        case kiriview::ImageDecodeDataSource::Original:
            return &m_originalData;
        case kiriview::ImageDecodeDataSource::AvifCompatible:
            return compatibleData();
        }

        return &m_originalData;
    }

    [[nodiscard]] bool resourceExhausted() const { return m_resourceExhausted; }

    [[nodiscard]] bool compatibleDataRequiresRetention() const
    {
        return m_compatibleData.has_value()
            && m_compatibleData->storage
            == kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement;
    }

    [[nodiscard]] qsizetype retainedInputWorkspaceByteCount() const
    {
        return compatibleDataRetainedByteCount();
    }

    kiriview::ImageDecodeWorkspaceHold takeRetainedWorkspace()
    {
        return compatibleDataRequiresRetention()
            ? m_workspaceLease.retainOnly(compatibleDataRetainedByteCount())
            : kiriview::ImageDecodeWorkspaceHold {};
    }

private:
    [[nodiscard]] qsizetype compatibleDataRetainedByteCount() const
    {
        return compatibleDataRequiresRetention() ? m_compatibleData->data.capacity() : 0;
    }

    const QByteArray* compatibleData()
    {
        if (!m_compatibleData.has_value()) {
            const std::optional<qsizetype> workspaceByteCost = m_compatibleDataTransform
                ? (m_compatibleDataTransform.workspaceByteCost
                          ? m_compatibleDataTransform.workspaceByteCost(m_originalData.size())
                          : std::nullopt)
                : kiriview::avifCompatibilityWorkspaceByteCost(m_originalData.size());
            if (!workspaceByteCost.has_value() || m_workspaceBudget == nullptr) {
                m_resourceExhausted = true;
                return nullptr;
            }
            m_workspaceLease = kiriview::ImageDecodeWorkspaceDetail::startLease(*m_workspaceBudget);
            if (!kiriview::ImageDecodeWorkspaceDetail::tryReserve(
                    m_workspaceLease, *workspaceByteCost)) {
                m_workspaceLease = {};
                m_resourceExhausted = true;
                return nullptr;
            }
            try {
                m_compatibleData = m_compatibleDataTransform
                    ? m_compatibleDataTransform.apply(m_originalData)
                    : avifCompatibleImageData(m_originalData);
            } catch (const std::bad_alloc&) {
                m_workspaceLease = {};
                m_resourceExhausted = true;
                return nullptr;
            }
            if (m_compatibleData->storage
                == kiriview::ImageDecodeCompatibleDataTransform::Storage::Original) {
                m_compatibleData->data = m_originalData;
            }
            if (compatibleDataRequiresRetention()
                && (m_compatibleData->data.capacity() < m_compatibleData->data.size()
                    || compatibleDataRetainedByteCount() > m_workspaceLease.reservedByteCount())) {
                m_compatibleData.reset();
                m_workspaceLease = {};
                m_resourceExhausted = true;
                return nullptr;
            }
            if (!compatibleDataRequiresRetention()) {
                m_workspaceLease = {};
            }
        }
        return &m_compatibleData->data;
    }

    const QByteArray& m_originalData;
    const kiriview::ImageDecodeCompatibleDataTransform& m_compatibleDataTransform;
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> m_workspaceBudget;
    kiriview::ImageDecodeWorkspaceLease m_workspaceLease;
    std::optional<kiriview::ImageDecodeCompatibleDataTransform::Result> m_compatibleData;
    bool m_resourceExhausted = false;
};

kiriview::DecodedImageResult failedReadImageDataResult()
{
    return kiriview::failedDecodedImageResult(
        kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData));
}

kiriview::DecodedImageFailure compatibleDataWorkspaceFailure(
    kiriview::ImageDecodeHandlerKind handlerKind)
{
    return kiriview::DecodedImageFailure {
        kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData),
        decodedFailureRouteForHandlerKind(handlerKind),
        kiriview::DecodedImageFailureOperation::OpenStaticImageSource,
        kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
        kiriview::DecodedImageFailureSeverity::Error,
        false,
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
    };
}

kiriview::DecodedImageResult failedCompatibleDataWorkspaceResult(
    kiriview::ImageDecodeHandlerKind handlerKind)
{
    return kiriview::failedDecodedImageResult(compatibleDataWorkspaceFailure(handlerKind));
}

kiriview::DecodedImageFailure preparedHardLimitFailure(kiriview::ImageDecodeHandlerKind handlerKind,
    std::optional<kiriview::DecodedImageFailureOperation> stageOperation = std::nullopt)
{
    kiriview::DecodedImageFailureOperation operation
        = kiriview::DecodedImageFailureOperation::DecodeBlockingDisplayImage;
    switch (handlerKind) {
    case kiriview::ImageDecodeHandlerKind::Apng:
        operation = kiriview::DecodedImageFailureOperation::DecodeAnimationOpen;
        break;
    case kiriview::ImageDecodeHandlerKind::Raw:
        operation = kiriview::DecodedImageFailureOperation::DecodeRawImage;
        break;
    case kiriview::ImageDecodeHandlerKind::HeifFamily:
        operation = kiriview::DecodedImageFailureOperation::DecodeHeifSequenceOpen;
        break;
    case kiriview::ImageDecodeHandlerKind::Svg:
    case kiriview::ImageDecodeHandlerKind::QtRaster:
    case kiriview::ImageDecodeHandlerKind::None:
        break;
    }
    if (stageOperation.has_value()) {
        operation = *stageOperation;
    }

    return kiriview::DecodedImageFailure {
        kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData),
        decodedFailureRouteForHandlerKind(handlerKind),
        operation,
        kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
        kiriview::DecodedImageFailureSeverity::Error,
        false,
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
    };
}

bool retainCompatibleDataWorkspace(
    kiriview::DecodedImageResult& result, kiriview::ImageDecodeWorkspaceHold workspaceHold)
{
    kiriview::DecodedImage* image = kiriview::decodedImageResultImage(result);
    if (image == nullptr || !workspaceHold.isManaged()) {
        return false;
    }
    std::visit(
        [workspaceHold = std::move(workspaceHold)](auto& decoded) mutable {
            if constexpr (requires { decoded.inputWorkspaceHold; }) {
                decoded.inputWorkspaceHold = std::move(workspaceHold);
            } else {
                decoded.displayImage.inputWorkspaceHold = std::move(workspaceHold);
            }
        },
        *image);
    return true;
}

QString decodedFailureOperationName(kiriview::DecodedImageFailureOperation operation)
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

QString adapterFailureDiagnosticDetail(const QString& adapterName,
    kiriview::DecodedImageFailureOperation operation, const QString& backendError)
{
    return QStringLiteral("%1 decoder %2 failed: %3")
        .arg(adapterName, decodedFailureOperationName(operation),
            backendError.isEmpty() ? QStringLiteral("<empty>") : backendError);
}

kiriview::DecodedImageResult failedAdapterDecodedImageResult(QString errorString,
    kiriview::DecodedImageFailureRoute route, kiriview::DecodedImageFailureOperation operation,
    const QString& adapterName,
    kiriview::DecodedImageFailureCause cause = kiriview::DecodedImageFailureCause::Unknown)
{
    const QString backendError = errorString;
    return kiriview::failedDecodedImageResult(kiriview::DecodedImageFailure {
        std::move(errorString),
        route,
        operation,
        adapterFailureDiagnosticDetail(adapterName, operation, backendError),
        kiriview::DecodedImageFailureSeverity::Error,
        false,
        cause,
    });
}

void stampAdapterFailure(kiriview::DecodedImageResult& result,
    kiriview::DecodedImageFailureRoute route, const QString& adapterName)
{
    kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    if (failure == nullptr) {
        return;
    }
    failure->route = route;
    failure->diagnosticDetail = adapterFailureDiagnosticDetail(
        adapterName, failure->operation, failure->diagnosticDetail);
}

kiriview::DecodedImageResult failedAnimationOpenResult(
    QString errorString, const QString& adapterName)
{
    const QString backendError = errorString;
    return kiriview::failedDecodedImageResult(kiriview::DecodedImageFailure {
        std::move(errorString),
        kiriview::DecodedImageFailureRoute::QtRaster,
        kiriview::DecodedImageFailureOperation::DecodeAnimationOpen,
        QStringLiteral("%1 animation open failed: %2")
            .arg(adapterName, backendError.isEmpty() ? QStringLiteral("<empty>") : backendError),
        kiriview::DecodedImageFailureSeverity::Error,
        false,
    });
}

kiriview::DecodedImageResult failedAnimationWorkspaceResult(const QString& adapterName)
{
    return kiriview::failedDecodedImageResult(kiriview::DecodedImageFailure {
        kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeImageAnimation),
        kiriview::DecodedImageFailureRoute::QtRaster,
        kiriview::DecodedImageFailureOperation::DecodeAnimationOpen,
        QStringLiteral("%1 decoder workspace admission failed: %2")
            .arg(adapterName, kiriview::imageDecodeWorkspaceResourceLimitDiagnostic()),
        kiriview::DecodedImageFailureSeverity::Error,
        false,
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
    });
}

QString sourceIdentityForRequest(const kiriview::ImageDecodeRequest& request)
{
    return kiriview::sourceKeyForUrl(request.imageUrl()).identity;
}

kiriview::DecodedImageResult decodeSvgImageData(const kiriview::ImageDecodeRouterInput& input)
{
    QString errorString;
    bool resourceExhausted = false;
    std::shared_ptr<kiriview::SvgDisplaySource> source = kiriview::SvgDisplaySource::open(
        input.data, &errorString, input.workspaceBudget, &resourceExhausted);
    if (source == nullptr) {
        if (errorString.isEmpty()) {
            errorString = kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData);
        }
        return failedAdapterDecodedImageResult(std::move(errorString),
            kiriview::DecodedImageFailureRoute::Svg,
            kiriview::DecodedImageFailureOperation::OpenStaticImageSource, QStringLiteral("SVG"),
            resourceExhausted ? kiriview::DecodedImageFailureCause::ResourceLimitExceeded
                              : kiriview::DecodedImageFailureCause::Unknown);
    }

    kiriview::DecodedImageResult result = kiriview::staticDecodedImageResult(
        std::move(source), input.request, &errorString, input.workspaceBudget);
    stampAdapterFailure(result, kiriview::DecodedImageFailureRoute::Svg, QStringLiteral("SVG"));
    return result;
}

kiriview::DecodedImageResult decodeApngImageData(const kiriview::ImageDecodeRouterInput& input)
{
    kiriview::ImageAnimationSourceCatalogResult catalog = kiriview::readImageAnimationSourceCatalog(
        kiriview::apngAnimationPlaybackRequest(input.data));
    if (!catalog.has_value()) {
        return failedAdapterDecodedImageResult(std::move(catalog.error().errorString),
            kiriview::DecodedImageFailureRoute::Apng,
            kiriview::DecodedImageFailureOperation::DecodeAnimationOpen, QStringLiteral("APNG"));
    }

    kiriview::ApngAnimationReader apngReader(input.workspaceBudget);
    kiriview::ApngOpenResult apngResult = apngReader.open(input.data);
    if (apngResult.status == kiriview::ApngOpenStatus::NotApng) {
        return failedAdapterDecodedImageResult(
            kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeApngAnimation),
            kiriview::DecodedImageFailureRoute::Apng,
            kiriview::DecodedImageFailureOperation::DecodeAnimationOpen, QStringLiteral("APNG"));
    }
    if (apngResult.status == kiriview::ApngOpenStatus::Error) {
        return failedAdapterDecodedImageResult(std::move(apngResult.errorString),
            kiriview::DecodedImageFailureRoute::Apng,
            kiriview::DecodedImageFailureOperation::DecodeAnimationOpen, QStringLiteral("APNG"));
    }
    if (apngResult.status == kiriview::ApngOpenStatus::ResourceLimitExceeded) {
        return kiriview::failedDecodedImageResult(kiriview::DecodedImageFailure {
            kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeApngAnimation),
            kiriview::DecodedImageFailureRoute::Apng,
            kiriview::DecodedImageFailureOperation::DecodeAnimationOpen,
            QStringLiteral("APNG decoder workspace admission failed: %1")
                .arg(kiriview::imageDecodeWorkspaceResourceLimitDiagnostic()),
            kiriview::DecodedImageFailureSeverity::Error,
            false,
            kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
        });
    }
    if (catalog->logicalSize != apngResult.firstFrame.size()) {
        return failedAdapterDecodedImageResult(
            QStringLiteral("animation source catalog size mismatch"),
            kiriview::DecodedImageFailureRoute::Apng,
            kiriview::DecodedImageFailureOperation::DecodeAnimationOpen, QStringLiteral("APNG"));
    }

    kiriview::ImageDecodeWorkspaceHold firstFrameWorkspaceHold
        = std::move(apngResult.workspaceHold);
    if (!firstFrameWorkspaceHold.isManaged()) {
        return kiriview::failedDecodedImageResult(kiriview::DecodedImageFailure {
            kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeApngAnimation),
            kiriview::DecodedImageFailureRoute::Apng,
            kiriview::DecodedImageFailureOperation::DecodeAnimationOpen,
            QStringLiteral("APNG first-frame workspace retention failed"),
            kiriview::DecodedImageFailureSeverity::Error,
            false,
            kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
        });
    }

    return kiriview::successfulDecodedImageResult(kiriview::ApngAnimationImage {
        std::move(firstFrameWorkspaceHold),
        std::move(apngResult.firstFrame),
        {},
        {},
        input.data,
        std::move(*catalog),
        {},
        sourceIdentityForRequest(input.request),
        input.request.sourceRevision(),
    });
}

kiriview::DecodedImageResult decodeHeifRouterImageData(
    const kiriview::ImageDecodeRouterInput& input)
{
    std::optional<kiriview::DecodedImageResult> result = kiriview::decodeHeifImageData(
        input.data, input.request, input.workspaceBudget, input.retainedInputWorkspaceByteCount);
    if (!result.has_value()) {
        return failedReadImageDataResult();
    }
    return std::move(*result);
}

kiriview::DecodedImageResult decodeRawRouterImageData(const kiriview::ImageDecodeRouterInput& input)
{
    return kiriview::decodeRawImageData(input.data, input.request, input.workspaceBudget);
}

kiriview::DecodedImageResult decodeQImageReaderRouterImageData(
    const kiriview::ImageDecodeRouterInput& input)
{
    if (input.qtRasterFormat == kiriview::QtRasterFormat::Webp) {
        kiriview::WebPAnimationReader reader(input.workspaceBudget);
        kiriview::WebPAnimationOpenResult openResult = reader.open(input.data);
        switch (openResult.status) {
        case kiriview::WebPAnimationOpenStatus::Success: {
            reader.close();
            kiriview::ImageAnimationSourceCatalogResult catalog
                = kiriview::readImageAnimationSourceCatalog(
                    kiriview::webpAnimationPlaybackRequest(input.data, {}, input.workspaceBudget));
            if (!catalog.has_value()) {
                return catalog.error().cause
                        == kiriview::ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded
                    ? failedAnimationWorkspaceResult(QStringLiteral("WebP"))
                    : failedAnimationOpenResult(
                          std::move(catalog.error().errorString), QStringLiteral("WebP"));
            }
            if (catalog->logicalSize != openResult.firstFrame.size()) {
                return failedAnimationOpenResult(
                    QStringLiteral("animation source catalog size mismatch"),
                    QStringLiteral("WebP"));
            }
            return kiriview::successfulDecodedImageResult(kiriview::WebPAnimationImage {
                std::move(openResult.workspaceHold),
                std::move(openResult.firstFrame),
                {},
                {},
                input.data,
                std::move(*catalog),
                {},
                sourceIdentityForRequest(input.request),
                input.request.sourceRevision(),
            });
        }
        case kiriview::WebPAnimationOpenStatus::Error:
            return failedAnimationOpenResult(openResult.errorString, QStringLiteral("WebP"));
        case kiriview::WebPAnimationOpenStatus::ResourceLimitExceeded:
            return failedAnimationWorkspaceResult(QStringLiteral("WebP"));
        case kiriview::WebPAnimationOpenStatus::NotWebP:
        case kiriview::WebPAnimationOpenStatus::NotAnimation:
            break;
        }
    }

    if (input.qtRasterFormat == kiriview::QtRasterFormat::Jxl) {
        kiriview::JxlAnimationReader reader(input.workspaceBudget);
        kiriview::JxlAnimationOpenResult openResult = reader.open(input.data);
        switch (openResult.status) {
        case kiriview::JxlAnimationOpenStatus::Success: {
            reader.close();
            kiriview::ImageAnimationSourceCatalogResult catalog
                = kiriview::readImageAnimationSourceCatalog(
                    kiriview::jxlAnimationPlaybackRequest(input.data, {}, input.workspaceBudget));
            if (!catalog.has_value()) {
                return catalog.error().cause
                        == kiriview::ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded
                    ? failedAnimationWorkspaceResult(QStringLiteral("JXL"))
                    : failedAnimationOpenResult(
                          std::move(catalog.error().errorString), QStringLiteral("JXL"));
            }
            if (catalog->logicalSize != openResult.firstFrame.size()) {
                return failedAnimationOpenResult(
                    QStringLiteral("animation source catalog size mismatch"),
                    QStringLiteral("JXL"));
            }
            return kiriview::successfulDecodedImageResult(kiriview::JxlAnimationImage {
                std::move(openResult.workspaceHold),
                std::move(openResult.firstFrame),
                {},
                {},
                input.data,
                std::move(*catalog),
                {},
                sourceIdentityForRequest(input.request),
                input.request.sourceRevision(),
            });
        }
        case kiriview::JxlAnimationOpenStatus::Error:
            return failedAnimationOpenResult(openResult.errorString, QStringLiteral("JXL"));
        case kiriview::JxlAnimationOpenStatus::ResourceLimitExceeded:
            return failedAnimationWorkspaceResult(QStringLiteral("JXL"));
        case kiriview::JxlAnimationOpenStatus::NotJxl:
        case kiriview::JxlAnimationOpenStatus::NotAnimation:
            break;
        }
    }

    return kiriview::decodeQImageReaderImageData(
        input.data, input.request, input.qtRasterFormat, input.workspaceBudget);
}

kiriview::ImageDecodeRouterHandlers defaultImageDecodeRouterHandlers()
{
    return kiriview::ImageDecodeRouterHandlers {
        decodeSvgImageData,
        decodeApngImageData,
        decodeHeifRouterImageData,
        decodeRawRouterImageData,
        decodeQImageReaderRouterImageData,
    };
}

kiriview::ImageDecodeRouterHandlers withDefaultHandlers(
    kiriview::ImageDecodeRouterHandlers handlers)
{
    const kiriview::ImageDecodeRouterHandlers defaults = defaultImageDecodeRouterHandlers();
    if (!handlers.svg) {
        handlers.svg = defaults.svg;
    }
    if (!handlers.apng) {
        handlers.apng = defaults.apng;
    }
    if (!handlers.heifFamily) {
        handlers.heifFamily = defaults.heifFamily;
    }
    if (!handlers.raw) {
        handlers.raw = defaults.raw;
    }
    if (!handlers.qtRaster) {
        handlers.qtRaster = defaults.qtRaster;
    }
    return handlers;
}

kiriview::DecodedImageResult dispatchToHandler(const kiriview::ImageDecodeRouterHandler& handler,
    const kiriview::ImageDecodeRouterInput& input, kiriview::ImageDecodeHandlerKind handlerKind)
{
    if (!handler) {
        return failedReadImageDataResult();
    }
    kiriview::DecodedImageResult result = handler(input);
    kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    if (failure != nullptr && failure->route == kiriview::DecodedImageFailureRoute::Unknown) {
        failure->route = decodedFailureRouteForHandlerKind(handlerKind);
    }
    return result;
}

const kiriview::ImageDecodeRouterHandler& emptyHandler()
{
    static const kiriview::ImageDecodeRouterHandler handler;
    return handler;
}

const kiriview::ImageDecodeRouterHandler& handlerForRoute(
    const kiriview::ImageDecodeRouterHandlers& handlers,
    kiriview::ImageDecodeHandlerKind handlerKind)
{
    switch (handlerKind) {
    case kiriview::ImageDecodeHandlerKind::Svg:
        return handlers.svg;
    case kiriview::ImageDecodeHandlerKind::Apng:
        return handlers.apng;
    case kiriview::ImageDecodeHandlerKind::HeifFamily:
        return handlers.heifFamily;
    case kiriview::ImageDecodeHandlerKind::Raw:
        return handlers.raw;
    case kiriview::ImageDecodeHandlerKind::QtRaster:
        return handlers.qtRaster;
    case kiriview::ImageDecodeHandlerKind::None:
        return emptyHandler();
    }

    return emptyHandler();
}
}

namespace kiriview {
class PreparedImageDecodeWork::Private final
{
public:
    Private(ImageDecodeWorkspaceAdmissionRequest admissionRequest,
        DecodedImageFailure hardLimitFailure, PreparedImageDecodeExecutor execute)
        : admissionRequest(admissionRequest)
        , hardLimitFailure(std::move(hardLimitFailure))
        , execute(std::move(execute))
    {
    }

    ImageDecodeWorkspaceAdmissionRequest admissionRequest;
    DecodedImageFailure hardLimitFailure;
    PreparedImageDecodeExecutor execute;
};

PreparedImageDecodeWork::PreparedImageDecodeWork(
    ImageDecodeWorkspaceAdmissionRequest admissionRequest, DecodedImageFailure hardLimitFailure,
    PreparedImageDecodeExecutor execute)
    : d(std::make_unique<Private>(
          admissionRequest, std::move(hardLimitFailure), std::move(execute)))
{
}

PreparedImageDecodeWork::~PreparedImageDecodeWork() = default;

PreparedImageDecodeWork::PreparedImageDecodeWork(PreparedImageDecodeWork&& other) noexcept
    = default;

PreparedImageDecodeWork& PreparedImageDecodeWork::operator=(
    PreparedImageDecodeWork&& other) noexcept
    = default;

const ImageDecodeWorkspaceAdmissionRequest& PreparedImageDecodeWork::admissionRequest() const
{
    return d->admissionRequest;
}

const DecodedImageFailure& PreparedImageDecodeWork::hardLimitFailure() const
{
    return d->hardLimitFailure;
}

PreparedImageDecodeResult PreparedImageDecodeWork::execute(ImageDecodeWorkspaceLease lease) &&
{
    PreparedImageDecodeExecutor execute = std::move(d->execute);
    d.reset();
    if (!execute) {
        return failedReadImageDataResult();
    }
    return execute(std::move(lease));
}

ImageDecodeRoute imageDecodeRouteForClassification(ImageInputClassification classification)
{
    switch (classification.kind) {
    case ImageInputKind::Svg:
        return ImageDecodeRoute {
            ImageDecodeHandlerKind::Svg,
            classification.dataSource,
            classification.qtFormat,
        };
    case ImageInputKind::Apng:
        return ImageDecodeRoute {
            ImageDecodeHandlerKind::Apng,
            classification.dataSource,
            classification.qtFormat,
        };
    case ImageInputKind::HeifFamily:
        return ImageDecodeRoute {
            ImageDecodeHandlerKind::HeifFamily,
            classification.dataSource,
            classification.qtFormat,
        };
    case ImageInputKind::Raw:
        return ImageDecodeRoute {
            ImageDecodeHandlerKind::Raw,
            classification.dataSource,
            classification.qtFormat,
        };
    case ImageInputKind::QtRaster:
        return ImageDecodeRoute {
            ImageDecodeHandlerKind::QtRaster,
            classification.dataSource,
            classification.qtFormat,
        };
    case ImageInputKind::Unknown:
        return {};
    }

    return {};
}

ImageDecodeRouterRuntime::ImageDecodeRouterRuntime(
    ImageDecodeRouterHandlers handlers, ImageDecodeCompatibleDataTransform compatibleDataTransform)
    : m_hasInjectedSvgHandler(static_cast<bool>(handlers.svg))
    , m_hasInjectedApngHandler(static_cast<bool>(handlers.apng))
    , m_hasInjectedHeifFamilyHandler(static_cast<bool>(handlers.heifFamily))
    , m_hasInjectedRawHandler(static_cast<bool>(handlers.raw))
    , m_hasInjectedQtRasterHandler(static_cast<bool>(handlers.qtRaster))
    , m_handlers(withDefaultHandlers(std::move(handlers)))
    , m_compatibleDataTransform(std::move(compatibleDataTransform))
{
}

DecodedImageResult ImageDecodeRouterRuntime::execute(ImageDecodeRoute route, const QByteArray& data,
    const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget) const
{
    if (!route.shouldDecode()) {
        return failedReadImageDataResult();
    }
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }

    ImageDecodeRouterByteInputs byteInputs(data, m_compatibleDataTransform, workspaceBudget);
    const QByteArray* routedData = byteInputs.dataFor(route.dataSource);
    if (routedData == nullptr || byteInputs.resourceExhausted()) {
        return failedCompatibleDataWorkspaceResult(route.handlerKind);
    }
    const ImageDecodeRouterInput input {
        *routedData,
        request,
        route.qtRasterFormat,
        workspaceBudget,
        byteInputs.retainedInputWorkspaceByteCount(),
    };

    DecodedImageResult result = executeRoutedData(route, input.data, request,
        std::move(workspaceBudget), input.retainedInputWorkspaceByteCount);
    if (decodedImageResultImage(result) != nullptr && byteInputs.compatibleDataRequiresRetention()
        && !retainCompatibleDataWorkspace(result, byteInputs.takeRetainedWorkspace())) {
        return failedCompatibleDataWorkspaceResult(route.handlerKind);
    }
    return result;
}

DecodedImageResult ImageDecodeRouterRuntime::executeRoutedData(ImageDecodeRoute route,
    const QByteArray& data, const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    qsizetype retainedInputWorkspaceByteCount) const
{
    const ImageDecodeRouterInput input {
        data,
        request,
        route.qtRasterFormat,
        std::move(workspaceBudget),
        retainedInputWorkspaceByteCount,
    };
    return dispatchToHandler(
        handlerForRoute(m_handlers, route.handlerKind), input, route.handlerKind);
}

bool ImageDecodeRouterRuntime::hasInjectedHandler(ImageDecodeHandlerKind handlerKind) const
{
    switch (handlerKind) {
    case ImageDecodeHandlerKind::Svg:
        return m_hasInjectedSvgHandler;
    case ImageDecodeHandlerKind::Apng:
        return m_hasInjectedApngHandler;
    case ImageDecodeHandlerKind::HeifFamily:
        return m_hasInjectedHeifFamilyHandler;
    case ImageDecodeHandlerKind::Raw:
        return m_hasInjectedRawHandler;
    case ImageDecodeHandlerKind::QtRaster:
        return m_hasInjectedQtRasterHandler;
    case ImageDecodeHandlerKind::None:
        return false;
    }
    return false;
}

const ImageDecodeCompatibleDataTransform& ImageDecodeRouterRuntime::compatibleDataTransform() const
{
    return m_compatibleDataTransform;
}

ImageDecodeRouter::ImageDecodeRouter(ImageDecodeRouterHandlers handlers,
    ImageDecodeInputClassifier classifier,
    ImageDecodeCompatibleDataTransform compatibleDataTransform)
    : m_classifier(std::move(classifier))
    , m_runtime(std::move(handlers), std::move(compatibleDataTransform))
{
    if (!m_classifier) {
        m_classifier = classifyImageInput;
    }
}

DecodedImageResult ImageDecodeRouter::decode(const QByteArray& data,
    const ImageDecodeRequest& request,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget) const
{
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    const ImageInputClassification classification
        = m_classifier(data, request.imageUrl().fileName());
    const ImageDecodeRoute route = imageDecodeRouteForClassification(classification);
    qCDebug(kiriviewDecodeLog) << "image decode route"
                               << "generation" << request.id() << "url"
                               << kiriview::diagnosticSourceReference(request.imageUrl())
                               << "inputKind" << imageInputKindName(classification.kind)
                               << "handler" << imageDecodeHandlerKindName(route.handlerKind)
                               << "dataSource" << imageDecodeDataSourceName(route.dataSource)
                               << "qtFormat" << qtRasterFormatName(route.qtRasterFormat) << "bytes"
                               << data.size();
    DecodedImageResult result = m_runtime.execute(route, data, request, workspaceBudget);
    DecodedImage* image = decodedImageResultImage(result);
    if (image != nullptr) {
        ImageDecodeWorkspaceLease metadataWorkspace
            = ImageDecodeWorkspaceDetail::startLeaseForOperation(
                *workspaceBudget, decodedImageWorkspaceByteCost(*image));
        if (ImageDecodeWorkspaceDetail::tryReserve(
                metadataWorkspace, embeddedMetadataWorkspaceReservation)) {
            EmbeddedMetadata metadata = parseImageEmbeddedMetadata(data);
            if (!metadata.isEmpty()) {
                setDecodedImageEmbeddedMetadata(*image, std::move(metadata));
            }
        }
    }
    return result;
}

namespace {
    bool hasInvalidPreparedPeakByteCost(const std::optional<qsizetype>& peakByteCost)
    {
        return !peakByteCost.has_value() || *peakByteCost <= 0
            || *peakByteCost == std::numeric_limits<qsizetype>::max();
    }

    bool hasInvalidPreparedWorkspaceByteCost(const std::optional<qsizetype>& workspaceByteCost)
    {
        return !workspaceByteCost.has_value() || *workspaceByteCost < 0
            || *workspaceByteCost == std::numeric_limits<qsizetype>::max();
    }

    std::optional<qsizetype> checkedPreparedByteSum(qsizetype left, qsizetype right)
    {
        if (left < 0 || right < 0 || right > std::numeric_limits<qsizetype>::max() - left) {
            return std::nullopt;
        }
        return left + right;
    }

    bool isPreparedQtRasterStillFormat(QtRasterFormat format)
    {
        switch (format) {
        case QtRasterFormat::Png:
        case QtRasterFormat::Jpeg:
        case QtRasterFormat::Bmp:
        case QtRasterFormat::Tiff:
        case QtRasterFormat::Jp2:
            return true;
        case QtRasterFormat::None:
        case QtRasterFormat::Gif:
        case QtRasterFormat::Webp:
        case QtRasterFormat::Jxl:
            return false;
        }
        return false;
    }

    PreparedImageDecodeResult finishPreparedDecode(DecodedImageResult result,
        ImageSourceData sourceData, const QByteArray& metadataData,
        ImageDecodeWorkspaceHold retainedInputWorkspace,
        const std::shared_ptr<ImageDecodeWorkspaceBudget>& workspaceBudget,
        ImageDecodeWorkspacePriority priority, ImageDecodeHandlerKind handlerKind)
    {
        if (decodedImageResultImage(result) != nullptr && retainedInputWorkspace.isManaged()
            && !retainCompatibleDataWorkspace(result, std::move(retainedInputWorkspace))) {
            return failedCompatibleDataWorkspaceResult(handlerKind);
        }

        DecodedImage* image = decodedImageResultImage(result);
        if (image != nullptr) {
            const ImageDecodeWorkspaceAdmissionRequest metadataRequest {
                embeddedMetadataWorkspaceReservation,
                decodedImageWorkspaceByteCost(*image),
                priority,
            };
            if (std::optional<ImageDecodeWorkspaceLease> metadataAdmission
                = ImageDecodeWorkspaceDetail::tryBestEffortAdmission(
                    *workspaceBudget, metadataRequest)) {
                EmbeddedMetadata metadata = parseImageEmbeddedMetadata(metadataData);
                if (!metadata.isEmpty()) {
                    setDecodedImageEmbeddedMetadata(*image, std::move(metadata));
                }
            }
        }
        retainSourceDataLease(result, std::move(sourceData.lease));
        return result;
    }

    PreparedImageDecodeResult prepareRoutedImageData(ImageDecodeRouterRuntime runtime,
        ImageDecodeRoute route, ImageSourceData sourceData, QByteArray metadataData,
        const ImageDecodeRequest& request, ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace = {},
        qsizetype retainedInputWorkspaceByteCount = 0);

    PreparedImageDecodeResult prepareStaticDisplaySource(ImageDecodeRoute route,
        ImageSourceData sourceData, QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount,
        std::shared_ptr<StaticImageDisplaySource> source, QString adapterName)
    {
        const std::optional<qsizetype> peakByteCost = source->initialDisplayDecodePeakByteCost(
            request.firstDisplay(), imageBlockingDisplayLongEdgeMax);
        if (hasInvalidPreparedPeakByteCost(peakByteCost)) {
            return failedDecodedImageResult(preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeBlockingDisplayImage));
        }

        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            *peakByteCost,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto execute
            = [route, sourceData = std::move(sourceData), metadataData = std::move(metadataData),
                  request, priority, workspaceBudget = std::move(workspaceBudget),
                  retainedInputWorkspace = std::move(retainedInputWorkspace),
                  source = std::move(source), adapterName = std::move(adapterName)](
                  ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            QString errorString;
            DecodedImageResult result = staticDecodedImageResult(
                std::move(source), request, &errorString, {}, std::move(lease));
            stampAdapterFailure(
                result, decodedFailureRouteForHandlerKind(route.handlerKind), adapterName);
            return finishPreparedDecode(std::move(result), std::move(sourceData), metadataData,
                std::move(retainedInputWorkspace), workspaceBudget, priority, route.handlerKind);
        };
        return std::make_unique<PreparedImageDecodeWork>(admissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeBlockingDisplayImage),
            std::move(execute));
    }

    PreparedImageDecodeResult prepareQtRasterStill(ImageDecodeRoute route,
        ImageSourceData sourceData, QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        const QByteArray readerFormat = qtImageReaderFormat(route.qtRasterFormat);
        QString errorString;
        std::shared_ptr<QImageReaderDisplaySource> source
            = QImageReaderDisplaySource::open(sourceData.data, readerFormat, &errorString);
        if (source == nullptr) {
            if (errorString.isEmpty()) {
                errorString = imageErrorText(ImageErrorTextId::ReadImageData);
            }
            return failedAdapterDecodedImageResult(std::move(errorString),
                DecodedImageFailureRoute::QtRaster,
                DecodedImageFailureOperation::OpenStaticImageSource,
                QStringLiteral("Qt image reader"));
        }
        return prepareStaticDisplaySource(route, std::move(sourceData), std::move(metadataData),
            request, priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
            retainedInputWorkspaceByteCount, std::move(source), QStringLiteral("Qt image reader"));
    }

    PreparedImageDecodeResult prepareApng(ImageDecodeRoute route, ImageSourceData sourceData,
        QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        ImageAnimationSourceCatalogResult catalog
            = readImageAnimationSourceCatalog(apngAnimationPlaybackRequest(sourceData.data));
        if (!catalog.has_value()) {
            return failedAdapterDecodedImageResult(std::move(catalog.error().errorString),
                DecodedImageFailureRoute::Apng, DecodedImageFailureOperation::DecodeAnimationOpen,
                QStringLiteral("APNG"));
        }

        const ApngAnimationWorkspacePlanResult planning = planApngAnimationOpen(sourceData.data);
        if (planning.status != ApngOpenStatus::Success) {
            if (planning.status == ApngOpenStatus::ResourceLimitExceeded) {
                return failedDecodedImageResult(preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen));
            }
            return failedAdapterDecodedImageResult(planning.errorString.isEmpty()
                    ? imageErrorText(ImageErrorTextId::DecodeApngAnimation)
                    : planning.errorString,
                DecodedImageFailureRoute::Apng, DecodedImageFailureOperation::DecodeAnimationOpen,
                QStringLiteral("APNG"));
        }

        const std::optional<qsizetype> openPeakByteCount = checkedPreparedByteSum(
            planning.plan.transientByteCount, planning.plan.firstFrameOutputByteCount);
        if (hasInvalidPreparedPeakByteCost(openPeakByteCount)) {
            return failedDecodedImageResult(preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen));
        }

        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            *openPeakByteCount,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto execute
            = [route, sourceData = std::move(sourceData), metadataData = std::move(metadataData),
                  request, priority, workspaceBudget = std::move(workspaceBudget),
                  retainedInputWorkspace = std::move(retainedInputWorkspace),
                  catalog = std::move(*catalog), plan = planning.plan,
                  retainedInputWorkspaceByteCount](
                  ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            std::shared_ptr<ImageDecodeWorkspaceBudget> producerBudget
                = prechargedImageDecodeWorkspaceBudget(
                    std::move(lease), retainedInputWorkspaceByteCount);
            if (producerBudget == nullptr) {
                return failedDecodedImageResult(preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen));
            }

            ApngOpenResult opened;
            {
                ApngAnimationReader reader(producerBudget);
                opened = reader.open(sourceData.data, plan);
            }
            DecodedImageResult result;
            if (opened.status != ApngOpenStatus::Success) {
                if (opened.status == ApngOpenStatus::ResourceLimitExceeded) {
                    result = failedDecodedImageResult(preparedHardLimitFailure(
                        route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen));
                } else {
                    result = failedAdapterDecodedImageResult(opened.errorString.isEmpty()
                            ? imageErrorText(ImageErrorTextId::DecodeApngAnimation)
                            : std::move(opened.errorString),
                        DecodedImageFailureRoute::Apng,
                        DecodedImageFailureOperation::DecodeAnimationOpen, QStringLiteral("APNG"));
                }
            } else if (catalog.logicalSize != opened.firstFrame.size()) {
                result = failedAdapterDecodedImageResult(
                    QStringLiteral("animation source catalog size mismatch"),
                    DecodedImageFailureRoute::Apng,
                    DecodedImageFailureOperation::DecodeAnimationOpen, QStringLiteral("APNG"));
            } else if (!opened.workspaceHold.isManaged()) {
                result = failedDecodedImageResult(preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen));
            } else {
                result = successfulDecodedImageResult(ApngAnimationImage {
                    std::move(opened.workspaceHold),
                    std::move(opened.firstFrame),
                    {},
                    {},
                    sourceData.data,
                    std::move(catalog),
                    {},
                    sourceIdentityForRequest(request),
                    request.sourceRevision(),
                });
            }
            producerBudget->finalizePrechargedAdmission();
            return finishPreparedDecode(std::move(result), std::move(sourceData), metadataData,
                std::move(retainedInputWorkspace), workspaceBudget, priority, route.handlerKind);
        };
        return std::make_unique<PreparedImageDecodeWork>(admissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen),
            std::move(execute));
    }

    PreparedImageDecodeResult prepareQtGif(ImageDecodeRouterRuntime runtime, ImageDecodeRoute route,
        ImageSourceData sourceData, QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        const QByteArray readerFormat = qtImageReaderFormat(route.qtRasterFormat);
        ImageAnimationSourceCatalogResult catalog = readImageAnimationSourceCatalog(
            readerAnimationPlaybackRequest(sourceData.data, readerFormat));
        if (!catalog.has_value()) {
            if (catalog.error().cause
                == ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded) {
                return failedAnimationWorkspaceResult(QStringLiteral("Qt image reader"));
            }
            return prepareQtRasterStill(route, std::move(sourceData), std::move(metadataData),
                request, priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                retainedInputWorkspaceByteCount);
        }

        const std::optional<qsizetype> transientByteCount
            = qImageReaderGifTransientWorkspaceByteCount(catalog->logicalSize);
        const std::optional<qsizetype> outputByteCount
            = checkedImageDecodeWorkspaceByteCount(catalog->logicalSize, 4, 1);
        const std::optional<qsizetype> peakByteCount
            = transientByteCount.has_value() && outputByteCount.has_value()
            ? checkedPreparedByteSum(*transientByteCount, *outputByteCount)
            : std::nullopt;
        if (hasInvalidPreparedPeakByteCost(peakByteCount)) {
            return failedAnimationWorkspaceResult(QStringLiteral("Qt image reader"));
        }

        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            *peakByteCount,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto execute = [runtime = std::move(runtime), route, sourceData = std::move(sourceData),
                           metadataData = std::move(metadataData), request, priority,
                           workspaceBudget = std::move(workspaceBudget),
                           retainedInputWorkspace = std::move(retainedInputWorkspace),
                           retainedInputWorkspaceByteCount](
                           ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            std::shared_ptr<ImageDecodeWorkspaceBudget> producerBudget
                = prechargedImageDecodeWorkspaceBudget(
                    std::move(lease), retainedInputWorkspaceByteCount);
            if (producerBudget == nullptr) {
                return failedAnimationWorkspaceResult(QStringLiteral("Qt image reader"));
            }
            DecodedImageResult result = runtime.executeRoutedData(
                route, sourceData.data, request, producerBudget, retainedInputWorkspaceByteCount);
            producerBudget->finalizePrechargedAdmission();
            return finishPreparedDecode(std::move(result), std::move(sourceData), metadataData,
                std::move(retainedInputWorkspace), workspaceBudget, priority, route.handlerKind);
        };
        return std::make_unique<PreparedImageDecodeWork>(admissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen),
            std::move(execute));
    }

    PreparedImageDecodeResult prepareWebPAnimation(ImageDecodeRouterRuntime runtime,
        ImageDecodeRoute route, ImageSourceData sourceData, QByteArray metadataData,
        const ImageDecodeRequest& request, ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        const WebPAnimationWorkspacePlanResult planning = planWebPAnimationOpen(sourceData.data);
        switch (planning.status) {
        case WebPAnimationOpenStatus::NotWebP:
        case WebPAnimationOpenStatus::NotAnimation:
            return prepareQtRasterStill(route, std::move(sourceData), std::move(metadataData),
                request, priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                retainedInputWorkspaceByteCount);
        case WebPAnimationOpenStatus::Error:
            return failedAnimationOpenResult(planning.errorString, QStringLiteral("WebP"));
        case WebPAnimationOpenStatus::ResourceLimitExceeded:
            return failedAnimationWorkspaceResult(QStringLiteral("WebP"));
        case WebPAnimationOpenStatus::Success:
            break;
        }

        const std::optional<qsizetype> catalogByteCount
            = webpAnimationCatalogWorkspaceByteCount(sourceData.data.size());
        const std::optional<qsizetype> producerTransientByteCount = catalogByteCount.has_value()
            ? std::optional<qsizetype>(
                  std::max(*catalogByteCount, planning.plan.transientByteCount))
            : std::nullopt;
        const std::optional<qsizetype> peakByteCount = producerTransientByteCount.has_value()
            ? checkedPreparedByteSum(
                  *producerTransientByteCount, planning.plan.firstFrameOutputByteCount)
            : std::nullopt;
        if (hasInvalidPreparedPeakByteCost(peakByteCount)) {
            return failedAnimationWorkspaceResult(QStringLiteral("WebP"));
        }

        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            *peakByteCount,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto execute = [runtime = std::move(runtime), route, sourceData = std::move(sourceData),
                           metadataData = std::move(metadataData), request, priority,
                           workspaceBudget = std::move(workspaceBudget),
                           retainedInputWorkspace = std::move(retainedInputWorkspace),
                           retainedInputWorkspaceByteCount](
                           ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            std::shared_ptr<ImageDecodeWorkspaceBudget> producerBudget
                = prechargedImageDecodeWorkspaceBudget(
                    std::move(lease), retainedInputWorkspaceByteCount);
            if (producerBudget == nullptr) {
                return failedAnimationWorkspaceResult(QStringLiteral("WebP"));
            }
            DecodedImageResult result = runtime.executeRoutedData(
                route, sourceData.data, request, producerBudget, retainedInputWorkspaceByteCount);
            producerBudget->finalizePrechargedAdmission();
            return finishPreparedDecode(std::move(result), std::move(sourceData), metadataData,
                std::move(retainedInputWorkspace), workspaceBudget, priority, route.handlerKind);
        };
        return std::make_unique<PreparedImageDecodeWork>(admissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen),
            std::move(execute));
    }

    PreparedImageDecodeResult prepareJxlAnimationOpen(ImageDecodeRoute route,
        ImageSourceData sourceData, QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount,
        ImageAnimationSourceCatalog catalog)
    {
        const std::optional<qsizetype> peakByteCount
            = jxlAnimationOpenWorkspaceByteCount(catalog.logicalSize);
        if (hasInvalidPreparedPeakByteCost(peakByteCount)) {
            return failedAnimationWorkspaceResult(QStringLiteral("JXL"));
        }
        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            *peakByteCount,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto execute
            = [route, sourceData = std::move(sourceData), metadataData = std::move(metadataData),
                  request, priority, workspaceBudget = std::move(workspaceBudget),
                  retainedInputWorkspace = std::move(retainedInputWorkspace),
                  retainedInputWorkspaceByteCount, catalog = std::move(catalog)](
                  ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            std::shared_ptr<ImageDecodeWorkspaceBudget> producerBudget
                = prechargedImageDecodeWorkspaceBudget(
                    std::move(lease), retainedInputWorkspaceByteCount);
            if (producerBudget == nullptr) {
                return failedAnimationWorkspaceResult(QStringLiteral("JXL"));
            }
            JxlAnimationOpenResult opened;
            {
                JxlAnimationReader reader(producerBudget);
                opened = reader.open(sourceData.data);
            }
            DecodedImageResult result;
            if (opened.status != JxlAnimationOpenStatus::Success) {
                if (opened.status == JxlAnimationOpenStatus::ResourceLimitExceeded) {
                    result = failedAnimationWorkspaceResult(QStringLiteral("JXL"));
                } else {
                    result = failedAnimationOpenResult(opened.errorString.isEmpty()
                            ? imageErrorText(ImageErrorTextId::DecodeImageAnimation)
                            : std::move(opened.errorString),
                        QStringLiteral("JXL"));
                }
            } else if (catalog.logicalSize != opened.firstFrame.size()) {
                result = failedAnimationOpenResult(
                    QStringLiteral("animation source catalog size mismatch"),
                    QStringLiteral("JXL"));
            } else if (!opened.workspaceHold.isManaged()) {
                result = failedAnimationWorkspaceResult(QStringLiteral("JXL"));
            } else {
                result = successfulDecodedImageResult(JxlAnimationImage {
                    std::move(opened.workspaceHold),
                    std::move(opened.firstFrame),
                    {},
                    {},
                    sourceData.data,
                    std::move(catalog),
                    {},
                    sourceIdentityForRequest(request),
                    request.sourceRevision(),
                });
            }
            producerBudget->finalizePrechargedAdmission();
            return finishPreparedDecode(std::move(result), std::move(sourceData), metadataData,
                std::move(retainedInputWorkspace), workspaceBudget, priority, route.handlerKind);
        };
        return std::make_unique<PreparedImageDecodeWork>(admissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen),
            std::move(execute));
    }

    PreparedImageDecodeResult prepareJxlAnimation(ImageDecodeRoute route,
        ImageSourceData sourceData, QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            jxlAnimationDecoderAllocationByteLimit,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto execute
            = [route, sourceData = std::move(sourceData), metadataData = std::move(metadataData),
                  request, priority, workspaceBudget = std::move(workspaceBudget),
                  retainedInputWorkspace = std::move(retainedInputWorkspace),
                  retainedInputWorkspaceByteCount](
                  ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            std::shared_ptr<ImageDecodeWorkspaceBudget> catalogBudget
                = prechargedImageDecodeWorkspaceBudget(
                    std::move(lease), retainedInputWorkspaceByteCount);
            if (catalogBudget == nullptr) {
                return failedAnimationWorkspaceResult(QStringLiteral("JXL"));
            }
            ImageAnimationSourceCatalogResult catalog = readImageAnimationSourceCatalog(
                jxlAnimationPlaybackRequest(sourceData.data, {}, catalogBudget));
            catalogBudget->finalizePrechargedAdmission();
            if (!catalog.has_value()) {
                if (catalog.error().cause
                    == ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded) {
                    return failedAnimationWorkspaceResult(QStringLiteral("JXL"));
                }
                return prepareQtRasterStill(route, std::move(sourceData), std::move(metadataData),
                    request, priority, std::move(workspaceBudget),
                    std::move(retainedInputWorkspace), retainedInputWorkspaceByteCount);
            }
            return prepareJxlAnimationOpen(route, std::move(sourceData), std::move(metadataData),
                request, priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                retainedInputWorkspaceByteCount, std::move(*catalog));
        };
        return std::make_unique<PreparedImageDecodeWork>(admissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeAnimationOpen),
            std::move(execute));
    }

    PreparedImageDecodeResult prepareSvg(ImageDecodeRouterRuntime runtime, ImageDecodeRoute route,
        ImageSourceData sourceData, QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        const std::optional<qsizetype> parserByteCost
            = svgParserWorkspaceByteCost(sourceData.data.size());
        if (hasInvalidPreparedPeakByteCost(parserByteCost)) {
            return failedDecodedImageResult(preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::OpenStaticImageSource));
        }

        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            *parserByteCost,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto execute = [runtime = std::move(runtime), route, sourceData = std::move(sourceData),
                           metadataData = std::move(metadataData), request, priority,
                           workspaceBudget = std::move(workspaceBudget),
                           retainedInputWorkspace = std::move(retainedInputWorkspace),
                           retainedInputWorkspaceByteCount](
                           ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            std::shared_ptr<ImageDecodeWorkspaceBudget> parserBudget
                = prechargedImageDecodeWorkspaceBudget(
                    std::move(lease), retainedInputWorkspaceByteCount);
            if (parserBudget == nullptr) {
                return failedDecodedImageResult(preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::OpenStaticImageSource));
            }

            QString errorString;
            bool resourceExhausted = false;
            std::shared_ptr<SvgDisplaySource> source = SvgDisplaySource::open(
                sourceData.data, &errorString, parserBudget, &resourceExhausted);
            parserBudget->finalizePrechargedAdmission();
            if (source == nullptr) {
                if (errorString.isEmpty()) {
                    errorString = imageErrorText(ImageErrorTextId::ReadImageData);
                }
                return failedAdapterDecodedImageResult(std::move(errorString),
                    DecodedImageFailureRoute::Svg,
                    DecodedImageFailureOperation::OpenStaticImageSource, QStringLiteral("SVG"),
                    resourceExhausted ? DecodedImageFailureCause::ResourceLimitExceeded
                                      : DecodedImageFailureCause::Unknown);
            }
            return prepareStaticDisplaySource(route, std::move(sourceData), std::move(metadataData),
                request, priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                retainedInputWorkspaceByteCount, std::move(source), QStringLiteral("SVG"));
        };
        return std::make_unique<PreparedImageDecodeWork>(admissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::OpenStaticImageSource),
            std::move(execute));
    }

    PreparedImageDecodeResult prepareRaw(ImageDecodeRoute route, ImageSourceData sourceData,
        QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        const ImageDecodeWorkspaceAdmissionRequest openAdmissionRequest {
            rawImageOpenWorkspaceByteCount,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto open
            = [route, sourceData = std::move(sourceData), metadataData = std::move(metadataData),
                  request, priority, workspaceBudget = std::move(workspaceBudget),
                  retainedInputWorkspace = std::move(retainedInputWorkspace),
                  retainedInputWorkspaceByteCount](
                  ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            OpenedRawImageResult opened = [&]() -> OpenedRawImageResult {
                try {
                    return openRawImageData(sourceData.data, std::move(lease));
                } catch (const std::bad_alloc&) {
                    return std::unexpected(preparedHardLimitFailure(
                        route.handlerKind, DecodedImageFailureOperation::DecodeRawImage));
                }
            }();
            if (!opened.has_value()) {
                return failedDecodedImageResult(std::move(opened.error()));
            }

            const std::optional<qsizetype> productionBaselineByteCount = checkedPreparedByteSum(
                retainedInputWorkspaceByteCount, (*opened)->retainedWorkspaceByteCount());
            const qsizetype productionPeakByteCount
                = (*opened)->productionAdditionalPeakByteCount();
            if (!productionBaselineByteCount.has_value() || productionPeakByteCount <= 0) {
                return failedDecodedImageResult(preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::DecodeRawImage));
            }

            const ImageDecodeWorkspaceAdmissionRequest productionAdmissionRequest {
                productionPeakByteCount,
                *productionBaselineByteCount,
                priority,
            };
            auto produce =
                [route, sourceData = std::move(sourceData), metadataData = std::move(metadataData),
                    request, priority, workspaceBudget = std::move(workspaceBudget),
                    retainedInputWorkspace = std::move(retainedInputWorkspace),
                    opened = std::move(*opened)](
                    ImageDecodeWorkspaceLease producerLease) mutable -> PreparedImageDecodeResult {
                DecodedImageResult result = [&]() -> DecodedImageResult {
                    try {
                        return std::move(*opened).decode(request, std::move(producerLease));
                    } catch (const std::bad_alloc&) {
                        return failedDecodedImageResult(preparedHardLimitFailure(
                            route.handlerKind, DecodedImageFailureOperation::DecodeRawImage));
                    }
                }();
                return finishPreparedDecode(std::move(result), std::move(sourceData), metadataData,
                    std::move(retainedInputWorkspace), workspaceBudget, priority,
                    route.handlerKind);
            };
            return std::make_unique<PreparedImageDecodeWork>(productionAdmissionRequest,
                preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::DecodeRawImage),
                std::move(produce));
        };
        return std::make_unique<PreparedImageDecodeWork>(openAdmissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeRawImage),
            std::move(open));
    }

    PreparedImageDecodeResult prepareHeifStill(ImageDecodeRoute route, ImageSourceData sourceData,
        QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        const ImageDecodeWorkspaceAdmissionRequest openAdmissionRequest {
            heifDisplaySourceOpenWorkspaceByteCount,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto open
            = [route, sourceData = std::move(sourceData), metadataData = std::move(metadataData),
                  request, priority, workspaceBudget = std::move(workspaceBudget),
                  retainedInputWorkspace = std::move(retainedInputWorkspace),
                  retainedInputWorkspaceByteCount](
                  ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            std::shared_ptr<ImageDecodeWorkspaceBudget> openBudget
                = prechargedImageDecodeWorkspaceBudget(
                    std::move(lease), retainedInputWorkspaceByteCount);
            if (openBudget == nullptr) {
                return failedDecodedImageResult(preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::OpenStaticImageSource));
            }

            QString errorString;
            bool resourceExhausted = false;
            std::shared_ptr<HeifDisplaySource> source;
            try {
                source = openHeifDisplaySource(sourceData.data, &errorString, openBudget,
                    retainedInputWorkspaceByteCount, &resourceExhausted);
            } catch (const std::bad_alloc&) {
                resourceExhausted = true;
            }
            openBudget->finalizePrechargedAdmission();
            if (source == nullptr) {
                if (errorString.isEmpty()) {
                    errorString = imageErrorText(ImageErrorTextId::ReadImageData);
                }
                return failedAdapterDecodedImageResult(std::move(errorString),
                    DecodedImageFailureRoute::HeifFamily,
                    DecodedImageFailureOperation::OpenStaticImageSource, QStringLiteral("HEIF"),
                    resourceExhausted ? DecodedImageFailureCause::ResourceLimitExceeded
                                      : DecodedImageFailureCause::Unknown);
            }
            return prepareStaticDisplaySource(route, std::move(sourceData), std::move(metadataData),
                request, priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                retainedInputWorkspaceByteCount, std::move(source), QStringLiteral("HEIF"));
        };
        return std::make_unique<PreparedImageDecodeWork>(openAdmissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::OpenStaticImageSource),
            std::move(open));
    }

    PreparedImageDecodeResult prepareHeifSequence(ImageDecodeRoute route,
        ImageSourceData sourceData, QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount,
        bool allowStillFallback)
    {
        const ImageDecodeWorkspaceAdmissionRequest probeAdmissionRequest {
            heifSequenceProbeWorkspaceByteCount,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto probe
            = [route, sourceData = std::move(sourceData), metadataData = std::move(metadataData),
                  request, priority, workspaceBudget = std::move(workspaceBudget),
                  retainedInputWorkspace = std::move(retainedInputWorkspace),
                  retainedInputWorkspaceByteCount, allowStillFallback](
                  ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            std::shared_ptr<ImageDecodeWorkspaceBudget> probeBudget
                = prechargedImageDecodeWorkspaceBudget(
                    std::move(lease), retainedInputWorkspaceByteCount);
            if (probeBudget == nullptr) {
                return failedDecodedImageResult(preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::DecodeHeifSequenceOpen));
            }
            HeifSequenceWorkspacePlanResult planning;
            try {
                planning = planHeifSequenceOpen(
                    sourceData.data, probeBudget, retainedInputWorkspaceByteCount);
            } catch (const std::bad_alloc&) {
                planning.status = HeifSequenceOpenStatus::ResourceLimitExceeded;
                planning.errorString = imageDecodeWorkspaceResourceLimitDiagnostic();
            }
            probeBudget->finalizePrechargedAdmission();
            switch (planning.status) {
            case HeifSequenceOpenStatus::NotHeif:
            case HeifSequenceOpenStatus::NotSequence:
                if (allowStillFallback) {
                    return prepareHeifStill(route, std::move(sourceData), std::move(metadataData),
                        request, priority, std::move(workspaceBudget),
                        std::move(retainedInputWorkspace), retainedInputWorkspaceByteCount);
                }
                return failedAdapterDecodedImageResult(planning.errorString.isEmpty()
                        ? imageErrorText(ImageErrorTextId::DecodeHeifSequence)
                        : std::move(planning.errorString),
                    DecodedImageFailureRoute::HeifFamily,
                    DecodedImageFailureOperation::DecodeHeifSequenceOpen, QStringLiteral("HEIF"));
            case HeifSequenceOpenStatus::Error:
                return failedAdapterDecodedImageResult(planning.errorString.isEmpty()
                        ? imageErrorText(ImageErrorTextId::DecodeHeifSequence)
                        : std::move(planning.errorString),
                    DecodedImageFailureRoute::HeifFamily,
                    DecodedImageFailureOperation::DecodeHeifSequenceOpen, QStringLiteral("HEIF"));
            case HeifSequenceOpenStatus::ResourceLimitExceeded:
                return failedDecodedImageResult(preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::DecodeHeifSequenceOpen));
            case HeifSequenceOpenStatus::Success:
                break;
            }

            // Catalog inspection retains the published first frame while decoding one later
            // frame, so both output holds overlap the reader's transient envelope.
            const std::optional<qsizetype> overlappingFrameOutputByteCount = checkedPreparedByteSum(
                planning.plan.outputByteCount, planning.plan.outputByteCount);
            const std::optional<qsizetype> producerPeakByteCount
                = overlappingFrameOutputByteCount.has_value()
                ? checkedPreparedByteSum(
                      planning.plan.transientByteCount, *overlappingFrameOutputByteCount)
                : std::nullopt;
            if (hasInvalidPreparedPeakByteCost(producerPeakByteCount)) {
                return failedDecodedImageResult(preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::DecodeHeifSequenceFrame));
            }
            const ImageDecodeWorkspaceAdmissionRequest producerAdmissionRequest {
                *producerPeakByteCount,
                retainedInputWorkspaceByteCount,
                priority,
            };
            auto produce =
                [route, sourceData = std::move(sourceData), metadataData = std::move(metadataData),
                    request, priority, workspaceBudget = std::move(workspaceBudget),
                    retainedInputWorkspace = std::move(retainedInputWorkspace),
                    retainedInputWorkspaceByteCount, plan = planning.plan](
                    ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
                std::shared_ptr<ImageDecodeWorkspaceBudget> producerBudget
                    = prechargedImageDecodeWorkspaceBudget(
                        std::move(lease), retainedInputWorkspaceByteCount);
                if (producerBudget == nullptr) {
                    return failedDecodedImageResult(preparedHardLimitFailure(
                        route.handlerKind, DecodedImageFailureOperation::DecodeHeifSequenceFrame));
                }
                std::optional<DecodedImageResult> decoded;
                try {
                    decoded = decodePlannedHeifSequenceImageData(sourceData.data, request, plan,
                        producerBudget, retainedInputWorkspaceByteCount);
                } catch (const std::bad_alloc&) {
                    decoded = failedDecodedImageResult(preparedHardLimitFailure(
                        route.handlerKind, DecodedImageFailureOperation::DecodeHeifSequenceFrame));
                }
                producerBudget->finalizePrechargedAdmission();
                DecodedImageResult result = decoded.has_value()
                    ? std::move(*decoded)
                    : failedAdapterDecodedImageResult(
                          imageErrorText(ImageErrorTextId::DecodeHeifSequence),
                          DecodedImageFailureRoute::HeifFamily,
                          DecodedImageFailureOperation::DecodeHeifSequenceOpen,
                          QStringLiteral("HEIF"));
                return finishPreparedDecode(std::move(result), std::move(sourceData), metadataData,
                    std::move(retainedInputWorkspace), workspaceBudget, priority,
                    route.handlerKind);
            };
            return std::make_unique<PreparedImageDecodeWork>(producerAdmissionRequest,
                preparedHardLimitFailure(
                    route.handlerKind, DecodedImageFailureOperation::DecodeHeifSequenceFrame),
                std::move(produce));
        };
        return std::make_unique<PreparedImageDecodeWork>(probeAdmissionRequest,
            preparedHardLimitFailure(
                route.handlerKind, DecodedImageFailureOperation::DecodeHeifSequenceOpen),
            std::move(probe));
    }

    PreparedImageDecodeResult prepareHeif(ImageDecodeRoute route, ImageSourceData sourceData,
        QByteArray metadataData, const ImageDecodeRequest& request,
        ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        const HeifContainerInfo info = heifContainerInfo(sourceData.data);
        if (info.imageSequence) {
            return prepareHeifSequence(route, std::move(sourceData), std::move(metadataData),
                request, priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                retainedInputWorkspaceByteCount, info.stillImage);
        }
        if (info.stillImage) {
            return prepareHeifStill(route, std::move(sourceData), std::move(metadataData), request,
                priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                retainedInputWorkspaceByteCount);
        }
        return failedAdapterDecodedImageResult(imageErrorText(ImageErrorTextId::ReadImageData),
            DecodedImageFailureRoute::HeifFamily,
            DecodedImageFailureOperation::OpenStaticImageSource, QStringLiteral("HEIF"));
    }

    PreparedImageDecodeResult prepareBroadRoutedImageData(ImageDecodeRouterRuntime runtime,
        ImageDecodeRoute route, ImageSourceData sourceData, QByteArray metadataData,
        const ImageDecodeRequest& request, ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        const qsizetype perOperationByteLimit = workspaceBudget->perOperationByteLimit();
        const qsizetype additionalPeakByteCount
            = retainedInputWorkspaceByteCount <= perOperationByteLimit
            ? perOperationByteLimit - retainedInputWorkspaceByteCount
            : 0;
        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            additionalPeakByteCount,
            retainedInputWorkspaceByteCount,
            priority,
        };
        auto execute = [runtime = std::move(runtime), route, sourceData = std::move(sourceData),
                           metadataData = std::move(metadataData), request, priority,
                           workspaceBudget = std::move(workspaceBudget),
                           retainedInputWorkspace = std::move(retainedInputWorkspace),
                           retainedInputWorkspaceByteCount](
                           ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            std::shared_ptr<ImageDecodeWorkspaceBudget> producerBudget
                = prechargedImageDecodeWorkspaceBudget(
                    std::move(lease), retainedInputWorkspaceByteCount);
            if (producerBudget == nullptr) {
                return failedDecodedImageResult(preparedHardLimitFailure(route.handlerKind));
            }
            DecodedImageResult result = runtime.executeRoutedData(
                route, sourceData.data, request, producerBudget, retainedInputWorkspaceByteCount);
            producerBudget->finalizePrechargedAdmission();
            return finishPreparedDecode(std::move(result), std::move(sourceData), metadataData,
                std::move(retainedInputWorkspace), workspaceBudget, priority, route.handlerKind);
        };
        return std::make_unique<PreparedImageDecodeWork>(
            admissionRequest, preparedHardLimitFailure(route.handlerKind), std::move(execute));
    }

    PreparedImageDecodeResult prepareRoutedImageData(ImageDecodeRouterRuntime runtime,
        ImageDecodeRoute route, ImageSourceData sourceData, QByteArray metadataData,
        const ImageDecodeRequest& request, ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        ImageDecodeWorkspaceHold retainedInputWorkspace, qsizetype retainedInputWorkspaceByteCount)
    {
        route.dataSource = ImageDecodeDataSource::Original;
        if (!runtime.hasInjectedHandler(route.handlerKind)) {
            if (route.handlerKind == ImageDecodeHandlerKind::Apng) {
                return prepareApng(route, std::move(sourceData), std::move(metadataData), request,
                    priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                    retainedInputWorkspaceByteCount);
            }
            if (route.handlerKind == ImageDecodeHandlerKind::Svg) {
                return prepareSvg(std::move(runtime), route, std::move(sourceData),
                    std::move(metadataData), request, priority, std::move(workspaceBudget),
                    std::move(retainedInputWorkspace), retainedInputWorkspaceByteCount);
            }
            if (route.handlerKind == ImageDecodeHandlerKind::Raw) {
                return prepareRaw(route, std::move(sourceData), std::move(metadataData), request,
                    priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                    retainedInputWorkspaceByteCount);
            }
            if (route.handlerKind == ImageDecodeHandlerKind::HeifFamily) {
                return prepareHeif(route, std::move(sourceData), std::move(metadataData), request,
                    priority, std::move(workspaceBudget), std::move(retainedInputWorkspace),
                    retainedInputWorkspaceByteCount);
            }
            if (route.handlerKind == ImageDecodeHandlerKind::QtRaster) {
                if (isPreparedQtRasterStillFormat(route.qtRasterFormat)) {
                    return prepareQtRasterStill(route, std::move(sourceData),
                        std::move(metadataData), request, priority, std::move(workspaceBudget),
                        std::move(retainedInputWorkspace), retainedInputWorkspaceByteCount);
                }
                if (route.qtRasterFormat == QtRasterFormat::Gif) {
                    return prepareQtGif(std::move(runtime), route, std::move(sourceData),
                        std::move(metadataData), request, priority, std::move(workspaceBudget),
                        std::move(retainedInputWorkspace), retainedInputWorkspaceByteCount);
                }
                if (route.qtRasterFormat == QtRasterFormat::Webp) {
                    return prepareWebPAnimation(std::move(runtime), route, std::move(sourceData),
                        std::move(metadataData), request, priority, std::move(workspaceBudget),
                        std::move(retainedInputWorkspace), retainedInputWorkspaceByteCount);
                }
                if (route.qtRasterFormat == QtRasterFormat::Jxl) {
                    return prepareJxlAnimation(route, std::move(sourceData),
                        std::move(metadataData), request, priority, std::move(workspaceBudget),
                        std::move(retainedInputWorkspace), retainedInputWorkspaceByteCount);
                }
            }
        }
        return prepareBroadRoutedImageData(std::move(runtime), route, std::move(sourceData),
            std::move(metadataData), request, priority, std::move(workspaceBudget),
            std::move(retainedInputWorkspace), retainedInputWorkspaceByteCount);
    }

    PreparedImageDecodeResult prepareCompatibleImageData(ImageDecodeRouterRuntime runtime,
        ImageDecodeRoute route, ImageSourceData sourceData, QByteArray metadataData,
        const ImageDecodeRequest& request, ImageDecodeWorkspacePriority priority,
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
    {
        const ImageDecodeCompatibleDataTransform transform = runtime.compatibleDataTransform();
        const std::optional<qsizetype> workspaceByteCost = transform
            ? (transform.workspaceByteCost ? transform.workspaceByteCost(sourceData.data.size())
                                           : std::nullopt)
            : avifCompatibilityWorkspaceByteCost(sourceData.data.size());
        if (hasInvalidPreparedWorkspaceByteCost(workspaceByteCost)) {
            return failedCompatibleDataWorkspaceResult(route.handlerKind);
        }

        const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
            *workspaceByteCost,
            0,
            priority,
        };
        const DecodedImageFailure hardLimitFailure
            = compatibleDataWorkspaceFailure(route.handlerKind);
        auto execute
            = [runtime = std::move(runtime), transform, route, sourceData = std::move(sourceData),
                  metadataData = std::move(metadataData), request, priority,
                  workspaceBudget = std::move(workspaceBudget)](
                  ImageDecodeWorkspaceLease lease) mutable -> PreparedImageDecodeResult {
            ImageDecodeCompatibleDataTransform::Result compatibleData;
            try {
                compatibleData = transform ? transform.apply(sourceData.data)
                                           : avifCompatibleImageData(sourceData.data);
            } catch (const std::bad_alloc&) {
                return failedCompatibleDataWorkspaceResult(route.handlerKind);
            }

            if (compatibleData.storage == ImageDecodeCompatibleDataTransform::Storage::Original) {
                route.dataSource = ImageDecodeDataSource::Original;
                return prepareRoutedImageData(std::move(runtime), route, std::move(sourceData),
                    std::move(metadataData), request, priority, std::move(workspaceBudget));
            }

            const qsizetype retainedInputWorkspaceByteCount = compatibleData.data.capacity();
            if (compatibleData.data.capacity() < compatibleData.data.size()
                || retainedInputWorkspaceByteCount > lease.reservedByteCount()) {
                return failedCompatibleDataWorkspaceResult(route.handlerKind);
            }
            ImageDecodeWorkspaceHold retainedInputWorkspace
                = lease.retainOnly(retainedInputWorkspaceByteCount);
            if (!retainedInputWorkspace.isManaged()) {
                return failedCompatibleDataWorkspaceResult(route.handlerKind);
            }
            sourceData.data = std::move(compatibleData.data);
            route.dataSource = ImageDecodeDataSource::Original;
            return prepareRoutedImageData(std::move(runtime), route, std::move(sourceData),
                std::move(metadataData), request, priority, std::move(workspaceBudget),
                std::move(retainedInputWorkspace), retainedInputWorkspaceByteCount);
        };
        return std::make_unique<PreparedImageDecodeWork>(
            admissionRequest, hardLimitFailure, std::move(execute));
    }
}

PreparedImageDecodeResult ImageDecodeRouter::prepare(ImageSourceData sourceData,
    const ImageDecodeRequest& request, ImageDecodeWorkspacePriority priority,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget) const
{
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    const ImageInputClassification classification
        = m_classifier(sourceData.data, request.imageUrl().fileName());
    const ImageDecodeRoute route = imageDecodeRouteForClassification(classification);
    qCDebug(kiriviewDecodeLog) << "image decode route"
                               << "generation" << request.id() << "url"
                               << kiriview::diagnosticSourceReference(request.imageUrl())
                               << "inputKind" << imageInputKindName(classification.kind)
                               << "handler" << imageDecodeHandlerKindName(route.handlerKind)
                               << "dataSource" << imageDecodeDataSourceName(route.dataSource)
                               << "qtFormat" << qtRasterFormatName(route.qtRasterFormat) << "bytes"
                               << sourceData.data.size();
    if (!route.shouldDecode()) {
        return failedReadImageDataResult();
    }
    QByteArray metadataData = sourceData.data;
    if (route.dataSource == ImageDecodeDataSource::AvifCompatible) {
        return prepareCompatibleImageData(m_runtime, route, std::move(sourceData),
            std::move(metadataData), request, priority, std::move(workspaceBudget));
    }
    return prepareRoutedImageData(m_runtime, route, std::move(sourceData), std::move(metadataData),
        request, priority, std::move(workspaceBudget));
}

DecodedImageResult decodeImageDataWithDefaultRouter(const QByteArray& data,
    const ImageDecodeRequest& request, std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    static const ImageDecodeRouter router;
    return router.decode(data, request, std::move(workspaceBudget));
}

ImageDataDecodePlanner defaultImageDataDecodePlanner(
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    auto router = std::make_shared<ImageDecodeRouter>();
    return [router = std::move(router), workspaceBudget = std::move(workspaceBudget)](
               ImageSourceData sourceData, const ImageDecodeRequest& request,
               ImageDecodeWorkspacePriority priority) {
        return router->prepare(std::move(sourceData), request, priority, workspaceBudget);
    };
}
}
