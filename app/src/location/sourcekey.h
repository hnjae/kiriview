// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_SOURCEKEY_H
#define KIRIVIEW_SOURCEKEY_H

#include <QHash>
#include <QString>
#include <QUrl>
#include <QtGlobal>

namespace kiriview {
struct SourceKey
{
    QUrl normalizedUrl;
    QString identity;
    bool valid = false;
};

struct ThumbnailRowKey
{
    int rowNumber = 0;
    SourceKey source;
    QString label;
    QString pageKind;
    QString sourceKind;
};

struct ThumbnailDemandKey
{
    int rowNumber = 0;
    SourceKey source;
    quint64 navigationGeneration = 0;
};

struct ThumbnailSourceRevisionKey
{
    ThumbnailRowKey row;
    QUrl sourceUrl;
    quint64 navigationGeneration = 0;
};

SourceKey sourceKeyForUrl(const QUrl& url);
ThumbnailDemandKey thumbnailDemandKey(int rowNumber, const QUrl& url, quint64 navigationGeneration);
ThumbnailSourceRevisionKey thumbnailSourceRevisionKey(int rowNumber, const QUrl& url,
    const QString& label, const QString& pageKind, const QString& sourceKind,
    quint64 navigationGeneration);
bool sameSourceKey(const SourceKey& left, const SourceKey& right);
bool isValidThumbnailRowKey(const ThumbnailRowKey& key);
bool isValidThumbnailDemandKey(const ThumbnailDemandKey& key);
bool isValidThumbnailSourceRevisionKey(const ThumbnailSourceRevisionKey& key);
bool sameThumbnailRowKey(const ThumbnailRowKey& left, const ThumbnailRowKey& right);
bool operator==(const ThumbnailRowKey& left, const ThumbnailRowKey& right);
bool operator!=(const ThumbnailRowKey& left, const ThumbnailRowKey& right);
bool operator==(const ThumbnailDemandKey& left, const ThumbnailDemandKey& right);
bool operator!=(const ThumbnailDemandKey& left, const ThumbnailDemandKey& right);
bool operator==(const ThumbnailSourceRevisionKey& left, const ThumbnailSourceRevisionKey& right);
bool operator!=(const ThumbnailSourceRevisionKey& left, const ThumbnailSourceRevisionKey& right);
uint qHash(const SourceKey& key, uint seed = 0);
uint qHash(const ThumbnailRowKey& key, uint seed = 0);
uint qHash(const ThumbnailDemandKey& key, uint seed = 0);
uint qHash(const ThumbnailSourceRevisionKey& key, uint seed = 0);
}

#endif
