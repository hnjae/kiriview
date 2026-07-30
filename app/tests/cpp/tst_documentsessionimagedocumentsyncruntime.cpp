// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionimagedocumentsyncruntime.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <memory>
#include <vector>

class TestDocumentSessionImageDocumentSyncRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ignoresRoutingAndInactiveDocumentKind();
    void confirmsDirectImageCursorWithoutRefreshingNavigation();
    void mirrorsDeletionProgressWhenImageDocumentOwnsSourceScope();
    void syncsCollectionScopeWithoutInactiveDirectMediaRefresh();
    void recomputesDirectMediaProjectionWhenImagePageNavigationChanges();
    void publishesImagePageNavigationWhenTheLeafNavigationChanges();
    void sourceIdentityCallbackCanDestroyRuntime();
    void nestedSyncSupersedesRemainingMutation();
};

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

struct ImageSyncFixture
{
    enum class Event {
        ConfirmDirectImageCursor,
        RestoreDirectImageCursorAfterFailure,
        SetSourceIdentity,
        SetFileDeletionInProgress,
        RefreshNavigation,
        CacheDisplayedPredecode,
        PublishImagePages,
        Publish,
    };

    std::vector<Event> events;
    QUrl confirmedCursorUrl;
    bool restoredCursorChanged = true;
    QUrl sourceIdentity;
    bool fileDeletionInProgress = false;
    kiriview::DocumentSessionImageDocumentSyncRuntime runtime {
        kiriview::DocumentSessionImageDocumentSyncRuntimePorts {
            [this](const QUrl& url) {
                events.push_back(Event::ConfirmDirectImageCursor);
                confirmedCursorUrl = url;
                return kiriview::DirectMediaConfirmation::Committed;
            },
            [this]() {
                events.push_back(Event::RestoreDirectImageCursorAfterFailure);
                return restoredCursorChanged;
            },
            [this](const QUrl& url) {
                events.push_back(Event::SetSourceIdentity);
                sourceIdentity = url;
            },
            [this](bool inProgress) {
                events.push_back(Event::SetFileDeletionInProgress);
                fileDeletionInProgress = inProgress;
            },
            [this]() { events.push_back(Event::RefreshNavigation); },
            [this]() { events.push_back(Event::CacheDisplayedPredecode); },
            [this]() { events.push_back(Event::PublishImagePages); },
            [this]() { events.push_back(Event::Publish); },
        }
    };
};

kiriview::DocumentSessionImageDocumentSyncRuntimeInput activeInput(const QUrl& url)
{
    kiriview::DocumentSessionImageDocumentSyncRuntimeInput input;
    input.documentKind = kiriview::DocumentSessionKind::Image;
    input.directMediaNavigationActive = true;
    input.directMediaNavigationKnown = true;
    input.image.sourceUrl = url;
    input.image.displayedUrl = url;
    input.image.ordinaryDirectMediaScopeActive = true;
    return input;
}
}

void TestDocumentSessionImageDocumentSyncRuntime::ignoresRoutingAndInactiveDocumentKind()
{
    ImageSyncFixture fixture;
    kiriview::DocumentSessionImageDocumentSyncRuntimeInput input
        = activeInput(localUrl(QStringLiteral("/media/01.png")));
    input.routingSource = true;

    fixture.runtime.sync(input);

    QVERIFY(fixture.events.empty());

    input.routingSource = false;
    input.documentKind = kiriview::DocumentSessionKind::Video;

    fixture.runtime.sync(input);

    QVERIFY(fixture.events.empty());
}

void TestDocumentSessionImageDocumentSyncRuntime::
    confirmsDirectImageCursorWithoutRefreshingNavigation()
{
    ImageSyncFixture fixture;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    kiriview::DocumentSessionImageDocumentSyncRuntimeInput input = activeInput(imageUrl);
    input.directImageLoadMayUseImageDocumentSourceScope = true;
    input.directMediaCursor.pendingSource = { imageUrl, {}, imageUrl };

    fixture.runtime.sync(input);

    QCOMPARE(fixture.confirmedCursorUrl, imageUrl);
    QCOMPARE(fixture.sourceIdentity, imageUrl);
    QCOMPARE(fixture.events,
        (std::vector<ImageSyncFixture::Event> {
            ImageSyncFixture::Event::ConfirmDirectImageCursor,
            ImageSyncFixture::Event::SetSourceIdentity,
            ImageSyncFixture::Event::CacheDisplayedPredecode,
            ImageSyncFixture::Event::Publish,
        }));
}

void TestDocumentSessionImageDocumentSyncRuntime::
    mirrorsDeletionProgressWhenImageDocumentOwnsSourceScope()
{
    ImageSyncFixture fixture;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    kiriview::DocumentSessionImageDocumentSyncRuntimeInput input = activeInput(imageUrl);
    input.directImageLoadMayUseImageDocumentSourceScope = false;
    input.directMediaNavigationActive = false;
    input.image.fileDeletionInProgress = true;

    fixture.runtime.sync(input);

    QVERIFY(fixture.fileDeletionInProgress);
    QCOMPARE(fixture.events,
        (std::vector<ImageSyncFixture::Event> {
            ImageSyncFixture::Event::ConfirmDirectImageCursor,
            ImageSyncFixture::Event::SetSourceIdentity,
            ImageSyncFixture::Event::SetFileDeletionInProgress,
            ImageSyncFixture::Event::RefreshNavigation,
            ImageSyncFixture::Event::Publish,
        }));
}

void TestDocumentSessionImageDocumentSyncRuntime::
    syncsCollectionScopeWithoutInactiveDirectMediaRefresh()
{
    ImageSyncFixture fixture;
    const QUrl imageUrl = localUrl(QStringLiteral("/books/book.cbz"));
    kiriview::DocumentSessionImageDocumentSyncRuntimeInput input = activeInput(imageUrl);
    input.directImageLoadMayUseImageDocumentSourceScope = false;
    input.directMediaNavigationActive = false;
    input.directMediaNavigationKnown = false;
    input.image.ordinaryDirectMediaScopeActive = false;
    input.image.openedCollectionScopeActive = true;
    input.image.fileDeletionInProgress = true;

    fixture.runtime.sync(input);

    QVERIFY(fixture.fileDeletionInProgress);
    QCOMPARE(fixture.events,
        (std::vector<ImageSyncFixture::Event> {
            ImageSyncFixture::Event::SetSourceIdentity,
            ImageSyncFixture::Event::SetFileDeletionInProgress,
            ImageSyncFixture::Event::Publish,
        }));
}

void TestDocumentSessionImageDocumentSyncRuntime::
    publishesImagePageNavigationWhenTheLeafNavigationChanges()
{
    ImageSyncFixture fixture;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    kiriview::DocumentSessionImageDocumentSyncRuntimeInput input = activeInput(imageUrl);
    input.directImageLoadMayUseImageDocumentSourceScope = true;
    input.directMediaNavigationActive = false;
    input.directMediaNavigationKnown = false;
    input.image.ordinaryDirectMediaScopeActive = false;
    input.image.openedCollectionScopeActive = true;
    input.previousPageNavigation.known = false;
    input.image.pageNavigation.known = true;
    input.image.pageNavigation.currentNumber = 2;
    input.image.pageNavigation.count = 3;

    fixture.runtime.sync(input);

    QCOMPARE(fixture.events,
        (std::vector<ImageSyncFixture::Event> {
            ImageSyncFixture::Event::SetSourceIdentity,
            ImageSyncFixture::Event::PublishImagePages,
        }));
}

void TestDocumentSessionImageDocumentSyncRuntime::
    recomputesDirectMediaProjectionWhenImagePageNavigationChanges()
{
    ImageSyncFixture fixture;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    kiriview::DocumentSessionImageDocumentSyncRuntimeInput input = activeInput(imageUrl);
    input.directImageLoadMayUseImageDocumentSourceScope = true;
    input.previousPageNavigation.known = true;
    input.previousPageNavigation.currentNumber = 2;
    input.previousPageNavigation.count = 5;

    fixture.runtime.sync(input);

    QCOMPARE(fixture.events,
        (std::vector<ImageSyncFixture::Event> {
            ImageSyncFixture::Event::ConfirmDirectImageCursor,
            ImageSyncFixture::Event::SetSourceIdentity,
            ImageSyncFixture::Event::CacheDisplayedPredecode,
            ImageSyncFixture::Event::Publish,
        }));
}

void TestDocumentSessionImageDocumentSyncRuntime::sourceIdentityCallbackCanDestroyRuntime()
{
    int remainingMutationCount = 0;
    using Runtime = kiriview::DocumentSessionImageDocumentSyncRuntime;
    std::unique_ptr<Runtime> runtime;
    kiriview::DocumentSessionImageDocumentSyncRuntimePorts ports;
    ports.setSourceIdentity = [&](const QUrl&) { runtime.reset(); };
    ports.setFileDeletionInProgress = [&](bool) { ++remainingMutationCount; };
    ports.refreshDirectMediaNavigation = [&]() { ++remainingMutationCount; };
    ports.recomputePublicProjection = [&]() { ++remainingMutationCount; };
    runtime = std::make_unique<Runtime>(std::move(ports));
    kiriview::DocumentSessionImageDocumentSyncRuntimeInput input
        = activeInput(localUrl(QStringLiteral("/media/01.png")));
    input.directImageLoadMayUseImageDocumentSourceScope = false;
    input.directMediaNavigationActive = false;

    runtime->sync(input);

    QCOMPARE(remainingMutationCount, 0);
}

void TestDocumentSessionImageDocumentSyncRuntime::nestedSyncSupersedesRemainingMutation()
{
    const QUrl originalUrl = localUrl(QStringLiteral("/media/original.png"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/media/replacement.png"));
    QUrl sourceIdentity;
    int deletionProgressCount = 0;
    int publishCount = 0;
    bool nestedSyncSubmitted = false;
    using Runtime = kiriview::DocumentSessionImageDocumentSyncRuntime;
    Runtime* runtime = nullptr;
    kiriview::DocumentSessionImageDocumentSyncRuntimePorts ports;
    ports.setSourceIdentity = [&](const QUrl& url) {
        sourceIdentity = url;
        if (nestedSyncSubmitted || url != originalUrl) {
            return;
        }
        nestedSyncSubmitted = true;
        kiriview::DocumentSessionImageDocumentSyncRuntimeInput nestedInput
            = activeInput(replacementUrl);
        nestedInput.directImageLoadMayUseImageDocumentSourceScope = false;
        nestedInput.directMediaNavigationActive = false;
        nestedInput.image.openedCollectionScopeActive = true;
        runtime->sync(nestedInput);
    };
    ports.setFileDeletionInProgress = [&](bool) { ++deletionProgressCount; };
    ports.recomputePublicProjection = [&]() { ++publishCount; };
    Runtime ownedRuntime(std::move(ports));
    runtime = &ownedRuntime;
    kiriview::DocumentSessionImageDocumentSyncRuntimeInput input = activeInput(originalUrl);
    input.directImageLoadMayUseImageDocumentSourceScope = false;
    input.directMediaNavigationActive = false;
    input.image.openedCollectionScopeActive = true;

    runtime->sync(input);

    QVERIFY(nestedSyncSubmitted);
    QCOMPARE(sourceIdentity, replacementUrl);
    QCOMPARE(deletionProgressCount, 1);
    QCOMPARE(publishCount, 1);
}

QTEST_GUILESS_MAIN(TestDocumentSessionImageDocumentSyncRuntime)
#include "tst_documentsessionimagedocumentsyncruntime.moc"
