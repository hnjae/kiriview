// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DIRECTMEDIACURSOR_H
#define KIRIVIEW_DIRECTMEDIACURSOR_H

#include "location/imageurl.h"
#include "location/sourcekey.h"

#include <QUrl>
#include <QtGlobal>

namespace kiriview {
struct DirectMediaCursor
{
    ResolvedNavigationSource stableSource;
    ResolvedNavigationSource pendingSource;
    quint64 generation = 0;
};

struct DirectMediaScope
{
    QUrl currentUrl;
    QUrl parentUrl;
    quint64 generation = 0;
    SourceKey currentKey;
    SourceKey parentKey;
    QUrl navigationUrl;

    friend bool operator==(const DirectMediaScope& left, const DirectMediaScope& right)
    {
        return sameSourceKey(left.currentKey, right.currentKey)
            && sameSourceKey(left.parentKey, right.parentKey)
            && left.generation == right.generation;
    }
};

QUrl effectiveDirectMediaCursorUrl(const DirectMediaCursor& cursor);
DirectMediaScope directMediaScopeForCursor(const DirectMediaCursor& cursor);
bool directMediaScopeMatchesCursor(const DirectMediaCursor& cursor, const DirectMediaScope& scope);
bool clearDirectMediaCursor(DirectMediaCursor& cursor);
bool requestDirectImageCursor(DirectMediaCursor& cursor, ResolvedNavigationSource source);
bool confirmDirectImageCursor(DirectMediaCursor& cursor, const QUrl& url);
bool restoreDirectImageCursorAfterFailure(DirectMediaCursor& cursor);
bool setDirectVideoCursor(DirectMediaCursor& cursor, ResolvedNavigationSource source);
}

#endif
