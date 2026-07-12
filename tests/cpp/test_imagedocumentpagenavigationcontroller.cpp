// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/imagecallback.h"
#include "candidate_test_support.h"
#include "navigation/imagedocumentpagenavigationcontroller.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <cstddef>
#include <functional>
#include <utility>
#include <variant>
#include <vector>

namespace {
using kiriview::ImageDocumentPageCandidate;
using kiriview::ImageDocumentPageCandidateListContext;
using kiriview::ImageDocumentPageCandidateRepository;
using kiriview::ImageDocumentPageNavigationController;
using kiriview::NavigationDirection;
using kiriview::OpenedCollectionScopeKind;
using kiriview::OpenedCollectionScopeLocation;
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::localUrl;

using FakeCandidateProvider = kiriview::TestSupport::FakeImageDocumentPageCandidateProvider;

class DelayedDirectoryImageProvider
{
public:
    kiriview::ImageDocumentPageCandidateProvider provider()
    {
        return kiriview::ImageDocumentPageCandidateProvider {
            [this](QObject*, QUrl directoryUrl,
                kiriview::ImageDocumentPageCandidatesCallback callback, kiriview::ErrorCallback) {
                m_loads.push_back(Load { std::move(directoryUrl), std::move(callback) });
                return kiriview::ImageIoJob();
            },
            [](QObject*, QUrl, kiriview::ContainerCandidatesCallback, kiriview::ErrorCallback) {
                return kiriview::ImageIoJob();
            },
            [this](QObject*, kiriview::OpenedCollectionScopeLocation openedCollectionScope,
                kiriview::ImageDocumentPageCandidatesCallback callback, kiriview::ErrorCallback) {
                m_openedCollectionLoads.push_back(OpenedCollectionLoad {
                    std::move(openedCollectionScope),
                    std::move(callback),
                });
                return kiriview::ImageIoJob();
            },
            [](QObject*, QUrl, kiriview::ImageDocumentPageCandidatesCallback,
                kiriview::ErrorCallback) { return kiriview::ImageIoJob(); },
        };
    }

    std::size_t loadCount() const { return m_loads.size(); }

    const QUrl& loadDirectory(std::size_t index) const { return m_loads.at(index).directoryUrl; }

    std::size_t openedCollectionLoadCount() const { return m_openedCollectionLoads.size(); }

    void finishLoad(std::size_t index, std::vector<ImageDocumentPageCandidate> candidates)
    {
        kiriview::ImageDocumentPageCandidatesCallback callback = m_loads.at(index).callback;
        if (callback) {
            callback(std::move(candidates));
        }
    }

    void finishOpenedCollectionLoad(
        std::size_t index, std::vector<ImageDocumentPageCandidate> candidates)
    {
        kiriview::ImageDocumentPageCandidatesCallback callback
            = m_openedCollectionLoads.at(index).callback;
        if (callback) {
            callback(std::move(candidates));
        }
    }

private:
    struct Load
    {
        QUrl directoryUrl;
        kiriview::ImageDocumentPageCandidatesCallback callback;
    };

    struct OpenedCollectionLoad
    {
        kiriview::OpenedCollectionScopeLocation openedCollectionScope;
        kiriview::ImageDocumentPageCandidatesCallback callback;
    };

    std::vector<Load> m_loads;
    std::vector<OpenedCollectionLoad> m_openedCollectionLoads;
};

ImageDocumentPageNavigationController::Callbacks controllerCallbacks(
    std::function<void(const QUrl&)> openUrl = {}, std::function<void()> pageNavigationChanged = {},
    std::function<void()> clearCurrentImage = {},
    ImageDocumentPageNavigationController::DeletionInProgressCallback deletionInProgress = {})
{
    return ImageDocumentPageNavigationController::Callbacks {
        [openUrl = std::move(openUrl), pageNavigationChanged = std::move(pageNavigationChanged),
            clearCurrentImage = std::move(clearCurrentImage)](
            kiriview::ImageDocumentPageNavigationCommit commit) mutable {
            if (commit.pageNavigationChanged) {
                kiriview::invokeIfSet(pageNavigationChanged);
            }
            for (const kiriview::ImageDocumentPageNavigationEffect& effect : commit.effects) {
                if (const auto* openEffect
                    = std::get_if<kiriview::OpenImageDocumentPageUrlEffect>(&effect)) {
                    kiriview::invokeIfSet(openUrl, openEffect->target.url);
                } else if (std::holds_alternative<
                               kiriview::ClearCurrentImageDocumentPageNavigationEffect>(effect)) {
                    kiriview::invokeIfSet(clearCurrentImage);
                }
            }
        },
        std::move(deletionInProgress),
    };
}

ImageDocumentPageCandidateListContext directoryContext(
    const QUrl& currentUrl, const QUrl& directoryUrl)
{
    return ImageDocumentPageCandidateListContext::forDirectory(currentUrl, directoryUrl);
}

OpenedCollectionScopeLocation archiveCollectionScope()
{
    return OpenedCollectionScopeLocation::fromUrls(localUrl(QStringLiteral("/books/book.cbz")),
        QUrl(QStringLiteral("zip:///books/book.cbz!/")),
        OpenedCollectionScopeKind::ComicBookArchive);
}

ImageDocumentPageCandidateListContext openedCollectionContext(
    const QUrl& currentUrl, const OpenedCollectionScopeLocation& openedCollectionScope)
{
    return ImageDocumentPageCandidateListContext::forOpenedCollectionScope(
        currentUrl, openedCollectionScope);
}
}

class TestImageDocumentPageNavigationController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void updateOwnsRefreshStateAndReusesWatcher();
    void changedCandidatesRecoverRemovedImageAfterStateUpdate();
    void deletionInProgressSuppressesRemovedCurrentImageRecovery();
    void fallbackAdjacentNavigationPublishesTargetBeforeOpening();
    void knownSelectionAdjacentNavigationDoesNotRelistCandidates();
    void canceledFallbackAdjacentNavigationCompletionIsIgnored();
    void staleUpdateCompletionCannotReplaceCurrentRefreshContext();
    void staleSameSourceUpdateCompletionIsRejected();
    void cancelUpdateStopsCandidateChangeWatcher();
};

void TestImageDocumentPageNavigationController::updateOwnsRefreshStateAndReusesWatcher()
{
    FakeCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/images/"));
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/images/02.png"));
    fakeProvider.setDirectoryImages(directoryUrl,
        {
            imageDocumentPageCandidate(firstUrl),
            imageDocumentPageCandidate(secondUrl),
        });

    ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    int changeCount = 0;
    ImageDocumentPageNavigationController controller(
        nullptr, repository, controllerCallbacks({}, [&changeCount]() { ++changeCount; }));

    controller.update(directoryContext(firstUrl, directoryUrl));
    QCOMPARE(controller.currentPageNumber(), 1);
    QCOMPARE(controller.pageCount(), 2);
    QCOMPARE(changeCount, 1);
    QCOMPARE(fakeProvider.directoryImageChangeSubscriptionCount(directoryUrl), 1);

    controller.update(directoryContext(secondUrl, directoryUrl));
    QCOMPARE(controller.currentPageNumber(), 2);
    QCOMPARE(controller.pageCount(), 2);
    QCOMPARE(changeCount, 2);
    QCOMPARE(fakeProvider.directoryImageChangeSubscriptionCount(directoryUrl), 1);
}

void TestImageDocumentPageNavigationController::
    changedCandidatesRecoverRemovedImageAfterStateUpdate()
{
    FakeCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/images/"));
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/images/02.png"));
    const QUrl thirdUrl = localUrl(QStringLiteral("/images/03.png"));
    fakeProvider.setDirectoryImages(directoryUrl,
        {
            imageDocumentPageCandidate(firstUrl),
            imageDocumentPageCandidate(secondUrl),
            imageDocumentPageCandidate(thirdUrl),
        });

    ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    QUrl openedUrl;
    int clearCount = 0;
    ImageDocumentPageNavigationController controller(nullptr, repository,
        controllerCallbacks([&openedUrl](const QUrl& url) { openedUrl = url; }, {},
            [&clearCount]() { ++clearCount; }));
    controller.update(directoryContext(secondUrl, directoryUrl));

    fakeProvider.emitDirectoryImageChanges(directoryUrl,
        {
            imageDocumentPageCandidate(firstUrl),
            imageDocumentPageCandidate(thirdUrl),
        });

    QCOMPARE(controller.currentPageNumber(), 0);
    QCOMPARE(controller.pageCount(), 2);
    QCOMPARE(clearCount, 1);
    QCOMPARE(openedUrl, thirdUrl);
}

void TestImageDocumentPageNavigationController::
    deletionInProgressSuppressesRemovedCurrentImageRecovery()
{
    FakeCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/images/"));
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/images/02.png"));
    const QUrl thirdUrl = localUrl(QStringLiteral("/images/03.png"));
    fakeProvider.setDirectoryImages(directoryUrl,
        {
            imageDocumentPageCandidate(firstUrl),
            imageDocumentPageCandidate(secondUrl),
            imageDocumentPageCandidate(thirdUrl),
        });

    ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    QUrl openedUrl;
    int clearCount = 0;
    bool deletionInProgress = true;
    ImageDocumentPageNavigationController controller(nullptr, repository,
        controllerCallbacks([&openedUrl](const QUrl& url) { openedUrl = url; }, {},
            [&clearCount]() { ++clearCount; },
            [&deletionInProgress]() { return deletionInProgress; }));
    controller.update(directoryContext(secondUrl, directoryUrl));

    fakeProvider.emitDirectoryImageChanges(directoryUrl,
        {
            imageDocumentPageCandidate(firstUrl),
            imageDocumentPageCandidate(thirdUrl),
        });

    QCOMPARE(controller.currentPageNumber(), 0);
    QCOMPARE(controller.pageCount(), 2);
    QCOMPARE(clearCount, 0);
    QVERIFY(openedUrl.isEmpty());
}

void TestImageDocumentPageNavigationController::
    fallbackAdjacentNavigationPublishesTargetBeforeOpening()
{
    FakeCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/images/"));
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/images/02.png"));
    const QUrl thirdUrl = localUrl(QStringLiteral("/images/03.png"));
    fakeProvider.setDirectoryImages(directoryUrl,
        {
            imageDocumentPageCandidate(firstUrl),
            imageDocumentPageCandidate(secondUrl),
            imageDocumentPageCandidate(thirdUrl),
        });

    ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    QUrl openedUrl;
    int currentPageAtOpen = 0;
    int pageCountAtOpen = 0;
    ImageDocumentPageNavigationController* controllerPtr = nullptr;
    ImageDocumentPageNavigationController controller(
        nullptr, repository, controllerCallbacks([&](const QUrl& url) {
            openedUrl = url;
            currentPageAtOpen = controllerPtr->currentPageNumber();
            pageCountAtOpen = controllerPtr->pageCount();
        }));
    controllerPtr = &controller;

    controller.openAdjacentPage(
        directoryContext(firstUrl, directoryUrl), NavigationDirection::Next);

    QCOMPARE(openedUrl, secondUrl);
    QCOMPARE(currentPageAtOpen, 2);
    QCOMPARE(pageCountAtOpen, 3);
    QCOMPARE(controller.currentPageNumber(), 2);
    QCOMPARE(controller.pageCount(), 3);
}

void TestImageDocumentPageNavigationController::
    knownSelectionAdjacentNavigationDoesNotRelistCandidates()
{
    DelayedDirectoryImageProvider delayedProvider;
    ImageDocumentPageCandidateRepository repository(delayedProvider.provider());
    const OpenedCollectionScopeLocation archiveCollection = archiveCollectionScope();
    const QUrl firstUrl = archivePageUrl(archiveCollection.rootUrl(), QStringLiteral("01.png"));
    const QUrl secondUrl = archivePageUrl(archiveCollection.rootUrl(), QStringLiteral("02.png"));
    QUrl openedUrl;
    int changeCount = 0;
    ImageDocumentPageNavigationController controller(nullptr, repository,
        controllerCallbacks([&openedUrl](const QUrl& url) { openedUrl = url; },
            [&changeCount]() { ++changeCount; }));

    controller.update(openedCollectionContext(firstUrl, archiveCollection));
    QCOMPARE(delayedProvider.openedCollectionLoadCount(), std::size_t(1));
    delayedProvider.finishOpenedCollectionLoad(0,
        {
            imageDocumentPageCandidate(firstUrl),
            imageDocumentPageCandidate(secondUrl),
        });
    QCOMPARE(controller.currentPageNumber(), 1);
    QCOMPARE(controller.pageCount(), 2);
    QCOMPARE(changeCount, 1);

    controller.openAdjacentPage(
        openedCollectionContext(firstUrl, archiveCollection), NavigationDirection::Next);

    QCOMPARE(openedUrl, secondUrl);
    QCOMPARE(controller.currentPageNumber(), 2);
    QCOMPARE(controller.pageCount(), 2);
    QCOMPARE(delayedProvider.openedCollectionLoadCount(), std::size_t(1));
    QCOMPARE(changeCount, 2);
    QVERIFY(controller.candidateSnapshot().has_value());

    controller.update(openedCollectionContext(secondUrl, archiveCollection));

    QCOMPARE(controller.currentPageNumber(), 2);
    QCOMPARE(controller.pageCount(), 2);
    QCOMPARE(delayedProvider.openedCollectionLoadCount(), std::size_t(1));
    QCOMPARE(changeCount, 3);
}

void TestImageDocumentPageNavigationController::
    canceledFallbackAdjacentNavigationCompletionIsIgnored()
{
    DelayedDirectoryImageProvider delayedProvider;
    ImageDocumentPageCandidateRepository repository(delayedProvider.provider());
    const QUrl directoryUrl = localUrl(QStringLiteral("/images/"));
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/images/02.png"));
    QUrl openedUrl;
    int changeCount = 0;
    ImageDocumentPageNavigationController controller(nullptr, repository,
        controllerCallbacks([&openedUrl](const QUrl& url) { openedUrl = url; },
            [&changeCount]() { ++changeCount; }));

    controller.openAdjacentPage(
        directoryContext(firstUrl, directoryUrl), NavigationDirection::Next);
    QCOMPARE(delayedProvider.loadCount(), std::size_t(1));

    controller.cancelNavigation();
    delayedProvider.finishLoad(0,
        {
            imageDocumentPageCandidate(firstUrl),
            imageDocumentPageCandidate(secondUrl),
        });

    QVERIFY(openedUrl.isEmpty());
    QCOMPARE(controller.currentPageNumber(), 0);
    QCOMPARE(controller.pageCount(), 0);
    QCOMPARE(changeCount, 0);
}

void TestImageDocumentPageNavigationController::
    staleUpdateCompletionCannotReplaceCurrentRefreshContext()
{
    DelayedDirectoryImageProvider delayedProvider;
    ImageDocumentPageCandidateRepository repository(delayedProvider.provider());
    int changeCount = 0;
    ImageDocumentPageNavigationController controller(
        nullptr, repository, controllerCallbacks({}, [&changeCount]() { ++changeCount; }));
    const QUrl firstDirectoryUrl = localUrl(QStringLiteral("/images/"));
    const QUrl secondDirectoryUrl = localUrl(QStringLiteral("/other/"));
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/other/02.png"));

    controller.update(directoryContext(firstUrl, firstDirectoryUrl));
    controller.update(directoryContext(secondUrl, secondDirectoryUrl));
    QCOMPARE(delayedProvider.loadCount(), std::size_t(2));
    QCOMPARE(delayedProvider.loadDirectory(0), firstDirectoryUrl);
    QCOMPARE(delayedProvider.loadDirectory(1), secondDirectoryUrl);

    delayedProvider.finishLoad(0, { imageDocumentPageCandidate(firstUrl) });
    QCOMPARE(controller.currentPageNumber(), 0);
    QCOMPARE(controller.pageCount(), 0);
    QCOMPARE(changeCount, 0);

    delayedProvider.finishLoad(1, { imageDocumentPageCandidate(secondUrl) });
    QCOMPARE(controller.currentPageNumber(), 1);
    QCOMPARE(controller.pageCount(), 1);
    QCOMPARE(changeCount, 1);
}

void TestImageDocumentPageNavigationController::staleSameSourceUpdateCompletionIsRejected()
{
    DelayedDirectoryImageProvider delayedProvider;
    ImageDocumentPageCandidateRepository repository(delayedProvider.provider());
    int changeCount = 0;
    ImageDocumentPageNavigationController controller(
        nullptr, repository, controllerCallbacks({}, [&changeCount]() { ++changeCount; }));
    const QUrl directoryUrl = localUrl(QStringLiteral("/images/"));
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/images/02.png"));

    controller.update(directoryContext(firstUrl, directoryUrl));
    controller.update(directoryContext(secondUrl, directoryUrl));
    QCOMPARE(delayedProvider.loadCount(), std::size_t(2));
    QCOMPARE(delayedProvider.loadDirectory(0), directoryUrl);
    QCOMPARE(delayedProvider.loadDirectory(1), directoryUrl);

    delayedProvider.finishLoad(0, { imageDocumentPageCandidate(firstUrl) });
    QCOMPARE(controller.currentPageNumber(), 0);
    QCOMPARE(controller.pageCount(), 0);
    QCOMPARE(changeCount, 0);

    delayedProvider.finishLoad(
        1, { imageDocumentPageCandidate(firstUrl), imageDocumentPageCandidate(secondUrl) });
    QCOMPARE(controller.currentPageNumber(), 2);
    QCOMPARE(controller.pageCount(), 2);
    QCOMPARE(changeCount, 1);
}

void TestImageDocumentPageNavigationController::cancelUpdateStopsCandidateChangeWatcher()
{
    FakeCandidateProvider fakeProvider;
    const QUrl directoryUrl = localUrl(QStringLiteral("/images/"));
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/images/02.png"));
    fakeProvider.setDirectoryImages(directoryUrl,
        {
            imageDocumentPageCandidate(firstUrl),
            imageDocumentPageCandidate(secondUrl),
        });

    ImageDocumentPageCandidateRepository repository(fakeProvider.provider());
    ImageDocumentPageNavigationController controller(nullptr, repository, controllerCallbacks());
    controller.update(directoryContext(firstUrl, directoryUrl));
    QCOMPARE(fakeProvider.directoryImageChangeSubscriptionCount(directoryUrl), 1);

    controller.cancelUpdate();
    QCOMPARE(fakeProvider.directoryImageChangeSubscriptionCount(directoryUrl), 0);

    fakeProvider.emitDirectoryImageChanges(directoryUrl, { imageDocumentPageCandidate(secondUrl) });
    QCOMPARE(controller.currentPageNumber(), 1);
    QCOMPARE(controller.pageCount(), 2);
}

QTEST_GUILESS_MAIN(TestImageDocumentPageNavigationController)

#include "test_imagedocumentpagenavigationcontroller.moc"
