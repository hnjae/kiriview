// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_NAVIGATIONBOUNDARYCORRELATION_H
#define KIRIVIEW_NAVIGATIONBOUNDARYCORRELATION_H

#include "location/imageurl.h"

#include <QMetaType>
#include <QUrl>
#include <optional>

namespace kiriview {
enum class NavigationBoundaryEdge {
    First,
    Last,
};

struct NavigationBoundaryFacts
{
    bool available = false;
    bool known = false;
    bool atKnownFirst = false;
    bool atKnownLast = false;
    int scope = -1;
    QUrl selectionUrl;
};

struct NavigationBoundaryCorrelation
{
    NavigationBoundaryEdge edge = NavigationBoundaryEdge::First;
    int scope = -1;
    QUrl selectionUrl;
};

[[nodiscard]] inline bool navigationBoundaryIsCurrent(
    NavigationBoundaryEdge edge, const NavigationBoundaryFacts& facts)
{
    return facts.available && facts.known && !facts.selectionUrl.isEmpty()
        && (edge == NavigationBoundaryEdge::First ? facts.atKnownFirst : facts.atKnownLast);
}

[[nodiscard]] inline std::optional<NavigationBoundaryCorrelation> correlateNavigationBoundary(
    NavigationBoundaryEdge edge, const NavigationBoundaryFacts& facts)
{
    if (!navigationBoundaryIsCurrent(edge, facts)) {
        return std::nullopt;
    }
    return NavigationBoundaryCorrelation { edge, facts.scope, facts.selectionUrl };
}

[[nodiscard]] inline bool navigationBoundaryCorrelationIsCurrent(
    const NavigationBoundaryCorrelation& correlation, const NavigationBoundaryFacts& facts)
{
    return facts.scope == correlation.scope
        && sameNormalizedUrl(facts.selectionUrl, correlation.selectionUrl)
        && navigationBoundaryIsCurrent(correlation.edge, facts);
}
}

Q_DECLARE_METATYPE(kiriview::NavigationBoundaryCorrelation)

#endif
