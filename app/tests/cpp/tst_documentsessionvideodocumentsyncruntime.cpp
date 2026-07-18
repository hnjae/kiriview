// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionvideodocumentsyncruntime.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <vector>

class TestDocumentSessionVideoDocumentSyncRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ignoresInactiveDocumentKind();
    void emptyVideoSourceClearsSessionDirectMedia();
    void directVideoSourceCommitsCursorAndRefreshesWhenScopeChanged();
    void staleDirectVideoConfirmationPreservesSourceIdentity();
    void openedCollectionVideoSourceDoesNotCommitDirectCursor();
};

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

struct VideoSyncFixture
{
    enum class Event {
        ClearCursor,
        SetSourceIdentity,
        SetDocumentKind,
        ClearNavigation,
        SetDirectVideoCursor,
        RefreshNavigation,
        Publish,
    };

    std::vector<Event> events;
    QUrl sourceIdentity;
    kiriview::DocumentSessionKind documentKind = kiriview::DocumentSessionKind::Video;
    QUrl directVideoCursorUrl;
    kiriview::DirectMediaConfirmation confirmation = kiriview::DirectMediaConfirmation::Committed;
    kiriview::DocumentSessionVideoDocumentSyncRuntime runtime {
        kiriview::DocumentSessionVideoDocumentSyncRuntimePorts {
            [this]() { events.push_back(Event::ClearCursor); },
            [this](const QUrl& url) {
                events.push_back(Event::SetSourceIdentity);
                sourceIdentity = url;
            },
            [this](kiriview::DocumentSessionKind kind) {
                events.push_back(Event::SetDocumentKind);
                documentKind = kind;
            },
            [this]() { events.push_back(Event::ClearNavigation); },
            [this](const QUrl& url) {
                events.push_back(Event::SetDirectVideoCursor);
                directVideoCursorUrl = url;
                return confirmation;
            },
            [this]() { events.push_back(Event::RefreshNavigation); },
            [this]() { events.push_back(Event::Publish); },
        }
    };
};

kiriview::DocumentSessionPublicVideoLeafSnapshot videoSnapshot(const QUrl& url)
{
    kiriview::DocumentSessionPublicVideoLeafSnapshot snapshot;
    snapshot.sourceUrl = url;
    return snapshot;
}
}

void TestDocumentSessionVideoDocumentSyncRuntime::ignoresInactiveDocumentKind()
{
    VideoSyncFixture fixture;

    fixture.runtime.sync(kiriview::DocumentSessionKind::Image,
        videoSnapshot(localUrl(QStringLiteral("/media/clip.mkv"))));

    QVERIFY(fixture.events.empty());
}

void TestDocumentSessionVideoDocumentSyncRuntime::emptyVideoSourceClearsSessionDirectMedia()
{
    VideoSyncFixture fixture;

    fixture.runtime.sync(kiriview::DocumentSessionKind::Video, videoSnapshot(QUrl()));

    QCOMPARE(fixture.documentKind, kiriview::DocumentSessionKind::Empty);
    QVERIFY(fixture.sourceIdentity.isEmpty());
    QCOMPARE(fixture.events,
        (std::vector<VideoSyncFixture::Event> { VideoSyncFixture::Event::ClearCursor,
            VideoSyncFixture::Event::SetSourceIdentity, VideoSyncFixture::Event::SetDocumentKind,
            VideoSyncFixture::Event::ClearNavigation, VideoSyncFixture::Event::Publish }));
}

void TestDocumentSessionVideoDocumentSyncRuntime::
    directVideoSourceCommitsCursorAndRefreshesWhenScopeChanged()
{
    VideoSyncFixture fixture;
    const QUrl clipUrl = localUrl(QStringLiteral("/media/clip.mkv"));

    fixture.runtime.sync(kiriview::DocumentSessionKind::Video, videoSnapshot(clipUrl));

    QCOMPARE(fixture.directVideoCursorUrl, clipUrl);
    QCOMPARE(fixture.sourceIdentity, clipUrl);
    QCOMPARE(fixture.events,
        (std::vector<VideoSyncFixture::Event> { VideoSyncFixture::Event::SetDirectVideoCursor,
            VideoSyncFixture::Event::SetSourceIdentity, VideoSyncFixture::Event::Publish }));
}

void TestDocumentSessionVideoDocumentSyncRuntime::
    staleDirectVideoConfirmationPreservesSourceIdentity()
{
    VideoSyncFixture fixture;
    fixture.confirmation = kiriview::DirectMediaConfirmation::Stale;
    const QUrl clipUrl = localUrl(QStringLiteral("/media/clip.mkv"));

    fixture.runtime.sync(kiriview::DocumentSessionKind::Video, videoSnapshot(clipUrl));

    QCOMPARE(fixture.directVideoCursorUrl, clipUrl);
    QVERIFY(fixture.sourceIdentity.isEmpty());
    QCOMPARE(fixture.events,
        (std::vector<VideoSyncFixture::Event> { VideoSyncFixture::Event::SetDirectVideoCursor }));
}

void TestDocumentSessionVideoDocumentSyncRuntime::
    openedCollectionVideoSourceDoesNotCommitDirectCursor()
{
    VideoSyncFixture fixture;
    const QUrl clipUrl(QStringLiteral("zip:///books/book.zip!/clip.mp4"));
    kiriview::DocumentSessionVideoDocumentSyncRuntimeInput input;
    input.documentKind = kiriview::DocumentSessionKind::Video;
    input.openedCollectionVideoActive = true;
    input.video = videoSnapshot(clipUrl);

    fixture.runtime.sync(input);

    QVERIFY(fixture.directVideoCursorUrl.isEmpty());
    QCOMPARE(fixture.sourceIdentity, clipUrl);
    QCOMPARE(fixture.events,
        (std::vector<VideoSyncFixture::Event> {
            VideoSyncFixture::Event::SetSourceIdentity, VideoSyncFixture::Event::Publish }));
}

QTEST_GUILESS_MAIN(TestDocumentSessionVideoDocumentSyncRuntime)
#include "tst_documentsessionvideodocumentsyncruntime.moc"
