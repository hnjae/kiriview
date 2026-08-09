// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessiondirectmedianavigationcoordinator.h"

#include "image_async_test_support.h"

#include <QObject>
#include <QTest>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class TestDocumentSessionDirectMediaNavigationCoordinator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void inactiveRefreshAppliesInactiveNavigationWithoutLoadingCandidates();
    void inactiveRefreshCancelsExistingCandidateWatch();
    void missingScopeRefreshCancelsExistingCandidateWatch();
    void activeRefreshLoadsCandidatesAndAppliesNavigation();
    void initialMissingCurrentWithoutConfirmedBaselineKeepsMediaOpen();
    void confirmedCurrentMissingFromInitialRefreshRecovers();
    void candidateAdditionRepublishesLiveNavigation();
    void currentRemovalRecoversToNextCandidateAndStopsWatch();
    void candidateWatchFailureMakesNavigationUnavailableWithoutRecovery();
    void boundaryOpenRebindsCandidateWatchAndAcceptsChanges();
    void confirmedCurrentMissingFromOpenRecovers();
    void activeOpenRoutesTargetUsingCurrentCursor();
    void activeOpenStopsWhenPublicationReplacesOriginatingScope();
    void activeOpenStopsWhenPublicationStartsNewDocumentTransition();
    void activeOpenStopsWhenPublicationStartsNewNavigationRequest();
    void cancellationDuringOpenPublicationStopsContinuation();
    void cancellationBarrierRejectsNavigationStartedByCanceledLoad();
    void cancellationCanDestroyCoordinator();
    void refreshPreflightReentrantOpenPreservesNewerLoad();
    void refreshPreflightCaptureCanDestroyCoordinatorBeforeLoadStarts();
};

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(const QUrl& url)
{
    return kiriview::DirectMediaNavigationCandidate { url, url.fileName(QUrl::PrettyDecoded) };
}

kiriview::DirectMediaScope directMediaScope(const QUrl& currentUrl)
{
    return *kiriview::DirectMediaScope::fromSource(
        kiriview::ResolvedNavigationSource(currentUrl, {}, currentUrl), 7);
}

kiriview::ActiveNavigationSnapshot knownActiveNavigation(int currentNumber, int count)
{
    return kiriview::ActiveNavigationSnapshot {
        true,
        true,
        true,
        currentNumber > 1,
        currentNumber < count,
        currentNumber == 1,
        currentNumber == count,
        currentNumber,
        count,
    };
}

struct ManualDirectMediaNavigationCandidateLoad
{
    QObject* object = nullptr;
    QUrl parentUrl;
    kiriview::DirectMediaNavigationCandidatesCallback callback;
    kiriview::KioOperationFailureCallback errorCallback;
    kiriview::ImageIoJobCompletion completion;
    bool canceled = false;
};

struct ManualDirectMediaNavigationCandidateSubscription
{
    QObject* object = nullptr;
    QUrl parentUrl;
    kiriview::DirectMediaNavigationCandidatesCallback callback;
    kiriview::KioOperationFailureCallback errorCallback;
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
                kiriview::KioOperationFailureCallback errorCallback) {
                auto load = std::make_shared<ManualDirectMediaNavigationCandidateLoad>();
                load->parentUrl = std::move(parentUrl);
                load->callback = std::move(callback);
                load->errorCallback = std::move(errorCallback);
                m_loads.push_back(load);
                return kiriview::TestSupport::Detail::startManualIoJob(
                    receiver, load, m_cancelHook);
            },
            [this](QObject* receiver, QUrl parentUrl,
                kiriview::DirectMediaNavigationCandidatesCallback callback,
                kiriview::KioOperationFailureCallback errorCallback) {
                auto subscription
                    = std::make_shared<ManualDirectMediaNavigationCandidateSubscription>();
                subscription->parentUrl = std::move(parentUrl);
                subscription->callback = std::move(callback);
                subscription->errorCallback = std::move(errorCallback);
                m_subscriptions.push_back(subscription);
                return kiriview::TestSupport::Detail::startManualIoJob(receiver, subscription);
            },
        };
    }

    std::size_t loadCount() const { return m_loads.size(); }

    ManualDirectMediaNavigationCandidateLoad& loadAt(std::size_t index)
    {
        return *m_loads.at(index);
    }

    std::size_t subscriptionCount() const { return m_subscriptions.size(); }

    ManualDirectMediaNavigationCandidateSubscription& subscriptionAt(std::size_t index)
    {
        return *m_subscriptions.at(index);
    }

    void deliver(
        std::size_t index, std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
    {
        ManualDirectMediaNavigationCandidateLoad& load = loadAt(index);
        if (load.callback) {
            load.callback(std::move(candidates));
        }
    }

    void deliverChange(
        std::size_t index, std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
    {
        ManualDirectMediaNavigationCandidateSubscription& subscription = subscriptionAt(index);
        if (subscription.callback) {
            subscription.callback(std::move(candidates));
        }
    }

    void deliverWatchError(std::size_t index, kiriview::KioOperationFailure failure)
    {
        ManualDirectMediaNavigationCandidateSubscription& subscription = subscriptionAt(index);
        if (subscription.errorCallback) {
            subscription.errorCallback(std::move(failure));
        }
    }

private:
    std::vector<std::shared_ptr<ManualDirectMediaNavigationCandidateLoad>> m_loads;
    std::vector<std::shared_ptr<ManualDirectMediaNavigationCandidateSubscription>> m_subscriptions;
    std::function<void()> m_cancelHook;
};

struct AppliedNavigation
{
    kiriview::DirectMediaNavigationBoundaryState state;
    bool known = false;
    std::vector<kiriview::DirectMediaNavigationCandidate> candidates;
};

struct CoordinatorFixture
{
    enum class Event {
        SetNavigation,
        Reveal,
        Publish,
        Predecode,
        Route,
        Recover,
    };

    QObject receiver;
    ManualDirectMediaNavigationCandidateProvider provider;
    bool navigationActive = true;
    bool scopeAvailable = true;
    quint64 documentTransitionRevision = 1;
    kiriview::DirectMediaScope scope = directMediaScope(localUrl(QStringLiteral("/media/02.png")));
    QUrl activeCursorUrl = localUrl(QStringLiteral("/media/02.png"));
    kiriview::ActiveNavigationSourceKind activeNavigationSourceKind
        = kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia;
    kiriview::ActiveNavigationSnapshot activeNavigation = knownActiveNavigation(2, 3);
    std::vector<Event> events;
    AppliedNavigation navigation;
    kiriview::DocumentSessionDirectMediaNavigationRevealAction revealAction
        = kiriview::DocumentSessionDirectMediaNavigationRevealAction::None;
    QUrl predecodeTargetUrl;
    QUrl routeTargetUrl;
    std::optional<QUrl> recoveryTargetUrl;
    std::function<bool()> recoveryCurrent;
    std::function<void()> onPublish;
    std::function<void()> onCaptureRefreshTransitionCurrent;
    std::unique_ptr<kiriview::DocumentSessionDirectMediaNavigationCoordinator> coordinator;

    CoordinatorFixture()
    {
        kiriview::DocumentSessionDirectMediaNavigationCoordinatorPorts ports;
        ports.navigationActive = [this]() { return navigationActive; };
        ports.currentScope = [this]() -> std::optional<kiriview::DirectMediaScope> {
            return scopeAvailable ? std::optional<kiriview::DirectMediaScope>(scope) : std::nullopt;
        };
        ports.cursorMatches = [this](const kiriview::DirectMediaScope& candidateScope) {
            return candidateScope.currentUrl() == scope.currentUrl()
                && candidateScope.parentUrl() == scope.parentUrl()
                && candidateScope.generation() == scope.generation();
        };
        const auto captureTransitionCurrent = [this]() {
            const quint64 capturedRevision = documentTransitionRevision;
            return [this, capturedRevision]() {
                return documentTransitionRevision == capturedRevision;
            };
        };
        ports.captureRefreshTransitionCurrent = [this, captureTransitionCurrent]() {
            if (onCaptureRefreshTransitionCurrent) {
                onCaptureRefreshTransitionCurrent();
            }
            return captureTransitionCurrent();
        };
        ports.captureOpenTransitionCurrent = captureTransitionCurrent;
        ports.activeCursorUrl = [this]() { return activeCursorUrl; };
        ports.activeNavigationSourceKind = [this]() { return activeNavigationSourceKind; };
        ports.activeNavigationSnapshot = [this]() { return activeNavigation; };
        ports.setDirectMediaNavigation
            = [this](kiriview::DirectMediaNavigationBoundaryState state, bool known,
                  std::vector<kiriview::DirectMediaNavigationCandidate> candidates) {
                  events.push_back(Event::SetNavigation);
                  navigation = AppliedNavigation { state, known, std::move(candidates) };
              };
        ports.applyRevealAction
            = [this](kiriview::DocumentSessionDirectMediaNavigationRevealAction action) {
                  events.push_back(Event::Reveal);
                  revealAction = action;
              };
        ports.recomputePublicProjection = [this]() {
            events.push_back(Event::Publish);
            if (onPublish) {
                onPublish();
            }
        };
        ports.schedulePredecode = [this](const QUrl& targetUrl) {
            events.push_back(Event::Predecode);
            predecodeTargetUrl = targetUrl;
        };
        ports.openMediaUrl = [this](const QUrl& url, std::function<bool()>) {
            events.push_back(Event::Route);
            routeTargetUrl = url;
        };
        ports.recoverRemovedDirectMedia
            = [this](std::optional<QUrl> url, std::function<bool()> current) {
                  events.push_back(Event::Recover);
                  recoveryTargetUrl = std::move(url);
                  recoveryCurrent = std::move(current);
              };
        coordinator = std::make_unique<kiriview::DocumentSessionDirectMediaNavigationCoordinator>(
            provider.provider(), std::move(ports));
    }
};
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    inactiveRefreshAppliesInactiveNavigationWithoutLoadingCandidates()
{
    CoordinatorFixture fixture;
    fixture.navigationActive = false;

    fixture.coordinator->refresh(&fixture.receiver);

    QCOMPARE(fixture.provider.loadCount(), std::size_t(0));
    QVERIFY(!fixture.navigation.known);
    QVERIFY(fixture.navigation.candidates.empty());
    QCOMPARE(fixture.revealAction,
        kiriview::DocumentSessionDirectMediaNavigationRevealAction::ProgrammaticSync);
    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::SetNavigation,
            CoordinatorFixture::Event::Reveal, CoordinatorFixture::Event::Publish }));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    inactiveRefreshCancelsExistingCandidateWatch()
{
    CoordinatorFixture fixture;
    fixture.coordinator->refresh(&fixture.receiver);
    QCOMPARE(fixture.provider.subscriptionCount(), std::size_t(1));

    fixture.navigationActive = false;
    fixture.coordinator->refresh(&fixture.receiver);

    QVERIFY(fixture.provider.subscriptionAt(0).canceled);
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    missingScopeRefreshCancelsExistingCandidateWatch()
{
    CoordinatorFixture fixture;
    fixture.coordinator->refresh(&fixture.receiver);
    QCOMPARE(fixture.provider.subscriptionCount(), std::size_t(1));

    fixture.scopeAvailable = false;
    fixture.coordinator->refresh(&fixture.receiver);

    QVERIFY(fixture.provider.subscriptionAt(0).canceled);
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    activeRefreshLoadsCandidatesAndAppliesNavigation()
{
    CoordinatorFixture fixture;
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    fixture.coordinator->refresh(&fixture.receiver);
    fixture.provider.deliver(0,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(currentUrl),
            directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.provider.loadAt(0).parentUrl, localUrl(QStringLiteral("/media/")));
    QVERIFY(fixture.navigation.known);
    QCOMPARE(fixture.navigation.state.currentNumber, 2);
    QCOMPARE(fixture.navigation.state.count, 3);
    QVERIFY(fixture.predecodeTargetUrl.isEmpty());
    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::SetNavigation,
            CoordinatorFixture::Event::Reveal, CoordinatorFixture::Event::Publish,
            CoordinatorFixture::Event::Predecode }));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    initialMissingCurrentWithoutConfirmedBaselineKeepsMediaOpen()
{
    CoordinatorFixture fixture;
    fixture.activeNavigation = kiriview::ActiveNavigationSnapshot { true, false, false, false,
        false, false, false, 0, 0 };
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    fixture.coordinator->refresh(&fixture.receiver);
    fixture.provider.deliver(0,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(nextUrl) });

    QVERIFY(!fixture.recoveryTargetUrl.has_value());
    QVERIFY(fixture.routeTargetUrl.isEmpty());
    QVERIFY(!fixture.navigation.known);
    QVERIFY(fixture.navigation.candidates.empty());
    QCOMPARE(fixture.navigation.state.currentNumber, 0);
    QCOMPARE(fixture.navigation.state.count, 0);
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    confirmedCurrentMissingFromInitialRefreshRecovers()
{
    CoordinatorFixture fixture;
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    fixture.coordinator->refresh(&fixture.receiver);
    fixture.provider.deliver(0,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(nextUrl) });

    QVERIFY(fixture.recoveryTargetUrl.has_value());
    QCOMPARE(*fixture.recoveryTargetUrl, nextUrl);
    QVERIFY(fixture.provider.subscriptionAt(0).canceled);
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    candidateAdditionRepublishesLiveNavigation()
{
    CoordinatorFixture fixture;
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));
    const QUrl addedUrl = localUrl(QStringLiteral("/media/04.png"));

    fixture.coordinator->refresh(&fixture.receiver);
    fixture.provider.deliver(0,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(currentUrl),
            directMediaNavigationCandidate(nextUrl) });
    fixture.events.clear();
    fixture.provider.deliverChange(0,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(currentUrl),
            directMediaNavigationCandidate(nextUrl), directMediaNavigationCandidate(addedUrl) });

    QVERIFY(fixture.navigation.known);
    QCOMPARE(fixture.navigation.state.currentNumber, 2);
    QCOMPARE(fixture.navigation.state.count, 4);
    QCOMPARE(fixture.navigation.candidates.size(), std::size_t(4));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    currentRemovalRecoversToNextCandidateAndStopsWatch()
{
    CoordinatorFixture fixture;
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    fixture.coordinator->refresh(&fixture.receiver);
    fixture.provider.deliver(0,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(currentUrl),
            directMediaNavigationCandidate(nextUrl) });
    fixture.events.clear();
    fixture.provider.deliverChange(0,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::Recover }));
    QVERIFY(fixture.recoveryTargetUrl.has_value());
    QCOMPARE(*fixture.recoveryTargetUrl, nextUrl);
    QVERIFY(fixture.recoveryCurrent);
    QVERIFY(fixture.recoveryCurrent());
    QVERIFY(fixture.provider.subscriptionAt(0).canceled);

    fixture.provider.deliverChange(0, { directMediaNavigationCandidate(previousUrl) });
    QCOMPARE(fixture.events.size(), std::size_t(1));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    candidateWatchFailureMakesNavigationUnavailableWithoutRecovery()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    fixture.coordinator->refresh(&fixture.receiver);
    fixture.provider.deliver(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });
    fixture.events.clear();
    fixture.provider.deliverWatchError(0,
        kiriview::KioOperationFailure { kiriview::KioOperationKind::DirectoryListing,
            localUrl(QStringLiteral("/media/")), 73, false, QStringLiteral("watch failed"),
            QStringLiteral("watch failed"), true });

    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::SetNavigation,
            CoordinatorFixture::Event::Reveal, CoordinatorFixture::Event::Publish }));
    QVERIFY(!fixture.navigation.known);
    QCOMPARE(fixture.navigation.state.count, 0);
    QVERIFY(fixture.navigation.candidates.empty());
    QVERIFY(!fixture.recoveryTargetUrl.has_value());
    QVERIFY(fixture.routeTargetUrl.isEmpty());
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    boundaryOpenRebindsCandidateWatchAndAcceptsChanges()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl addedUrl = localUrl(QStringLiteral("/media/03.png"));
    fixture.coordinator->refresh(&fixture.receiver);
    QCOMPARE(fixture.provider.subscriptionCount(), std::size_t(1));
    fixture.provider.deliver(0, { directMediaNavigationCandidate(currentUrl) });

    fixture.coordinator->openNext(&fixture.receiver);
    QVERIFY(fixture.provider.subscriptionAt(0).canceled);
    QCOMPARE(fixture.provider.loadCount(), std::size_t(2));

    fixture.provider.deliver(1, { directMediaNavigationCandidate(currentUrl) });
    QCOMPARE(fixture.provider.subscriptionCount(), std::size_t(2));
    QCOMPARE(fixture.provider.loadCount(), std::size_t(3));
    QVERIFY(!fixture.provider.subscriptionAt(1).canceled);
    QVERIFY(fixture.routeTargetUrl.isEmpty());

    fixture.events.clear();
    fixture.provider.deliverChange(1,
        { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(addedUrl) });

    QVERIFY(fixture.navigation.known);
    QCOMPARE(fixture.navigation.state.currentNumber, 1);
    QCOMPARE(fixture.navigation.state.count, 2);
    QCOMPARE(fixture.navigation.candidates.size(), std::size_t(2));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::confirmedCurrentMissingFromOpenRecovers()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    fixture.coordinator->refresh(&fixture.receiver);
    fixture.provider.deliver(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });
    fixture.events.clear();

    fixture.coordinator->openNext(&fixture.receiver);
    fixture.provider.deliver(1, { directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::Recover }));
    QVERIFY(fixture.recoveryTargetUrl.has_value());
    QCOMPARE(*fixture.recoveryTargetUrl, nextUrl);
    QVERIFY(fixture.routeTargetUrl.isEmpty());
}

void TestDocumentSessionDirectMediaNavigationCoordinator::activeOpenRoutesTargetUsingCurrentCursor()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    fixture.coordinator->openNext(&fixture.receiver);
    fixture.provider.deliver(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.routeTargetUrl, nextUrl);
    QCOMPARE(fixture.predecodeTargetUrl, nextUrl);
    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::SetNavigation,
            CoordinatorFixture::Event::Reveal, CoordinatorFixture::Event::Publish,
            CoordinatorFixture::Event::Predecode, CoordinatorFixture::Event::Route }));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    activeOpenStopsWhenPublicationReplacesOriginatingScope()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/replacement/01.png"));
    fixture.onPublish = [&fixture, replacementUrl]() {
        fixture.scope = directMediaScope(replacementUrl);
        fixture.activeCursorUrl = replacementUrl;
    };

    fixture.coordinator->openNext(&fixture.receiver);
    fixture.provider.deliver(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });

    QVERIFY(fixture.predecodeTargetUrl.isEmpty());
    QVERIFY(fixture.routeTargetUrl.isEmpty());
    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::SetNavigation,
            CoordinatorFixture::Event::Reveal, CoordinatorFixture::Event::Publish }));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    activeOpenStopsWhenPublicationStartsNewDocumentTransition()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));
    fixture.onPublish = [&fixture]() { ++fixture.documentTransitionRevision; };

    fixture.coordinator->openNext(&fixture.receiver);
    fixture.provider.deliver(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });

    QVERIFY(fixture.predecodeTargetUrl.isEmpty());
    QVERIFY(fixture.routeTargetUrl.isEmpty());
    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::SetNavigation,
            CoordinatorFixture::Event::Reveal, CoordinatorFixture::Event::Publish }));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    activeOpenStopsWhenPublicationStartsNewNavigationRequest()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));
    fixture.onPublish = [&fixture]() { fixture.coordinator->openNext(&fixture.receiver); };

    fixture.coordinator->openNext(&fixture.receiver);
    fixture.provider.deliver(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.provider.loadCount(), std::size_t(2));
    QVERIFY(fixture.predecodeTargetUrl.isEmpty());
    QVERIFY(fixture.routeTargetUrl.isEmpty());
    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::SetNavigation,
            CoordinatorFixture::Event::Reveal, CoordinatorFixture::Event::Publish }));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    cancellationDuringOpenPublicationStopsContinuation()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));
    std::function<bool()> cancellationCurrent;
    fixture.onPublish
        = [&]() { cancellationCurrent = fixture.coordinator->cancelAndCaptureCurrent(); };

    fixture.coordinator->openNext(&fixture.receiver);
    fixture.provider.deliver(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });

    QVERIFY(cancellationCurrent);
    QVERIFY(cancellationCurrent());
    QVERIFY(fixture.predecodeTargetUrl.isEmpty());
    QVERIFY(fixture.routeTargetUrl.isEmpty());
    QCOMPARE(fixture.events,
        (std::vector<CoordinatorFixture::Event> { CoordinatorFixture::Event::SetNavigation,
            CoordinatorFixture::Event::Reveal, CoordinatorFixture::Event::Publish }));
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    cancellationBarrierRejectsNavigationStartedByCanceledLoad()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));
    bool reentered = false;
    fixture.provider.setCancelHook([&]() {
        if (std::exchange(reentered, true)) {
            return;
        }
        fixture.coordinator->openNext(&fixture.receiver);
    });
    fixture.coordinator->openNext(&fixture.receiver);

    const std::function<bool()> cancellationCurrent
        = fixture.coordinator->cancelAndCaptureCurrent();

    QVERIFY(!cancellationCurrent());
    QVERIFY(reentered);
    QCOMPARE(fixture.provider.loadCount(), std::size_t(2));
    QVERIFY(fixture.provider.loadAt(0).canceled);
    QVERIFY(!fixture.provider.loadAt(1).canceled);

    fixture.provider.deliver(
        1, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.routeTargetUrl, nextUrl);
}

void TestDocumentSessionDirectMediaNavigationCoordinator::cancellationCanDestroyCoordinator()
{
    CoordinatorFixture fixture;
    using Coordinator = kiriview::DocumentSessionDirectMediaNavigationCoordinator;
    fixture.provider.setCancelHook([&]() { fixture.coordinator.reset(); });
    fixture.coordinator->openNext(&fixture.receiver);
    Coordinator* executingCoordinator = fixture.coordinator.get();

    const std::function<bool()> cancellationCurrent
        = executingCoordinator->cancelAndCaptureCurrent();

    QVERIFY(fixture.coordinator == nullptr);
    QVERIFY(!cancellationCurrent());
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    refreshPreflightReentrantOpenPreservesNewerLoad()
{
    CoordinatorFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));
    bool reentered = false;
    fixture.onCaptureRefreshTransitionCurrent = [&]() {
        if (std::exchange(reentered, true)) {
            return;
        }
        fixture.coordinator->openNext(&fixture.receiver);
    };

    fixture.coordinator->refresh(&fixture.receiver);

    QCOMPARE(fixture.provider.loadCount(), std::size_t(1));
    QVERIFY(!fixture.provider.loadAt(0).canceled);

    fixture.provider.deliver(
        0, { directMediaNavigationCandidate(currentUrl), directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.routeTargetUrl, nextUrl);
}

void TestDocumentSessionDirectMediaNavigationCoordinator::
    refreshPreflightCaptureCanDestroyCoordinatorBeforeLoadStarts()
{
    CoordinatorFixture fixture;
    using Coordinator = kiriview::DocumentSessionDirectMediaNavigationCoordinator;
    bool destroyed = false;
    fixture.onCaptureRefreshTransitionCurrent = [&]() {
        destroyed = true;
        fixture.coordinator.reset();
    };
    Coordinator* executingCoordinator = fixture.coordinator.get();

    executingCoordinator->refresh(&fixture.receiver);

    QVERIFY(destroyed);
    QVERIFY(fixture.coordinator == nullptr);
    QCOMPARE(fixture.provider.loadCount(), std::size_t(0));
}

QTEST_GUILESS_MAIN(TestDocumentSessionDirectMediaNavigationCoordinator)
#include "tst_documentsessiondirectmedianavigationcoordinator.moc"
