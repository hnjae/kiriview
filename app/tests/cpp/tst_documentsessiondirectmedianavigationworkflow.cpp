// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessiondirectmedianavigationworkflow.h"

#include <QDir>
#include <QObject>
#include <QTest>
#include <QUrl>

class TestDocumentSessionDirectMediaNavigationWorkflow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void failedRefreshClearsNavigationAndRequestsProgrammaticReveal();
    void successfulRefreshWithoutCurrentRemainsUnknown();
    void refreshRequestsRevealWhenSelectionChanges();
    void successfulOpenOfNewTargetKeepsPendingRevealAndRoutes();
    void equivalentSourceKeysDoNotKeepPendingReveal();
    void successfulOpenWithoutTargetClearsPendingReveal();
    void successfulOpenWithoutCurrentRemainsUnknown();
};

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(const QUrl& url)
{
    return kiriview::DirectMediaNavigationCandidate { url, url.fileName(QUrl::PrettyDecoded) };
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
}

void TestDocumentSessionDirectMediaNavigationWorkflow::
    failedRefreshClearsNavigationAndRequestsProgrammaticReveal()
{
    const kiriview::DocumentSessionDirectMediaNavigationRefreshApplication application
        = kiriview::documentSessionDirectMediaNavigationRefreshApplication(
            kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia, knownActiveNavigation(2, 3),
            kiriview::DocumentSessionDirectMediaNavigationRefreshResult {
                {}, {}, false, QStringLiteral("listing failed") });

    QVERIFY(!application.known);
    QVERIFY(application.candidates.empty());
    QCOMPARE(application.revealAction,
        kiriview::DocumentSessionDirectMediaNavigationRevealAction::ProgrammaticSync);
    QVERIFY(!application.schedulePredecode);
}

void TestDocumentSessionDirectMediaNavigationWorkflow::refreshRequestsRevealWhenSelectionChanges()
{
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const std::vector<kiriview::DirectMediaNavigationCandidate> candidates {
        directMediaNavigationCandidate(previousUrl),
        directMediaNavigationCandidate(currentUrl),
    };
    const kiriview::DirectMediaNavigationBoundaryState boundary {
        true,
        false,
        false,
        true,
        2,
        2,
    };

    const kiriview::DocumentSessionDirectMediaNavigationRefreshApplication application
        = kiriview::documentSessionDirectMediaNavigationRefreshApplication(
            kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia, knownActiveNavigation(1, 2),
            kiriview::DocumentSessionDirectMediaNavigationRefreshResult {
                candidates, boundary, true, QString() });

    QVERIFY(application.known);
    QCOMPARE(application.boundaryState.currentNumber, 2);
    QCOMPARE(application.candidates.size(), std::size_t(2));
    QCOMPARE(application.revealAction,
        kiriview::DocumentSessionDirectMediaNavigationRevealAction::UsePendingOrProgrammaticSync);
    QVERIFY(application.schedulePredecode);
}

void TestDocumentSessionDirectMediaNavigationWorkflow::
    successfulRefreshWithoutCurrentRemainsUnknown()
{
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    const kiriview::DocumentSessionDirectMediaNavigationRefreshApplication application
        = kiriview::documentSessionDirectMediaNavigationRefreshApplication(
            kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia, {},
            kiriview::DocumentSessionDirectMediaNavigationRefreshResult {
                { directMediaNavigationCandidate(nextUrl) },
                kiriview::directMediaNavigationBoundaryState(
                    { directMediaNavigationCandidate(nextUrl) }, currentUrl),
                true,
                QString(),
            });

    QVERIFY(!application.known);
    QVERIFY(application.candidates.empty());
    QCOMPARE(application.revealAction,
        kiriview::DocumentSessionDirectMediaNavigationRevealAction::ProgrammaticSync);
    QVERIFY(!application.schedulePredecode);
}

void TestDocumentSessionDirectMediaNavigationWorkflow::
    successfulOpenOfNewTargetKeepsPendingRevealAndRoutes()
{
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/02.png"));
    const std::vector<kiriview::DirectMediaNavigationCandidate> candidates {
        directMediaNavigationCandidate(currentUrl),
        directMediaNavigationCandidate(nextUrl),
    };

    const kiriview::DocumentSessionDirectMediaNavigationOpenApplication application
        = kiriview::documentSessionDirectMediaNavigationOpenApplication(currentUrl,
            kiriview::DocumentSessionDirectMediaNavigationOpenResult {
                candidates,
                kiriview::DirectMediaNavigationOpenPlan {
                    kiriview::DirectMediaNavigationBoundaryState {
                        false,
                        true,
                        true,
                        false,
                        1,
                        2,
                    },
                    nextUrl,
                },
                true,
                QString(),
            });

    QVERIFY(application.known);
    QCOMPARE(application.routeTargetUrl.value_or(QUrl()), nextUrl);
    QCOMPARE(application.revealAction,
        kiriview::DocumentSessionDirectMediaNavigationRevealAction::
            UsePendingOrProgrammaticSyncAndKeepPending);
    QVERIFY(application.schedulePredecode);
}

void TestDocumentSessionDirectMediaNavigationWorkflow::equivalentSourceKeysDoNotKeepPendingReveal()
{
    const QString relativePath = QStringLiteral("relative/media/01.png");
    const QUrl relativeUrl = QUrl::fromLocalFile(relativePath);
    const QUrl absoluteUrl = QUrl::fromLocalFile(QDir::current().absoluteFilePath(relativePath));
    const auto applicationForTarget = [&absoluteUrl](QUrl targetUrl) {
        return kiriview::documentSessionDirectMediaNavigationOpenApplication(absoluteUrl,
            kiriview::DocumentSessionDirectMediaNavigationOpenResult {
                { directMediaNavigationCandidate(targetUrl) },
                kiriview::DirectMediaNavigationOpenPlan {
                    kiriview::DirectMediaNavigationBoundaryState {
                        false,
                        false,
                        true,
                        true,
                        1,
                        1,
                    },
                    std::move(targetUrl),
                },
                true,
                QString(),
            });
    };

    const kiriview::DocumentSessionDirectMediaNavigationOpenApplication equivalentApplication
        = applicationForTarget(relativeUrl);
    QCOMPARE(equivalentApplication.routeTargetUrl.value_or(QUrl()), relativeUrl);
    QCOMPARE(equivalentApplication.revealAction,
        kiriview::DocumentSessionDirectMediaNavigationRevealAction::Clear);

    QUrl distinctUrl = relativeUrl;
    distinctUrl.setFragment(QStringLiteral("alternate"));
    const kiriview::DocumentSessionDirectMediaNavigationOpenApplication distinctApplication
        = applicationForTarget(distinctUrl);
    QCOMPARE(distinctApplication.revealAction,
        kiriview::DocumentSessionDirectMediaNavigationRevealAction::
            UsePendingOrProgrammaticSyncAndKeepPending);
}

void TestDocumentSessionDirectMediaNavigationWorkflow::
    successfulOpenWithoutTargetClearsPendingReveal()
{
    const QUrl currentUrl = localUrl(QStringLiteral("/media/01.png"));

    const kiriview::DocumentSessionDirectMediaNavigationOpenApplication application
        = kiriview::documentSessionDirectMediaNavigationOpenApplication(currentUrl,
            kiriview::DocumentSessionDirectMediaNavigationOpenResult {
                { directMediaNavigationCandidate(currentUrl) },
                kiriview::DirectMediaNavigationOpenPlan {
                    kiriview::DirectMediaNavigationBoundaryState {
                        false,
                        false,
                        true,
                        true,
                        1,
                        1,
                    },
                    std::nullopt,
                },
                true,
                QString(),
            });

    QVERIFY(application.known);
    QVERIFY(!application.routeTargetUrl.has_value());
    QCOMPARE(application.revealAction,
        kiriview::DocumentSessionDirectMediaNavigationRevealAction::Clear);
    QVERIFY(application.schedulePredecode);
}

void TestDocumentSessionDirectMediaNavigationWorkflow::successfulOpenWithoutCurrentRemainsUnknown()
{
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    const kiriview::DocumentSessionDirectMediaNavigationOpenApplication application
        = kiriview::documentSessionDirectMediaNavigationOpenApplication(currentUrl,
            kiriview::DocumentSessionDirectMediaNavigationOpenResult {
                { directMediaNavigationCandidate(nextUrl) },
                kiriview::DirectMediaNavigationOpenPlan {},
                true,
                QString(),
            });

    QVERIFY(!application.known);
    QVERIFY(application.candidates.empty());
    QVERIFY(!application.routeTargetUrl.has_value());
    QCOMPARE(application.revealAction,
        kiriview::DocumentSessionDirectMediaNavigationRevealAction::ProgrammaticSync);
    QVERIFY(!application.schedulePredecode);
}

QTEST_GUILESS_MAIN(TestDocumentSessionDirectMediaNavigationWorkflow)

#include "tst_documentsessiondirectmedianavigationworkflow.moc"
