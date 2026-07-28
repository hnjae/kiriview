// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationactionhost.h"
#include "application/applicationactionruntime.h"
#include "application/applicationactionsourceattachment.h"
#include "facade/kiridocumentsession.h"
#include "session/documentsessiondocumentports.h"

#include <KirigamiActionCollection>
#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QEvent>
#include <QMetaObject>
#include <QObject>
#include <QTest>
#include <QUrl>

#include <functional>
#include <memory>
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
    kiriview::DocumentSessionActionStateSnapshotPort snapshotPort(
        Qt::ConnectionType connectionType = Qt::AutoConnection)
    {
        return kiriview::DocumentSessionActionStateSnapshotPort {
            [this]() { return snapshot; },
            [this, connectionType](QObject* context, std::function<void()> refresh) {
                ++connectCount;
                return std::vector<QMetaObject::Connection> { QObject::connect(
                    this, &FakeDocumentSessionActionStateSource::changed, context,
                    [refresh = std::move(refresh)]() { refresh(); }, connectionType) };
            },
        };
    }

    kiriview::DocumentSessionActionStateSnapshot snapshot;
    int connectCount = 0;

Q_SIGNALS:
    void changed();
};

void deliverQueuedMetaCalls() { QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall); }
}

class TestApplicationActionSourceAttachment : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sessionSnapshotSignalCommitsSnapshotToRuntime();
    void sessionSourceReplacementDisconnectsPreviousSource();
    void sessionSourceReplacementDoesNotReuseRetainedPresentationHistory();
    void destroyedSessionCommitsUnavailableSnapshot();
    void staleQueuedSourceChangeDoesNotRefreshReplacement();
    void queuedSourceChangeAfterAttachmentDestructionIsIgnored();
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

void TestApplicationActionSourceAttachment::
    sessionSourceReplacementDoesNotReuseRetainedPresentationHistory()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionSourceAttachment attachment(runtime, host.object);
    FakeDocumentSessionActionStateSource firstSource;
    firstSource.snapshot.imagePresentationPhase
        = kiriview::ImagePresentationPhase::CurrentAuthoritative;
    firstSource.snapshot.availability.imageReady = true;
    firstSource.snapshot.availability.rightToLeftReadingAvailable = true;
    firstSource.snapshot.imageCollectionControlsVisible = true;
    FakeDocumentSessionActionStateSource secondSource;
    secondSource.snapshot.imagePresentationPhase
        = kiriview::ImagePresentationPhase::RetainedPreviousAuthoritative;
    secondSource.snapshot.imageCollectionControlsVisible = true;

    attachment.setDocumentSessionSnapshotPort(firstSource.snapshotPort());
    QCOMPARE(runtime.imageToolbarPresentationSnapshot().phase,
        kiriview::ImagePresentationPhase::CurrentAuthoritative);
    QVERIFY(runtime.imageToolbarPresentationSnapshot().rightToLeftReading.appearanceEnabled);

    attachment.setDocumentSessionSnapshotPort(secondSource.snapshotPort());

    QCOMPARE(runtime.actionStateRevision(), 2);
    const Actions::ImageToolbarPresentationSnapshot presentation
        = runtime.imageToolbarPresentationSnapshot();
    QCOMPARE(presentation.phase, kiriview::ImagePresentationPhase::Unavailable);
    QVERIFY(presentation.collectionControlsVisible);
    QVERIFY(!presentation.rightToLeftReading.appearanceEnabled);
}

void TestApplicationActionSourceAttachment::destroyedSessionCommitsUnavailableSnapshot()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionSourceAttachment attachment(runtime, host.object);
    auto session = std::make_unique<KiriDocumentSession>();
    session->setSourceUrl(QUrl(QStringLiteral("file:///media/session-destruction.mp4")));
    QVERIFY(session->actionStateSnapshot().videoMode);

    attachment.setDocumentSessionSnapshotPort(session->actionStateSnapshotPort());
    const int revisionBeforeDestruction = runtime.actionStateRevision();
    QVERIFY(runtime.commandRouterInput().videoMode);

    session.reset();

    QCOMPARE(runtime.actionStateRevision(), revisionBeforeDestruction + 1);
    QCOMPARE(runtime.imageToolbarPresentationSnapshot().phase,
        kiriview::ImagePresentationPhase::Unavailable);
    QVERIFY(!runtime.commandRouterInput().videoMode);
    QVERIFY(!runtime.commandRouterInput().imagePannable);
}

void TestApplicationActionSourceAttachment::staleQueuedSourceChangeDoesNotRefreshReplacement()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    Actions::ApplicationActionSourceAttachment attachment(runtime, host.object);
    FakeDocumentSessionActionStateSource firstSource;
    FakeDocumentSessionActionStateSource secondSource;

    attachment.setDocumentSessionSnapshotPort(firstSource.snapshotPort(Qt::QueuedConnection));
    Q_EMIT firstSource.changed();
    attachment.setDocumentSessionSnapshotPort(secondSource.snapshotPort(Qt::QueuedConnection));
    const int revisionAfterReplacement = runtime.actionStateRevision();

    deliverQueuedMetaCalls();

    QCOMPARE(runtime.actionStateRevision(), revisionAfterReplacement);

    secondSource.snapshot.videoMode = true;
    Q_EMIT secondSource.changed();
    deliverQueuedMetaCalls();

    QCOMPARE(runtime.actionStateRevision(), revisionAfterReplacement + 1);
    QVERIFY(runtime.commandRouterInput().videoMode);
}

void TestApplicationActionSourceAttachment::queuedSourceChangeAfterAttachmentDestructionIsIgnored()
{
    FakeApplicationActionHost host;
    Actions::ApplicationActionRuntime runtime(host);
    FakeDocumentSessionActionStateSource source;
    int revisionBeforeDestruction = 0;
    {
        Actions::ApplicationActionSourceAttachment attachment(runtime, host.object);
        attachment.setDocumentSessionSnapshotPort(source.snapshotPort(Qt::QueuedConnection));
        revisionBeforeDestruction = runtime.actionStateRevision();
        Q_EMIT source.changed();
    }

    deliverQueuedMetaCalls();

    QCOMPARE(runtime.actionStateRevision(), revisionBeforeDestruction);
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);
    TestApplicationActionSourceAttachment test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_applicationactionsourceattachment.moc"
