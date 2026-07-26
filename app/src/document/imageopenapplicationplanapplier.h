// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEOPENAPPLICATIONPLANAPPLIER_H
#define KIRIVIEW_IMAGEOPENAPPLICATIONPLANAPPLIER_H

#include "imageopenapplicationplan.h"

#include <expected>

namespace kiriview {
class ImageDocumentState;

enum class ImageOpenApplicationError {
    InvalidFinalState,
};

using ImageOpenApplicationResult
    = std::expected<ImageDocumentRuntimePlan, ImageOpenApplicationError>;

[[nodiscard]] ImageOpenApplicationResult tryApplyImageOpenApplicationPlan(
    ImageDocumentState& state, ImageOpenApplicationPlan plan);
ImageDocumentRuntimePlan applyImageOpenApplicationPlan(
    ImageDocumentState& state, ImageOpenApplicationPlan plan);
}

#endif
