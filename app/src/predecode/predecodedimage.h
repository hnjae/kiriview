// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODEDIMAGE_H
#define KIRIVIEW_PREDECODEDIMAGE_H

#include "location/imagelocation.h"
#include "metadata/embeddedmetadata.h"
#include "rendering/staticimage.h"

#include <optional>

namespace kiriview {
struct PredecodedImage
{
    StaticDisplayImagePayload displayImage;
    DisplayedImageLocation location;
    EmbeddedMetadata embeddedMetadata;
};

struct DisplayedPredecodeImage
{
    DisplayedImageLocation location;
    bool cacheable = false;
    std::optional<StaticDisplayImagePayload> displayImage;
    EmbeddedMetadata embeddedMetadata;

    [[nodiscard]] bool hasLocation() const { return !location.isEmpty(); }

    [[nodiscard]] bool hasDisplayImage() const
    {
        return displayImage.has_value() && displayImage->isValid();
    }

    [[nodiscard]] bool isCacheable() const { return cacheable && hasDisplayImage(); }
};
}

#endif
