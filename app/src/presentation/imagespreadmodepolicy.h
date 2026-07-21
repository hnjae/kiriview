// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESPREADMODEPOLICY_H
#define KIRIVIEW_IMAGESPREADMODEPOLICY_H

namespace kiriview {
struct ImageSpreadReadingAvailability
{
    bool hasImage = false;
    bool hasDisplayedImage = false;
    bool displayedDocumentIsComicBook = false;
};

bool imageSpreadReadingControlsAvailable(ImageSpreadReadingAvailability availability);
}

#endif
