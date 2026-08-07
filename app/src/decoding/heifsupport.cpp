// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "heifsupport.h"

#include "localization/imageerrortext.h"
#include "rendering/imagerendering.h"

#include <QColorSpace>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <utility>

namespace {
void setHeifSupportError(QString* errorString, const QString& message)
{
    if (errorString != nullptr) {
        *errorString = message;
    }
}
}

namespace kiriview {
QString heifErrorString(const QString& action, heif_error error)
{
    QString message = imageErrorText(ImageErrorTextId::UnknownLibheifError);
    if (error.message != nullptr) {
        message = QString::fromUtf8(error.message);
    }
    return heifDecodeErrorText(action, message);
}

namespace {
    struct HeifInitializationFailure
    {
        QString errorString;
        bool resourceLimitExceeded = false;
    };

    std::optional<HeifInitializationFailure> initializeHeifLibrary()
    {
        static std::once_flag initFlag;
        static heif_error initError {};
        std::call_once(initFlag, [] { initError = heif_init(nullptr); });

        if (initError.code != heif_error_Ok) {
            return HeifInitializationFailure {
                heifErrorString(
                    imageErrorActionText(ImageErrorActionTextId::InitializeLibheif), initError),
                initError.code == heif_error_Memory_allocation_error
                    || initError.subcode == heif_suberror_Security_limit_exceeded,
            };
        }
        return std::nullopt;
    }
}

HeifContext::HeifContext()
    : m_context(heif_context_alloc())
{
}

heif_context* HeifContext::get() const { return m_context.get(); }

heif_image_handle** HeifImageHandle::out() { return m_handle.out(); }

const heif_image_handle* HeifImageHandle::get() const { return m_handle.get(); }

HeifTrack::HeifTrack(heif_track* track)
    : m_track(track)
{
}

heif_track* HeifTrack::get() const { return m_track.get(); }

heif_image** HeifImage::out() { return m_image.out(); }

const heif_image* HeifImage::get() const { return m_image.get(); }

HeifDecodingOptions::HeifDecodingOptions()
    : m_options(heif_decoding_options_alloc())
{
    if (m_options.get() != nullptr) {
        m_options.get()->convert_hdr_to_8bit = 1;
    }
}

const heif_decoding_options* HeifDecodingOptions::get() const { return m_options.get(); }

void HeifDecodingOptions::setIgnoreSequenceEditList(bool ignore)
{
    if (m_options.get() != nullptr) {
        m_options.get()->ignore_sequence_editlist = ignore ? 1 : 0;
    }
}

std::optional<HeifContext> openHeifContext(const QByteArray& data, QString* errorString,
    HeifContextOpenLimits requestedLimits, bool* resourceLimitExceeded,
    HeifContextInheritedLimits* inheritedLimits)
{
    if (resourceLimitExceeded != nullptr) {
        *resourceLimitExceeded = false;
    }
    if (std::optional<HeifInitializationFailure> initFailure = initializeHeifLibrary()) {
        if (resourceLimitExceeded != nullptr) {
            *resourceLimitExceeded = initFailure->resourceLimitExceeded;
        }
        setHeifSupportError(errorString, initFailure->errorString);
        return std::nullopt;
    }

    HeifContext context;
    if (context.get() == nullptr) {
        if (resourceLimitExceeded != nullptr) {
            *resourceLimitExceeded = true;
        }
        setHeifSupportError(
            errorString, imageErrorText(ImageErrorTextId::HeifContextAllocationFailed));
        return std::nullopt;
    }

    const heif_security_limits originalLimits = *heif_context_get_security_limits(context.get());
    if (inheritedLimits != nullptr) {
        inheritedLimits->maximumTotalMemory = originalLimits.max_total_memory;
    }

    if (requestedLimits.maximumTotalMemory > 0) {
        heif_security_limits limits = originalLimits;
        limits.max_total_memory = originalLimits.max_total_memory == 0
            ? requestedLimits.maximumTotalMemory
            : std::min(originalLimits.max_total_memory, requestedLimits.maximumTotalMemory);
        const heif_error limitsError = heif_context_set_security_limits(context.get(), &limits);
        if (limitsError.code != heif_error_Ok) {
            if (resourceLimitExceeded != nullptr) {
                *resourceLimitExceeded = limitsError.code == heif_error_Memory_allocation_error
                    || limitsError.subcode == heif_suberror_Security_limit_exceeded;
            }
            setHeifSupportError(errorString,
                heifErrorString(
                    imageErrorActionText(ImageErrorActionTextId::ReadHeifContainer), limitsError));
            return std::nullopt;
        }
    }

    heif_error error = heif_context_read_from_memory_without_copy(
        context.get(), data.constData(), static_cast<size_t>(data.size()), nullptr);
    if (error.code != heif_error_Ok) {
        if (resourceLimitExceeded != nullptr) {
            *resourceLimitExceeded = error.code == heif_error_Memory_allocation_error
                || error.subcode == heif_suberror_Security_limit_exceeded;
        }
        setHeifSupportError(errorString,
            heifErrorString(
                imageErrorActionText(ImageErrorActionTextId::ReadHeifContainer), error));
        return std::nullopt;
    }

    return std::optional<HeifContext>(std::move(context));
}

std::optional<HeifPrimaryImage> openHeifPrimaryImage(const QByteArray& data, QString* errorString)
{
    std::optional<HeifContext> context = openHeifContext(data, errorString);
    if (!context.has_value()) {
        return std::nullopt;
    }

    HeifImageHandle handle;
    const heif_error error = heif_context_get_primary_image_handle(context->get(), handle.out());
    if (error.code != heif_error_Ok) {
        setHeifSupportError(errorString,
            heifErrorString(imageErrorActionText(ImageErrorActionTextId::ReadPrimaryImage), error));
        return std::nullopt;
    }

    return HeifPrimaryImage { std::move(*context), std::move(handle) };
}

std::optional<QImage> qImageFromHeifImage(const heif_image* heifImage, QString* errorString,
    HeifImageConversionFailureCause* failureCause)
{
    if (failureCause != nullptr) {
        *failureCause = HeifImageConversionFailureCause::Invalid;
    }
    if (heifImage == nullptr) {
        if (errorString != nullptr) {
            *errorString = imageErrorText(ImageErrorTextId::HeifDecodedImageInvalid);
        }
        return std::nullopt;
    }

    const int imageWidth = heif_image_get_width(heifImage, heif_channel_interleaved);
    const int imageHeight = heif_image_get_height(heifImage, heif_channel_interleaved);
    if (imageWidth <= 0 || imageHeight <= 0) {
        if (errorString != nullptr) {
            *errorString = imageErrorText(ImageErrorTextId::HeifDecodedImageSizeInvalid);
        }
        return std::nullopt;
    }

    size_t sourceStride = 0;
    const uint8_t* source
        = heif_image_get_plane_readonly2(heifImage, heif_channel_interleaved, &sourceStride);
    constexpr size_t bytesPerPixel = 4;
    const size_t rowBytes = static_cast<size_t>(imageWidth) * bytesPerPixel;
    if (source == nullptr || sourceStride < rowBytes) {
        if (errorString != nullptr) {
            *errorString = imageErrorText(ImageErrorTextId::HeifDecodedPixelDataInvalid);
        }
        return std::nullopt;
    }

    QImage image(imageWidth, imageHeight, QImage::Format_RGBA8888);
    if (image.isNull()) {
        if (failureCause != nullptr) {
            *failureCause = HeifImageConversionFailureCause::ResourceLimitExceeded;
        }
        if (errorString != nullptr) {
            *errorString = imageErrorText(ImageErrorTextId::HeifDecodedImageAllocationFailed);
        }
        return std::nullopt;
    }

    for (int y = 0; y < imageHeight; ++y) {
        std::memcpy(image.scanLine(y), source + (static_cast<size_t>(y) * sourceStride), rowBytes);
    }
    image.setColorSpace(QColorSpace(QColorSpace::SRgb));

    QImage displayImage = displayReadyImage(image);
    if (displayImage.isNull()) {
        if (failureCause != nullptr) {
            *failureCause = HeifImageConversionFailureCause::ResourceLimitExceeded;
        }
        if (errorString != nullptr) {
            *errorString = imageErrorText(ImageErrorTextId::HeifDecodedImageAllocationFailed);
        }
        return std::nullopt;
    }
    return displayImage;
}

}
