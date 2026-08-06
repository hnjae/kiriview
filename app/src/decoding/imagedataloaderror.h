// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDATALOADERROR_H
#define KIRIVIEW_IMAGEDATALOADERROR_H

#include "archive/mediaentrysourceerror.h"

#include <QString>
#include <functional>
#include <variant>

namespace kiriview {
using ImageDataLoadError = std::variant<QString, MediaEntrySourceError>;
using ImageDataLoadErrorCallback = std::function<void(ImageDataLoadError)>;
}

#endif
