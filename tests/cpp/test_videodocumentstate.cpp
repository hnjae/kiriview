// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videodocumentstate.h"

#include <QObject>
#include <QSize>
#include <QTest>
#include <QUrl>
#include <algorithm>
#include <optional>
#include <vector>

class TestVideoDocumentState : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sourceLoadResetsPublicPlaybackStateInOrder();
    void clearedSourceResetsPublicStateInOrder();
    void sourceLoadFailureStoresTypedFailureAndPublishesUserMessage();
    void backendFailureStoresTypedFailureAndPublishesUserMessage();
    void scalarSettersOnlyNotifyOnChangedValues();
    void zoomPercentStatePublishesKnownValuePair();
    void publishDeduplicatesChangesInOrder();
};

namespace {
using Change = kiriview::VideoDocumentChange;

std::vector<Change> flatten(const std::vector<std::vector<Change>>& batches)
{
    std::vector<Change> changes;
    for (const std::vector<Change>& batch : batches) {
        changes.insert(changes.end(), batch.cbegin(), batch.cend());
    }
    return changes;
}

void compareChanges(const std::vector<Change>& actual, const std::vector<Change>& expected)
{
    QCOMPARE(actual.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        QCOMPARE(actual.at(index), expected.at(index));
    }
}
}

void TestVideoDocumentState::sourceLoadResetsPublicPlaybackStateInOrder()
{
    std::vector<std::vector<Change>> batches;
    kiriview::VideoDocumentState state(
        [&batches](const std::vector<Change>& changes) { batches.push_back(changes); });
    const QUrl initialUrl = QUrl::fromLocalFile(QStringLiteral("/videos/old.mp4"));
    const QUrl sourceUrl(QStringLiteral("zip:///videos/archive.zip!/new.mov"));

    state.resetForSourceLoad(initialUrl);
    state.setErrorString(QStringLiteral("backend error"));
    state.setStatus(kiriview::VideoDocumentStatus::Ready);
    state.setHasVideo(true);
    state.setHasAudio(true);
    state.setVideoSize(QSize(1920, 1080));
    state.setZoomPercent(std::optional<int>(125));
    batches.clear();

    state.resetForSourceLoad(sourceUrl);

    QCOMPARE(state.sourceUrl(), sourceUrl);
    QCOMPARE(state.windowTitleFileName(), QStringLiteral("new.mov"));
    QCOMPARE(state.errorString(), QString());
    QCOMPARE(state.status(), kiriview::VideoDocumentStatus::Loading);
    QVERIFY(!state.hasVideo());
    QVERIFY(!state.hasAudio());
    QCOMPARE(state.videoSize(), QSize());
    QVERIFY(!state.zoomPercentKnown());
    QCOMPARE(state.zoomPercent(), 0);
    compareChanges(batches.front(),
        { Change::SourceUrl, Change::WindowTitleFileName, Change::ErrorString, Change::Status,
            Change::HasVideo, Change::HasAudio, Change::VideoSize, Change::ZoomPercentKnown,
            Change::ZoomPercent, Change::EmbeddedMetadata });
}

void TestVideoDocumentState::clearedSourceResetsPublicStateInOrder()
{
    std::vector<std::vector<Change>> batches;
    kiriview::VideoDocumentState state(
        [&batches](const std::vector<Change>& changes) { batches.push_back(changes); });

    state.resetForSourceLoad(QUrl::fromLocalFile(QStringLiteral("/videos/clip.mp4")));
    state.setErrorString(QStringLiteral("backend error"));
    state.setStatus(kiriview::VideoDocumentStatus::Ready);
    state.setHasVideo(true);
    state.setHasAudio(true);
    state.setVideoSize(QSize(1920, 1080));
    state.setZoomPercent(std::optional<int>(125));
    batches.clear();

    state.resetForClearedSource();

    QCOMPARE(state.sourceUrl(), QUrl());
    QCOMPARE(state.windowTitleFileName(), QString());
    QCOMPARE(state.errorString(), QString());
    QCOMPARE(state.status(), kiriview::VideoDocumentStatus::Null);
    QVERIFY(!state.hasVideo());
    QVERIFY(!state.hasAudio());
    QCOMPARE(state.videoSize(), QSize());
    QVERIFY(!state.zoomPercentKnown());
    QCOMPARE(state.zoomPercent(), 0);
    compareChanges(batches.front(),
        { Change::SourceUrl, Change::Status, Change::ErrorString, Change::WindowTitleFileName,
            Change::HasVideo, Change::HasAudio, Change::VideoSize, Change::ZoomPercentKnown,
            Change::ZoomPercent, Change::EmbeddedMetadata });
}

void TestVideoDocumentState::sourceLoadFailureStoresTypedFailureAndPublishesUserMessage()
{
    std::vector<std::vector<Change>> batches;
    kiriview::VideoDocumentState state(
        [&batches](const std::vector<Change>& changes) { batches.push_back(changes); });
    const QUrl sourceUrl(QStringLiteral("zip:///videos/archive.zip!/clip.mp4"));
    const QString userMessage = QStringLiteral("Could not open the selected video.");
    const QString diagnosticDetail = QStringLiteral("resolver rejected archive entry");

    state.resetForSourceLoad(sourceUrl);
    batches.clear();
    state.setSourceLoadFailure(kiriview::VideoSourceLoadFailure {
        sourceUrl,
        kiriview::VideoSourceLoadFailureKind::PlaybackUrlResolution,
        userMessage,
        diagnosticDetail,
        kiriview::VideoSourceLoadFailureSeverity::Error,
        false,
    });

    QVERIFY(state.sourceLoadFailure().has_value());
    QCOMPARE(state.sourceLoadFailure()->sourceUrl, sourceUrl);
    QVERIFY(state.sourceLoadFailure()->kind
        == kiriview::VideoSourceLoadFailureKind::PlaybackUrlResolution);
    QCOMPARE(state.sourceLoadFailure()->userMessage, userMessage);
    QCOMPARE(state.sourceLoadFailure()->diagnosticDetail, diagnosticDetail);
    QVERIFY(state.sourceLoadFailure()->severity == kiriview::VideoSourceLoadFailureSeverity::Error);
    QVERIFY(!state.sourceLoadFailure()->retryable);
    QCOMPARE(state.errorString(), userMessage);
    QCOMPARE(state.status(), kiriview::VideoDocumentStatus::Error);
    compareChanges(batches.front(), { Change::ErrorString, Change::Status });

    batches.clear();
    state.resetForSourceLoad(QUrl::fromLocalFile(QStringLiteral("/videos/next.mp4")));

    QVERIFY(!state.sourceLoadFailure().has_value());
    QCOMPARE(state.errorString(), QString());
    QCOMPARE(state.status(), kiriview::VideoDocumentStatus::Loading);
}

void TestVideoDocumentState::backendFailureStoresTypedFailureAndPublishesUserMessage()
{
    std::vector<std::vector<Change>> batches;
    kiriview::VideoDocumentState state(
        [&batches](const std::vector<Change>& changes) { batches.push_back(changes); });
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/videos/clip.mp4"));
    const QString userMessage = QStringLiteral("Could not open the selected video.");
    const QString backendError = QStringLiteral("Qt Multimedia backend rejected stream");

    state.resetForSourceLoad(sourceUrl);
    batches.clear();
    state.setBackendFailure(kiriview::VideoBackendFailure {
        sourceUrl,
        kiriview::VideoBackendFailureKind::Playback,
        kiriview::VideoMediaErrorCategory::Format,
        2,
        userMessage,
        backendError,
        kiriview::VideoBackendFailureSeverity::Error,
        false,
    });

    QVERIFY(state.backendFailure().has_value());
    QCOMPARE(state.backendFailure()->sourceUrl, sourceUrl);
    QVERIFY(state.backendFailure()->kind == kiriview::VideoBackendFailureKind::Playback);
    QVERIFY(state.backendFailure()->errorCategory == kiriview::VideoMediaErrorCategory::Format);
    QCOMPARE(state.backendFailure()->rawErrorCode, 2);
    QCOMPARE(state.backendFailure()->userMessage, userMessage);
    QCOMPARE(state.backendFailure()->diagnosticDetail, backendError);
    QVERIFY(state.backendFailure()->severity == kiriview::VideoBackendFailureSeverity::Error);
    QVERIFY(!state.backendFailure()->retryable);
    QVERIFY(!state.sourceLoadFailure().has_value());
    QCOMPARE(state.errorString(), userMessage);
    QCOMPARE(state.status(), kiriview::VideoDocumentStatus::Error);
    compareChanges(batches.front(), { Change::ErrorString, Change::Status });

    batches.clear();
    state.setStatusAndError(kiriview::VideoDocumentStatus::Ready);

    QVERIFY(!state.backendFailure().has_value());
    QCOMPARE(state.errorString(), QString());
    QCOMPARE(state.status(), kiriview::VideoDocumentStatus::Ready);
    compareChanges(batches.front(), { Change::ErrorString, Change::Status });

    state.setBackendFailure(kiriview::VideoBackendFailure {
        sourceUrl,
        kiriview::VideoBackendFailureKind::Playback,
        kiriview::VideoMediaErrorCategory::Format,
        2,
        userMessage,
        backendError,
        kiriview::VideoBackendFailureSeverity::Error,
        false,
    });
    state.resetForSourceLoad(QUrl::fromLocalFile(QStringLiteral("/videos/next.mp4")));

    QVERIFY(!state.backendFailure().has_value());
    QCOMPARE(state.errorString(), QString());
    QCOMPARE(state.status(), kiriview::VideoDocumentStatus::Loading);
}

void TestVideoDocumentState::scalarSettersOnlyNotifyOnChangedValues()
{
    std::vector<std::vector<Change>> batches;
    kiriview::VideoDocumentState state(
        [&batches](const std::vector<Change>& changes) { batches.push_back(changes); });

    state.setVideoSize(QSize(-1, 1080));
    QVERIFY(batches.empty());

    state.setHasVideo(true);
    state.setHasVideo(true);
    state.setHasAudio(true);
    state.setHasAudio(true);
    state.setVideoSize(QSize(1920, 1080));
    state.setVideoSize(QSize(1920, 1080));

    QVERIFY(state.hasVideo());
    QVERIFY(state.hasAudio());
    QCOMPARE(state.videoSize(), QSize(1920, 1080));
    compareChanges(flatten(batches), { Change::HasVideo, Change::HasAudio, Change::VideoSize });
}

void TestVideoDocumentState::zoomPercentStatePublishesKnownValuePair()
{
    std::vector<std::vector<Change>> batches;
    kiriview::VideoDocumentState state(
        [&batches](const std::vector<Change>& changes) { batches.push_back(changes); });

    state.setZoomPercent(std::optional<int>(150));

    QVERIFY(state.zoomPercentKnown());
    QCOMPARE(state.zoomPercent(), 150);
    compareChanges(batches.front(), { Change::ZoomPercent, Change::ZoomPercentKnown });

    batches.clear();
    state.setZoomPercent(std::optional<int>(150));
    QVERIFY(batches.empty());

    state.setZoomPercent(std::nullopt);

    QVERIFY(!state.zoomPercentKnown());
    QCOMPARE(state.zoomPercent(), 0);
    compareChanges(batches.front(), { Change::ZoomPercentKnown, Change::ZoomPercent });
}

void TestVideoDocumentState::publishDeduplicatesChangesInOrder()
{
    std::vector<Change> published;
    kiriview::VideoDocumentState state(
        [&published](const std::vector<Change>& changes) { published = changes; });

    state.publish({ Change::HasVideo, Change::Status, Change::HasVideo, Change::HasAudio });

    compareChanges(published, { Change::HasVideo, Change::Status, Change::HasAudio });
}

QTEST_GUILESS_MAIN(TestVideoDocumentState)

#include "test_videodocumentstate.moc"
