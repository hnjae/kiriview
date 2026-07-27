// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imagedocumentdeletioncontroller.h"

#include "document/imagedocumentstate.h"
#include "image_test_support.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <functional>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace {
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

class ManualDeletionCandidateProvider
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
                  return job;
              };
        provider.directoryContainers
            = [](QObject*, QUrl, kiriview::ContainerCandidatesCallback, kiriview::ErrorCallback) {
                  return kiriview::ImageIoJob();
              };
        provider.openedCollectionCandidates
            = [](QObject*, kiriview::OpenedCollectionScopeLocation,
                  kiriview::ImageDocumentPageCandidatesCallback,
                  kiriview::ErrorCallback) { return kiriview::ImageIoJob(); };
        provider.directoryImageDocumentPageChanges
            = [](QObject*, QUrl, kiriview::ImageDocumentPageCandidatesCallback,
                  kiriview::ErrorCallback) { return kiriview::ImageIoJob(); };
        return provider;
    }

    std::size_t imageLoadCount() const { return m_imageLoads.size(); }

    ManualImageDocumentPageCandidateLoad& backImageLoad() { return *m_imageLoads.back(); }

    void deliverBackImageDocumentPageCandidatesIgnoringCancellation(
        std::vector<kiriview::ImageDocumentPageCandidate> candidates)
    {
        if (m_imageLoads.back()->callback) {
            m_imageLoads.back()->callback(std::move(candidates));
        }
    }

private:
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

class DeletionControllerFixture
{
public:
    explicit DeletionControllerFixture(const QUrl& imageUrl)
    {
        state.setStatus(kiriview::ImageDocumentStatus::Ready);
        state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(imageUrl));
        controller = std::make_unique<kiriview::ImageDocumentDeletionController>(
            &parent, state, []() { return true; }, candidateProvider.provider(),
            kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
            kiriview::ImageDocumentDeletionController::Callbacks {
                [this]() {
                    ++inProgressChangeCount;
                    if (inProgressChangedHook) {
                        inProgressChangedHook();
                    }
                },
                [this](kiriview::ImageDocumentRuntimePlan plan) {
                    if (runtimePlanHook) {
                        runtimePlanHook(plan);
                    }
                    runtimePlans.push_back(std::move(plan));
                },
                [this](const QString& errorString) { failures.push_back(errorString); },
            },
            [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });
    }

    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    std::vector<QString> failures;
    int inProgressChangeCount = 0;
    std::function<void()> inProgressChangedHook;
    std::function<void(const kiriview::ImageDocumentRuntimePlan&)> runtimePlanHook;
    std::unique_ptr<kiriview::ImageDocumentDeletionController> controller;
};
}

class TestImageDocumentDeletionController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadingDocumentWithRetainedImageDoesNotStartFileOperation();
    void erroredDocumentWithRetainedImageDoesNotStartFileOperation();
    void canceledFileDeletionCompletionIsIgnored();
    void canceledFallbackCandidateCompletionIsIgnored();
    void reentrantCancellationDuringStartNotificationPreventsFileOperation();
    void reentrantDisplayProbePreservesNestedDeletion();
    void presentationChangeDuringDisplayProbePreventsDeletion();
    void successInvalidatesDeletedTargetBeforeCompletionNotification();
    void failureSettlesBeforeNotificationAndAllowsImmediateRetry();
    void destructionDuringStartNotificationDoesNotStartFileOperation();
    void destructionDuringSettlementSuppressesLaterFailureNotification();
};

void TestImageDocumentDeletionController::
    loadingDocumentWithRetainedImageDoesNotStartFileOperation()
{
    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    std::vector<QString> failures;
    int inProgressChangeCount = 0;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/01.png"));
    state.setStatus(kiriview::ImageDocumentStatus::Loading);
    state.setLoading(true);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(imageUrl));

    kiriview::ImageDocumentDeletionController controller(
        &parent, state, []() { return true; }, candidateProvider.provider(),
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        kiriview::ImageDocumentDeletionController::Callbacks {
            [&inProgressChangeCount]() { ++inProgressChangeCount; },
            [&runtimePlans](kiriview::ImageDocumentRuntimePlan plan) {
                runtimePlans.push_back(std::move(plan));
            },
            [&failures](const QString& errorString) { failures.push_back(errorString); },
        },
        [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });

    controller.deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));
    QVERIFY(!controller.inProgress());
    QCOMPARE(inProgressChangeCount, 0);
    QVERIFY(runtimePlans.empty());
    QVERIFY(failures.empty());
}

void TestImageDocumentDeletionController::
    erroredDocumentWithRetainedImageDoesNotStartFileOperation()
{
    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    std::vector<QString> failures;
    int inProgressChangeCount = 0;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/01.png"));
    state.setStatus(kiriview::ImageDocumentStatus::Error);
    state.setErrorString(QStringLiteral("decode failed"));
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(imageUrl));

    kiriview::ImageDocumentDeletionController controller(
        &parent, state, []() { return true; }, candidateProvider.provider(),
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        kiriview::ImageDocumentDeletionController::Callbacks {
            [&inProgressChangeCount]() { ++inProgressChangeCount; },
            [&runtimePlans](kiriview::ImageDocumentRuntimePlan plan) {
                runtimePlans.push_back(std::move(plan));
            },
            [&failures](const QString& errorString) { failures.push_back(errorString); },
        },
        [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });

    controller.deleteDisplayedFile(kiriview::FileDeletionMode::DeletePermanently);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));
    QVERIFY(!controller.inProgress());
    QCOMPARE(inProgressChangeCount, 0);
    QVERIFY(runtimePlans.empty());
    QVERIFY(failures.empty());
}

void TestImageDocumentDeletionController::canceledFileDeletionCompletionIsIgnored()
{
    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    std::vector<QString> failures;
    int inProgressChangeCount = 0;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/01.png"));
    state.setStatus(kiriview::ImageDocumentStatus::Ready);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(imageUrl));

    kiriview::ImageDocumentDeletionController controller(
        &parent, state, []() { return true; }, candidateProvider.provider(),
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        kiriview::ImageDocumentDeletionController::Callbacks {
            [&inProgressChangeCount]() { ++inProgressChangeCount; },
            [&runtimePlans](kiriview::ImageDocumentRuntimePlan plan) {
                runtimePlans.push_back(std::move(plan));
            },
            [&failures](const QString& errorString) { failures.push_back(errorString); },
        },
        [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });

    controller.deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QVERIFY(controller.inProgress());

    controller.cancel();
    QVERIFY(!controller.inProgress());
    QVERIFY(fileDeletionProvider.backOperation().canceled);

    fileDeletionProvider.backOperation().callback(kiriview::FileDeletionResult::Succeeded,
        kiriview::TestSupport::manualFileDeletionFailure(
            fileDeletionProvider.backOperation().request, kiriview::FileDeletionResult::Succeeded,
            QString()));

    QVERIFY(runtimePlans.empty());
    QVERIFY(failures.empty());
    QCOMPARE(inProgressChangeCount, 2);
}

void TestImageDocumentDeletionController::canceledFallbackCandidateCompletionIsIgnored()
{
    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    std::vector<QString> failures;
    const QUrl currentUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/images/02.png"));
    state.setStatus(kiriview::ImageDocumentStatus::Ready);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(currentUrl));

    kiriview::ImageDocumentDeletionController controller(
        &parent, state, []() { return true; }, candidateProvider.provider(),
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        kiriview::ImageDocumentDeletionController::Callbacks {
            {},
            [&runtimePlans](kiriview::ImageDocumentRuntimePlan plan) {
                runtimePlans.push_back(std::move(plan));
            },
            [&failures](const QString& errorString) { failures.push_back(errorString); },
        },
        [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });

    controller.deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);
    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);
    QCOMPARE(candidateProvider.imageLoadCount(), std::size_t(1));
    QCOMPARE(runtimePlans.size(), std::size_t(1));
    QVERIFY(
        findOperation<kiriview::FinishEmptySourceLoadOperation>(runtimePlans.front()) != nullptr);

    controller.cancel();
    QVERIFY(candidateProvider.backImageLoad().canceled);

    candidateProvider.deliverBackImageDocumentPageCandidatesIgnoringCancellation(
        { imageDocumentPageCandidate(nextUrl) });

    QCOMPARE(runtimePlans.size(), std::size_t(1));
    QVERIFY(findOperation<kiriview::LoadUrlOperation>(runtimePlans.front()) == nullptr);
    QVERIFY(failures.empty());
}

void TestImageDocumentDeletionController::
    reentrantCancellationDuringStartNotificationPreventsFileOperation()
{
    const QUrl imageUrl = localUrl(QStringLiteral("/images/reentrant-start.png"));
    DeletionControllerFixture fixture(imageUrl);
    bool canceled = false;
    fixture.inProgressChangedHook = [&]() {
        if (canceled || !fixture.controller->inProgress()) {
            return;
        }
        canceled = true;
        fixture.controller->cancel();
    };

    fixture.controller->deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);

    QVERIFY(canceled);
    QVERIFY(!fixture.controller->inProgress());
    QCOMPARE(fixture.inProgressChangeCount, 2);
    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(0));
    QVERIFY(fixture.runtimePlans.empty());
    QVERIFY(fixture.failures.empty());
}

void TestImageDocumentDeletionController::reentrantDisplayProbePreservesNestedDeletion()
{
    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/reentrant-probe.png"));
    state.setStatus(kiriview::ImageDocumentStatus::Ready);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(imageUrl));

    std::unique_ptr<kiriview::ImageDocumentDeletionController> controller;
    bool reentered = false;
    controller = std::make_unique<kiriview::ImageDocumentDeletionController>(
        &parent, state,
        [&]() {
            if (!reentered) {
                reentered = true;
                controller->deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);
            }
            return true;
        },
        candidateProvider.provider(),
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        kiriview::ImageDocumentDeletionController::Callbacks {},
        [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });

    controller->deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);

    QVERIFY(reentered);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));
    QVERIFY(!fileDeletionProvider.backOperation().canceled);
    QVERIFY(controller->inProgress());
}

void TestImageDocumentDeletionController::presentationChangeDuringDisplayProbePreventsDeletion()
{
    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/probe-target.png"));
    state.setStatus(kiriview::ImageDocumentStatus::Ready);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(imageUrl));

    kiriview::ImageDocumentDeletionController controller(
        &parent, state,
        [&]() {
            state.advancePresentationLifecycle();
            return true;
        },
        candidateProvider.provider(),
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        kiriview::ImageDocumentDeletionController::Callbacks {},
        [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });

    controller.deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);

    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));
    QVERIFY(!controller.inProgress());
}

void TestImageDocumentDeletionController::
    successInvalidatesDeletedTargetBeforeCompletionNotification()
{
    const QUrl imageUrl = localUrl(QStringLiteral("/images/reentrant-completion.png"));
    DeletionControllerFixture fixture(imageUrl);
    bool deletionPlanApplied = false;
    bool completionNotificationObservedAppliedPlan = false;
    bool replacementAttempted = false;
    fixture.runtimePlanHook = [&](const kiriview::ImageDocumentRuntimePlan& plan) {
        if (findOperation<kiriview::FinishEmptySourceLoadOperation>(plan) == nullptr) {
            return;
        }
        deletionPlanApplied = true;
        fixture.state.setStatus(kiriview::ImageDocumentStatus::Null);
        fixture.state.clearDisplayedImageLocation();
    };
    fixture.inProgressChangedHook = [&]() {
        if (fixture.controller->inProgress() || replacementAttempted) {
            return;
        }
        replacementAttempted = true;
        completionNotificationObservedAppliedPlan = deletionPlanApplied;
        fixture.controller->deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);
    };

    fixture.controller->deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);
    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(1));
    fixture.fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QVERIFY(deletionPlanApplied);
    QVERIFY(replacementAttempted);
    QVERIFY(completionNotificationObservedAppliedPlan);
    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fixture.runtimePlans.size(), std::size_t(1));
    QVERIFY(!fixture.controller->inProgress());
    QVERIFY(fixture.failures.empty());
}

void TestImageDocumentDeletionController::failureSettlesBeforeNotificationAndAllowsImmediateRetry()
{
    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/retry-after-failure.png"));
    state.setStatus(kiriview::ImageDocumentStatus::Ready);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(imageUrl));

    bool inProgressAtFailure = true;
    QString failureMessage;
    std::unique_ptr<kiriview::ImageDocumentDeletionController> controller;
    controller = std::make_unique<kiriview::ImageDocumentDeletionController>(
        &parent, state, []() { return true; }, candidateProvider.provider(),
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        kiriview::ImageDocumentDeletionController::Callbacks {
            {},
            {},
            [&](const QString& message) {
                inProgressAtFailure = controller->inProgress();
                failureMessage = message;
                controller->deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);
            },
        },
        [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });

    controller->deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Failed);

    QVERIFY(!inProgressAtFailure);
    QVERIFY(!failureMessage.isEmpty());
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(2));
    QCOMPARE(fileDeletionProvider.backOperation().request.targetUrl, imageUrl);
    QVERIFY(controller->inProgress());
}

void TestImageDocumentDeletionController::
    destructionDuringStartNotificationDoesNotStartFileOperation()
{
    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/destroy-on-start.png"));
    state.setStatus(kiriview::ImageDocumentStatus::Ready);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(imageUrl));

    std::unique_ptr<kiriview::ImageDocumentDeletionController> controller;
    controller = std::make_unique<kiriview::ImageDocumentDeletionController>(
        &parent, state, []() { return true; }, candidateProvider.provider(),
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        kiriview::ImageDocumentDeletionController::Callbacks {
            [&]() { controller.reset(); },
            {},
            {},
        },
        [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });

    controller->deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);

    QVERIFY(controller == nullptr);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(0));
}

void TestImageDocumentDeletionController::
    destructionDuringSettlementSuppressesLaterFailureNotification()
{
    QObject parent;
    kiriview::ImageDocumentState state;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDeletionCandidateProvider candidateProvider;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/destroy-on-settlement.png"));
    state.setStatus(kiriview::ImageDocumentStatus::Ready);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(imageUrl));

    int progressNotificationCount = 0;
    bool failureNotified = false;
    std::unique_ptr<kiriview::ImageDocumentDeletionController> controller;
    controller = std::make_unique<kiriview::ImageDocumentDeletionController>(
        &parent, state, []() { return true; }, candidateProvider.provider(),
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        kiriview::ImageDocumentDeletionController::Callbacks {
            [&]() {
                ++progressNotificationCount;
                if (progressNotificationCount == 2) {
                    controller.reset();
                }
            },
            {},
            [&](const QString&) { failureNotified = true; },
        },
        [](const QUrl& url) { return kiriview::resolvedNavigationSource(url, {}); });

    controller->deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(1));

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Failed);

    QVERIFY(controller == nullptr);
    QCOMPARE(progressNotificationCount, 2);
    QVERIFY(!failureNotified);
}

QTEST_GUILESS_MAIN(TestImageDocumentDeletionController)

#include "tst_imagedocumentdeletioncontroller.moc"
