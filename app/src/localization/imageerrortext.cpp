// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "localization/imageerrortext.h"

#include "decoding/decodedimagefailure.h"

#include <KLocalizedString>

namespace kiriview {
QString imageErrorText(ImageErrorTextId id)
{
    switch (id) {
    case ImageErrorTextId::ReadImageData:
        return i18n("Could not read the selected image data.");
    case ImageErrorTextId::DecodeImage:
        return i18n("Could not decode the selected image.");
    case ImageErrorTextId::DecodeImageResourceLimitExceeded:
        return i18n("The selected image is too large to decode within KiriView’s resource limits.");
    case ImageErrorTextId::DecodeSvgImage:
        return i18n("Could not decode the selected SVG image.");
    case ImageErrorTextId::DecodeHeifImage:
        return i18n("Could not decode the selected HEIF image.");
    case ImageErrorTextId::DecodeRawImage:
        return i18n("Could not decode the selected RAW image.");
    case ImageErrorTextId::OpenVideo:
        return i18n("Could not open the selected video.");
    case ImageErrorTextId::DecodeApngAnimation:
        return i18n("Could not decode the selected APNG animation.");
    case ImageErrorTextId::DecodeImageAnimation:
        return i18n("Could not decode the selected image animation.");
    case ImageErrorTextId::EmptyOpenedCollection:
        return i18n("The selected collection does not contain any supported media.");
    case ImageErrorTextId::OpenOpenedCollection:
        return i18n("Could not open the selected collection.");
    case ImageErrorTextId::OpenComicBookArchive:
        return i18n("Could not open the selected comic book archive.");
    case ImageErrorTextId::DeleteFile:
        return i18n("Could not delete the selected file.");
    case ImageErrorTextId::DecodeHeifSequence:
        return i18n("Could not decode the selected HEIF image sequence.");
    }

    return {};
}

QString decodedImageFailureText(const DecodedImageFailure& failure)
{
    if (failure.cause == DecodedImageFailureCause::ResourceLimitExceeded) {
        return imageErrorText(ImageErrorTextId::DecodeImageResourceLimitExceeded);
    }

    if (failure.route == DecodedImageFailureRoute::Apng) {
        return imageErrorText(ImageErrorTextId::DecodeApngAnimation);
    }
    if (failure.operation == DecodedImageFailureOperation::DecodeHeifSequenceOpen
        || failure.operation == DecodedImageFailureOperation::DecodeHeifSequenceFrame) {
        return imageErrorText(ImageErrorTextId::DecodeHeifSequence);
    }
    if (failure.operation == DecodedImageFailureOperation::DecodeAnimationOpen) {
        return imageErrorText(ImageErrorTextId::DecodeImageAnimation);
    }
    if (failure.route == DecodedImageFailureRoute::Svg) {
        return imageErrorText(ImageErrorTextId::DecodeSvgImage);
    }
    if (failure.route == DecodedImageFailureRoute::HeifFamily) {
        return imageErrorText(ImageErrorTextId::DecodeHeifImage);
    }
    if (failure.route == DecodedImageFailureRoute::Raw) {
        return imageErrorText(ImageErrorTextId::DecodeRawImage);
    }
    return imageErrorText(ImageErrorTextId::DecodeImage);
}
}
