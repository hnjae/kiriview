// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_THUMBNAILORIGINALIDENTITY_H
#define KIRIVIEW_THUMBNAILORIGINALIDENTITY_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <utility>

namespace kiriview {
enum class ThumbnailOriginalIdentityMode {
    LocalPath,
    NonFileUri,
};

struct ThumbnailOriginalIdentity
{
    ThumbnailOriginalIdentityMode mode = ThumbnailOriginalIdentityMode::LocalPath;
    QByteArray localPathBytes;
    QString uri;
    qint64 mtimeSeconds = 0;
    qint64 originalByteSize = -1;
    QString mimeType;

    static ThumbnailOriginalIdentity fromLocalPathBytes(QByteArray localPathBytes)
    {
        ThumbnailOriginalIdentity identity;
        identity.mode = ThumbnailOriginalIdentityMode::LocalPath;
        identity.localPathBytes = std::move(localPathBytes);
        return identity;
    }

    static ThumbnailOriginalIdentity fromNonFileUri(
        QString uri, qint64 mtimeSeconds, qint64 originalByteSize, QString mimeType = {})
    {
        ThumbnailOriginalIdentity identity;
        identity.mode = ThumbnailOriginalIdentityMode::NonFileUri;
        identity.uri = std::move(uri);
        identity.mtimeSeconds = mtimeSeconds;
        identity.originalByteSize = originalByteSize;
        identity.mimeType = std::move(mimeType);
        return identity;
    }

    [[nodiscard]] bool isNonFileUri() const
    {
        return mode == ThumbnailOriginalIdentityMode::NonFileUri;
    }
    [[nodiscard]] bool isValid() const
    {
        return (mode == ThumbnailOriginalIdentityMode::LocalPath && !localPathBytes.isEmpty())
            || (isNonFileUri() && !uri.isEmpty());
    }
};
}

#endif
