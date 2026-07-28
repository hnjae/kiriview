// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpredecodedimagelookup.h"

#include "imagedocumentpredecodecontroller.h"

#include <utility>

namespace kiriview {
ImageDocumentPredecodedImageLookup::ImageDocumentPredecodedImageLookup(
    const ImageDocumentPredecodeController& predecodeController, ExternalFinder externalFinder)
    : m_predecodeController(predecodeController)
    , m_externalFinder(std::move(externalFinder))
{
}

std::optional<PredecodedImage> ImageDocumentPredecodedImageLookup::find(
    const DisplayedImageLocation& location) const
{
    if (m_externalFinder) {
        std::optional<PredecodedImage> predecoded = m_externalFinder(location);
        if (predecoded.has_value()) {
            return predecoded;
        }
    }

    return m_predecodeController.findPredecodedImage(location);
}
}
