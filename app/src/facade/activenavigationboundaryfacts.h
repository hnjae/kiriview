// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONBOUNDARYFACTS_H
#define KIRIVIEW_ACTIVENAVIGATIONBOUNDARYFACTS_H

#include "application/navigationboundarycorrelation.h"
#include "facade/kiridocumentsession.h"
#include "facade/kiriimagedocument.h"

#include <cstddef>

namespace kiriview {
[[nodiscard]] inline NavigationBoundaryFacts activeNavigationBoundaryFacts(
    const KiriDocumentSession* session)
{
    NavigationBoundaryFacts facts;
    if (session == nullptr) {
        return facts;
    }

    facts.available = session->activeNavigationAvailable();
    facts.known = session->activeNavigationKnown();
    facts.atKnownFirst = session->atKnownFirstActiveNavigation();
    facts.atKnownLast = session->atKnownLastActiveNavigation();
    facts.scope = static_cast<int>(session->activeNavigationBoundaryScope());
    switch (session->activeNavigationBoundaryScope()) {
    case KiriDocumentSession::ActiveNavigationBoundaryScope::DirectMediaNavigationBoundary:
        facts.selectionUrl = session->sourceUrl();
        break;
    case KiriDocumentSession::ActiveNavigationBoundaryScope::ImageDocumentPageNavigationBoundary: {
        const KiriImageDocument* imageDocument = session->imageDocument();
        if (imageDocument == nullptr) {
            break;
        }
        const ImageDocumentPageNavigationSnapshot snapshot
            = imageDocument->pageNavigationSnapshot();
        if (snapshot.state.currentIndex < 0) {
            break;
        }
        const std::size_t currentIndex = static_cast<std::size_t>(snapshot.state.currentIndex);
        if (currentIndex < snapshot.state.targets.size()) {
            facts.selectionUrl = snapshot.state.targets.at(currentIndex).url;
        }
        break;
    }
    case KiriDocumentSession::ActiveNavigationBoundaryScope::NoNavigationBoundary:
        break;
    }

    return facts;
}
}

#endif
