// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTNOTIFICATIONS_H
#define KIRIVIEW_IMAGEDOCUMENTNOTIFICATIONS_H

#include "imagedocumenttypes.h"

#include <vector>

namespace kiriview {
std::vector<ImageDocumentChange> imageDocumentTwoPageModeNotifications();
std::vector<ImageDocumentChange> imageDocumentRightToLeftReadingNotifications(
    bool secondaryPageVisible);
}

#endif
