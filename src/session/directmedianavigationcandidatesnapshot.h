// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DIRECTMEDIANAVIGATIONCANDIDATESNAPSHOT_H
#define KIRIVIEW_DIRECTMEDIANAVIGATIONCANDIDATESNAPSHOT_H

#include "navigation/directmedianavigationmodel.h"
#include "session/directmediacursor.h"

#include <QtGlobal>
#include <memory>
#include <vector>

namespace kiriview {
using DirectMediaNavigationCandidateRows = std::vector<DirectMediaNavigationCandidate>;

struct DirectMediaNavigationCandidateSnapshot
{
    DirectMediaScope source;
    quint64 revision = 0;
    std::shared_ptr<const DirectMediaNavigationCandidateRows> candidates
        = std::make_shared<const DirectMediaNavigationCandidateRows>();
    DirectMediaNavigationBoundaryState boundaryState;
    bool known = false;
};

inline const DirectMediaNavigationCandidateRows& directMediaNavigationCandidateRows(
    const DirectMediaNavigationCandidateSnapshot& snapshot)
{
    static const DirectMediaNavigationCandidateRows emptyRows;
    return snapshot.candidates != nullptr ? *snapshot.candidates : emptyRows;
}
}

#endif
