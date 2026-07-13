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
    return !cursor.pendingSource.isEmpty() ? cursor.pendingSource : cursor.stableSource;
}

bool sameEffectiveDirectMediaCursorUrl(
    const kiriview::DirectMediaCursor& left, const kiriview::DirectMediaCursor& right)
{
    const std::optional<kiriview::DirectMediaScope> leftScope
        = kiriview::directMediaScopeForCursor(left);
    const std::optional<kiriview::DirectMediaScope> rightScope
        = kiriview::directMediaScopeForCursor(right);
    if (!leftScope.has_value() || !rightScope.has_value()) {
        return !leftScope.has_value() && !rightScope.has_value();
    }
    return kiriview::sameSourceKey(leftScope->currentKey(), rightScope->currentKey())
        && kiriview::sameSourceKey(leftScope->parentKey(), rightScope->parentKey());
}

bool replaceDirectMediaCursor(
    kiriview::DirectMediaCursor& current, kiriview::DirectMediaCursor next)
{
    if (current.stableSource.requestedUrl() == next.stableSource.requestedUrl()
        && current.pendingSource.requestedUrl() == next.pendingSource.requestedUrl()) {
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
    const std::optional<kiriview::DirectMediaScope> scope
        = kiriview::directMediaScopeForCursor(cursor);
    qCDebug(kiriviewNavigationLog)
        << "direct media cursor operation"
        << "operation" << operation << "effectiveUrlChanged" << effectiveUrlChanged << "stableUrl"
        << cursor.stableSource.requestedUrl() << "pendingUrl" << cursor.pendingSource.requestedUrl()
        << "currentUrl" << (scope.has_value() ? scope->currentUrl() : QUrl()) << "parentUrl"
        << (scope.has_value() ? scope->parentUrl() : QUrl()) << "generation" << cursor.generation;
}
}

namespace kiriview {
DirectMediaScope::DirectMediaScope(QUrl currentUrlValue, QUrl parentUrlValue,
    quint64 generationValue, SourceKey currentKeyValue, SourceKey parentKeyValue,
    QUrl navigationUrlValue)
    : m_currentUrl(std::move(currentUrlValue))
    , m_parentUrl(std::move(parentUrlValue))
    , m_generation(generationValue)
    , m_currentKey(std::move(currentKeyValue))
    , m_parentKey(std::move(parentKeyValue))
    , m_navigationUrl(std::move(navigationUrlValue))
{
}

std::optional<DirectMediaScope> DirectMediaScope::fromSource(
    const ResolvedNavigationSource& source, quint64 generation)
{
    if (source.isEmpty()) {
        return std::nullopt;
    }
    const DirectoryNavigationLocation location = directoryNavigationLocationForSource(source);
    const SourceKey currentKey = sourceKeyForUrl(location.fileUrl);
    const SourceKey parentKey = sourceKeyForUrl(location.directoryUrl);
    if (!location.isValid() || !currentKey.valid || !parentKey.valid) {
        return std::nullopt;
    }
    return DirectMediaScope(source.requestedUrl(), location.directoryUrl, generation, currentKey,
        parentKey, location.fileUrl);
}

QUrl effectiveDirectMediaCursorUrl(const DirectMediaCursor& cursor)
{
    return effectiveDirectMediaCursorSource(cursor).requestedUrl();
}

std::optional<DirectMediaScope> directMediaScopeForCursor(const DirectMediaCursor& cursor)
{
    const ResolvedNavigationSource& source = effectiveDirectMediaCursorSource(cursor);
    return DirectMediaScope::fromSource(source, cursor.generation);
}

bool directMediaScopeMatchesCursor(const DirectMediaCursor& cursor, const DirectMediaScope& scope)
{
    const std::optional<DirectMediaScope> currentScope = directMediaScopeForCursor(cursor);
    return currentScope.has_value() && *currentScope == scope;
}

bool clearDirectMediaCursor(DirectMediaCursor& cursor)
{
    DirectMediaCursor next;
    next.generation = cursor.generation;
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("clear", cursor, effectiveUrlChanged);
    return effectiveUrlChanged;
}

bool requestDirectImageCursor(DirectMediaCursor& cursor, ResolvedNavigationSource source)
{
    DirectMediaCursor next = cursor;
    next.pendingSource = std::move(source);
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("request-direct-image", cursor, effectiveUrlChanged);
    return effectiveUrlChanged;
}

DirectMediaConfirmation confirmDirectImageCursor(DirectMediaCursor& cursor, const QUrl& url)
{
    if (cursor.pendingSource.isEmpty()) {
        return cursor.stableSource.isEmpty()
            ? DirectMediaConfirmation::Bypassed
            : (sameNormalizedUrl(cursor.stableSource.requestedUrl(), url)
                      ? DirectMediaConfirmation::Committed
                      : DirectMediaConfirmation::Stale);
    }
    DirectMediaCursor next = cursor;
    if (!sameNormalizedUrl(cursor.pendingSource.requestedUrl(), url)) {
        return DirectMediaConfirmation::Stale;
    }
    next.stableSource = cursor.pendingSource;
    next.pendingSource = {};
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("confirm-direct-image", cursor, effectiveUrlChanged);
    Q_UNUSED(effectiveUrlChanged);
    return DirectMediaConfirmation::Committed;
}

DirectMediaConfirmation confirmDirectVideoCursor(const DirectMediaCursor& cursor, const QUrl& url)
{
    if (cursor.stableSource.isEmpty()) {
        return DirectMediaConfirmation::Bypassed;
    }
    return sameNormalizedUrl(cursor.stableSource.requestedUrl(), url)
        ? DirectMediaConfirmation::Committed
        : DirectMediaConfirmation::Stale;
}

bool restoreDirectImageCursorAfterFailure(DirectMediaCursor& cursor)
{
    DirectMediaCursor next = cursor;
    next.pendingSource = {};
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("restore-direct-image-after-failure", cursor, effectiveUrlChanged);
    return effectiveUrlChanged;
}

bool setDirectVideoCursor(DirectMediaCursor& cursor, ResolvedNavigationSource source)
{
    DirectMediaCursor next = cursor;
    next.stableSource = std::move(source);
    next.pendingSource = {};
    const bool effectiveUrlChanged = replaceDirectMediaCursor(cursor, std::move(next));
    logCursorOperation("set-direct-video", cursor, effectiveUrlChanged);
    return effectiveUrlChanged;
}
}
