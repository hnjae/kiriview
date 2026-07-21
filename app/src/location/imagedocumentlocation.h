// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTLOCATION_H
#define KIRIVIEW_IMAGEDOCUMENTLOCATION_H

#include "location/imagelocation.h"

#include <QString>
#include <QUrl>
#include <optional>

namespace kiriview {
std::optional<OpenedCollectionScopeLocation> openedCollectionScopeLocationForLocalArchiveSource(
    const ResolvedNavigationSource& source);
std::optional<OpenedCollectionScopeLocation> openedCollectionScopeLocationForResolvedExternalSource(
    const ResolvedNavigationSource& source);
bool openedCollectionScopeContainsUrl(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& url);
bool displayedLocationIsInsideOpenedCollectionScope(const DisplayedImageLocation& location);
QString windowTitleFileNameForDisplayedLocation(const DisplayedImageLocation& location);
QUrl containerNavigationUrlForLocation(const DisplayedImageLocation& location);
}

#endif
