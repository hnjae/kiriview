// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDATALOADERROR_H
#define KIRIVIEW_IMAGEDATALOADERROR_H

#include "archive/mediaentrysourceerror.h"
#include "system/kiooperationfailure.h"

#include <functional>
#include <variant>

namespace kiriview {
using ImageDataLoadError = std::variant<KioOperationFailure, MediaEntrySourceError>;
using ImageDataLoadErrorCallback = std::function<void(ImageDataLoadError)>;
}

#endif
