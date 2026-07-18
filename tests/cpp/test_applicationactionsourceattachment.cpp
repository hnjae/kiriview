// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationactionhost.h"
#include "application/applicationactionruntime.h"
#include "application/applicationactionsourceattachment.h"
#include "session/documentsessiondocumentports.h"

#include <KirigamiActionCollection>
#include <QApplication>
#include <QByteArray>
#include <QMetaObject>
#include <QObject>
#include <QTest>

#include <functional>
#include <utility>
#include <vector>

namespace {
namespace Actions = kiriview::ApplicationActions;

class FakeApplicationActionHost final : public Actions::ApplicationActionHost
{
public:
    FakeApplicationActionHost()
        : collection(&object)
    {
    }

    QObject* actionContext() override { return &object; }
    KirigamiActionCollection* mainActionCollection() override { return &collection; }
    QAction* inheritedAction(const QString& actionName) override
    {
        return collection.action(actionName);
    }
    void readActionSettings() override { }

    QObject object;
    KirigamiActionCollection collection;
};

class FakeDocumentSessionActionStateSource final : public QObject
{
    Q_OBJECT

public:
    kiriview::DocumentSessionActionStateSnapshotPort snapshotPort()
    {
        return kiriview::DocumentSessionActionStateSnapshotPort {
            [this]() { return snapshot; },
            [this](QObject* context, std::function<void()> refresh) {
                ++connectCount;
                return std::vector<QMetaObject::Connection> { QObject::connect(this,
                    &FakeDocumentSessionActionStateSource::changed, context,
                    [refresh = std::move(refresh)]() { refresh(); }) };
            },
        };
    }

    kiriview::DocumentSessionActionStateSnapshot snapshot;
    int connectCount = 0;

Q_SIGNALS:
    void changed();
};
}

class TestApplicationActionSourceAttachment : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sessionSnapshotSignalCommitsSnapshotToRuntime();
    void sessionSourceReplacementDisconnectsPreviousSource();
};

void TestApplicationActionSourceAttachment::sessionSnapshotSignalCommitsSnapshotToRuntime()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionSourceAttachment attachment(runtime, host.object);
    FakeDocumentSessionActionStateSource source;

    attachment.setDocumentSessionSnapshotPort(source.snapshotPort());

    QCOMPARE(source.connectCount, 1);
    QCOMPARE(runtime.actionStateRevision(), 1);

    source.snapshot.videoMode = true;
    source.snapshot.imagePannable = true;
    Q_EMIT source.changed();

    const Actions::ApplicationCommandRouterInput input = runtime.commandRouterInput();
    QCOMPARE(runtime.actionStateRevision(), 2);
    QVERIFY(input.videoMode);
    QVERIFY(input.imagePannable);
}

void TestApplicationActionSourceAttachment::sessionSourceReplacementDisconnectsPreviousSource()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionSourceAttachment attachment(runtime, host.object);
    FakeDocumentSessionActionStateSource firstSource;
    FakeDocumentSessionActionStateSource secondSource;

    attachment.setDocumentSessionSnapshotPort(firstSource.snapshotPort());
    attachment.setDocumentSessionSnapshotPort(secondSource.snapshotPort());
    const int revisionAfterReplacement = runtime.actionStateRevision();

    firstSource.snapshot.videoMode = true;
    Q_EMIT firstSource.changed();

    QCOMPARE(runtime.actionStateRevision(), revisionAfterReplacement);

    secondSource.snapshot.videoMode = true;
    Q_EMIT secondSource.changed();

    QCOMPARE(runtime.actionStateRevision(), revisionAfterReplacement + 1);
    QVERIFY(runtime.commandRouterInput().videoMode);
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);
    TestApplicationActionSourceAttachment test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_applicationactionsourceattachment.moc"
