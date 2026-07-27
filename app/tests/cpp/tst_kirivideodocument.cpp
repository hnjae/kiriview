// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kirivideodocument.h"

#include "facade/kiridocumentsession.h"
#include "video/videomediabackend.h"
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
    void videoOutputSignalObserversSeeCommittedAttachment();
    void leavingVideoRejectsReentrantSurfaceClaim();
    void leavingVideoKeepsSurfaceClaimsRetiredUntilImageProjection();
    void reentrantVideoRouteCanActivateNewSurfaceClaimEpoch();
    void supersededVideoEntryCannotKeepClaimEpochActive();
    void sessionDestructionSilentlyDetachesRetainedVideoDocument();
    void sessionDestructionDuringVideoSignalSilentlySettlesPlaybackProjection();
};

namespace {
class TeardownCallbackVideoBackend final : public kiriview::VideoMediaBackend
{
public:
    void setCallbacks(kiriview::VideoMediaBackendCallbacks callbacks) override
    {
        m_callbacks = std::move(callbacks);
    }

    void setSource(const QUrl&) override { }
    void setSourceDevice(QIODevice*, const QUrl&) override { }
    void play() override { }
    void pause() override { }
    void stop() override { }
    void setPosition(qint64) override { }
    void setMuted(bool muted) override { m_muted = muted; }

    void setVideoOutput(QObject* videoOutput) override
    {
        const bool detaching = m_videoOutput != nullptr && videoOutput == nullptr;
        m_videoOutput = videoOutput;
        if (!detaching) {
            return;
        }

        m_muted = true;
        const std::function<void()> mutedChanged = m_callbacks.mutedChanged;
        if (mutedChanged) {
            mutedChanged();
        }
    }

    [[nodiscard]] QObject* videoOutput() const override { return m_videoOutput.data(); }
    [[nodiscard]] kiriview::VideoMediaStatus mediaStatus() const override
    {
        return kiriview::VideoMediaStatus::Loading;
    }
    [[nodiscard]] qint64 duration() const override { return 0; }
    [[nodiscard]] qint64 position() const override { return 0; }
    [[nodiscard]] bool playing() const override { return false; }
    [[nodiscard]] bool seekable() const override { return false; }
    [[nodiscard]] bool hasVideo() const override { return false; }
    [[nodiscard]] bool hasAudio() const override { return m_hasAudio; }
    [[nodiscard]] QSize videoSize() const override { return {}; }
    [[nodiscard]] bool muted() const override { return m_muted; }

    void emitHasAudio()
    {
        m_hasAudio = true;
        const std::function<void()> hasAudioChanged = m_callbacks.hasAudioChanged;
        if (hasAudioChanged) {
            hasAudioChanged();
        }
    }

private:
    kiriview::VideoMediaBackendCallbacks m_callbacks;
    QPointer<QObject> m_videoOutput;
    bool m_muted = false;
    bool m_hasAudio = false;
};

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
    QVERIFY(!session.reportVideoOutputSurfaceClaim(staleProjectionClaimToken,
        session.publicProjectionRevision(), &staleSurfaceOwner, output, true, QRectF(), QRectF()));
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

void TestKiriVideoDocument::videoOutputSignalObserversSeeCommittedAttachment()
{
    QObject firstOwner;
    QObject replacementOwner;
    QObject firstOutput;
    QObject replacementOutput;
    KiriDocumentSession session;
    KiriVideoDocument& document = *session.videoDocument();
    session.setSourceUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/movie.mp4")));
    QList<QObject*> laterReceiverObservations;
    bool reentered = false;

    connect(&document, &KiriVideoDocument::videoOutputChanged, &document, [&]() {
        if (reentered) {
            return;
        }
        QCOMPARE(document.videoOutput(), &firstOutput);
        reentered = true;
        QVERIFY(session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
            session.publicProjectionRevision(), &replacementOwner, &replacementOutput, true, {},
            {}));
        QCOMPARE(document.videoOutput(), &firstOutput);
    });
    connect(&document, &KiriVideoDocument::videoOutputChanged, &document,
        [&]() { laterReceiverObservations.push_back(document.videoOutput()); });

    QVERIFY(session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
        session.publicProjectionRevision(), &firstOwner, &firstOutput, true, {}, {}));

    QVERIFY(reentered);
    QCOMPARE(laterReceiverObservations, QList<QObject*>({ &firstOutput, &replacementOutput }));
    QCOMPARE(document.videoOutput(), &replacementOutput);
}

void TestKiriVideoDocument::leavingVideoRejectsReentrantSurfaceClaim()
{
    QObject surfaceOwner;
    QObject videoOutput;
    KiriDocumentSession session;
    KiriVideoDocument& document = *session.videoDocument();
    session.setSourceUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/movie.mp4")));
    QVERIFY(session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
        session.publicProjectionRevision(), &surfaceOwner, &videoOutput, true, {}, {}));
    bool reattachAttempted = false;
    bool reattachAccepted = true;

    connect(&document, &KiriVideoDocument::videoOutputChanged, &document, [&]() {
        if (reattachAttempted || document.videoOutput() != nullptr) {
            return;
        }
        reattachAttempted = true;
        reattachAccepted
            = session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
                session.publicProjectionRevision(), &surfaceOwner, &videoOutput, true, {}, {});
    });

    session.setSourceUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/picture.jpg")));

    QVERIFY(reattachAttempted);
    QVERIFY(!reattachAccepted);
    QCOMPARE(document.videoOutput(), nullptr);
    QCOMPARE(session.documentKind(), KiriDocumentSession::DocumentKind::Image);
}

void TestKiriVideoDocument::leavingVideoKeepsSurfaceClaimsRetiredUntilImageProjection()
{
    const QUrl imageUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/picture.jpg"));
    QObject surfaceOwner;
    QObject videoOutput;
    KiriDocumentSession session;
    KiriVideoDocument& document = *session.videoDocument();
    session.setSourceUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/movie.mp4")));
    QVERIFY(session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
        session.publicProjectionRevision(), &surfaceOwner, &videoOutput, true, {}, {}));
    bool reattachAttempted = false;
    bool reattachAccepted = true;

    connect(session.imageDocument(), &KiriImageDocument::sourceUrlChanged, session.imageDocument(),
        [&]() {
            if (reattachAttempted || session.imageDocument()->sourceUrl() != imageUrl) {
                return;
            }
            reattachAttempted = true;
            reattachAccepted
                = session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
                    session.publicProjectionRevision(), &surfaceOwner, &videoOutput, true, {}, {});
        });

    session.setSourceUrl(imageUrl);

    QVERIFY(reattachAttempted);
    QVERIFY(!reattachAccepted);
    QCOMPARE(document.videoOutput(), nullptr);
    QCOMPARE(session.documentKind(), KiriDocumentSession::DocumentKind::Image);
}

void TestKiriVideoDocument::reentrantVideoRouteCanActivateNewSurfaceClaimEpoch()
{
    const QUrl firstVideoUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/first.mp4"));
    const QUrl replacementVideoUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/replacement.mp4"));
    const QUrl supersededImageUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/superseded.jpg"));
    QObject firstSurfaceOwner;
    QObject firstVideoOutput;
    QObject replacementSurfaceOwner;
    QObject replacementVideoOutput;
    KiriDocumentSession session;
    KiriVideoDocument& document = *session.videoDocument();
    session.setSourceUrl(firstVideoUrl);
    QVERIFY(session.reportVideoOutputSurfaceClaim(session.nextVideoOutputSurfaceClaimToken(),
        session.publicProjectionRevision(), &firstSurfaceOwner, &firstVideoOutput, true, {}, {}));
    bool replacementRouteSubmitted = false;
    bool replacementClaimAttempted = false;
    bool replacementClaimAccepted = false;

    connect(&session, &KiriDocumentSession::publicProjectionRevisionChanged, &session, [&]() {
        if (!replacementRouteSubmitted || replacementClaimAttempted
            || session.sourceUrl() != replacementVideoUrl
            || session.documentKind() != KiriDocumentSession::DocumentKind::Video) {
            return;
        }
        replacementClaimAttempted = true;
        replacementClaimAccepted = session.reportVideoOutputSurfaceClaim(
            session.nextVideoOutputSurfaceClaimToken(), session.publicProjectionRevision(),
            &replacementSurfaceOwner, &replacementVideoOutput, true, {}, {});
    });
    connect(&document, &KiriVideoDocument::videoOutputChanged, &document, [&]() {
        if (replacementRouteSubmitted || document.videoOutput() != nullptr) {
            return;
        }
        replacementRouteSubmitted = true;
        session.setSourceUrl(replacementVideoUrl);
    });

    session.setSourceUrl(supersededImageUrl);

    QVERIFY(replacementRouteSubmitted);
    QVERIFY(replacementClaimAttempted);
    QVERIFY(replacementClaimAccepted);
    QCOMPARE(session.sourceUrl(), replacementVideoUrl);
    QCOMPARE(session.documentKind(), KiriDocumentSession::DocumentKind::Video);
    QCOMPARE(document.videoOutput(), &replacementVideoOutput);
}

void TestKiriVideoDocument::supersededVideoEntryCannotKeepClaimEpochActive()
{
    const QUrl supersededVideoUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/superseded.mp4"));
    const QUrl imageUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/current.jpg"));
    const QUrl currentVideoUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/current.mp4"));
    QObject surfaceOwner;
    QObject videoOutput;
    KiriDocumentSession session;
    KiriVideoDocument& document = *session.videoDocument();
    bool imageRouteSubmitted = false;

    connect(&document, &KiriVideoDocument::sourceUrlChanged, &document, [&]() {
        if (imageRouteSubmitted || document.sourceUrl() != supersededVideoUrl) {
            return;
        }
        imageRouteSubmitted = true;
        session.setSourceUrl(imageUrl);
    });

    session.setSourceUrl(supersededVideoUrl);

    QVERIFY(imageRouteSubmitted);
    QCOMPARE(session.sourceUrl(), imageUrl);
    QCOMPARE(session.documentKind(), KiriDocumentSession::DocumentKind::Image);
    const QString tokenIssuedWhileImageActive = session.nextVideoOutputSurfaceClaimToken();

    session.setSourceUrl(currentVideoUrl);

    QCOMPARE(session.documentKind(), KiriDocumentSession::DocumentKind::Video);
    QVERIFY(!session.reportVideoOutputSurfaceClaim(tokenIssuedWhileImageActive,
        session.publicProjectionRevision(), &surfaceOwner, &videoOutput, true, {}, {}));
    QCOMPARE(document.videoOutput(), nullptr);
}

void TestKiriVideoDocument::sessionDestructionSilentlyDetachesRetainedVideoDocument()
{
    QObject surfaceOwner;
    QObject videoOutput;
    auto session = std::make_unique<KiriDocumentSession>();
    KiriVideoDocument* document = session->videoDocument();
    session->setSourceUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/movie.mp4")));
    QVERIFY(session->reportVideoOutputSurfaceClaim(session->nextVideoOutputSurfaceClaimToken(),
        session->publicProjectionRevision(), &surfaceOwner, &videoOutput, true, {}, {}));
    QCOMPARE(document->videoOutput(), &videoOutput);
    QSignalSpy videoOutputSpy(document, &KiriVideoDocument::videoOutputChanged);
    document->setParent(nullptr);
    const std::unique_ptr<KiriVideoDocument> retainedDocument(document);

    session.reset();

    QCOMPARE(retainedDocument->videoOutput(), nullptr);
    QCOMPARE(videoOutputSpy.count(), 0);
}

void TestKiriVideoDocument::sessionDestructionDuringVideoSignalSilentlySettlesPlaybackProjection()
{
    kiriview::KiriDocumentSessionDependencies dependencies;
    TeardownCallbackVideoBackend* backend = nullptr;
    dependencies.videoMediaBackendFactory
        = [&backend]() -> std::unique_ptr<kiriview::VideoMediaBackend> {
        auto candidate = std::make_unique<TeardownCallbackVideoBackend>();
        backend = candidate.get();
        return candidate;
    };

    QObject surfaceOwner;
    QObject videoOutput;
    auto session = std::make_unique<KiriDocumentSession>(std::move(dependencies));
    QPointer<KiriDocumentSession> sessionGuard(session.get());
    KiriVideoDocument* document = session->videoDocument();
    session->setSourceUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/movie.mp4")));
    QVERIFY(backend != nullptr);
    QVERIFY(session->reportVideoOutputSurfaceClaim(session->nextVideoOutputSurfaceClaimToken(),
        session->publicProjectionRevision(), &surfaceOwner, &videoOutput, true, {}, {}));
    QCOMPARE(document->videoOutput(), &videoOutput);

    document->setParent(nullptr);
    const std::unique_ptr<KiriVideoDocument> retainedDocument(document);
    QSignalSpy projectionSpy(
        retainedDocument->playbackControls(), &KiriVideoPlaybackControls::projectionChanged);
    connect(retainedDocument.get(), &KiriVideoDocument::hasAudioChanged, retainedDocument.get(),
        [&session]() { session.reset(); });

    backend->emitHasAudio();

    QVERIFY(sessionGuard.isNull());
    QVERIFY(retainedDocument->hasAudio());
    QCOMPARE(retainedDocument->videoOutput(), nullptr);
    QVERIFY(retainedDocument->playbackControls()->muted());
    QCOMPARE(projectionSpy.count(), 0);
}

QTEST_GUILESS_MAIN(TestKiriVideoDocument)

#include "tst_kirivideodocument.moc"
