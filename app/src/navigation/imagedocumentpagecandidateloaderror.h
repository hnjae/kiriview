// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATELOADERROR_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATELOADERROR_H

#include "archive/mediaentrysourceerror.h"
#include "system/kiooperationfailure.h"

#include <QString>
#include <functional>
#include <variant>

namespace kiriview {
using ImageDocumentPageCandidateLoadError
    = std::variant<QString, KioOperationFailure, MediaEntrySourceError>;
using ImageDocumentPageCandidateLoadErrorCallback
    = std::function<void(ImageDocumentPageCandidateLoadError)>;
}

#endif
