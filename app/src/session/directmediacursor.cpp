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

bool sameEffectiveDirectMediaCursorIdentity(
    const kiriview::DirectMediaCursor& left, const kiriview::DirectMediaCursor& right)
{
    const std::optional<kiriview::DirectMediaScope> leftScope
        = kiriview::directMediaScopeForCursor(left);
    const std::optional<kiriview::DirectMediaScope> rightScope
        = kiriview::directMediaScopeForCursor(right);
    if (!leftScope.has_value() || !rightScope.has_value()) {
        return !leftScope.has_value() && !rightScope.has_value();
    }
    return leftScope->pageScopeIdentity() == rightScope->pageScopeIdentity();
}

bool replaceDirectMediaCursor(
    kiriview::DirectMediaCursor& current, kiriview::DirectMediaCursor next)
{
    const bool effectiveIdentityChanged = !sameEffectiveDirectMediaCursorIdentity(current, next);
    next.generation = effectiveIdentityChanged ? current.generation + 1 : current.generation;
    current = std::move(next);
    return effectiveIdentityChanged;
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
DirectMediaScope::DirectMediaScope(ResolvedNavigationSource sourceValue, QUrl parentUrlValue,
    DirectMediaPageScopeIdentity pageScopeIdentityValue, quint64 generationValue)
    : m_source(std::move(sourceValue))
    , m_parentUrl(std::move(parentUrlValue))
    , m_pageScopeIdentity(std::move(pageScopeIdentityValue))
    , m_generation(generationValue)
{
}

std::optional<DirectMediaScope> DirectMediaScope::fromSource(
    const ResolvedNavigationSource& source, quint64 generation)
{
    if (source.isEmpty()) {
        return std::nullopt;
    }
    const std::optional<DirectMediaPageScopeIdentity> pageScopeIdentity
        = directMediaPageScopeIdentityForSource(source);
    if (!pageScopeIdentity.has_value()) {
        return std::nullopt;
    }
    return DirectMediaScope(
        source, pageScopeIdentity->parentNavigationUrl(), *pageScopeIdentity, generation);
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
            : (sameSourceKey(
                   sourceKeyForUrl(cursor.stableSource.requestedUrl()), sourceKeyForUrl(url))
                      ? DirectMediaConfirmation::Committed
                      : DirectMediaConfirmation::Stale);
    }
    DirectMediaCursor next = cursor;
    if (!sameSourceKey(
            sourceKeyForUrl(cursor.pendingSource.requestedUrl()), sourceKeyForUrl(url))) {
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
    return sameSourceKey(sourceKeyForUrl(cursor.stableSource.requestedUrl()), sourceKeyForUrl(url))
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
