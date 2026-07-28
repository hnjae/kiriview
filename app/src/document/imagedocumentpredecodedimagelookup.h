// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPREDECODEDIMAGELOOKUP_H
#define KIRIVIEW_IMAGEDOCUMENTPREDECODEDIMAGELOOKUP_H

#include "predecode/predecodedimage.h"

#include <functional>
#include <optional>

namespace kiriview {
class ImageDocumentPredecodeController;

class ImageDocumentPredecodedImageLookup final
{
public:
    using ExternalFinder
        = std::function<std::optional<PredecodedImage>(const DisplayedImageLocation&)>;

    explicit ImageDocumentPredecodedImageLookup(
        const ImageDocumentPredecodeController& predecodeController,
        ExternalFinder externalFinder = {});

    [[nodiscard]] std::optional<PredecodedImage> find(const DisplayedImageLocation& location) const;

private:
    const ImageDocumentPredecodeController& m_predecodeController;
    ExternalFinder m_externalFinder;
};
}

#endif
