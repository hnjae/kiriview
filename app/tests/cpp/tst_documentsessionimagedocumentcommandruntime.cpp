// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionimagedocumentcommandruntime.h"

#include <QObject>
#include <QTest>
#include <memory>
#include <optional>

class TestDocumentSessionImageDocumentCommandRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void forwardsSourceRoutingThroughPort();
    void forwardsPageNavigationThroughPort();
    void forwardsImageDocumentDeletionThroughPort();
    void reentrantSourceCommandSupersedesOuterCommand();
    void sourceCommandCanDestroyRuntime();
};

namespace {
struct ImageCommandProbe
{
    kiriview::DocumentSessionImageDocumentCommandPort port()
    {
        return kiriview::DocumentSessionImageDocumentCommandPort {
            { [this]() {
                 sourceUrl = QUrl();
                 events.push_back(QStringLiteral("clear-source"));
             },
                {},
                [this](const kiriview::ResolvedNavigationSource& source) {
                    sourceUrl = source.requestedUrl();
                    events.push_back(QStringLiteral("set-source"));
                } },
            { [this]() { events.push_back(QStringLiteral("previous-page")); },
                [this]() { events.push_back(QStringLiteral("next-page")); },
                [this](int number) {
                    openedPageNumber = number;
                    events.push_back(QStringLiteral("open-page"));
                } },
            { [this](kiriview::FileDeletionMode mode) {
                deletionMode = mode;
                events.push_back(QStringLiteral("delete-displayed-file"));
            } },
        };
    }

    QUrl sourceUrl;
    int openedPageNumber = 0;
    std::optional<kiriview::FileDeletionMode> deletionMode;
    QStringList events;
};
}

void TestDocumentSessionImageDocumentCommandRuntime::forwardsSourceRoutingThroughPort()
{
    ImageCommandProbe probe;
    kiriview::DocumentSessionImageDocumentCommandRuntime runtime(probe.port());
    const QUrl imageUrl(QStringLiteral("file:///tmp/image.png"));

    QVERIFY(runtime.setSource(kiriview::resolvedNavigationSource(imageUrl, {})));
    QVERIFY(runtime.clearSourceUrl());

    QCOMPARE(probe.sourceUrl, QUrl());
    QCOMPARE(probe.events,
        QStringList({ QStringLiteral("set-source"), QStringLiteral("clear-source") }));
}

void TestDocumentSessionImageDocumentCommandRuntime::forwardsPageNavigationThroughPort()
{
    ImageCommandProbe probe;
    kiriview::DocumentSessionImageDocumentCommandRuntime runtime(probe.port());

    runtime.openPreviousPage();
    runtime.openNextPage();
    runtime.openImageAtPage(42);

    QCOMPARE(probe.openedPageNumber, 42);
    QCOMPARE(probe.events,
        QStringList({ QStringLiteral("previous-page"), QStringLiteral("next-page"),
            QStringLiteral("open-page") }));
}

void TestDocumentSessionImageDocumentCommandRuntime::forwardsImageDocumentDeletionThroughPort()
{
    ImageCommandProbe probe;
    kiriview::DocumentSessionImageDocumentCommandRuntime runtime(probe.port());

    runtime.deleteDisplayedFile(kiriview::FileDeletionMode::MoveToTrash);

    QVERIFY(probe.deletionMode.has_value());
    QCOMPARE(*probe.deletionMode, kiriview::FileDeletionMode::MoveToTrash);
    QCOMPARE(probe.events, QStringList({ QStringLiteral("delete-displayed-file") }));
}

void TestDocumentSessionImageDocumentCommandRuntime::reentrantSourceCommandSupersedesOuterCommand()
{
    const QUrl outerUrl(QStringLiteral("file:///tmp/outer.png"));
    const QUrl replacementUrl(QStringLiteral("file:///tmp/replacement.png"));
    QStringList events;
    bool replacementAccepted = false;
    kiriview::DocumentSessionImageDocumentCommandRuntime* runtime = nullptr;
    kiriview::DocumentSessionImageDocumentCommandPort commands;
    commands.source.setSource = [&](const kiriview::ResolvedNavigationSource& source) {
        events.push_back(source.requestedUrl().fileName());
        if (source.requestedUrl() == outerUrl) {
            replacementAccepted
                = runtime->setSource(kiriview::resolvedNavigationSource(replacementUrl, {}));
        }
    };
    kiriview::DocumentSessionImageDocumentCommandRuntime ownedRuntime(std::move(commands));
    runtime = &ownedRuntime;

    const bool outerAccepted = runtime->setSource(kiriview::resolvedNavigationSource(outerUrl, {}));

    QVERIFY(replacementAccepted);
    QVERIFY(!outerAccepted);
    QCOMPARE(
        events, QStringList({ QStringLiteral("outer.png"), QStringLiteral("replacement.png") }));
}

void TestDocumentSessionImageDocumentCommandRuntime::sourceCommandCanDestroyRuntime()
{
    std::unique_ptr<kiriview::DocumentSessionImageDocumentCommandRuntime> runtime;
    kiriview::DocumentSessionImageDocumentCommandPort commands;
    commands.source.clearSource = [&]() { runtime.reset(); };
    runtime = std::make_unique<kiriview::DocumentSessionImageDocumentCommandRuntime>(
        std::move(commands));

    kiriview::DocumentSessionImageDocumentCommandRuntime* executingRuntime = runtime.get();
    const bool accepted = executingRuntime->clearSourceUrl();

    QVERIFY(!accepted);
    QVERIFY(runtime == nullptr);
}

QTEST_GUILESS_MAIN(TestDocumentSessionImageDocumentCommandRuntime)

#include "tst_documentsessionimagedocumentcommandruntime.moc"
