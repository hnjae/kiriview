// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "directmediacursor.h"

#include "navigation/directmedianavigationmodel.h"
#include "navigation/navigationlogging.h"

#include <QDebug>
#include <utility>

namespace {
const kiriview::ResolvedNavigationSource& effectiveDirectMediaCursorSource(
    const kiriview::DirectMediaCursor& cursor)
{
    return !cursor.pendingUrl.isEmpty() ? cursor.pendingSource : cursor.stableSource;
}

bool sameEffectiveDirectMediaCursorUrl(
    const kiriview::DirectMediaCursor& left, const kiriview::DirectMediaCursor& right)
{
    const kiriview::DirectMediaScope leftScope = kiriview::directMediaScopeForCursor(left);
    const kiriview::DirectMediaScope rightScope = kiriview::directMediaScopeForCursor(right);
    return kiriview::sameSourceKey(leftScope.currentKey, rightScope.currentKey)
        && kiriview::sameSourceKey(leftScope.parentKey, rightScope.parentKey);
}

bool replaceDirectMediaCursor(
    kiriview::DirectMediaCursor& current, kiriview::DirectMediaCursor next)
{
    if (current.stableUrl == next.stableUrl && current.pendingUrl == next.pendingUrl) {
        next.generation = current.generation;
        current = std::move(next);
        return false;
    }

    const bool effectiveUrlChanged = !sameEffectiveDirectMediaCursorUrl(current, next);
    next.generation = effectiveUrlChanged ? current.generation + 1 : current.generation;
    current = std::move(next);
    return effectiveUrlChanged;
}

void logCursorOperation(
    const char* operation, const kiriview::DirectMediaCursor& cursor, bool effectiveUrlChanged)
{
    const kiriview::DirectMediaScope scope = kiriview::directMediaScopeForCursor(cursor);
    qCDebug(kiriviewNavigationLog)
        << "direct media cursor operation"
        << "operation" << operation << "effectiveUrlChanged" << effectiveUrlChanged << "stableUrl"
        << cursor.stableUrl << "pendingUrl" << cursor.pendingUrl << "currentUrl" << scope.currentUrl
        << "parentUrl" << scope.parentUrl << "generation" << scope.generation;
}
}

namespace kiriview {
QUrl effectiveDirectMediaCursorUrl(const DirectMediaCursor& cursor)
{
    return !cursor.pendingUrl.isEmpty() ? cursor.pendingUrl : cursor.stableUrl;
}

DirectMediaScope directMediaScopeForCursor(const DirectMediaCursor& cursor)
{
    const QUrl currentUrl = effectiveDirectMediaCursorUrl(cursor);
    const ResolvedNavigationSource& source = effectiveDirectMediaCursorSource(cursor);
    const DirectoryNavigationLocation location = source.isEmpty()
        ? DirectoryNavigationLocation {}
        : directoryNavigationLocationForSource(source);
    return DirectMediaScope {
        currentUrl,
        location.directoryUrl,
        cursor.generation,
        sourceKeyForUrl(location.fileUrl),
        sourceKeyForUrl(location.directoryUrl),
        location.fileUrl,
    };
}

bool directMediaScopeMatchesCursor(const DirectMediaCursor& cursor, const DirectMediaScope& scope)
{
    const DirectMediaScope currentScope = directMediaScopeForCursor(cursor);
    return currentScope == scope;
}

bool clearDirectMediaCursor(DirectMediaCursor& cursor)
{
    DirectMediaCursor next;
    next.generation = cursor.generation;
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("clear", cursor, effectiveUrlChanged);
    return effectiveUrlChanged;
}

bool requestDirectImageCursor(DirectMediaCursor& cursor, const QUrl& url)
{
    return requestDirectImageCursor(cursor, resolveNavigationSource(url));
}

bool requestDirectImageCursor(DirectMediaCursor& cursor, ResolvedNavigationSource source)
{
    DirectMediaCursor next = cursor;
    next.pendingUrl = source.requestedUrl();
    next.pendingSource = std::move(source);
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("request-direct-image", cursor, effectiveUrlChanged);
    return effectiveUrlChanged;
}

bool confirmDirectImageCursor(DirectMediaCursor& cursor, const QUrl& url)
{
    DirectMediaCursor next = cursor;
    next.stableUrl = url;
    next.stableSource = !cursor.pendingSource.isEmpty()
            && sameNormalizedUrl(cursor.pendingSource.requestedUrl(), url)
        ? cursor.pendingSource
        : resolveNavigationSource(url);
    next.pendingUrl = QUrl();
    next.pendingSource = {};
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("confirm-direct-image", cursor, effectiveUrlChanged);
    return effectiveUrlChanged;
}

bool restoreDirectImageCursorAfterFailure(DirectMediaCursor& cursor)
{
    DirectMediaCursor next = cursor;
    next.pendingUrl = QUrl();
    next.pendingSource = {};
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("restore-direct-image-after-failure", cursor, effectiveUrlChanged);
    return effectiveUrlChanged;
}

bool setDirectVideoCursor(DirectMediaCursor& cursor, const QUrl& url)
{
    return setDirectVideoCursor(cursor, resolveNavigationSource(url));
}

bool setDirectVideoCursor(DirectMediaCursor& cursor, ResolvedNavigationSource source)
{
    DirectMediaCursor next = cursor;
    next.stableUrl = source.requestedUrl();
    next.stableSource = std::move(source);
    next.pendingUrl = QUrl();
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("set-direct-video", cursor, effectiveUrlChanged);
    return effectiveUrlChanged;
}
}
