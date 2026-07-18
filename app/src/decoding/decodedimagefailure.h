// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DECODEDIMAGEFAILURE_H
#define KIRIVIEW_DECODEDIMAGEFAILURE_H

#include <QString>

namespace kiriview {
enum class DecodedImageFailureRoute {
    Unknown,
    Svg,
    Apng,
    HeifFamily,
    Raw,
    QtRaster,
};

enum class DecodedImageFailureOperation {
    Unknown,
    OpenStaticImageSource,
    DecodeFirstDisplayImage,
    DecodeBlockingDisplayImage,
    DecodeAnimationOpen,
    DecodeRawImage,
    DecodeHeifSequenceOpen,
    DecodeHeifSequenceFrame,
};

enum class DecodedImageFailureSeverity {
    Error,
};

struct DecodedImageFailure
{
    QString errorString;
    DecodedImageFailureRoute route = DecodedImageFailureRoute::Unknown;
    DecodedImageFailureOperation operation = DecodedImageFailureOperation::Unknown;
    QString diagnosticDetail;
    DecodedImageFailureSeverity severity = DecodedImageFailureSeverity::Error;
    bool retryable = false;
};
}

#endif
