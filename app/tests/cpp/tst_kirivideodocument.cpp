// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kirivideodocument.h"

#include "facade/kiridocumentsession.h"
#include <QMetaMethod>
#include <QMetaProperty>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <functional>
#include <memory>

class SignalCallback final : public QObject
{
    Q_OBJECT

public:
    std::function<void()> callback;

public Q_SLOTS:
    void invoke()
    {
        const std::function<void()> currentCallback = callback;
        if (currentCallback) {
            currentCallback();
        }
    }
};

class TestKiriVideoDocument : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initialStateIsNull();
    void sourceUrlAndVideoOutputPropertiesAreReadOnlyObservations();
    void mutedPropertyNotifiesAndToggles();
    void sourceUrlAndTitleNotifyOnSetAndClear();
    void sourceReplacementReentryCoalescesSupersededNotifications();
    void playbackProjectionReentryPreservesDocumentNotifications();
    void playbackProjectionReentryCoalescesSupersededProjection();
    void sessionSnapshotPublicationCanDestroyDocument();
    void videoOutputCanDetachAndToleratesDestroyedOutput();
};

namespace {
QMetaObject::Connection connectSessionSnapshotSignal(
    KiriVideoDocument& document, SignalCallback& callback)
{
    const QMetaObject& documentMetaObject = *document.metaObject();
    const int signalIndex = documentMetaObject.indexOfSignal("documentSessionSnapshotChanged()");
    const int slotIndex = callback.metaObject()->indexOfSlot("invoke()");
    if (signalIndex < 0 || slotIndex < 0) {
        return {};
    }
    return QObject::connect(&document, documentMetaObject.method(signalIndex), &callback,
        callback.metaObject()->method(slotIndex), Qt::DirectConnection);
}
}

void TestKiriVideoDocument::initialStateIsNull()
{
    KiriVideoDocument document;

    QCOMPARE(document.sourceUrl(), QUrl());
    QCOMPARE(document.status(), KiriVideoDocument::Status::Null);
    QCOMPARE(document.errorString(), QString());
    QCOMPARE(document.windowTitleFileName(), QString());
    QCOMPARE(document.duration(), 0);
    QCOMPARE(document.position(), 0);
    QVERIFY(!document.playing());
    QVERIFY(!document.seekable());
    QVERIFY(!document.hasVideo());
    QVERIFY(!document.hasAudio());
    QCOMPARE(document.videoSize(), QSize());
    QVERIFY(!document.zoomPercentKnown());
    QCOMPARE(document.zoomPercent(), 0);
    QVERIFY(!document.muted());
    QCOMPARE(document.videoOutput(), nullptr);
}

void TestKiriVideoDocument::sourceUrlAndVideoOutputPropertiesAreReadOnlyObservations()
{
    const QMetaObject& metaObject = KiriVideoDocument::staticMetaObject;
    const int sourceUrlIndex = metaObject.indexOfProperty("sourceUrl");
    QVERIFY(sourceUrlIndex >= 0);

    const QMetaProperty sourceUrlProperty = metaObject.property(sourceUrlIndex);
    QVERIFY(sourceUrlProperty.hasNotifySignal());
    QVERIFY(!sourceUrlProperty.isWritable());

    const int videoOutputIndex = metaObject.indexOfProperty("videoOutput");
    QVERIFY(videoOutputIndex >= 0);

    const QMetaProperty videoOutputProperty = metaObject.property(videoOutputIndex);
    QVERIFY(videoOutputProperty.hasNotifySignal());
    QVERIFY(!videoOutputProperty.isWritable());

    const int playbackControlsIndex = metaObject.indexOfProperty("playbackControls");
    QVERIFY(playbackControlsIndex >= 0);
    const QMetaProperty playbackControlsProperty = metaObject.property(playbackControlsIndex);
    QVERIFY(playbackControlsProperty.isConstant());
    QVERIFY(!playbackControlsProperty.isWritable());
    QVERIFY(metaObject.indexOfProperty("duration") < 0);
    QVERIFY(metaObject.indexOfProperty("position") < 0);
    QVERIFY(metaObject.indexOfProperty("playing") < 0);
    QVERIFY(metaObject.indexOfProperty("seekable") < 0);
    QVERIFY(metaObject.indexOfProperty("muted") < 0);
}

void TestKiriVideoDocument::mutedPropertyNotifiesAndToggles()
{
    KiriVideoDocument document;
    QSignalSpy projectionSpy(
        document.playbackControls(), &KiriVideoPlaybackControls::projectionChanged);

    document.setMuted(true);
    QVERIFY(document.muted());
    QVERIFY(document.playbackControls()->muted());
    QCOMPARE(projectionSpy.count(), 1);

    document.setMuted(true);
    QCOMPARE(projectionSpy.count(), 1);

    document.toggleMuted();
    QVERIFY(!document.muted());
    QVERIFY(!document.playbackControls()->muted());
    QCOMPARE(projectionSpy.count(), 2);
}

void TestKiriVideoDocument::sourceUrlAndTitleNotifyOnSetAndClear()
{
    KiriDocumentSession session;
    KiriVideoDocument& document = *session.videoDocument();
    QSignalSpy sourceUrlSpy(&document, &KiriVideoDocument::sourceUrlChanged);
    QSignalSpy titleSpy(&document, &KiriVideoDocument::windowTitleFileNameChanged);
    QSignalSpy statusSpy(&document, &KiriVideoDocument::statusChanged);
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/clip.mp4"));

    session.setSourceUrl(sourceUrl);
    QCOMPARE(document.sourceUrl(), sourceUrl);
    QCOMPARE(document.windowTitleFileName(), QStringLiteral("clip.mp4"));
    QCOMPARE(document.status(), KiriVideoDocument::Status::Loading);
    QCOMPARE(sourceUrlSpy.count(), 1);
    QCOMPARE(titleSpy.count(), 1);
    QVERIFY(statusSpy.count() >= 1);

    session.setSourceUrl(QUrl());
    QCOMPARE(document.sourceUrl(), QUrl());
    QCOMPARE(document.windowTitleFileName(), QString());
    QCOMPARE(document.status(), KiriVideoDocument::Status::Null);
    QCOMPARE(sourceUrlSpy.count(), 2);
    QCOMPARE(titleSpy.count(), 2);
}

void TestKiriVideoDocument::sourceReplacementReentryCoalescesSupersededNotifications()
{
    KiriDocumentSession session;
    KiriVideoDocument& document = *session.videoDocument();
    const QUrl firstSourceUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/first.mp4"));
    const QUrl replacementSourceUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/replacement.mp4"));
    QStringList notifiedTitles;
    int statusNotificationCount = 0;
    bool reentered = false;

    connect(&document, &KiriVideoDocument::sourceUrlChanged, &document, [&]() {
        if (reentered) {
            return;
        }
        reentered = true;
        session.setSourceUrl(replacementSourceUrl);
    });
    connect(&document, &KiriVideoDocument::windowTitleFileNameChanged, &document,
        [&]() { notifiedTitles.append(document.windowTitleFileName()); });
    connect(&document, &KiriVideoDocument::statusChanged, &document,
        [&]() { ++statusNotificationCount; });

    session.setSourceUrl(firstSourceUrl);

    QVERIFY(reentered);
    QCOMPARE(document.sourceUrl(), replacementSourceUrl);
    QCOMPARE(document.status(), KiriVideoDocument::Status::Loading);
    QCOMPARE(notifiedTitles, QStringList { QStringLiteral("replacement.mp4") });
    QCOMPARE(statusNotificationCount, 1);
}

void TestKiriVideoDocument::playbackProjectionReentryPreservesDocumentNotifications()
{
    KiriDocumentSession session;
    KiriVideoDocument& document = *session.videoDocument();
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/clip.mp4"));
    QSignalSpy titleSpy(&document, &KiriVideoDocument::windowTitleFileNameChanged);
    QSignalSpy statusSpy(&document, &KiriVideoDocument::statusChanged);
    bool reentered = false;

    connect(&document, &KiriVideoDocument::sourceUrlChanged, &document, [&]() {
        if (reentered) {
            return;
        }
        reentered = true;
        document.setMuted(true);
    });

    session.setSourceUrl(sourceUrl);

    QVERIFY(reentered);
    QVERIFY(document.muted());
    QCOMPARE(document.windowTitleFileName(), QStringLiteral("clip.mp4"));
    QCOMPARE(document.status(), KiriVideoDocument::Status::Loading);
    QCOMPARE(titleSpy.count(), 1);
    QCOMPARE(statusSpy.count(), 1);
}

void TestKiriVideoDocument::playbackProjectionReentryCoalescesSupersededProjection()
{
    KiriVideoDocument document;
    SignalCallback sessionSnapshotCallback;
    bool reentered = false;
    sessionSnapshotCallback.callback = [&]() {
        if (reentered) {
            return;
        }
        reentered = true;
        document.setMuted(false);
    };
    const QMetaObject::Connection connection
        = connectSessionSnapshotSignal(document, sessionSnapshotCallback);
    QVERIFY(connection);
    QSignalSpy projectionSpy(
        document.playbackControls(), &KiriVideoPlaybackControls::projectionChanged);

    document.setMuted(true);

    QVERIFY(reentered);
    QVERIFY(!document.muted());
    QCOMPARE(projectionSpy.count(), 1);
}

void TestKiriVideoDocument::sessionSnapshotPublicationCanDestroyDocument()
{
    std::unique_ptr<KiriVideoDocument> document = std::make_unique<KiriVideoDocument>();
    QPointer<KiriVideoDocument> documentGuard(document.get());
    KiriVideoPlaybackControls* playbackControls = document->playbackControls();
    // Keep the signal target alive only so a post-owner emission remains observable.
    playbackControls->setParent(nullptr);
    const std::unique_ptr<KiriVideoPlaybackControls> playbackControlsOwner(playbackControls);
    QSignalSpy projectionSpy(playbackControls, &KiriVideoPlaybackControls::projectionChanged);
    SignalCallback sessionSnapshotCallback;
    bool destroyed = false;
    sessionSnapshotCallback.callback = [&]() {
        destroyed = true;
        document.reset();
    };
    const QMetaObject::Connection connection
        = connectSessionSnapshotSignal(*document, sessionSnapshotCallback);
    QVERIFY(connection);
    KiriVideoDocument* documentPointer = document.get();

    documentPointer->setMuted(true);

    QVERIFY(destroyed);
    QVERIFY(documentGuard == nullptr);
    QCOMPARE(projectionSpy.count(), 0);
}

void TestKiriVideoDocument::videoOutputCanDetachAndToleratesDestroyedOutput()
{
    KiriDocumentSession session;
    KiriVideoDocument& document = *session.videoDocument();
    QSignalSpy videoOutputSpy(&document, &KiriVideoDocument::videoOutputChanged);
    auto* output = new QObject();
    QObject surfaceOwner;
    QObject staleSurfaceOwner;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/clip.mp4"));
    const QUrl replacementSourceUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/replacement.mp4"));

    session.setSourceUrl(sourceUrl);
    QCOMPARE(session.documentKind(), KiriDocumentSession::DocumentKind::Video);
    const quint64 staleProjectionRevision = session.publicProjectionRevision();
    const QString staleProjectionClaimToken = session.nextVideoOutputSurfaceClaimToken();

    session.setSourceUrl(replacementSourceUrl);
    QVERIFY(session.publicProjectionRevision() > staleProjectionRevision);
    QVERIFY(!session.reportVideoOutputSurfaceClaim(staleProjectionClaimToken,
        staleProjectionRevision, &staleSurfaceOwner, output, true, QRectF(), QRectF()));
    QCOMPARE(document.videoOutput(), nullptr);
    QCOMPARE(videoOutputSpy.count(), 0);

    QVERIFY(!session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
        session.publicProjectionRevision(), nullptr, output, true, QRectF(), QRectF()));
    QCOMPARE(document.videoOutput(), nullptr);
    QCOMPARE(videoOutputSpy.count(), 0);

    QVERIFY(!session.reportVideoOutputSurfaceClaim(QStringLiteral("not-a-claim-token"),
        session.publicProjectionRevision(), &surfaceOwner, output, true, QRectF(), QRectF()));
    QCOMPARE(document.videoOutput(), nullptr);
    QCOMPARE(videoOutputSpy.count(), 0);

    const QString staleSameOwnerDetachToken = session.nextVideoOutputSurfaceClaimToken();
    const QString activeAttachToken = session.nextVideoOutputSurfaceClaimToken();
    QVERIFY(session.reportVideoOutputSurfaceClaim(activeAttachToken,
        session.publicProjectionRevision(), &surfaceOwner, output, true, QRectF(), QRectF()));
    session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
        session.publicProjectionRevision(), &staleSurfaceOwner, nullptr, false, QRectF(), QRectF());
    QCOMPARE(document.videoOutput(), output);
    QCOMPARE(videoOutputSpy.count(), 1);

    QVERIFY(!session.reportVideoOutputSurfaceClaim(staleSameOwnerDetachToken,
        session.publicProjectionRevision(), &surfaceOwner, nullptr, false, QRectF(), QRectF()));
    QCOMPARE(document.videoOutput(), output);
    QCOMPARE(videoOutputSpy.count(), 1);

    QVERIFY(session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
        session.publicProjectionRevision(), &surfaceOwner, nullptr, false, QRectF(), QRectF()));
    QCOMPARE(document.videoOutput(), nullptr);
    QCOMPARE(videoOutputSpy.count(), 2);

    output = new QObject();
    QVERIFY(session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
        session.publicProjectionRevision(), &surfaceOwner, output, true, QRectF(), QRectF()));
    QCOMPARE(document.videoOutput(), output);
    delete output;
    QCOMPARE(document.videoOutput(), nullptr);
    QCOMPARE(videoOutputSpy.count(), 4);
}

QTEST_GUILESS_MAIN(TestKiriVideoDocument)

#include "tst_kirivideodocument.moc"
