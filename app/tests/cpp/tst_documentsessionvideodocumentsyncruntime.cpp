// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionvideodocumentsyncruntime.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <memory>
#include <vector>

class TestDocumentSessionVideoDocumentSyncRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ignoresInactiveDocumentKind();
    void emptyVideoSourceClearsSessionDirectMedia();
    void supersededDocumentKindClearStopsRemainingMutation();
    void documentKindClearCanDestroyRuntime();
    void nestedSyncSupersedesRemainingClearMutation();
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
                return true;
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
        (std::vector<VideoSyncFixture::Event> { VideoSyncFixture::Event::SetDocumentKind,
            VideoSyncFixture::Event::ClearCursor, VideoSyncFixture::Event::SetSourceIdentity,
            VideoSyncFixture::Event::ClearNavigation, VideoSyncFixture::Event::Publish }));
}

void TestDocumentSessionVideoDocumentSyncRuntime::
    supersededDocumentKindClearStopsRemainingMutation()
{
    int clearCursorCount = 0;
    int clearSourceIdentityCount = 0;
    int clearNavigationCount = 0;
    int publishCount = 0;
    kiriview::DocumentSessionVideoDocumentSyncRuntimePorts ports;
    ports.clearDirectMediaCursor = [&]() { ++clearCursorCount; };
    ports.setSourceIdentity = [&](const QUrl&) { ++clearSourceIdentityCount; };
    ports.setDocumentKind = [](kiriview::DocumentSessionKind) { return false; };
    ports.clearDirectMediaNavigation = [&]() { ++clearNavigationCount; };
    ports.recomputePublicProjection = [&]() { ++publishCount; };
    kiriview::DocumentSessionVideoDocumentSyncRuntime runtime(std::move(ports));

    runtime.sync(kiriview::DocumentSessionKind::Video, videoSnapshot(QUrl()));

    QCOMPARE(clearCursorCount, 0);
    QCOMPARE(clearSourceIdentityCount, 0);
    QCOMPARE(clearNavigationCount, 0);
    QCOMPARE(publishCount, 0);
}

void TestDocumentSessionVideoDocumentSyncRuntime::documentKindClearCanDestroyRuntime()
{
    int remainingMutationCount = 0;
    using Runtime = kiriview::DocumentSessionVideoDocumentSyncRuntime;
    std::unique_ptr<Runtime> runtime;
    kiriview::DocumentSessionVideoDocumentSyncRuntimePorts ports;
    ports.setDocumentKind = [&](kiriview::DocumentSessionKind) {
        runtime.reset();
        return false;
    };
    ports.clearDirectMediaNavigation = [&]() { ++remainingMutationCount; };
    ports.recomputePublicProjection = [&]() { ++remainingMutationCount; };
    runtime = std::make_unique<Runtime>(std::move(ports));

    runtime->sync(kiriview::DocumentSessionKind::Video, videoSnapshot(QUrl()));

    QCOMPARE(remainingMutationCount, 0);
}

void TestDocumentSessionVideoDocumentSyncRuntime::nestedSyncSupersedesRemainingClearMutation()
{
    const QUrl replacementUrl = localUrl(QStringLiteral("/media/replacement.mkv"));
    QUrl sourceIdentity;
    int clearNavigationCount = 0;
    int publishCount = 0;
    bool nestedSyncSubmitted = false;
    using Runtime = kiriview::DocumentSessionVideoDocumentSyncRuntime;
    Runtime* runtime = nullptr;
    kiriview::DocumentSessionVideoDocumentSyncRuntimePorts ports;
    ports.clearDirectMediaCursor = [&]() {
        if (nestedSyncSubmitted) {
            return;
        }
        nestedSyncSubmitted = true;
        runtime->sync(kiriview::DocumentSessionKind::Video, videoSnapshot(replacementUrl));
    };
    ports.setSourceIdentity = [&](const QUrl& url) { sourceIdentity = url; };
    ports.setDocumentKind = [](kiriview::DocumentSessionKind) { return true; };
    ports.clearDirectMediaNavigation = [&]() { ++clearNavigationCount; };
    ports.confirmDirectVideoCursor
        = [](const QUrl&) { return kiriview::DirectMediaConfirmation::Committed; };
    ports.recomputePublicProjection = [&]() { ++publishCount; };
    Runtime ownedRuntime(std::move(ports));
    runtime = &ownedRuntime;

    runtime->sync(kiriview::DocumentSessionKind::Video, videoSnapshot(QUrl()));

    QVERIFY(nestedSyncSubmitted);
    QCOMPARE(sourceIdentity, replacementUrl);
    QCOMPARE(clearNavigationCount, 0);
    QCOMPARE(publishCount, 1);
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
