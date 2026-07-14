// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEOPENAPPLICATIONPLANAPPLIER_H
#define KIRIVIEW_IMAGEOPENAPPLICATIONPLANAPPLIER_H

#include "imageopenapplicationplan.h"

namespace kiriview {
class ImageDocumentState;

ImageDocumentRuntimePlan applyImageOpenApplicationPlan(
    ImageDocumentState& state, ImageOpenApplicationPlan plan);
}

#endif
