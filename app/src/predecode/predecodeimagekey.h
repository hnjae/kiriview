// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODEIMAGEKEY_H
#define KIRIVIEW_PREDECODEIMAGEKEY_H

#include "decoding/imagesourcerevision.h"
#include "location/imagelocation.h"

#include <QtGlobal>

namespace kiriview {
enum class PredecodeWorkScopeKind : quint8 {
    Invalid,
    CandidateSnapshot,
    ScheduleFallback,
};

struct PredecodeWorkScope
{
    PredecodeWorkScopeKind kind = PredecodeWorkScopeKind::Invalid;
    quint64 revision = 0;

    static PredecodeWorkScope candidateSnapshot(quint64 revision)
    {
        return { PredecodeWorkScopeKind::CandidateSnapshot, revision };
    }

    static PredecodeWorkScope scheduleFallback(quint64 generation)
    {
        return { PredecodeWorkScopeKind::ScheduleFallback, generation };
    }

    [[nodiscard]] bool isValid() const { return kind != PredecodeWorkScopeKind::Invalid; }

    friend bool operator==(const PredecodeWorkScope&, const PredecodeWorkScope&) = default;
};

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
    PredecodeWorkScope scope;
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

    return left.scope.isValid() && right.scope.isValid() && left.scope == right.scope;
}
}

#endif
