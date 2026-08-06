// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imagedocumentdeletionfallbackcontroller.h"

#include "candidate_test_support.h"
#include "image_async_test_support.h"
#include "location/imagedocumentlocation.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::comicBookContainerCandidate;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::localUrl;

struct ManualImageDocumentPageCandidateLoad
{
    QObject* object = nullptr;
    QUrl url;
    kiriview::ImageDocumentPageCandidatesCallback callback;
    kiriview::ErrorCallback errorCallback;
    kiriview::ImageIoJobCompletion completion;
    bool canceled = false;
};

class ManualDeletionFallbackCandidateProvider
{
public:
    kiriview::ImageDocumentPageCandidateProvider provider()
    {
        kiriview::ImageDocumentPageCandidateProvider provider;
        provider.directoryImageDocumentPages
            = [this](QObject* receiver, QUrl directoryUrl,
                  kiriview::ImageDocumentPageCandidatesCallback callback,
                  kiriview::ErrorCallback errorCallback) {
                  auto load = std::make_shared<ManualImageDocumentPageCandidateLoad>();
                  load->url = std::move(directoryUrl);
                  load->callback = std::move(callback);
                  load->errorCallback = std::move(errorCallback);

                  kiriview::ImageIoJob job
                      = kiriview::TestSupport::Detail::startManualIoJob(receiver, load);
                  m_imageLoads.push_back(load);
                  if (m_synchronousFirstCandidates.has_value() && m_imageLoads.size() == 1) {
                      const std::vector<kiriview::ImageDocumentPageCandidate> candidates
                          = std::move(*m_synchronousFirstCandidates);
                      m_synchronousFirstCandidates.reset();
                      load->completion.claimAndRun(
                          [load, candidates]() mutable { load->callback(std::move(candidates)); });
                  }
                  return job;
              };
        provider.directoryContainers
            = [](QObject*, QUrl, kiriview::ContainerCandidatesCallback, kiriview::ErrorCallback) {
                  return kiriview::ImageIoJob();
              };
        provider.openedCollectionCandidates
            = [](QObject*, kiriview::OpenedCollectionScopeLocation,
                  kiriview::ImageDocumentPageCandidatesCallback,
                  kiriview::MediaEntrySourceErrorCallback) { return kiriview::ImageIoJob(); };
        provider.directoryImageDocumentPageChanges
            = [](QObject*, QUrl, kiriview::ImageDocumentPageCandidatesCallback,
                  kiriview::ErrorCallback) { return kiriview::ImageIoJob(); };
        return provider;
    }

    std::size_t imageLoadCount() const { return m_imageLoads.size(); }

    ManualImageDocumentPageCandidateLoad& imageLoad(std::size_t index)
    {
        return *m_imageLoads.at(index);
    }

    ManualImageDocumentPageCandidateLoad& backImageLoad() { return *m_imageLoads.back(); }

    void synchronouslyCompleteFirstImageLoadWith(
        std::vector<kiriview::ImageDocumentPageCandidate> candidates)
    {
        m_synchronousFirstCandidates = std::move(candidates);
    }

    void deliverBackImageDocumentPageCandidatesIgnoringCancellation(
        std::vector<kiriview::ImageDocumentPageCandidate> candidates)
    {
        if (m_imageLoads.back()->callback) {
            m_imageLoads.back()->callback(std::move(candidates));
        }
    }

private:
    std::optional<std::vector<kiriview::ImageDocumentPageCandidate>> m_synchronousFirstCandidates;
    std::vector<std::shared_ptr<ManualImageDocumentPageCandidateLoad>> m_imageLoads;
};

class SynchronousContainerThenManualImageProvider
{
public:
    explicit SynchronousContainerThenManualImageProvider(QUrl containerUrl)
        : m_containerUrl(std::move(containerUrl))
    {
    }

    kiriview::ImageDocumentPageCandidateProvider provider()
    {
        kiriview::ImageDocumentPageCandidateProvider provider;
        provider.directoryImageDocumentPages
            = [](QObject*, QUrl, kiriview::ImageDocumentPageCandidatesCallback,
                  kiriview::ErrorCallback) { return kiriview::ImageIoJob(); };
        provider.directoryContainers
            = [this](QObject* receiver, QUrl, kiriview::ContainerCandidatesCallback callback,
                  kiriview::ErrorCallback) {
                  auto load = std::make_shared<ManualImageDocumentPageCandidateLoad>();
                  kiriview::ImageIoJob job
                      = kiriview::TestSupport::Detail::startManualIoJob(receiver, load);
                  load->completion.claimAndRun([this, callback = std::move(callback)]() mutable {
                      callback({ comicBookContainerCandidate(m_containerUrl) });
                  });
                  return job;
              };
        provider.openedCollectionCandidates
            = [this](QObject* receiver, kiriview::OpenedCollectionScopeLocation,
                  kiriview::ImageDocumentPageCandidatesCallback callback,
                  kiriview::MediaEntrySourceErrorCallback) {
                  auto load = std::make_shared<ManualImageDocumentPageCandidateLoad>();
                  load->callback = std::move(callback);
                  kiriview::ImageIoJob job
                      = kiriview::TestSupport::Detail::startManualIoJob(receiver, load);
                  m_imageLoads.push_back(load);
                  return job;
              };
        provider.directoryImageDocumentPageChanges
            = [](QObject*, QUrl, kiriview::ImageDocumentPageCandidatesCallback,
                  kiriview::ErrorCallback) { return kiriview::ImageIoJob(); };
        return provider;
    }

    std::size_t imageLoadCount() const { return m_imageLoads.size(); }

    ManualImageDocumentPageCandidateLoad& backImageLoad() { return *m_imageLoads.back(); }

private:
    QUrl m_containerUrl;
    std::vector<std::shared_ptr<ManualImageDocumentPageCandidateLoad>> m_imageLoads;
};

template <typename Operation>
const Operation* findOperation(const kiriview::ImageDocumentRuntimePlan& plan)
{
    for (const kiriview::ImageDocumentRuntimeOperation& operation : plan) {
        if (const auto* payload = std::get_if<Operation>(&operation)) {
            return payload;
        }
    }

    return nullptr;
}

bool planLoadsUrl(const kiriview::ImageDocumentRuntimePlan& plan, const QUrl& url)
{
    const auto* operation = findOperation<kiriview::LoadUrlOperation>(plan);
    return operation != nullptr && operation->target.url == url;
}

bool planLoadsContainerImage(
    const kiriview::ImageDocumentRuntimePlan& plan, const QUrl& imageUrl, const QUrl& containerUrl)
{
    const auto* operation = findOperation<kiriview::LoadContainerImageOperation>(plan);
    return operation != nullptr && operation->target.url == imageUrl
        && operation->openedCollectionScope.fileUrl() == containerUrl;
}

kiriview::ResolvedNavigationSource resolveExternalSource(const QUrl& url)
{
    return kiriview::resolvedNavigationSource(url, {});
}
}

class TestImageDocumentDeletionFallbackController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void imageFallbackOpensNextSibling();
    void canceledImageFallbackCompletionIsIgnored();
    void synchronousReplacementKeepsReplacementJob();
    void synchronousContainerPhaseKeepsImagePhaseJob();
    void resolverReentryPreventsStaleImagePhase();
    void comicBookFallbackTriesPreviousContainerWhenPreferredIsEmpty();
};

void TestImageDocumentDeletionFallbackController::imageFallbackOpensNextSibling()
{
    QObject parent;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider provider;
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    const QUrl currentUrl = localUrl(QStringLiteral("/images/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/images/03.png"));
    provider.setDirectoryImages(localUrl(QStringLiteral("/images/")),
        {
            imageDocumentPageCandidate(localUrl(QStringLiteral("/images/01.png"))),
            imageDocumentPageCandidate(nextUrl),
        });

    kiriview::ImageDocumentDeletionFallbackController controller(
        &parent, provider.provider(),
        [&runtimePlans](
            kiriview::ImageDocumentRuntimePlan plan) { runtimePlans.push_back(std::move(plan)); },
        resolveExternalSource);

    controller.open(kiriview::ImageRemovalFallback {
        kiriview::ImageDocumentPageCandidateListContext::forDirectory(
            currentUrl, localUrl(QStringLiteral("/images/"))),
        currentUrl,
        QStringLiteral("02.png"),
    });

    QCOMPARE(runtimePlans.size(), std::size_t(1));
    QVERIFY(planLoadsUrl(runtimePlans.front(), nextUrl));
}

void TestImageDocumentDeletionFallbackController::canceledImageFallbackCompletionIsIgnored()
{
    QObject parent;
    ManualDeletionFallbackCandidateProvider provider;
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    const QUrl currentUrl = localUrl(QStringLiteral("/images/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/images/03.png"));

    kiriview::ImageDocumentDeletionFallbackController controller(
        &parent, provider.provider(),
        [&runtimePlans](
            kiriview::ImageDocumentRuntimePlan plan) { runtimePlans.push_back(std::move(plan)); },
        resolveExternalSource);

    controller.open(kiriview::ImageRemovalFallback {
        kiriview::ImageDocumentPageCandidateListContext::forDirectory(
            currentUrl, localUrl(QStringLiteral("/images/"))),
        currentUrl,
        QStringLiteral("02.png"),
    });
    QCOMPARE(provider.imageLoadCount(), std::size_t(1));

    controller.cancel();
    QVERIFY(provider.backImageLoad().canceled);

    provider.deliverBackImageDocumentPageCandidatesIgnoringCancellation(
        { imageDocumentPageCandidate(nextUrl) });

    QVERIFY(runtimePlans.empty());
}

void TestImageDocumentDeletionFallbackController::synchronousReplacementKeepsReplacementJob()
{
    QObject parent;
    ManualDeletionFallbackCandidateProvider provider;
    const QUrl firstCurrentUrl = localUrl(QStringLiteral("/first/02.png"));
    const QUrl firstNextUrl = localUrl(QStringLiteral("/first/03.png"));
    const QUrl secondCurrentUrl = localUrl(QStringLiteral("/second/02.png"));
    provider.synchronouslyCompleteFirstImageLoadWith({ imageDocumentPageCandidate(firstNextUrl) });

    std::unique_ptr<kiriview::ImageDocumentDeletionFallbackController> controller;
    bool replacementStarted = false;
    controller = std::make_unique<kiriview::ImageDocumentDeletionFallbackController>(
        &parent, provider.provider(),
        [&](kiriview::ImageDocumentRuntimePlan plan) {
            if (!planLoadsUrl(plan, firstNextUrl) || replacementStarted) {
                return;
            }
            replacementStarted = true;
            controller->open(kiriview::ImageRemovalFallback {
                kiriview::ImageDocumentPageCandidateListContext::forDirectory(
                    secondCurrentUrl, localUrl(QStringLiteral("/second/"))),
                secondCurrentUrl,
                QStringLiteral("02.png"),
            });
        },
        resolveExternalSource);

    controller->open(kiriview::ImageRemovalFallback {
        kiriview::ImageDocumentPageCandidateListContext::forDirectory(
            firstCurrentUrl, localUrl(QStringLiteral("/first/"))),
        firstCurrentUrl,
        QStringLiteral("02.png"),
    });

    QVERIFY(replacementStarted);
    QCOMPARE(provider.imageLoadCount(), std::size_t(2));
    QVERIFY(!provider.imageLoad(1).canceled);
}

void TestImageDocumentDeletionFallbackController::synchronousContainerPhaseKeepsImagePhaseJob()
{
    QObject parent;
    const QUrl nextContainerUrl = localUrl(QStringLiteral("/books/c.cbz"));
    SynchronousContainerThenManualImageProvider provider(nextContainerUrl);
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    kiriview::ImageDocumentDeletionFallbackController controller(
        &parent, provider.provider(),
        [&runtimePlans](
            kiriview::ImageDocumentRuntimePlan plan) { runtimePlans.push_back(std::move(plan)); },
        resolveExternalSource);

    controller.open(kiriview::ComicBookRemovalFallback {
        localUrl(QStringLiteral("/books/b.cbz")),
        localUrl(QStringLiteral("/books/")),
        QStringLiteral("b.cbz"),
    });

    QCOMPARE(provider.imageLoadCount(), std::size_t(1));
    QVERIFY(!provider.backImageLoad().canceled);
    QVERIFY(runtimePlans.empty());
}

void TestImageDocumentDeletionFallbackController::resolverReentryPreventsStaleImagePhase()
{
    QObject parent;
    const QUrl nextContainerUrl = localUrl(QStringLiteral("/books/c.cbz"));
    SynchronousContainerThenManualImageProvider provider(nextContainerUrl);
    std::unique_ptr<kiriview::ImageDocumentDeletionFallbackController> controller;
    bool reentered = false;
    controller = std::make_unique<kiriview::ImageDocumentDeletionFallbackController>(&parent,
        provider.provider(),
        kiriview::ImageDocumentDeletionFallbackController::RuntimePlanCallback {},
        [&](const QUrl& url) {
            if (!reentered) {
                reentered = true;
                controller->open(kiriview::NoImageRemovalFallback {});
            }
            return resolveExternalSource(url);
        });

    controller->open(kiriview::ComicBookRemovalFallback {
        localUrl(QStringLiteral("/books/b.cbz")),
        localUrl(QStringLiteral("/books/")),
        QStringLiteral("b.cbz"),
    });

    QVERIFY(reentered);
    QCOMPARE(provider.imageLoadCount(), std::size_t(0));
}

void TestImageDocumentDeletionFallbackController::
    comicBookFallbackTriesPreviousContainerWhenPreferredIsEmpty()
{
    QObject parent;
    kiriview::TestSupport::FakeImageDocumentPageCandidateProvider provider;
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    const QUrl previousContainerUrl = localUrl(QStringLiteral("/books/a.cbz"));
    const QUrl currentContainerUrl = localUrl(QStringLiteral("/books/b.cbz"));
    const QUrl nextContainerUrl = localUrl(QStringLiteral("/books/c.cbz"));
    const QUrl candidateDirectoryUrl = localUrl(QStringLiteral("/books/"));
    provider.setContainerCandidates(candidateDirectoryUrl,
        {
            comicBookContainerCandidate(previousContainerUrl),
            comicBookContainerCandidate(nextContainerUrl),
        });

    const std::optional<kiriview::OpenedCollectionScopeLocation> previousArchive
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(previousContainerUrl, {}));
    QVERIFY(previousArchive.has_value());
    const std::optional<kiriview::OpenedCollectionScopeLocation> nextArchive
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(nextContainerUrl, {}));
    QVERIFY(nextArchive.has_value());
    const QUrl previousPageUrl
        = archivePageUrl(previousArchive->rootUrl(), QStringLiteral("page.png"));
    provider.setOpenedCollectionCandidates(nextArchive->rootUrl(), {});
    provider.setOpenedCollectionCandidates(
        previousArchive->rootUrl(), { imageDocumentPageCandidate(previousPageUrl) });

    kiriview::ImageDocumentDeletionFallbackController controller(
        &parent, provider.provider(),
        [&runtimePlans](
            kiriview::ImageDocumentRuntimePlan plan) { runtimePlans.push_back(std::move(plan)); },
        resolveExternalSource);

    controller.open(kiriview::ComicBookRemovalFallback {
        currentContainerUrl,
        candidateDirectoryUrl,
        QStringLiteral("b.cbz"),
    });

    QCOMPARE(runtimePlans.size(), std::size_t(1));
    QVERIFY(planLoadsContainerImage(runtimePlans.front(), previousPageUrl, previousContainerUrl));
}

QTEST_GUILESS_MAIN(TestImageDocumentDeletionFallbackController)

#include "tst_imagedocumentdeletionfallbackcontroller.moc"
