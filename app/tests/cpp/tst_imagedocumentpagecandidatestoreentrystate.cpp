// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidatestoreentrystate.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::ImageDocumentPageCandidate candidate(const QString& path, const QString& name)
{
    return kiriview::ImageDocumentPageCandidate {
        localUrl(path),
        name,
    };
}

std::vector<kiriview::ImageDocumentPageCandidate> candidates(std::initializer_list<QString> names)
{
    std::vector<kiriview::ImageDocumentPageCandidate> result;
    result.reserve(names.size());
    for (const QString& name : names) {
        result.push_back(candidate(QStringLiteral("/images/%1").arg(name), name));
    }
    return result;
}

kiriview::ImageIoJob addPendingLoad(kiriview::ImageDocumentPageCandidateStoreEntryState& state,
    QObject* parent, kiriview::ImageDocumentPageCandidatesCallback callback,
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback,
    std::function<void(QObject*)> removeToken)
{
    QObject* token = new QObject(parent);
    kiriview::ImageIoJob job(token, [removeToken = std::move(removeToken)](QObject* object) {
        removeToken(object);
        object->deleteLater();
    });
    state.addPendingLoad(job.completion(), std::move(callback), std::move(errorCallback));
    return job;
}

kiriview::ImageIoJob addSubscriber(kiriview::ImageDocumentPageCandidateStoreEntryState& state,
    QObject* parent, kiriview::ImageDocumentPageCandidatesCallback callback,
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback,
    std::function<void(QObject*)> removeToken)
{
    QObject* token = new QObject(parent);
    kiriview::ImageIoJob job(token, [removeToken = std::move(removeToken)](QObject* object) {
        removeToken(object);
        object->deleteLater();
    });
    state.addSubscriber(job.completion(), std::move(callback), std::move(errorCallback));
    return job;
}

void deliverCandidatePlan(
    const kiriview::ImageDocumentPageCandidateStoreEntryNotificationPlan& plan)
{
    for (const kiriview::ImageDocumentPageCandidateStoreEntryPendingLoad& load :
        plan.completedLoads) {
        load.completion.claimAndDelete([&]() {
            if (load.callback) {
                load.callback(plan.candidates);
            }
        });
    }
    for (const kiriview::ImageDocumentPageCandidateStoreEntrySubscriber& subscriber :
        plan.changedSubscribers) {
        if (subscriber.callback) {
            subscriber.callback(plan.candidates);
        }
    }
}

void deliverErrorPlan(const kiriview::ImageDocumentPageCandidateStoreEntryNotificationPlan& plan)
{
    for (const kiriview::ImageDocumentPageCandidateStoreEntryPendingLoad& load : plan.failedLoads) {
        load.completion.claimAndDelete([&]() {
            if (load.errorCallback) {
                load.errorCallback(plan.error);
            }
        });
    }
    for (const kiriview::ImageDocumentPageCandidateStoreEntrySubscriber& subscriber :
        plan.failedSubscribers) {
        if (subscriber.errorCallback) {
            subscriber.errorCallback(plan.error);
        }
    }
}
}

class TestImageDocumentPageCandidateStoreEntryState : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void completedListingReturnsPendingLoadPlanAndCachesCandidates();
    void cancelledPendingLoadDoesNotReceiveCompletion();
    void subscribersOnlyReceivePlansAfterInitialListing();
    void failedListingReturnsErrorPlansForPendingLoadsAndSubscribers();
    void successfulUpdateClearsPriorObservationFailure();
};

void TestImageDocumentPageCandidateStoreEntryState::
    completedListingReturnsPendingLoadPlanAndCachesCandidates()
{
    kiriview::ImageDocumentPageCandidateStoreEntryState state;
    std::vector<kiriview::ImageDocumentPageCandidate> loadedCandidates;
    int errorCount = 0;
    int cancelCount = 0;

    kiriview::ImageIoJob job = addPendingLoad(
        state, this,
        [&loadedCandidates](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            loadedCandidates = std::move(candidates);
        },
        [&errorCount](const kiriview::ImageDocumentPageCandidateLoadError&) { ++errorCount; },
        [&state, &cancelCount](QObject* token) {
            ++cancelCount;
            state.removePendingLoad(token);
        });

    QVERIFY(job.isActive());
    const kiriview::ImageDocumentPageCandidateStoreEntryNotificationPlan plan
        = state.completeListing(candidates({ QStringLiteral("01.png"), QStringLiteral("02.png") }));

    QVERIFY(state.listed());
    QVERIFY(!state.failed());
    QCOMPARE(plan.completedLoads.size(), std::size_t(1));
    QCOMPARE(plan.failedLoads.size(), std::size_t(0));
    QCOMPARE(plan.changedSubscribers.size(), std::size_t(0));
    QCOMPARE(plan.candidates.size(), std::size_t(2));
    QVERIFY(job.isActive());
    deliverCandidatePlan(plan);
    QVERIFY(!job.isActive());
    QCOMPARE(errorCount, 0);
    QCOMPARE(cancelCount, 0);
    QCOMPARE(loadedCandidates.size(), std::size_t(2));
    QCOMPARE(loadedCandidates.size(), state.candidates().size());
    QCOMPARE(loadedCandidates.front().url, state.candidates().front().url);
    QCOMPARE(loadedCandidates.front().name, state.candidates().front().name);
    QCOMPARE(loadedCandidates.back().url, state.candidates().back().url);
    QCOMPARE(loadedCandidates.back().name, state.candidates().back().name);
}

void TestImageDocumentPageCandidateStoreEntryState::cancelledPendingLoadDoesNotReceiveCompletion()
{
    kiriview::ImageDocumentPageCandidateStoreEntryState state;
    int loadCount = 0;
    int cancelCount = 0;

    kiriview::ImageIoJob job = addPendingLoad(
        state, this,
        [&loadCount](std::vector<kiriview::ImageDocumentPageCandidate>) { ++loadCount; },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {},
        [&state, &cancelCount](QObject* token) {
            ++cancelCount;
            state.removePendingLoad(token);
        });

    job.cancel();
    const kiriview::ImageDocumentPageCandidateStoreEntryNotificationPlan plan
        = state.completeListing(candidates({ QStringLiteral("01.png") }));

    QCOMPARE(plan.completedLoads.size(), std::size_t(0));
    QCOMPARE(cancelCount, 1);
    QCOMPARE(loadCount, 0);
    QVERIFY(!job.isActive());
}

void TestImageDocumentPageCandidateStoreEntryState::subscribersOnlyReceivePlansAfterInitialListing()
{
    kiriview::ImageDocumentPageCandidateStoreEntryState state;
    std::vector<kiriview::ImageDocumentPageCandidate> changedCandidates;
    int changeCount = 0;
    int cancelCount = 0;

    kiriview::ImageIoJob job = addSubscriber(
        state, this,
        [&changedCandidates, &changeCount](
            std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            changedCandidates = std::move(candidates);
            ++changeCount;
        },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {},
        [&state, &cancelCount](QObject* token) {
            ++cancelCount;
            state.removeSubscriber(token);
        });

    kiriview::ImageDocumentPageCandidateStoreEntryNotificationPlan plan
        = state.completeListing(candidates({ QStringLiteral("01.png") }));
    QCOMPARE(plan.changedSubscribers.size(), std::size_t(0));
    QCOMPARE(changeCount, 0);

    plan = state.updateListing(candidates({ QStringLiteral("01.png") }));
    QCOMPARE(plan.changedSubscribers.size(), std::size_t(0));
    QCOMPARE(changeCount, 0);

    plan = state.updateListing(candidates({ QStringLiteral("01.png"), QStringLiteral("02.png") }));
    QCOMPARE(plan.changedSubscribers.size(), std::size_t(1));
    QCOMPARE(plan.candidates.size(), std::size_t(2));
    deliverCandidatePlan(plan);
    QCOMPARE(changeCount, 1);
    QCOMPARE(changedCandidates.size(), std::size_t(2));

    job.cancel();
    plan = state.updateListing(candidates(
        { QStringLiteral("01.png"), QStringLiteral("02.png"), QStringLiteral("03.png") }));
    QCOMPARE(plan.changedSubscribers.size(), std::size_t(0));
    QCOMPARE(cancelCount, 1);
    QCOMPARE(changeCount, 1);
}

void TestImageDocumentPageCandidateStoreEntryState::
    failedListingReturnsErrorPlansForPendingLoadsAndSubscribers()
{
    kiriview::ImageDocumentPageCandidateStoreEntryState state;
    std::optional<kiriview::ImageDocumentPageCandidateLoadError> pendingError;
    std::optional<kiriview::ImageDocumentPageCandidateLoadError> subscriberError;
    int loadCount = 0;
    int subscriberChangeCount = 0;

    kiriview::ImageIoJob pending = addPendingLoad(
        state, this,
        [&loadCount](std::vector<kiriview::ImageDocumentPageCandidate>) { ++loadCount; },
        [&pendingError](kiriview::ImageDocumentPageCandidateLoadError error) {
            pendingError = std::move(error);
        },
        [&state](QObject* token) { state.removePendingLoad(token); });
    kiriview::ImageIoJob subscriber = addSubscriber(
        state, this,
        [&subscriberChangeCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) { ++subscriberChangeCount; },
        [&subscriberError](kiriview::ImageDocumentPageCandidateLoadError error) {
            subscriberError = std::move(error);
        },
        [&state](QObject* token) { state.removeSubscriber(token); });

    const kiriview::KioOperationFailure expected {
        kiriview::KioOperationKind::DirectoryListing,
        localUrl(QStringLiteral("/images/")),
        73,
        true,
        QString(),
        QStringLiteral("listing canceled"),
        false,
    };
    const kiriview::ImageDocumentPageCandidateStoreEntryNotificationPlan plan
        = state.failListing(kiriview::ImageDocumentPageCandidateLoadError { expected });

    QVERIFY(state.failed());
    const auto* stateFailure = std::get_if<kiriview::KioOperationFailure>(&state.error());
    QVERIFY(stateFailure != nullptr);
    QCOMPARE(stateFailure->rawErrorCode, expected.rawErrorCode);
    QVERIFY(stateFailure->canceled);
    QCOMPARE(plan.failedLoads.size(), std::size_t(1));
    QCOMPARE(plan.failedSubscribers.size(), std::size_t(1));
    const auto* planFailure = std::get_if<kiriview::KioOperationFailure>(&plan.error);
    QVERIFY(planFailure != nullptr);
    QCOMPARE(planFailure->diagnosticDetail, expected.diagnosticDetail);
    QVERIFY(pending.isActive());
    QVERIFY(subscriber.isActive());
    deliverErrorPlan(plan);
    QVERIFY(!pending.isActive());
    QVERIFY(subscriber.isActive());
    QCOMPARE(loadCount, 0);
    QCOMPARE(subscriberChangeCount, 0);
    QVERIFY(pendingError.has_value());
    QVERIFY(subscriberError.has_value());
    const auto* pendingFailure = std::get_if<kiriview::KioOperationFailure>(&*pendingError);
    const auto* subscriberFailure = std::get_if<kiriview::KioOperationFailure>(&*subscriberError);
    QVERIFY(pendingFailure != nullptr);
    QVERIFY(subscriberFailure != nullptr);
    QCOMPARE(pendingFailure->operationKind, expected.operationKind);
    QCOMPARE(pendingFailure->targetUrl, expected.targetUrl);
    QCOMPARE(pendingFailure->rawErrorCode, expected.rawErrorCode);
    QCOMPARE(pendingFailure->canceled, expected.canceled);
    QCOMPARE(pendingFailure->userMessage, expected.userMessage);
    QCOMPARE(pendingFailure->diagnosticDetail, expected.diagnosticDetail);
    QCOMPARE(pendingFailure->retryable, expected.retryable);
    QCOMPARE(subscriberFailure->rawErrorCode, expected.rawErrorCode);
    QCOMPARE(subscriberFailure->canceled, expected.canceled);
}

void TestImageDocumentPageCandidateStoreEntryState::successfulUpdateClearsPriorObservationFailure()
{
    kiriview::ImageDocumentPageCandidateStoreEntryState state;
    static_cast<void>(state.completeListing(candidates({ QStringLiteral("01.png") })));
    static_cast<void>(state.failListing(
        kiriview::ImageDocumentPageCandidateLoadError { QStringLiteral("watch failed") }));
    QVERIFY(state.listed());
    QVERIFY(state.failed());

    static_cast<void>(
        state.updateListing(candidates({ QStringLiteral("01.png"), QStringLiteral("02.png") })));

    QVERIFY(state.listed());
    QVERIFY(!state.failed());
    QCOMPARE(state.candidates().size(), std::size_t(2));
}

QTEST_GUILESS_MAIN(TestImageDocumentPageCandidateStoreEntryState)

#include "tst_imagedocumentpagecandidatestoreentrystate.moc"
