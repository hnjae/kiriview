// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedecodepipeline.h"

#include "apnganimationreader.h"
#include "avifcompatibility.h"
#include "cache/imagebyteaccounting.h"
#include "diagnostics/diagnosticlogprojection.h"
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
#include "rendering/svgdisplaysource.h"
#include "staticimagedecode.h"
#include "webpanimationreader.h"

#include <QDebug>
#include <QString>
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
            m_workspaceLease = m_workspaceBudget->startLease();
            if (!m_workspaceLease.tryReserve(*workspaceByteCost)) {
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

kiriview::DecodedImageResult failedCompatibleDataWorkspaceResult(
    kiriview::ImageDecodeHandlerKind handlerKind)
{
    return kiriview::failedDecodedImageResult(kiriview::DecodedImageFailure {
        kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData),
        decodedFailureRouteForHandlerKind(handlerKind),
        kiriview::DecodedImageFailureOperation::OpenStaticImageSource,
        kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
        kiriview::DecodedImageFailureSeverity::Error,
        false,
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
    });
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
    : m_handlers(withDefaultHandlers(std::move(handlers)))
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

    DecodedImageResult result = dispatchToHandler(
        handlerForRoute(m_handlers, route.handlerKind), input, route.handlerKind);
    if (decodedImageResultImage(result) != nullptr && byteInputs.compatibleDataRequiresRetention()
        && !retainCompatibleDataWorkspace(result, byteInputs.takeRetainedWorkspace())) {
        return failedCompatibleDataWorkspaceResult(route.handlerKind);
    }
    return result;
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
            = workspaceBudget->startLeaseForOperation(decodedImageWorkspaceByteCost(*image));
        if (metadataWorkspace.tryReserve(embeddedMetadataWorkspaceReservation)) {
            EmbeddedMetadata metadata = parseImageEmbeddedMetadata(data);
            if (!metadata.isEmpty()) {
                setDecodedImageEmbeddedMetadata(*image, std::move(metadata));
            }
        }
    }
    return result;
}

DecodedImageResult decodeImageDataWithDefaultRouter(const QByteArray& data,
    const ImageDecodeRequest& request, std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
{
    static const ImageDecodeRouter router;
    return router.decode(data, request, std::move(workspaceBudget));
}
}
