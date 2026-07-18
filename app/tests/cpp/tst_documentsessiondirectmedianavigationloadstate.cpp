// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessiondirectmedianavigationloadstate.h"

#include <QObject>
#include <QTest>
#include <QUrl>

class TestDocumentSessionDirectMediaNavigationLoadState : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void onlyCurrentLoadCanFinish();
    void cursorScopeMustMatchCurrentLoad();
    void sourceKeyEquivalentScopeCanFinish();
    void cancelRejectsPendingLoad();
};

namespace {
kiriview::DirectMediaScope directMediaScope(
    const QString& currentPath, const QString& parentPath, quint64 generation)
{
    const QUrl currentUrl = QUrl::fromLocalFile(currentPath);
    Q_UNUSED(parentPath);
    return *kiriview::DirectMediaScope::fromSource(
        kiriview::ResolvedNavigationSource(currentUrl, {}, currentUrl), generation);
}
}

void TestDocumentSessionDirectMediaNavigationLoadState::onlyCurrentLoadCanFinish()
{
    kiriview::DocumentSessionDirectMediaNavigationLoadState state;

    const kiriview::DocumentSessionDirectMediaNavigationLoad stale = state.start(
        directMediaScope(QStringLiteral("/media/01.mp4"), QStringLiteral("/media/"), 1));
    const kiriview::DocumentSessionDirectMediaNavigationLoad current = state.start(
        directMediaScope(QStringLiteral("/media/02.mp4"), QStringLiteral("/media/"), 2));

    QVERIFY(stale.operationId != 0);
    QVERIFY(current.operationId != 0);
    QVERIFY(stale.operationId != current.operationId);
    QVERIFY(!state.accepts(stale));
    QVERIFY(!state.finish(stale));
    QVERIFY(state.accepts(current));
    QVERIFY(state.finish(current));
    QVERIFY(!state.accepts(current));
}

void TestDocumentSessionDirectMediaNavigationLoadState::cursorScopeMustMatchCurrentLoad()
{
    kiriview::DocumentSessionDirectMediaNavigationLoadState state;

    const kiriview::DocumentSessionDirectMediaNavigationLoad first = state.start(
        directMediaScope(QStringLiteral("/first/01.mp4"), QStringLiteral("/first/"), 1));
    const kiriview::DocumentSessionDirectMediaNavigationLoad second = state.start(
        directMediaScope(QStringLiteral("/second/01.mp4"), QStringLiteral("/second/"), 2));

    const kiriview::DocumentSessionDirectMediaNavigationLoad wrongScope {
        second.operationId,
        first.scope,
    };
    const kiriview::DocumentSessionDirectMediaNavigationLoad wrongCurrent {
        second.operationId,
        directMediaScope(QStringLiteral("/second/02.mp4"), QStringLiteral("/second/"), 2),
    };
    const kiriview::DocumentSessionDirectMediaNavigationLoad wrongGeneration {
        second.operationId,
        directMediaScope(QStringLiteral("/second/01.mp4"), QStringLiteral("/second/"), 1),
    };

    QVERIFY(!state.accepts(first));
    QVERIFY(!state.finish(first));
    QVERIFY(!state.accepts(wrongScope));
    QVERIFY(!state.finish(wrongScope));
    QVERIFY(!state.accepts(wrongCurrent));
    QVERIFY(!state.finish(wrongCurrent));
    QVERIFY(!state.accepts(wrongGeneration));
    QVERIFY(!state.finish(wrongGeneration));
    QVERIFY(state.accepts(second));
    QVERIFY(state.finish(second));
}

void TestDocumentSessionDirectMediaNavigationLoadState::sourceKeyEquivalentScopeCanFinish()
{
    kiriview::DocumentSessionDirectMediaNavigationLoadState state;

    kiriview::DocumentSessionDirectMediaNavigationLoad load
        = state.start(*kiriview::DirectMediaScope::fromSource(
            kiriview::ResolvedNavigationSource(
                QUrl(QStringLiteral("file:///media/chapter/../01.mp4")), {},
                QUrl(QStringLiteral("file:///media/01.mp4"))),
            3));

    const kiriview::DocumentSessionDirectMediaNavigationLoad equivalent {
        load.operationId,
        *kiriview::DirectMediaScope::fromSource(
            kiriview::resolvedNavigationSource(QUrl(QStringLiteral("file:///media/01.mp4")), {}),
            3),
    };
    QVERIFY(state.accepts(equivalent));

    const kiriview::DocumentSessionDirectMediaNavigationLoad wrongGeneration {
        load.operationId,
        *kiriview::DirectMediaScope::fromSource(
            kiriview::resolvedNavigationSource(QUrl(QStringLiteral("file:///media/01.mp4")), {}),
            4),
    };
    QVERIFY(!state.accepts(wrongGeneration));
    QVERIFY(state.finish(equivalent));
}

void TestDocumentSessionDirectMediaNavigationLoadState::cancelRejectsPendingLoad()
{
    kiriview::DocumentSessionDirectMediaNavigationLoadState state;
    const kiriview::DocumentSessionDirectMediaNavigationLoad load = state.start(
        directMediaScope(QStringLiteral("/media/01.mp4"), QStringLiteral("/media/"), 1));

    QVERIFY(state.accepts(load));
    state.cancel();
    QVERIFY(!state.accepts(load));
    QVERIFY(!state.finish(load));
}

QTEST_GUILESS_MAIN(TestDocumentSessionDirectMediaNavigationLoadState)

#include "tst_documentsessiondirectmedianavigationloadstate.moc"
