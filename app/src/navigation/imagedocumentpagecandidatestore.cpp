// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidatestore.h"

#include "async/imagecallback.h"
#include "imagedocumentpagecandidatedirectoryentry.h"
#include "imagedocumentpagecandidateloading.h"
#include "location/imageurl.h"

#include <QMetaObject>
#include <QPointer>
#include <functional>
#include <memory>
#include <utility>

namespace {
bool isLiveLocalDirectoryUrl(const QUrl& url)
{
    return url.isLocalFile() && url.isValid() && !url.isEmpty();
}
}

namespace kiriview {
ImageDocumentPageCandidateStore::ImageDocumentPageCandidateStore(QObject* parent)
    : ImageDocumentPageCandidateStore(defaultImageDocumentPageCandidateWatchProvider(), parent)
{
}

ImageDocumentPageCandidateStore::ImageDocumentPageCandidateStore(
    ImageDocumentPageCandidateWatchProvider watchProvider, QObject* parent)
    : QObject(parent)
    , m_watchProvider(std::move(watchProvider))
{
    if (!m_watchProvider) {
        m_watchProvider = defaultImageDocumentPageCandidateWatchProvider();
    }
}

ImageDocumentPageCandidateStore::~ImageDocumentPageCandidateStore() = default;

ImageIoJob ImageDocumentPageCandidateStore::loadDirectoryImages(QObject* receiver,
    QUrl directoryUrl, ImageDocumentPageCandidatesCallback callback, ErrorCallback errorCallback)
{
    if (!isLiveLocalDirectoryUrl(directoryUrl)) {
        return startDirectoryImageDocumentPageCandidateList(
            receiver, directoryUrl, std::move(callback), std::move(errorCallback));
    }

    directoryUrl = normalizedDirectoryUrlForIdentity(directoryUrl);
    const QString key = directoryUrlIdentityKey(directoryUrl);
    ImageDocumentPageCandidateDirectoryEntry& entry = entryForLocalDirectory(directoryUrl);
    if (entry.failed()) {
        invokeIfSet(errorCallback, entry.errorString());
        return ImageIoJob();
    }
    if (entry.listed()) {
        invokeIfSet(callback, entry.candidates());
        return ImageIoJob();
    }

    QPointer<ImageDocumentPageCandidateStore> store(this);
    return entry.addPendingLoad(
        std::move(callback), std::move(errorCallback), receiver, [store, key](QObject* token) {
            if (!store.isNull()) {
                store->removePendingLoad(key, token);
            }
        });
}

ImageIoJob ImageDocumentPageCandidateStore::watchDirectoryImages(QObject* receiver,
    QUrl directoryUrl, ImageDocumentPageCandidatesCallback callback, ErrorCallback errorCallback)
{
    if (!isLiveLocalDirectoryUrl(directoryUrl)) {
        return ImageIoJob();
    }

    directoryUrl = normalizedDirectoryUrlForIdentity(directoryUrl);
    const QString key = directoryUrlIdentityKey(directoryUrl);
    ImageDocumentPageCandidateDirectoryEntry& entry = entryForLocalDirectory(directoryUrl);
    if (entry.failed()) {
        invokeIfSet(errorCallback, entry.errorString());
        return ImageIoJob();
    }

    QPointer<ImageDocumentPageCandidateStore> store(this);
    return entry.addSubscriber(
        std::move(callback), std::move(errorCallback), receiver, [store, key](QObject* token) {
            if (!store.isNull()) {
                store->removeSubscriber(key, token);
            }
        });
}

ImageDocumentPageCandidateDirectoryEntry& ImageDocumentPageCandidateStore::entryForLocalDirectory(
    const QUrl& directoryUrl)
{
    const QString key = directoryUrlIdentityKey(directoryUrl);
    auto entry = m_entries.find(key);
    if (entry != m_entries.end()) {
        return *entry->second;
    }

    const std::uint64_t identity = ++m_nextEntryIdentity;
    QPointer<ImageDocumentPageCandidateStore> store(this);
    auto insertedEntry = std::make_unique<ImageDocumentPageCandidateDirectoryEntry>(
        directoryUrl, m_watchProvider, this, identity, [store, key, identity]() {
            if (!store.isNull()) {
                store->scheduleIdleErase(key, identity);
            }
        });
    ImageDocumentPageCandidateDirectoryEntry& entryRef = *insertedEntry;
    m_entries.emplace(key, std::move(insertedEntry));

    if (!entryRef.open()) {
        entryRef.handleError(QString());
    }

    return entryRef;
}

void ImageDocumentPageCandidateStore::removePendingLoad(const QString& key, QObject* token)
{
    auto entry = m_entries.find(key);
    if (entry == m_entries.end()) {
        return;
    }

    entry->second->removePendingLoad(token);
}

void ImageDocumentPageCandidateStore::removeSubscriber(const QString& key, QObject* token)
{
    auto entry = m_entries.find(key);
    if (entry == m_entries.end()) {
        return;
    }

    entry->second->removeSubscriber(token);
}

void ImageDocumentPageCandidateStore::scheduleIdleErase(const QString& key, std::uint64_t identity)
{
    QMetaObject::invokeMethod(
        this,
        [this, key, identity]() {
            auto entry = m_entries.find(key);
            if (entry == m_entries.end() || entry->second->identity() != identity
                || entry->second->hasActiveClients()) {
                return;
            }
            m_entries.erase(entry);
        },
        Qt::QueuedConnection);
}
}
