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
    DecodeRasterDisplayImage,
    DecodeAnimationOpen,
    DecodeRawImage,
    DecodeHeifSequenceOpen,
    DecodeHeifSequenceFrame,
};

enum class DecodedImageFailureSeverity {
    Error,
};

enum class DecodedImageFailureCause {
    Unknown,
    ResourceLimitExceeded,
};

struct DecodedImageFailure
{
    DecodedImageFailureRoute route = DecodedImageFailureRoute::Unknown;
    DecodedImageFailureOperation operation = DecodedImageFailureOperation::Unknown;
    QString diagnosticDetail;
    DecodedImageFailureSeverity severity = DecodedImageFailureSeverity::Error;
    bool retryable = false;
    DecodedImageFailureCause cause = DecodedImageFailureCause::Unknown;
};
}

#endif
