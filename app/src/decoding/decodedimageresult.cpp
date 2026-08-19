// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decodedimageresult.h"

#include <type_traits>
#include <utility>
#include <variant>

namespace kiriview {
DecodedImageResult failedDecodedImageResult(QString diagnosticDetail)
{
    return std::unexpected(DecodedImageFailure {
        DecodedImageFailureRoute::Unknown,
        DecodedImageFailureOperation::Unknown,
        std::move(diagnosticDetail),
        DecodedImageFailureSeverity::Error,
        false,
    });
}

DecodedImageResult failedDecodedImageResult(DecodedImageFailure failure)
{
    return std::unexpected(std::move(failure));
}

DecodedImageResult successfulDecodedImageResult(DecodedImage image) { return image; }

const DecodedImageFailure* decodedImageResultFailure(const DecodedImageResult& result)
{
    return result ? nullptr : &result.error();
}

DecodedImageFailure* decodedImageResultFailure(DecodedImageResult& result)
{
    return result ? nullptr : &result.error();
}

const DecodedImage* decodedImageResultImage(const DecodedImageResult& result)
{
    return result ? &*result : nullptr;
}

DecodedImage* decodedImageResultImage(DecodedImageResult& result)
{
    return result ? &*result : nullptr;
}

const EmbeddedMetadata& decodedImageEmbeddedMetadata(const DecodedImage& image)
{
    return std::visit(
        [](const auto& decoded) -> const EmbeddedMetadata& { return decoded.embeddedMetadata; },
        image);
}

void setDecodedImageEmbeddedMetadata(DecodedImage& image, EmbeddedMetadata metadata)
{
    std::visit(
        [&metadata](auto& decoded) {
            decoded.embeddedMetadata = metadata;
            if constexpr (std::is_same_v<std::decay_t<decltype(decoded)>, StaticDecodedImage>) {
                decoded.displayImage.embeddedMetadata = metadata;
            }
        },
        image);
}
}
