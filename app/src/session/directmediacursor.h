// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DIRECTMEDIACURSOR_H
#define KIRIVIEW_DIRECTMEDIACURSOR_H

#include "location/imagelocation.h"

#include <QUrl>
#include <QtGlobal>
#include <optional>

namespace kiriview {
struct DirectMediaCursor
{
    ResolvedNavigationSource stableSource;
    ResolvedNavigationSource pendingSource;
    quint64 generation = 0;
};

class DirectMediaScope final
{
public:
    static std::optional<DirectMediaScope> fromSource(
        const ResolvedNavigationSource& source, quint64 generation);

    [[nodiscard]] const ResolvedNavigationSource& source() const { return m_source; }
    [[nodiscard]] const DirectMediaPageScopeIdentity& pageScopeIdentity() const
    {
        return m_pageScopeIdentity;
    }
    [[nodiscard]] const QUrl& currentUrl() const { return m_source.requestedUrl(); }
    [[nodiscard]] const QUrl& parentUrl() const { return m_parentUrl; }
    [[nodiscard]] quint64 generation() const { return m_generation; }
    [[nodiscard]] const SourceKey& currentKey() const { return m_pageScopeIdentity.currentKey(); }
    [[nodiscard]] const SourceKey& parentKey() const { return m_pageScopeIdentity.parentKey(); }
    [[nodiscard]] const QUrl& navigationUrl() const { return m_source.navigationUrl(); }

    friend bool operator==(const DirectMediaScope& left, const DirectMediaScope& right)
    {
        return left.m_pageScopeIdentity == right.m_pageScopeIdentity
            && left.m_generation == right.m_generation;
    }

private:
    DirectMediaScope(ResolvedNavigationSource source, QUrl parentUrl,
        DirectMediaPageScopeIdentity pageScopeIdentity, quint64 generation);

    ResolvedNavigationSource m_source;
    QUrl m_parentUrl;
    DirectMediaPageScopeIdentity m_pageScopeIdentity;
    quint64 m_generation = 0;
};

enum class DirectMediaConfirmation {
    Committed,
    Stale,
    Bypassed,
};

QUrl effectiveDirectMediaCursorUrl(const DirectMediaCursor& cursor);
std::optional<DirectMediaScope> directMediaScopeForCursor(const DirectMediaCursor& cursor);
bool directMediaScopeMatchesCursor(const DirectMediaCursor& cursor, const DirectMediaScope& scope);
bool clearDirectMediaCursor(DirectMediaCursor& cursor);
bool requestDirectImageCursor(DirectMediaCursor& cursor, ResolvedNavigationSource source);
DirectMediaConfirmation confirmDirectImageCursor(DirectMediaCursor& cursor, const QUrl& url);
DirectMediaConfirmation confirmDirectVideoCursor(const DirectMediaCursor& cursor, const QUrl& url);
bool restoreDirectImageCursorAfterFailure(DirectMediaCursor& cursor);
bool setDirectVideoCursor(DirectMediaCursor& cursor, ResolvedNavigationSource source);
}

#endif
