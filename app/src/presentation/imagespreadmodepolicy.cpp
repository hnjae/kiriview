// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagespreadmodepolicy.h"

namespace kiriview {
bool imageSpreadReadingControlsAvailable(ImageSpreadReadingAvailability availability)
{
    return availability.hasImage && availability.hasDisplayedImage
        && availability.displayedDocumentIsComicBook;
}

}
