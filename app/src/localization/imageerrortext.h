// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEERRORTEXT_H
#define KIRIVIEW_IMAGEERRORTEXT_H

#include <QString>

namespace kiriview {
struct DecodedImageFailure;

enum class ImageErrorTextId {
    ReadImageData,
    DecodeImage,
    DecodeImageResourceLimitExceeded,
    DecodeSvgImage,
    DecodeHeifImage,
    DecodeRawImage,
    OpenVideo,
    DecodeApngAnimation,
    DecodeImageAnimation,
    EmptyOpenedCollection,
    OpenOpenedCollection,
    OpenComicBookArchive,
    DeleteFile,
    DecodeHeifSequence,
};

QString imageErrorText(ImageErrorTextId id);
QString decodedImageFailureText(const DecodedImageFailure& failure);
}

#endif
