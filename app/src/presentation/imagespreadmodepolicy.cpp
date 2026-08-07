// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagespreadmodepolicy.h"

namespace kiriview {
bool imageSpreadPrimaryPageEligible(ImageSpreadPrimaryPageEligibility eligibility)
{
    return eligibility.hasImage && eligibility.hasDisplayedImage
        && eligibility.displayedDocumentIsComicBook;
}

}
