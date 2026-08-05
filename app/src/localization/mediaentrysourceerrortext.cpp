// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "localization/mediaentrysourceerrortext.h"

#include "location/imageurl.h"

#include <KLocalizedString>

namespace {
QString collectionOpenError(const QUrl& collectionUrl)
{
    const QString fileName = kiriview::userVisibleFileNameForUrl(collectionUrl);
    if (fileName.isEmpty()) {
        return i18nc("@info:status", "Could not open the selected collection.");
    }

    return ki18nc("@info:status", "Could not open %1.").subs(fileName).toString();
}
}

namespace kiriview {
QString mediaEntrySourceErrorText(const MediaEntrySourceError& error)
{
    switch (error.cause) {
    case MediaEntrySourceErrorCause::CollectionOpenFailed:
    case MediaEntrySourceErrorCause::UnsupportedCollection:
    case MediaEntrySourceErrorCause::CandidateListingFailed:
    case MediaEntrySourceErrorCause::ProviderUnavailable:
    case MediaEntrySourceErrorCause::OperationCancelled:
        return collectionOpenError(error.collectionUrl);
    case MediaEntrySourceErrorCause::EntryNotFound:
        if (error.operation == MediaEntrySourceOperation::OpenVideoPlaybackDevice) {
            return i18nc("@info:status", "Could not find the selected video in the collection.");
        }
        return i18nc("@info:status", "Could not find the selected image in the collection.");
    case MediaEntrySourceErrorCause::EntryReadFailed:
    case MediaEntrySourceErrorCause::ResourceLimitExceeded:
        if (error.operation == MediaEntrySourceOperation::ListCandidates) {
            return i18nc("@info:status", "The selected collection is too large to open.");
        }
        return i18nc("@info:status", "Could not read the selected collection image.");
    case MediaEntrySourceErrorCause::VideoPlaybackUnsupported:
        return i18nc(
            "@info:status", "KiriView can’t play this video from the selected collection.");
    case MediaEntrySourceErrorCause::ThumbnailMetadataUnsupported:
        return i18nc(
            "@info:status", "Could not cache a preview thumbnail for this collection item.");
    }

    return collectionOpenError(error.collectionUrl);
}
}
