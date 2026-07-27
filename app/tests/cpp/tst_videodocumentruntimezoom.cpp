// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videodocumentruntime.h"

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>
#include <QSize>
#include <QTest>
#include <algorithm>
#include <memory>
#include <utility>

class TestVideoDocumentRuntimeZoom : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void remainsUnknownWithoutRenderContext();
    void calculatesZoomWhenVideoOutputHasWindow();
    void attachmentPublishesOutputAndZoomAsOneTransaction();
};

namespace {
class FakeVideoMediaBackend final : public kiriview::VideoMediaBackend
{
public:
    void setCallbacks(kiriview::VideoMediaBackendCallbacks nextCallbacks) override
    {
        callbacks = std::move(nextCallbacks);
    }

    void setSource(const QUrl& nextSourceUrl) override { sourceUrl = nextSourceUrl; }
    void setSourceDevice(QIODevice*, const QUrl& nextSourceUrl) override
    {
        sourceUrl = nextSourceUrl;
    }
    void play() override
    {
        isPlaying = true;
        callbacks.playingChanged();
    }
    void pause() override
    {
        isPlaying = false;
        callbacks.playingChanged();
    }
    void stop() override
    {
        isPlaying = false;
        callbacks.playingChanged();
    }
    void setPosition(qint64 nextPosition) override
    {
        currentPosition = nextPosition;
        callbacks.positionChanged();
    }
    void setMuted(bool nextMuted) override
    {
        isMuted = nextMuted;
        callbacks.mutedChanged();
    }
    void setVideoOutput(QObject* nextVideoOutput) override { output = nextVideoOutput; }
    QObject* videoOutput() const override { return output.data(); }
    kiriview::VideoMediaStatus mediaStatus() const override { return currentStatus; }
    qint64 duration() const override { return 0; }
    qint64 position() const override { return currentPosition; }
    bool playing() const override { return isPlaying; }
    bool seekable() const override { return false; }
    bool hasVideo() const override { return videoAvailable; }
    bool hasAudio() const override { return false; }
    QSize videoSize() const override { return {}; }
    bool muted() const override { return isMuted; }

    void emitStatus(kiriview::VideoMediaStatus status)
    {
        currentStatus = status;
        callbacks.mediaStatusChanged();
    }

    void emitHasVideo(bool hasVideo)
    {
        videoAvailable = hasVideo;
        callbacks.hasVideoChanged();
    }

    kiriview::VideoMediaBackendCallbacks callbacks;
    QUrl sourceUrl;
    QPointer<QObject> output;
    kiriview::VideoMediaStatus currentStatus = kiriview::VideoMediaStatus::Null;
    qint64 currentPosition = 0;
    bool isPlaying = false;
    bool isMuted = false;
    bool videoAvailable = false;
};

class ImmediateVideoPlaybackUrlResolver final : public kiriview::VideoPlaybackUrlResolver
{
public:
    void resolve(quint64 operationId, const QUrl& sourceUrl, QObject*,
        kiriview::VideoPlaybackUrlResolvedCallback resolvedCallback,
        kiriview::VideoPlaybackUrlFailedCallback) override
    {
        resolvedCallback(kiriview::VideoPlaybackUrlResolution {
            operationId,
            sourceUrl,
            sourceUrl,
        });
    }

    void cancel() override { }
    void cleanup() override { }
};

struct RuntimeFixture
{
    QObject documentObject;
    FakeVideoMediaBackend* backend = nullptr;
    std::unique_ptr<kiriview::VideoDocumentRuntime> runtime;

    explicit RuntimeFixture(kiriview::VideoDocumentRuntime::ChangeCallback changeCallback = {})
    {
        runtime = std::make_unique<kiriview::VideoDocumentRuntime>(&documentObject,
            std::move(changeCallback), std::make_unique<ImmediateVideoPlaybackUrlResolver>(),
            [this]() {
                auto mediaBackend = std::make_unique<FakeVideoMediaBackend>();
                backend = mediaBackend.get();
                return mediaBackend;
            });
        runtime->setSourceUrl(QUrl::fromLocalFile(QStringLiteral("/videos/clip.mp4")));
        backend->emitHasVideo(true);
        backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    }
};
}

void TestVideoDocumentRuntimeZoom::remainsUnknownWithoutRenderContext()
{
    RuntimeFixture fixture;
    QObject output;

    fixture.runtime->setVideoOutputAttachment(
        &output, QRectF(0.0, 0.0, 1280.0, 720.0), QRectF(0.0, 0.0, 1280.0, 720.0));

    QVERIFY(!fixture.runtime->zoomPercentKnown());
    QCOMPARE(fixture.runtime->zoomPercent(), 0);
}

void TestVideoDocumentRuntimeZoom::calculatesZoomWhenVideoOutputHasWindow()
{
    RuntimeFixture fixture;
    QQuickWindow window;
    QQuickItem output;
    output.setParentItem(window.contentItem());

    fixture.runtime->setVideoOutputAttachment(
        &output, QRectF(0.0, 0.0, 1280.0, 720.0), QRectF(0.0, 0.0, 1280.0, 720.0));

    QVERIFY(fixture.runtime->zoomPercentKnown());
    QCOMPARE(fixture.runtime->zoomPercent(), 100);
}

void TestVideoDocumentRuntimeZoom::attachmentPublishesOutputAndZoomAsOneTransaction()
{
    std::vector<std::vector<kiriview::VideoDocumentChange>> transactions;
    RuntimeFixture fixture([&](const std::vector<kiriview::VideoDocumentChange>& changes) {
        transactions.push_back(changes);
    });
    transactions.clear();
    QQuickWindow window;
    QQuickItem output;
    output.setParentItem(window.contentItem());

    fixture.runtime->setVideoOutputAttachment(
        &output, QRectF(0.0, 0.0, 1280.0, 720.0), QRectF(0.0, 0.0, 1280.0, 720.0));

    bool observedOutputChange = false;
    bool observedZoomChange = false;
    for (const std::vector<kiriview::VideoDocumentChange>& transaction : transactions) {
        const bool containsOutput
            = std::ranges::contains(transaction, kiriview::VideoDocumentChange::VideoOutput);
        const bool containsZoom
            = std::ranges::contains(transaction, kiriview::VideoDocumentChange::ZoomPercentKnown)
            || std::ranges::contains(transaction, kiriview::VideoDocumentChange::ZoomPercent);
        if (containsOutput) {
            observedOutputChange = true;
            QVERIFY(containsZoom);
        }
        if (containsZoom) {
            observedZoomChange = true;
            QVERIFY(containsOutput);
        }
    }
    QVERIFY(observedOutputChange);
    QVERIFY(observedZoomChange);
}

QTEST_MAIN(TestVideoDocumentRuntimeZoom)

#include "tst_videodocumentruntimezoom.moc"
