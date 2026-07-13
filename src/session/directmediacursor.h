// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DIRECTMEDIACURSOR_H
#define KIRIVIEW_DIRECTMEDIACURSOR_H

#include "location/imageurl.h"
#include "location/sourcekey.h"

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

    const QUrl& currentUrl() const { return m_currentUrl; }
    const QUrl& parentUrl() const { return m_parentUrl; }
    quint64 generation() const { return m_generation; }
    const SourceKey& currentKey() const { return m_currentKey; }
    const SourceKey& parentKey() const { return m_parentKey; }
    const QUrl& navigationUrl() const { return m_navigationUrl; }

    friend bool operator==(const DirectMediaScope& left, const DirectMediaScope& right)
    {
        return sameSourceKey(left.m_currentKey, right.m_currentKey)
            && sameSourceKey(left.m_parentKey, right.m_parentKey)
            && left.m_generation == right.m_generation;
    }

private:
    DirectMediaScope(QUrl currentUrl, QUrl parentUrl, quint64 generation, SourceKey currentKey,
        SourceKey parentKey, QUrl navigationUrl);

    QUrl m_currentUrl;
    QUrl m_parentUrl;
    quint64 m_generation = 0;
    SourceKey m_currentKey;
    SourceKey m_parentKey;
    QUrl m_navigationUrl;
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
