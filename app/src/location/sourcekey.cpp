// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "sourcekey.h"

#include "location/imageurl.h"

#include <QDir>

namespace {
bool isRelativeLocalPathUrl(const QUrl& url)
{
    return url.scheme().isEmpty() && url.isRelative() && !url.path().isEmpty();
}

QUrl absoluteLocalFileIdentityUrl(const QUrl& url)
{
    QString localPath = url.isLocalFile() ? url.toLocalFile() : url.path();
    localPath = QDir::cleanPath(localPath);
    if (QDir::isRelativePath(localPath)) {
        localPath = QDir::cleanPath(QDir::current().absoluteFilePath(localPath));
    }

    QUrl normalizedUrl = QUrl::fromLocalFile(localPath);
    normalizedUrl.setQuery(url.query());
    normalizedUrl.setFragment(url.fragment());
    return normalizedUrl;
}

QUrl normalizedSourceIdentityUrl(const QUrl& url)
{
    const QUrl normalizedUrl = url.adjusted(QUrl::NormalizePathSegments);
    if (normalizedUrl.isLocalFile() || isRelativeLocalPathUrl(normalizedUrl)) {
        return absoluteLocalFileIdentityUrl(normalizedUrl);
    }

    return normalizedUrl;
}
}

namespace kiriview {
SourceKey sourceKeyForUrl(const QUrl& url)
{
    if (url.isEmpty() || !url.isValid()) {
        return {};
    }

    const QUrl normalizedUrl = normalizedSourceIdentityUrl(url);
    const QString identity = normalizedUrl.toString(QUrl::FullyEncoded);
    if (normalizedUrl.isEmpty() || !normalizedUrl.isValid() || identity.isEmpty()) {
        return {};
    }

    return SourceKey {
        normalizedUrl,
        identity,
        true,
    };
}

ThumbnailDemandKey thumbnailDemandKey(int rowNumber, const QUrl& url, quint64 navigationGeneration)
{
    return { rowNumber, sourceKeyForUrl(url), navigationGeneration };
}

ThumbnailSourceRevisionKey thumbnailSourceRevisionKey(int rowNumber, const QUrl& url,
    const QString& label, const QString& pageKind, const QString& sourceKind,
    quint64 navigationGeneration)
{
    return { { rowNumber, sourceKeyForUrl(url), label, pageKind, sourceKind }, url,
        navigationGeneration };
}

bool sameSourceKey(const SourceKey& left, const SourceKey& right)
{
    return left.valid && right.valid && left.identity == right.identity;
}

bool isValidThumbnailRowKey(const ThumbnailRowKey& key)
{
    return key.rowNumber > 0 && key.source.valid;
}

bool isValidThumbnailDemandKey(const ThumbnailDemandKey& key)
{
    return key.rowNumber > 0 && key.source.valid && key.navigationGeneration != 0;
}

bool isValidThumbnailSourceRevisionKey(const ThumbnailSourceRevisionKey& key)
{
    return isValidThumbnailRowKey(key.row) && !key.sourceUrl.isEmpty() && key.sourceUrl.isValid()
        && key.navigationGeneration != 0;
}

bool sameThumbnailRowKey(const ThumbnailRowKey& left, const ThumbnailRowKey& right)
{
    return left == right;
}

bool operator==(const ThumbnailRowKey& left, const ThumbnailRowKey& right)
{
    return left.rowNumber == right.rowNumber && left.source.valid == right.source.valid
        && left.source.identity == right.source.identity && left.label == right.label
        && left.pageKind == right.pageKind && left.sourceKind == right.sourceKind;
}

bool operator!=(const ThumbnailRowKey& left, const ThumbnailRowKey& right)
{
    return !(left == right);
}

bool operator==(const ThumbnailDemandKey& left, const ThumbnailDemandKey& right)
{
    return left.rowNumber == right.rowNumber && left.source.valid == right.source.valid
        && left.source.identity == right.source.identity
        && left.navigationGeneration == right.navigationGeneration;
}

bool operator!=(const ThumbnailDemandKey& left, const ThumbnailDemandKey& right)
{
    return !(left == right);
}

bool operator==(const ThumbnailSourceRevisionKey& left, const ThumbnailSourceRevisionKey& right)
{
    return left.row == right.row && left.navigationGeneration == right.navigationGeneration;
}

bool operator!=(const ThumbnailSourceRevisionKey& left, const ThumbnailSourceRevisionKey& right)
{
    return !(left == right);
}

uint qHash(const SourceKey& key, uint seed) { return qHash(key.identity, seed); }

uint qHash(const ThumbnailRowKey& key, uint seed)
{
    return qHashMulti(seed, key.rowNumber, key.source.valid, key.source.identity, key.label,
        key.pageKind, key.sourceKind);
}

uint qHash(const ThumbnailDemandKey& key, uint seed)
{
    return qHashMulti(
        seed, key.rowNumber, key.source.valid, key.source.identity, key.navigationGeneration);
}

uint qHash(const ThumbnailSourceRevisionKey& key, uint seed)
{
    return qHashMulti(seed, key.row, key.navigationGeneration);
}

}
