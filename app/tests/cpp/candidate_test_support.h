// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_TESTS_CANDIDATE_TEST_SUPPORT_H
#define KIRIVIEW_TESTS_CANDIDATE_TEST_SUPPORT_H

#include "async/imageiojob.h"
#include "navigation/imagedocumentpagecandidateprovider.h"
#include "navigation/imagedocumentpagenavigationtypes.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace kiriview::TestSupport {
inline QString keyForUrl(const QUrl& url)
{
    return url.adjusted(QUrl::NormalizePathSegments).toString();
}

inline QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

inline QString indexedImageFileName(int index)
{
    return QStringLiteral("%1.png").arg(index, 2, 10, QLatin1Char('0'));
}

inline QUrl indexedImageUrl(int index)
{
    return localUrl(QStringLiteral("/images/") + indexedImageFileName(index));
}

inline QUrl imagesDirectoryUrl() { return localUrl(QStringLiteral("/images/")); }

inline QUrl archivePageUrl(const QUrl& archiveRootUrl, const QString& pageName)
{
    QUrl pageUrl = archiveRootUrl;
    pageUrl.setPath(archiveRootUrl.path() + pageName);
    return pageUrl;
}

inline ImageDocumentPageCandidate imageDocumentPageCandidate(const QUrl& url)
{
    return ImageDocumentPageCandidate { url, url.fileName() };
}

inline ImageDocumentPageCandidate videoCandidate(const QUrl& url)
{
    return ImageDocumentPageCandidate {
        url,
        url.fileName(),
        ImageDocumentPageKind::Video,
    };
}

inline ContainerNavigationCandidate containerCandidate(
    const QUrl& url, ContainerNavigationCandidateType type)
{
    return ContainerNavigationCandidate { url, url.fileName(), type };
}

inline ContainerNavigationCandidate comicBookContainerCandidate(const QUrl& url)
{
    return containerCandidate(url, ContainerNavigationCandidateType::ComicBookArchive);
}

template <typename Candidates, typename Error = QString> class FakeCandidateListing
{
public:
    void setItems(const QUrl& url, Candidates candidates)
    {
        m_itemsByUrl[keyForUrl(url)] = std::move(candidates);
    }

    void setError(const QUrl& url, Error error)
    {
        m_errorsByUrl[keyForUrl(url)] = std::move(error);
    }

    template <typename Callback, typename ErrorCallback>
    void load(QUrl url, Callback callback, ErrorCallback errorCallback) const
    {
        const QString key = keyForUrl(url);
        const auto error = m_errorsByUrl.find(key);
        if (error != m_errorsByUrl.cend()) {
            if constexpr (requires { static_cast<bool>(errorCallback); }) {
                if (!errorCallback) {
                    return;
                }
            }
            errorCallback(error->second);
            return;
        }

        const auto items = m_itemsByUrl.find(key);
        if (items == m_itemsByUrl.cend()) {
            if constexpr (requires { static_cast<bool>(errorCallback); }) {
                if (!errorCallback) {
                    return;
                }
            }
            if constexpr (std::is_same_v<Error, QString>) {
                errorCallback(
                    QStringLiteral("missing fake candidate listing for %1").arg(url.toString()));
            } else if constexpr (std::is_same_v<Error, KioOperationFailure>) {
                errorCallback(kioOperationValidationFailure(KioOperationKind::DirectoryListing, url,
                    QStringLiteral("missing fake candidate listing")));
            } else {
                errorCallback(
                    Error { kioOperationValidationFailure(KioOperationKind::DirectoryListing, url,
                        QStringLiteral("missing fake candidate listing")) });
            }
            return;
        }

        if (callback) {
            callback(items->second);
        }
    }

private:
    std::map<QString, Candidates> m_itemsByUrl;
    std::map<QString, Error> m_errorsByUrl;
};

class FakeImageDocumentPageCandidateProvider
{
    struct FakeCandidateChangeSubscription;

public:
    void setDirectoryImages(
        const QUrl& directoryUrl, std::vector<ImageDocumentPageCandidate> candidates)
    {
        m_directoryImageDocumentPages.setItems(directoryUrl, std::move(candidates));
    }

    void setOpenedCollectionCandidates(
        const QUrl& archiveRootUrl, std::vector<ImageDocumentPageCandidate> candidates)
    {
        m_openedCollectionCandidates.setItems(archiveRootUrl, std::move(candidates));
    }

    void setContainerCandidates(
        const QUrl& directoryUrl, std::vector<ContainerNavigationCandidate> candidates)
    {
        m_containerCandidates.setItems(directoryUrl, std::move(candidates));
    }

    void setDirectoryImageError(const QUrl& directoryUrl, QString errorString)
    {
        setDirectoryImageFailure(directoryUrl,
            KioOperationFailure {
                KioOperationKind::DirectoryListing,
                directoryUrl,
                std::nullopt,
                false,
                errorString,
                std::move(errorString),
                false,
            });
    }

    void setDirectoryImageFailure(const QUrl& directoryUrl, KioOperationFailure failure)
    {
        m_directoryImageDocumentPages.setError(
            directoryUrl, ImageDocumentPageCandidateLoadError { std::move(failure) });
    }

    void setOpenedCollectionCandidateError(const QUrl& archiveRootUrl, QString errorString)
    {
        m_openedCollectionCandidates.setError(archiveRootUrl, std::move(errorString));
    }

    int openedCollectionCandidateLoadCount(const QUrl& archiveRootUrl) const
    {
        const auto loadCount
            = m_openedCollectionCandidateLoadCounts.find(keyForUrl(archiveRootUrl));
        if (loadCount == m_openedCollectionCandidateLoadCounts.cend()) {
            return 0;
        }

        return loadCount->second;
    }

    void setContainerError(const QUrl& directoryUrl, QString errorString)
    {
        setContainerFailure(directoryUrl,
            KioOperationFailure {
                KioOperationKind::DirectoryListing,
                directoryUrl,
                std::nullopt,
                false,
                errorString,
                std::move(errorString),
                false,
            });
    }

    void setContainerFailure(const QUrl& directoryUrl, KioOperationFailure failure)
    {
        m_containerCandidates.setError(directoryUrl, std::move(failure));
    }

    void emitDirectoryImageChanges(
        const QUrl& directoryUrl, std::vector<ImageDocumentPageCandidate> candidates)
    {
        const QString key = keyForUrl(directoryUrl);
        for (const std::shared_ptr<FakeCandidateChangeSubscription>& subscription :
            m_directoryImageChangeSubscriptions) {
            if (subscription == nullptr || subscription->canceled || subscription->key != key
                || !subscription->callback) {
                continue;
            }

            subscription->callback(candidates);
        }
    }

    int directoryImageChangeSubscriptionCount(const QUrl& directoryUrl) const
    {
        const QString key = keyForUrl(directoryUrl);
        return static_cast<int>(std::count_if(m_directoryImageChangeSubscriptions.cbegin(),
            m_directoryImageChangeSubscriptions.cend(),
            [&key](const std::shared_ptr<FakeCandidateChangeSubscription>& subscription) {
                return subscription != nullptr && !subscription->canceled
                    && subscription->key == key;
            }));
    }

    int directoryImageChangeSubscriptionCount() const
    {
        return static_cast<int>(std::count_if(m_directoryImageChangeSubscriptions.cbegin(),
            m_directoryImageChangeSubscriptions.cend(),
            [](const std::shared_ptr<FakeCandidateChangeSubscription>& subscription) {
                return subscription != nullptr && !subscription->canceled;
            }));
    }

    ImageDocumentPageCandidateProvider provider()
    {
        return ImageDocumentPageCandidateProvider {
            [this](QObject*, QUrl directoryUrl, ImageDocumentPageCandidatesCallback callback,
                ImageDocumentPageCandidateLoadErrorCallback errorCallback) {
                m_directoryImageDocumentPages.load(
                    std::move(directoryUrl), std::move(callback), std::move(errorCallback));
                return ImageIoJob();
            },
            [this](QObject*, QUrl directoryUrl, ContainerCandidatesCallback callback,
                KioOperationFailureCallback errorCallback) {
                m_containerCandidates.load(
                    std::move(directoryUrl), std::move(callback), std::move(errorCallback));
                return ImageIoJob();
            },
            [this](QObject*, OpenedCollectionScopeLocation openedCollectionScope,
                ImageDocumentPageCandidatesCallback callback,
                MediaEntrySourceErrorCallback errorCallback) {
                ++m_openedCollectionCandidateLoadCounts[keyForUrl(openedCollectionScope.rootUrl())];
                const QUrl collectionUrl = openedCollectionScope.fileUrl();
                m_openedCollectionCandidates.load(openedCollectionScope.rootUrl(),
                    std::move(callback),
                    [errorCallback = std::move(errorCallback), collectionUrl](
                        QString error) mutable {
                        if (errorCallback) {
                            errorCallback(MediaEntrySourceError {
                                MediaEntrySourceErrorCause::CandidateListingFailed,
                                MediaEntrySourceBackendKind::Unknown,
                                MediaEntrySourceOperation::ListCandidates,
                                collectionUrl,
                                {},
                                std::move(error),
                            });
                        }
                    });
                return ImageIoJob();
            },
            [this](QObject* receiver, QUrl directoryUrl,
                ImageDocumentPageCandidatesCallback callback,
                ImageDocumentPageCandidateLoadErrorCallback) {
                return subscribeToDirectoryImageChanges(
                    receiver, std::move(directoryUrl), std::move(callback));
            },
        };
    }

private:
    struct FakeCandidateChangeSubscription
    {
        QObject* object = nullptr;
        QString key;
        ImageDocumentPageCandidatesCallback callback;
        bool canceled = false;
    };

    ImageIoJob subscribeToDirectoryImageChanges(
        QObject* receiver, QUrl directoryUrl, ImageDocumentPageCandidatesCallback callback)
    {
        auto subscription = std::make_shared<FakeCandidateChangeSubscription>();
        subscription->object = new QObject(receiver);
        subscription->key = keyForUrl(directoryUrl);
        subscription->callback = std::move(callback);

        std::weak_ptr<FakeCandidateChangeSubscription> weakSubscription = subscription;
        ImageIoJob job(subscription->object, [weakSubscription](QObject* object) {
            if (std::shared_ptr<FakeCandidateChangeSubscription> subscription
                = weakSubscription.lock()) {
                subscription->canceled = true;
                subscription->object = nullptr;
            }
            if (object != nullptr) {
                object->deleteLater();
            }
        });
        m_directoryImageChangeSubscriptions.push_back(std::move(subscription));
        return job;
    }

    FakeCandidateListing<std::vector<ImageDocumentPageCandidate>,
        ImageDocumentPageCandidateLoadError>
        m_directoryImageDocumentPages;
    FakeCandidateListing<std::vector<ImageDocumentPageCandidate>> m_openedCollectionCandidates;
    FakeCandidateListing<std::vector<ContainerNavigationCandidate>, KioOperationFailure>
        m_containerCandidates;
    std::map<QString, int> m_openedCollectionCandidateLoadCounts;
    std::vector<std::shared_ptr<FakeCandidateChangeSubscription>>
        m_directoryImageChangeSubscriptions;
};
}

#endif
