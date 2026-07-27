// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessiondirectmedianavigationruntime.h"

#include "image_async_test_support.h"
#include "location/imageurl.h"

#include <QObject>
#include <QPointer>
#include <QTest>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class TestDocumentSessionDirectMediaNavigationRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void successfulLoadPublishesCandidates();
    void refreshPublishesBoundaryPlanAndCandidates();
    void openPublishesTargetPlanAndCandidates();
    void failedOpenPublishesUnknownResult();
    void cancelRejectsLateCompletion();
    void cancelRejectsLateError();
    void predicateRejectionDropsCurrentCompletion();
    void cancellationInvalidatesBeforeProviderCallback();
    void cancellationPreservesReentrantReplacementLoad();
    void replacementCancellationCanDestroyRuntime();
    void synchronousCompletionPreservesReplacementJob();
    void predicateReentryRejectsSupersededCompletion();
    void destructionRejectsLateProviderDelivery_data();
    void destructionRejectsLateProviderDelivery();
    void acceptedScopeAllowsEquivalentCursorConfirmation();
};

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(const QUrl& url)
{
    return kiriview::DirectMediaNavigationCandidate { url, url.fileName(QUrl::PrettyDecoded) };
}

struct ManualDirectMediaNavigationCandidateLoad
{
    QObject* object = nullptr;
    QUrl parentUrl;
    kiriview::DirectMediaNavigationCandidatesCallback callback;
    kiriview::ErrorCallback errorCallback;
    kiriview::ImageIoJobCompletion completion;
    bool canceled = false;
};

class ManualDirectMediaNavigationCandidateProvider
{
public:
    void setCancelHook(std::function<void()> cancelHook) { m_cancelHook = std::move(cancelHook); }

    kiriview::DirectMediaNavigationCandidateProvider provider()
    {
        return kiriview::DirectMediaNavigationCandidateProvider {
            [this](QObject* receiver, QUrl parentUrl,
                kiriview::DirectMediaNavigationCandidatesCallback callback,
                kiriview::ErrorCallback errorCallback) {
                auto load = std::make_shared<ManualDirectMediaNavigationCandidateLoad>();
                load->parentUrl = std::move(parentUrl);
                load->callback = std::move(callback);
                load->errorCallback = std::move(errorCallback);

                kiriview::ImageIoJob job
                    = kiriview::TestSupport::Detail::startManualIoJob(receiver, load, m_cancelHook);
                m_loads.push_back(load);
                if (m_synchronousFirstCandidates.has_value() && m_loads.size() == 1) {
                    const std::vector<kiriview::DirectMediaNavigationCandidate> candidates
                        = std::move(*m_synchronousFirstCandidates);
                    m_synchronousFirstCandidates.reset();
                    load->completion.claimAndRun(
                        [load, candidates]() mutable { load->callback(std::move(candidates)); });
                }
                return job;
            },
        };
    }

    std::size_t loadCount() const { return m_loads.size(); }

    ManualDirectMediaNavigationCandidateLoad& loadAt(std::size_t index)
    {
        return *m_loads.at(index);
    }

    void deliverIgnoringCancellation(
        std::size_t index, std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
    {
        ManualDirectMediaNavigationCandidateLoad& load = loadAt(index);
        if (load.callback) {
            load.callback(std::move(candidates));
        }
    }

    void deliverErrorIgnoringCancellation(std::size_t index, const QString& errorString)
    {
        ManualDirectMediaNavigationCandidateLoad& load = loadAt(index);
        if (load.errorCallback) {
            load.errorCallback(errorString);
        }
    }

    void synchronouslyCompleteFirstWith(
        std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
    {
        m_synchronousFirstCandidates = std::move(candidates);
    }

private:
    std::function<void()> m_cancelHook;
    std::optional<std::vector<kiriview::DirectMediaNavigationCandidate>>
        m_synchronousFirstCandidates;
    std::vector<std::shared_ptr<ManualDirectMediaNavigationCandidateLoad>> m_loads;
};

class CancellationCompletingCandidateProvider
{
public:
    kiriview::DirectMediaNavigationCandidateProvider provider()
    {
        return kiriview::DirectMediaNavigationCandidateProvider {
            [this](QObject* receiver, QUrl,
                kiriview::DirectMediaNavigationCandidatesCallback callback,
                kiriview::ErrorCallback) {
                QObject* token = new QObject(receiver);
                return kiriview::ImageIoJob(
                    token, [this, callback = std::move(callback)](QObject* object) mutable {
                        canceled = true;
                        callback({});
                        object->deleteLater();
                    });
            },
        };
    }

    bool canceled = false;
};

struct RuntimeFixture
{
    QObject receiver;
    ManualDirectMediaNavigationCandidateProvider provider;
    kiriview::DocumentSessionDirectMediaNavigationRuntime runtime { provider.provider() };
    int completionCount = 0;
    kiriview::DocumentSessionDirectMediaNavigationCandidatesResult result;
    bool acceptScope = true;

    void load(const kiriview::DirectMediaScope& scope)
    {
        runtime.loadCandidates(
            &receiver, scope, [this](const kiriview::DirectMediaScope&) { return acceptScope; },
            [this](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult loadResult) {
                ++completionCount;
                result = std::move(loadResult);
            });
    }
};

kiriview::DirectMediaScope directMediaScope(const QUrl& currentUrl)
{
    return *kiriview::DirectMediaScope::fromSource(
        kiriview::ResolvedNavigationSource(currentUrl, {}, currentUrl), 7);
}
}

void TestDocumentSessionDirectMediaNavigationRuntime::successfulLoadPublishesCandidates()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/02.png"));

    fixture.load(directMediaScope(currentUrl));
    fixture.provider.deliverIgnoringCancellation(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(fixture.result.succeeded);
    QCOMPARE(fixture.result.candidates.size(), std::size_t(2));
    QCOMPARE(fixture.result.candidates.at(1).url, nextUrl);
}

void TestDocumentSessionDirectMediaNavigationRuntime::refreshPublishesBoundaryPlanAndCandidates()
{
    RuntimeFixture fixture;
    const QUrl previousUrl = localUrl(QStringLiteral("/media/00.mp4"));
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/02.png"));
    kiriview::DocumentSessionDirectMediaNavigationRefreshResult result;

    fixture.runtime.refresh(
        &fixture.receiver, directMediaScope(currentUrl),
        [](const kiriview::DirectMediaScope&) { return true; },
        [&fixture, &result](
            kiriview::DocumentSessionDirectMediaNavigationRefreshResult loadResult) {
            ++fixture.completionCount;
            result = std::move(loadResult);
        });
    fixture.provider.deliverIgnoringCancellation(0,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(currentUrl),
            directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(result.succeeded);
    QCOMPARE(result.candidates.size(), std::size_t(3));
    QCOMPARE(result.boundaryState.currentNumber, 2);
    QCOMPARE(result.boundaryState.count, 3);
    QVERIFY(result.boundaryState.canOpenPrevious);
    QVERIFY(result.boundaryState.canOpenNext);
}

void TestDocumentSessionDirectMediaNavigationRuntime::openPublishesTargetPlanAndCandidates()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/02.png"));
    kiriview::DocumentSessionDirectMediaNavigationOpenResult result;

    fixture.runtime.open(
        &fixture.receiver, directMediaScope(currentUrl),
        kiriview::nextDirectMediaNavigationOpenRequest(),
        [](const kiriview::DirectMediaScope&) { return true; },
        [&fixture, &result](kiriview::DocumentSessionDirectMediaNavigationOpenResult loadResult) {
            ++fixture.completionCount;
            result = std::move(loadResult);
        });
    fixture.provider.deliverIgnoringCancellation(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(result.succeeded);
    QCOMPARE(result.candidates.size(), std::size_t(2));
    QVERIFY(result.plan.targetUrl.has_value());
    QCOMPARE(*result.plan.targetUrl, nextUrl);
    QCOMPARE(result.plan.boundaryState.currentNumber, 1);
    QCOMPARE(result.plan.boundaryState.count, 2);
}

void TestDocumentSessionDirectMediaNavigationRuntime::failedOpenPublishesUnknownResult()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.mp4"));
    kiriview::DocumentSessionDirectMediaNavigationOpenResult result;

    fixture.runtime.open(
        &fixture.receiver, directMediaScope(currentUrl),
        kiriview::nextDirectMediaNavigationOpenRequest(),
        [](const kiriview::DirectMediaScope&) { return true; },
        [&fixture, &result](kiriview::DocumentSessionDirectMediaNavigationOpenResult loadResult) {
            ++fixture.completionCount;
            result = std::move(loadResult);
        });
    fixture.provider.deliverErrorIgnoringCancellation(0, QStringLiteral("missing media"));

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(!result.succeeded);
    QVERIFY(!result.plan.targetUrl.has_value());
    QCOMPARE(result.errorString, QStringLiteral("missing media"));
}

void TestDocumentSessionDirectMediaNavigationRuntime::cancelRejectsLateCompletion()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.mp4"));

    fixture.load(directMediaScope(currentUrl));
    fixture.runtime.cancel();
    QVERIFY(fixture.provider.loadAt(0).canceled);
    fixture.provider.deliverIgnoringCancellation(0, { directMediaNavigationCandidate(currentUrl) });

    QCOMPARE(fixture.completionCount, 0);
}

void TestDocumentSessionDirectMediaNavigationRuntime::cancelRejectsLateError()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.mp4"));

    fixture.load(directMediaScope(currentUrl));
    fixture.runtime.cancel();
    QVERIFY(fixture.provider.loadAt(0).canceled);
    fixture.provider.deliverErrorIgnoringCancellation(0, QStringLiteral("failed"));

    QCOMPARE(fixture.completionCount, 0);
}

void TestDocumentSessionDirectMediaNavigationRuntime::predicateRejectionDropsCurrentCompletion()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.mp4"));

    fixture.acceptScope = false;
    fixture.load(directMediaScope(currentUrl));
    fixture.provider.deliverIgnoringCancellation(0, { directMediaNavigationCandidate(currentUrl) });

    QCOMPARE(fixture.completionCount, 0);
}

void TestDocumentSessionDirectMediaNavigationRuntime::
    cancellationInvalidatesBeforeProviderCallback()
{
    QObject receiver;
    CancellationCompletingCandidateProvider provider;
    kiriview::DocumentSessionDirectMediaNavigationRuntime runtime(provider.provider());
    int completionCount = 0;
    runtime.loadCandidates(
        &receiver, directMediaScope(localUrl(QStringLiteral("/media/01.mp4"))),
        [](const kiriview::DirectMediaScope&) { return true; },
        [&](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) { ++completionCount; });

    runtime.cancel();

    QVERIFY(provider.canceled);
    QCOMPARE(completionCount, 0);
}

void TestDocumentSessionDirectMediaNavigationRuntime::
    cancellationPreservesReentrantReplacementLoad()
{
    QObject receiver;
    ManualDirectMediaNavigationCandidateProvider provider;
    const QUrl firstUrl = localUrl(QStringLiteral("/first/01.mp4"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/replacement/01.mp4"));
    kiriview::DocumentSessionDirectMediaNavigationRuntime runtime(provider.provider());
    int firstCompletionCount = 0;
    int replacementCompletionCount = 0;
    bool replacementStarted = false;
    provider.setCancelHook([&]() {
        if (std::exchange(replacementStarted, true)) {
            return;
        }
        runtime.loadCandidates(
            &receiver, directMediaScope(replacementUrl),
            [](const kiriview::DirectMediaScope&) { return true; },
            [&](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) {
                ++replacementCompletionCount;
            });
    });
    runtime.loadCandidates(
        &receiver, directMediaScope(firstUrl),
        [](const kiriview::DirectMediaScope&) { return true; },
        [&](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) {
            ++firstCompletionCount;
        });

    runtime.cancel();

    QVERIFY(replacementStarted);
    QCOMPARE(provider.loadCount(), std::size_t(2));
    QVERIFY(provider.loadAt(0).canceled);
    QCOMPARE(provider.loadAt(1).parentUrl, replacementUrl.adjusted(QUrl::RemoveFilename));
    QVERIFY(!provider.loadAt(1).canceled);

    provider.deliverIgnoringCancellation(1, { directMediaNavigationCandidate(replacementUrl) });

    QCOMPARE(firstCompletionCount, 0);
    QCOMPARE(replacementCompletionCount, 1);
}

void TestDocumentSessionDirectMediaNavigationRuntime::replacementCancellationCanDestroyRuntime()
{
    auto receiver = std::make_unique<QObject>();
    const QPointer<QObject> guardedReceiver(receiver.get());
    ManualDirectMediaNavigationCandidateProvider provider;
    const QUrl firstUrl = localUrl(QStringLiteral("/first/01.mp4"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/replacement/01.mp4"));
    using Runtime = kiriview::DocumentSessionDirectMediaNavigationRuntime;
    std::unique_ptr<Runtime> runtime;
    int completionCount = 0;
    bool destroyed = false;
    provider.setCancelHook([&]() {
        if (std::exchange(destroyed, true)) {
            return;
        }
        runtime.reset();
        receiver.reset();
    });
    runtime = std::make_unique<Runtime>(provider.provider());
    runtime->loadCandidates(
        guardedReceiver, directMediaScope(firstUrl),
        [](const kiriview::DirectMediaScope&) { return true; },
        [&](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) { ++completionCount; });

    Runtime* executingRuntime = runtime.get();
    executingRuntime->loadCandidates(
        guardedReceiver, directMediaScope(replacementUrl),
        [](const kiriview::DirectMediaScope&) { return true; },
        [&](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) { ++completionCount; });

    QVERIFY(destroyed);
    QVERIFY(runtime == nullptr);
    QVERIFY(guardedReceiver == nullptr);
    QCOMPARE(provider.loadCount(), std::size_t(1));
    QCOMPARE(completionCount, 0);
}

void TestDocumentSessionDirectMediaNavigationRuntime::synchronousCompletionPreservesReplacementJob()
{
    QObject receiver;
    ManualDirectMediaNavigationCandidateProvider provider;
    const QUrl firstUrl = localUrl(QStringLiteral("/first/01.mp4"));
    const QUrl secondUrl = localUrl(QStringLiteral("/second/01.mp4"));
    provider.synchronouslyCompleteFirstWith({ directMediaNavigationCandidate(firstUrl) });
    kiriview::DocumentSessionDirectMediaNavigationRuntime runtime(provider.provider());
    int firstCompletionCount = 0;
    runtime.loadCandidates(
        &receiver, directMediaScope(firstUrl),
        [](const kiriview::DirectMediaScope&) { return true; },
        [&](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) {
            ++firstCompletionCount;
            runtime.loadCandidates(
                &receiver, directMediaScope(secondUrl),
                [](const kiriview::DirectMediaScope&) { return true; },
                [](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) {});
        });

    QCOMPARE(firstCompletionCount, 1);
    QCOMPARE(provider.loadCount(), std::size_t(2));
    QVERIFY(!provider.loadAt(1).canceled);
}

void TestDocumentSessionDirectMediaNavigationRuntime::predicateReentryRejectsSupersededCompletion()
{
    RuntimeFixture fixture;
    const QUrl firstUrl = localUrl(QStringLiteral("/first/01.mp4"));
    const QUrl secondUrl = localUrl(QStringLiteral("/second/01.mp4"));
    bool reentered = false;
    fixture.runtime.loadCandidates(
        &fixture.receiver, directMediaScope(firstUrl),
        [&](const kiriview::DirectMediaScope&) {
            if (!reentered) {
                reentered = true;
                fixture.load(directMediaScope(secondUrl));
            }
            return true;
        },
        [&](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) {
            ++fixture.completionCount;
        });

    fixture.provider.deliverIgnoringCancellation(0, { directMediaNavigationCandidate(firstUrl) });

    QVERIFY(reentered);
    QCOMPARE(fixture.provider.loadCount(), std::size_t(2));
    QCOMPARE(fixture.completionCount, 0);
    QVERIFY(!fixture.provider.loadAt(1).canceled);
}

void TestDocumentSessionDirectMediaNavigationRuntime::destructionRejectsLateProviderDelivery_data()
{
    QTest::addColumn<bool>("deliverError");

    QTest::newRow("success") << false;
    QTest::newRow("error") << true;
}

void TestDocumentSessionDirectMediaNavigationRuntime::destructionRejectsLateProviderDelivery()
{
    QFETCH(bool, deliverError);

    QObject receiver;
    ManualDirectMediaNavigationCandidateProvider provider;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const kiriview::DirectMediaScope scope = directMediaScope(currentUrl);
    using Runtime = kiriview::DocumentSessionDirectMediaNavigationRuntime;
    std::optional<Runtime> runtime;
    int retiredCompletionCount = 0;
    int currentCompletionCount = 0;

    runtime.emplace(provider.provider());
    runtime->loadCandidates(
        &receiver, scope, [](const kiriview::DirectMediaScope&) { return true; },
        [&](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) {
            ++retiredCompletionCount;
        });
    runtime.reset();
    QVERIFY(provider.loadAt(0).canceled);

    runtime.emplace(provider.provider());
    runtime->loadCandidates(
        &receiver, scope, [](const kiriview::DirectMediaScope&) { return true; },
        [&](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult) {
            ++currentCompletionCount;
        });

    if (deliverError) {
        provider.deliverErrorIgnoringCancellation(0, QStringLiteral("retired load failed"));
    } else {
        provider.deliverIgnoringCancellation(0, { directMediaNavigationCandidate(currentUrl) });
    }

    QCOMPARE(retiredCompletionCount, 0);
    QCOMPARE(currentCompletionCount, 0);
    QVERIFY(!provider.loadAt(1).canceled);

    provider.deliverIgnoringCancellation(1, { directMediaNavigationCandidate(currentUrl) });

    QCOMPARE(retiredCompletionCount, 0);
    QCOMPARE(currentCompletionCount, 1);
}

void TestDocumentSessionDirectMediaNavigationRuntime::
    acceptedScopeAllowsEquivalentCursorConfirmation()
{
    RuntimeFixture fixture;
    const QUrl currentUrl(QStringLiteral("file:///media/./01.mp4"));
    const QUrl confirmedUrl = localUrl(QStringLiteral("/media/01.mp4"));

    fixture.runtime.loadCandidates(
        &fixture.receiver, directMediaScope(currentUrl),
        [confirmedUrl](const kiriview::DirectMediaScope& scope) {
            return kiriview::sameNormalizedUrl(scope.currentUrl(), confirmedUrl)
                && scope.generation() == 7;
        },
        [&fixture](kiriview::DocumentSessionDirectMediaNavigationCandidatesResult loadResult) {
            ++fixture.completionCount;
            fixture.result = std::move(loadResult);
        });
    fixture.provider.deliverIgnoringCancellation(
        0, { directMediaNavigationCandidate(confirmedUrl) });

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(fixture.result.succeeded);
}

QTEST_GUILESS_MAIN(TestDocumentSessionDirectMediaNavigationRuntime)

#include "tst_documentsessiondirectmedianavigationruntime.moc"
