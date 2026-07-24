// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentnotifications.h"

namespace kiriview {
std::vector<ImageDocumentChange> imageDocumentTwoPageModeNotifications()
{
    return { ImageDocumentChange::TwoPageMode };
}

std::vector<ImageDocumentChange> imageDocumentRightToLeftReadingNotifications(
    bool secondaryPageVisible)
{
    std::vector<ImageDocumentChange> changes { ImageDocumentChange::RightToLeftReading };
    if (secondaryPageVisible) {
        changes.push_back(ImageDocumentChange::TwoPageMode);
    }
    return changes;
}

}
