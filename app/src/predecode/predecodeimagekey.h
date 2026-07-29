// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODEIMAGEKEY_H
#define KIRIVIEW_PREDECODEIMAGEKEY_H

#include "decoding/imagesourcerevision.h"
#include "location/imagelocation.h"

#include <QtGlobal>

namespace kiriview {
struct PredecodeImageKey
{
    DisplayedImageLocation location;
    ImageSourceRevision sourceRevision;

    [[nodiscard]] bool isValid() const { return !location.isEmpty() && sourceRevision.isValid(); }

    friend bool operator==(const PredecodeImageKey&, const PredecodeImageKey&) = default;
};

struct PredecodeWorkKey
{
    PredecodeImageKey image;
    quint64 lifecycleScope = 0;
};

inline bool samePredecodeWork(const PredecodeWorkKey& left, const PredecodeWorkKey& right)
{
    if (left.image.location != right.image.location) {
        return false;
    }
    if (left.image.sourceRevision.isValid() || right.image.sourceRevision.isValid()) {
        return left.image.sourceRevision.isValid() && right.image.sourceRevision.isValid()
            && left.image.sourceRevision == right.image.sourceRevision;
    }

    return left.lifecycleScope != 0 && right.lifecycleScope != 0
        && left.lifecycleScope == right.lifecycleScope;
}
}

#endif
