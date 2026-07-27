// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videodocumentruntime.h"

#include "image_async_test_support.h"
#include "metadata/embeddedmetadata.h"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

class TestVideoDocumentRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initialStateIsNull();
    void mediaBackendFactoryIsLazyUntilPlaybackUrlResolution();
    void backendFactoryFailureSettlesAndAllowsRetry();
    void playbackUrlResolutionStartsPlayback();
    void localResolverCompletionUsesOriginalPlaybackUrl();
    void settingAndClearingSourcePreservesUserFacingUrlAndTitle();
    void resolverCanReturnSeparatePlaybackUrl();
    void resolvedPlaybackPathPublishesEmbeddedMetadata();
    void resolverFailureSurfacesErrorWithoutChangingSourceUrl();
    void resolverFailurePreservesTypedFailureMetadata();
    void backendFailurePreservesTypedFailureMetadata();
    void backendFailureDoesNotRequireDiagnosticText();
    void backendRecoveryClearsStaleErrorText();
    void sourceDevicePlaybackBypassesResolverAndSkipsMetadata();
    void sourceDeviceOwnerLivesUntilReplacementAndDestruction();
    void defaultBackendCallbackRetainsItsTargetAcrossReentrantReplacement();
    void sourceDevicePlaybackInvalidatesPendingResolverCompletion();
    void staleResolverCompletionsAreIgnored();
    void loadingPublicationReentryAdmitsOnlyLatestResolver();
    void failurePublicationReentryKeepsLatestSourceLoading();
    void backendFactoryReentryRejectsSupersededBackend();
    void metadataPublicationReentryRejectsSupersededBackend();
    void sourceDeviceLoadingPublicationReentryRejectsSupersededDevice();
    void sourceDeviceVideoSizePublicationReentryRejectsSupersededDevice();
    void initialVideoSizeGetterReplacementRejectsSupersededLoad_data();
    void initialVideoSizeGetterReplacementRejectsSupersededLoad();
    void loadingPublicationCanDestroyRuntimeBeforeResolverAdmission();
    void resolverCompletionAfterRuntimeDestructionIsIgnored();
    void resolverCleanupRunsOnSourceChangeAndDestruction();
    void videoSizeFollowsBackendMetadata();
    void backendScalarGetterReplacementRejectsSupersededValue_data();
    void backendScalarGetterReplacementRejectsSupersededValue();
    void backendScalarGetterDestructionStopsBeforeCommit_data();
    void backendScalarGetterDestructionStopsBeforeCommit();
    void nestedBackendObservationKeepsLatestPlaybackFacts();
    void staleBackendCallbacksAfterSourceChangeAreIgnored();
    void supersededBackendLifecycleEventsAfterReplacementAreIgnored();
    void mutedStateDispatchesBackendAndPersistsAcrossSourceChanges();
    void muteReentryKeepsLatestIntent();
    void playbackControlsDispatchBackendOperations();
    void playbackControlScrubCommitsWithoutBackendOverwrite();
    void scrubCommitReentryDoesNotSeekReplacementSource();
    void playbackControlSeekReentryDoesNotSeekReplacementSource();
    void scrubCommitReentryPreservesSeekOrder();
    void seekProjectionReentryPreservesPositionOrder();
    void backendSeekReentryPreservesPositionOrder();
    void seekProjectionReentryQueuesStopBehindSeek();
    void adjustedSeekLandingRemainsAuthoritative();
    void seekGateRevocationDoesNotRevivePendingIntent();
    void environmentProjectionReentryPreservesAcceptedSeek();
    void scrubCommitPublicationCanDestroyRuntime();
    void playbackControlSeekPublicationCanDestroyRuntime();
    void backendSeekCallbackCanDestroyRuntime();
    void playbackControlAutoHideUsesInjectedTimer();
    void naturalPlaybackEndKeepsPresentationReadyWithoutBackendStop();
    void playAfterEndOfMediaRestartsFromBeginningWhenSeekable();
    void endedPlaybackRestartQueuesReentrantPause_data();
    void endedPlaybackRestartQueuesReentrantPause();
    void endedPlaybackRestartCompletesBeforeQueuedSeek();
    void endedPlaybackRestartDoesNotContinueOnReplacementBackend();
    void stopReentryReplansQueuedPositionCommand_data();
    void stopReentryReplansQueuedPositionCommand();
    void stopCompletesBeforeQueuedPlay();
    void positionCommandReentryPreservesPositionOrder_data();
    void positionCommandReentryPreservesPositionOrder();
    void playbackCommandDrainYieldsUnderSustainedReentry();
    void seekByClampsToKnownDuration();
    void seekByNoopsWhenNotSeekable();
    void videoOutputDetachAndDestructionClearBackendOutput();
    void backendReplacementDuringOutputEffectConverges_data();
    void backendReplacementDuringOutputEffectConverges();
};

namespace {
class FakeVideoMediaBackend final : public kiriview::VideoMediaBackend
{
public:
    ~FakeVideoMediaBackend() override
    {
        if (destroyed) {
            destroyed(this);
        }
    }

    void setCallbacks(kiriview::VideoMediaBackendCallbacks nextCallbacks) override
    {
        callbacks = std::move(nextCallbacks);
    }

    void setSource(const QUrl& nextSourceUrl) override
    {
        sourceUrl = nextSourceUrl;
        sourceDevice = nullptr;
        sourceDeviceUrl = QUrl();
        ++setSourceCount;
    }

    void setSourceDevice(QIODevice* nextDevice, const QUrl& nextSourceUrl) override
    {
        sourceDevice = nextDevice;
        sourceDeviceUrl = nextSourceUrl;
        sourceUrl = QUrl();
        ++setSourceDeviceCount;
    }

    void play() override
    {
        isPlaying = true;
        ++playCount;
        callbacks.playingChanged();
    }

    void pause() override
    {
        isPlaying = false;
        ++pauseCount;
        callbacks.playingChanged();
    }

    void stop() override
    {
        isPlaying = false;
        ++stopCount;
        callbacks.playingChanged();
    }

    void setPosition(qint64 nextPosition) override
    {
        currentPosition = nextPosition;
        ++setPositionCount;
        setPositionRequests.push_back(nextPosition);
        const std::function<void(qint64)> hook = setPositionHook;
        const std::function<void()> positionChanged = callbacks.positionChanged;
        if (hook) {
            hook(nextPosition);
        }
        if (positionChanged) {
            positionChanged();
        }
    }

    void setMuted(bool nextMuted) override
    {
        isMuted = nextMuted;
        ++setMutedCount;
        callbacks.mutedChanged();
    }

    void setVideoOutput(QObject* nextVideoOutput) override
    {
        output = nextVideoOutput;
        ++setVideoOutputCount;
        const std::function<void(QObject*)> hook = setVideoOutputHook;
        if (hook) {
            hook(nextVideoOutput);
        }
    }

    QObject* videoOutput() const override { return output.data(); }
    kiriview::VideoMediaStatus mediaStatus() const override { return currentStatus; }
    qint64 duration() const override { return currentDuration; }
    qint64 position() const override { return currentPosition; }
    bool playing() const override { return isPlaying; }
    bool seekable() const override { return isSeekable; }
    bool hasVideo() const override
    {
        const bool value = videoAvailable;
        const std::function<void()> hook = hasVideoGetterHook;
        if (hook) {
            hook();
        }
        return value;
    }

    bool hasAudio() const override
    {
        const bool value = audioAvailable;
        const std::function<void()> hook = hasAudioGetterHook;
        if (hook) {
            hook();
        }
        return value;
    }

    QSize videoSize() const override
    {
        const QSize value = currentVideoSize;
        const std::function<void()> hook = videoSizeGetterHook;
        if (hook) {
            hook();
        }
        return value;
    }
    bool muted() const override { return isMuted; }

    void emitStatus(kiriview::VideoMediaStatus status)
    {
        currentStatus = status;
        callbacks.mediaStatusChanged();
    }

    void emitError(
        kiriview::VideoMediaErrorCategory category, int rawErrorCode, QString diagnosticDetail)
    {
        callbacks.errorOccurred(kiriview::VideoMediaError {
            category,
            rawErrorCode,
            std::move(diagnosticDetail),
        });
    }

    void emitDuration(qint64 duration)
    {
        currentDuration = duration;
        callbacks.durationChanged();
    }

    void emitPosition(qint64 position)
    {
        currentPosition = position;
        callbacks.positionChanged();
    }

    void emitSeekable(bool seekable)
    {
        isSeekable = seekable;
        callbacks.seekableChanged();
    }

    void emitVideoSize(QSize size)
    {
        currentVideoSize = size;
        callbacks.videoSizeChanged();
    }

    void emitHasVideo(bool hasVideo)
    {
        videoAvailable = hasVideo;
        callbacks.hasVideoChanged();
    }

    QUrl sourceUrl;
    QPointer<QIODevice> sourceDevice;
    QUrl sourceDeviceUrl;
    QPointer<QObject> output;
    kiriview::VideoMediaBackendCallbacks callbacks;
    kiriview::VideoMediaStatus currentStatus = kiriview::VideoMediaStatus::Null;
    qint64 currentDuration = 0;
    qint64 currentPosition = 0;
    bool isPlaying = false;
    bool isSeekable = false;
    bool isMuted = false;
    bool videoAvailable = false;
    bool audioAvailable = false;
    QSize currentVideoSize;
    std::function<void(qint64)> setPositionHook;
    std::function<void(QObject*)> setVideoOutputHook;
    std::function<void()> hasVideoGetterHook;
    std::function<void()> hasAudioGetterHook;
    std::function<void()> videoSizeGetterHook;
    std::vector<qint64> setPositionRequests;
    int setSourceCount = 0;
    int setSourceDeviceCount = 0;
    int setPositionCount = 0;
    int setMutedCount = 0;
    int setVideoOutputCount = 0;
    int playCount = 0;
    int pauseCount = 0;
    int stopCount = 0;
    std::function<void(FakeVideoMediaBackend*)> destroyed;
};

struct FakeResolverState
{
    struct Request
    {
        quint64 operationId = 0;
        QUrl sourceUrl;
        kiriview::VideoPlaybackUrlResolvedCallback resolvedCallback;
        kiriview::VideoPlaybackUrlFailedCallback failedCallback;
    };

    std::vector<Request> requests;
    int cancelCount = 0;
    int cleanupCount = 0;
};

enum class BackendScalarGetter {
    HasVideo,
    HasAudio,
    VideoSize,
};

class FakeVideoPlaybackUrlResolver final : public kiriview::VideoPlaybackUrlResolver
{
public:
    explicit FakeVideoPlaybackUrlResolver(std::shared_ptr<FakeResolverState> resolverState)
        : state(std::move(resolverState))
    {
    }

    void resolve(quint64 operationId, const QUrl& sourceUrl, QObject*,
        kiriview::VideoPlaybackUrlResolvedCallback resolvedCallback,
        kiriview::VideoPlaybackUrlFailedCallback failedCallback) override
    {
        state->requests.push_back(FakeResolverState::Request {
            operationId, sourceUrl, std::move(resolvedCallback), std::move(failedCallback) });
    }

    void cancel() override { ++state->cancelCount; }
    void cleanup() override { ++state->cleanupCount; }

private:
    std::shared_ptr<FakeResolverState> state;
};

struct RuntimeFixture
{
    QObject documentObject;
    FakeVideoMediaBackend* backend = nullptr;
    std::shared_ptr<FakeResolverState> resolverState = std::make_shared<FakeResolverState>();
    std::vector<kiriview::VideoDocumentChange> changes;
    std::function<void(const std::vector<kiriview::VideoDocumentChange>&)> changeHook;
    std::function<void()> backendFactoryHook;
    std::function<void(const kiriview::VideoPlaybackControlProjection&)> projectionHook;
    std::unique_ptr<kiriview::VideoDocumentRuntime> runtime;

    explicit RuntimeFixture(kiriview::TimerScheduler playbackControlTimerScheduler = {})
    {
        runtime = std::make_unique<kiriview::VideoDocumentRuntime>(
            &documentObject,
            [this](const std::vector<kiriview::VideoDocumentChange>& nextChanges) {
                changes.insert(changes.end(), nextChanges.begin(), nextChanges.end());
                if (changeHook) {
                    changeHook(nextChanges);
                }
            },
            std::make_unique<FakeVideoPlaybackUrlResolver>(resolverState),
            [this]() {
                auto mediaBackend = std::make_unique<FakeVideoMediaBackend>();
                backend = mediaBackend.get();
                mediaBackend->destroyed = [this](FakeVideoMediaBackend* destroyedBackend) {
                    if (backend == destroyedBackend) {
                        backend = nullptr;
                    }
                };
                if (backendFactoryHook) {
                    backendFactoryHook();
                }
                return mediaBackend;
            },
            std::move(playbackControlTimerScheduler),
            [this](const kiriview::VideoPlaybackControlProjection& projection) {
                if (projectionHook) {
                    projectionHook(projection);
                }
            });
    }

    void resolveLatest(const QUrl& playbackUrl)
    {
        auto& request = resolverState->requests.back();
        request.resolvedCallback(kiriview::VideoPlaybackUrlResolution {
            request.operationId,
            request.sourceUrl,
            playbackUrl,
        });
    }

    void failLatest(const QString& errorString)
    {
        auto& request = resolverState->requests.back();
        request.failedCallback(request.operationId, request.sourceUrl, errorString);
    }
};

void prepareReadySeekableVideo(
    RuntimeFixture& fixture, qint64 durationMsec = 90000, qint64 positionMsec = 12000)
{
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(durationMsec);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(positionMsec);
}

void appendU32(QByteArray& data, quint32 value)
{
    data.append(static_cast<char>((value >> 24) & 0xff));
    data.append(static_cast<char>((value >> 16) & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
    data.append(static_cast<char>(value & 0xff));
}

void appendBox(QByteArray& data, const char* kind, const QByteArray& payload)
{
    appendU32(data, static_cast<quint32>(payload.size() + 8));
    data.append(kind, 4);
    data.append(payload);
}

bool writeTinyMetadataMp4(const QString& path)
{
    QByteArray ftypPayload;
    ftypPayload.append("isom", 4);
    ftypPayload.append(4, '\0');
    ftypPayload.append("isomiso2mp41", 12);

    QByteArray mvhdPayload(12, '\0');
    appendU32(mvhdPayload, 1000);
    appendU32(mvhdPayload, 1234);
    mvhdPayload.append(80, '\0');

    QByteArray tkhdPayload;
    tkhdPayload.append('\0');
    tkhdPayload.append('\0');
    tkhdPayload.append('\0');
    tkhdPayload.append('\7');
    tkhdPayload.append(16, '\0');
    appendU32(tkhdPayload, 1234);
    tkhdPayload.append(52, '\0');
    appendU32(tkhdPayload, 640 << 16);
    appendU32(tkhdPayload, 360 << 16);

    QByteArray trakPayload;
    appendBox(trakPayload, "tkhd", tkhdPayload);
    QByteArray hdlrPayload;
    hdlrPayload.append(8, '\0');
    hdlrPayload.append("vide", 4);
    hdlrPayload.append(12, '\0');
    QByteArray mdiaPayload;
    appendBox(mdiaPayload, "hdlr", hdlrPayload);
    appendBox(trakPayload, "mdia", mdiaPayload);

    QByteArray moovPayload;
    appendBox(moovPayload, "mvhd", mvhdPayload);
    appendBox(moovPayload, "trak", trakPayload);

    QByteArray data;
    appendBox(data, "ftyp", ftypPayload);
    appendBox(data, "moov", moovPayload);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}

struct SourceDeviceOwner
{
    explicit SourceDeviceOwner(int* destructionCount)
        : destructionCount(destructionCount)
    {
    }

    ~SourceDeviceOwner()
    {
        if (destructionCount != nullptr) {
            ++*destructionCount;
        }
    }

    int* destructionCount = nullptr;
};

kiriview::VideoPlaybackSourceDevice makePlaybackSourceDevice(
    std::shared_ptr<void> owner, QByteArray data = QByteArrayLiteral("video-bytes"))
{
    auto buffer = std::make_unique<QBuffer>();
    buffer->setData(std::move(data));
    buffer->open(QIODevice::ReadOnly);
    return kiriview::VideoPlaybackSourceDevice { std::move(owner), std::move(buffer) };
}
}

void TestVideoDocumentRuntime::initialStateIsNull()
{
    RuntimeFixture fixture;

    QCOMPARE(fixture.runtime->sourceUrl(), QUrl());
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Null);
    QCOMPARE(fixture.runtime->errorString(), QString());
    QCOMPARE(fixture.runtime->windowTitleFileName(), QString());
    QCOMPARE(fixture.runtime->duration(), 0);
    QCOMPARE(fixture.runtime->position(), 0);
    QVERIFY(!fixture.runtime->playing());
    QVERIFY(!fixture.runtime->seekable());
    QVERIFY(!fixture.runtime->hasVideo());
    QVERIFY(!fixture.runtime->hasAudio());
    QCOMPARE(fixture.runtime->videoSize(), QSize());
    QVERIFY(!fixture.runtime->muted());
    QCOMPARE(fixture.runtime->videoOutput(), nullptr);
}

void TestVideoDocumentRuntime::mediaBackendFactoryIsLazyUntilPlaybackUrlResolution()
{
    QObject documentObject;
    QObject output;
    auto resolverState = std::make_shared<FakeResolverState>();
    FakeVideoMediaBackend* backend = nullptr;
    int factoryCallCount = 0;
    kiriview::VideoDocumentRuntime runtime(
        &documentObject, {}, std::make_unique<FakeVideoPlaybackUrlResolver>(resolverState), [&] {
            ++factoryCallCount;
            auto mediaBackend = std::make_unique<FakeVideoMediaBackend>();
            backend = mediaBackend.get();
            return mediaBackend;
        });
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    QCOMPARE(factoryCallCount, 0);
    runtime.setVideoOutputAttachment(&output, {}, {});
    QCOMPARE(factoryCallCount, 0);
    runtime.setMuted(true);
    QVERIFY(runtime.muted());
    QCOMPARE(factoryCallCount, 0);

    runtime.setSourceUrl(sourceUrl);
    QCOMPARE(factoryCallCount, 0);
    QCOMPARE(runtime.status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(resolverState->requests.size(), std::size_t(1));

    auto& request = resolverState->requests.back();
    request.resolvedCallback(kiriview::VideoPlaybackUrlResolution {
        request.operationId,
        request.sourceUrl,
        sourceUrl,
    });

    QCOMPARE(factoryCallCount, 1);
    QVERIFY(backend != nullptr);
    QCOMPARE(backend->sourceUrl, sourceUrl);
    QVERIFY(backend->isMuted);
    QCOMPARE(backend->setMutedCount, 1);
    QCOMPARE(backend->videoOutput(), &output);
}

void TestVideoDocumentRuntime::backendFactoryFailureSettlesAndAllowsRetry()
{
    QObject documentObject;
    auto resolverState = std::make_shared<FakeResolverState>();
    kiriview::VideoDocumentRuntime runtime(&documentObject, {},
        std::make_unique<FakeVideoPlaybackUrlResolver>(resolverState),
        []() -> std::unique_ptr<kiriview::VideoMediaBackend> { return nullptr; });
    const QUrl sourceUrl(QStringLiteral("zip:///home/me/videos.zip!/clip.mp4"));
    const QUrl playbackUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/clip.mp4"));

    runtime.setSourceUrl(sourceUrl);
    const FakeResolverState::Request request = resolverState->requests.back();
    request.resolvedCallback(kiriview::VideoPlaybackUrlResolution {
        request.operationId,
        request.sourceUrl,
        playbackUrl,
    });

    QCOMPARE(runtime.status(), kiriview::VideoDocumentStatus::Error);
    QVERIFY(runtime.sourceLoadFailure().has_value());
    QCOMPARE(runtime.sourceLoadFailure()->sourceUrl, sourceUrl);
    QVERIFY(runtime.sourceLoadFailure()->kind
        == kiriview::VideoSourceLoadFailureKind::PlaybackBackendCreation);

    runtime.setSourceUrl(sourceUrl);

    QCOMPARE(runtime.status(), kiriview::VideoDocumentStatus::Loading);
    QVERIFY(!runtime.sourceLoadFailure().has_value());
    QCOMPARE(resolverState->requests.size(), std::size_t(2));
}

void TestVideoDocumentRuntime::playbackUrlResolutionStartsPlayback()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);

    QCOMPARE(fixture.backend->playCount, 1);
    QVERIFY(fixture.runtime->playing());
}

void TestVideoDocumentRuntime::localResolverCompletionUsesOriginalPlaybackUrl()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);

    QCOMPARE(fixture.runtime->sourceUrl(), sourceUrl);
    QCOMPARE(fixture.backend->sourceUrl, sourceUrl);
}

void TestVideoDocumentRuntime::settingAndClearingSourcePreservesUserFacingUrlAndTitle()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl(QStringLiteral("zip:///home/me/videos.zip!/chapter/clip.mov"));

    fixture.runtime->setSourceUrl(sourceUrl);
    QCOMPARE(fixture.runtime->sourceUrl(), sourceUrl);
    QCOMPARE(fixture.runtime->windowTitleFileName(), QStringLiteral("clip.mov"));
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.resolverState->requests.size(), std::size_t(1));

    const QUrl playbackUrl
        = QUrl::fromLocalFile(QStringLiteral("/run/user/1000/kio-fuse/clip.mov"));
    fixture.resolveLatest(playbackUrl);
    QCOMPARE(fixture.runtime->sourceUrl(), sourceUrl);
    QCOMPARE(fixture.backend->sourceUrl, playbackUrl);

    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Ready);

    fixture.runtime->setSourceUrl(QUrl());
    QCOMPARE(fixture.runtime->sourceUrl(), QUrl());
    QCOMPARE(fixture.runtime->windowTitleFileName(), QString());
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Null);
    QCOMPARE(fixture.backend, nullptr);
}

void TestVideoDocumentRuntime::resolverCanReturnSeparatePlaybackUrl()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl(QStringLiteral("zip:///home/me/videos.zip!/clip.mp4"));
    const QUrl playbackUrl
        = QUrl::fromLocalFile(QStringLiteral("/run/user/1000/kio-fuse/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(playbackUrl);

    QCOMPARE(fixture.runtime->sourceUrl(), sourceUrl);
    QCOMPARE(fixture.backend->sourceUrl, playbackUrl);
}

void TestVideoDocumentRuntime::resolvedPlaybackPathPublishesEmbeddedMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString playbackPath = directory.filePath(QStringLiteral("resolved.mp4"));
    QVERIFY(writeTinyMetadataMp4(playbackPath));

    RuntimeFixture fixture;
    const QUrl sourceUrl(QStringLiteral("zip:///books/book.zip!/clip.mp4"));
    const QUrl playbackUrl = QUrl::fromLocalFile(playbackPath);

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(playbackUrl);

    const kiriview::EmbeddedMetadata metadata = fixture.runtime->embeddedMetadata();
    QCOMPARE(metadata.duration, QStringLiteral("00:00:01.234"));
    QCOMPARE(metadata.frameSize, QStringLiteral("640×360 px"));
}

void TestVideoDocumentRuntime::resolverFailureSurfacesErrorWithoutChangingSourceUrl()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl(QStringLiteral("zip:///home/me/videos.zip!/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.failLatest(QStringLiteral("DBus backend detail"));

    QCOMPARE(fixture.runtime->sourceUrl(), sourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Error);
    QCOMPARE(fixture.runtime->errorString(), QStringLiteral("Could not open the selected video."));
    QCOMPARE(fixture.backend, nullptr);
}

void TestVideoDocumentRuntime::resolverFailurePreservesTypedFailureMetadata()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl(QStringLiteral("zip:///home/me/videos.zip!/clip.mp4"));
    const QString diagnosticDetail = QStringLiteral("KIO resolver failed");

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.failLatest(diagnosticDetail);

    QVERIFY(fixture.runtime->sourceLoadFailure().has_value());
    QCOMPARE(fixture.runtime->sourceLoadFailure()->sourceUrl, sourceUrl);
    QVERIFY(fixture.runtime->sourceLoadFailure()->kind
        == kiriview::VideoSourceLoadFailureKind::PlaybackUrlResolution);
    QCOMPARE(fixture.runtime->sourceLoadFailure()->userMessage,
        QStringLiteral("Could not open the selected video."));
    QCOMPARE(fixture.runtime->sourceLoadFailure()->diagnosticDetail, diagnosticDetail);
    QVERIFY(fixture.runtime->sourceLoadFailure()->severity
        == kiriview::VideoSourceLoadFailureSeverity::Error);
    QVERIFY(!fixture.runtime->sourceLoadFailure()->retryable);
    QCOMPARE(fixture.runtime->errorString(), fixture.runtime->sourceLoadFailure()->userMessage);

    fixture.runtime->setSourceUrl(QUrl::fromLocalFile(QStringLiteral("/home/me/next.mp4")));

    QVERIFY(!fixture.runtime->sourceLoadFailure().has_value());
    QCOMPARE(fixture.runtime->errorString(), QString());
}

void TestVideoDocumentRuntime::backendFailurePreservesTypedFailureMetadata()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    const QString backendError = QStringLiteral("Qt Multimedia backend failed");

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitError(kiriview::VideoMediaErrorCategory::Format, 2, backendError);

    QVERIFY(fixture.runtime->backendFailure().has_value());
    QCOMPARE(fixture.runtime->backendFailure()->sourceUrl, sourceUrl);
    QVERIFY(fixture.runtime->backendFailure()->kind == kiriview::VideoBackendFailureKind::Playback);
    QVERIFY(fixture.runtime->backendFailure()->errorCategory
        == kiriview::VideoMediaErrorCategory::Format);
    QCOMPARE(fixture.runtime->backendFailure()->rawErrorCode, 2);
    QCOMPARE(fixture.runtime->backendFailure()->userMessage,
        QStringLiteral("Could not open the selected video."));
    QCOMPARE(fixture.runtime->backendFailure()->diagnosticDetail, backendError);
    QVERIFY(fixture.runtime->backendFailure()->severity
        == kiriview::VideoBackendFailureSeverity::Error);
    QVERIFY(!fixture.runtime->backendFailure()->retryable);
    QVERIFY(!fixture.runtime->sourceLoadFailure().has_value());
    QCOMPARE(fixture.runtime->errorString(), QStringLiteral("Could not open the selected video."));

    fixture.runtime->setSourceUrl(QUrl::fromLocalFile(QStringLiteral("/home/me/next.mp4")));

    QVERIFY(!fixture.runtime->backendFailure().has_value());
    QCOMPARE(fixture.runtime->errorString(), QString());
}

void TestVideoDocumentRuntime::backendFailureDoesNotRequireDiagnosticText()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitError(kiriview::VideoMediaErrorCategory::Unknown, 77, QString());

    QVERIFY(fixture.runtime->backendFailure().has_value());
    QVERIFY(fixture.runtime->backendFailure()->errorCategory
        == kiriview::VideoMediaErrorCategory::Unknown);
    QCOMPARE(fixture.runtime->backendFailure()->rawErrorCode, 77);
    QVERIFY(fixture.runtime->backendFailure()->diagnosticDetail.isEmpty());
    QCOMPARE(fixture.runtime->errorString(), QStringLiteral("Could not open the selected video."));
}

void TestVideoDocumentRuntime::backendRecoveryClearsStaleErrorText()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitError(
        kiriview::VideoMediaErrorCategory::Resource, 1, QStringLiteral("backend failed"));

    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Error);
    QCOMPARE(fixture.runtime->errorString(), QStringLiteral("Could not open the selected video."));
    QVERIFY(fixture.runtime->backendFailure().has_value());

    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);

    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Ready);
    QCOMPARE(fixture.runtime->errorString(), QString());
    QVERIFY(!fixture.runtime->backendFailure().has_value());
}

void TestVideoDocumentRuntime::sourceDevicePlaybackBypassesResolverAndSkipsMetadata()
{
    RuntimeFixture fixture;
    int destructionCount = 0;
    const QUrl sourceUrl(QStringLiteral("zip:///home/me/videos.zip!/chapter/clip.mp4"));

    fixture.runtime->setSourceDevice(sourceUrl,
        makePlaybackSourceDevice(std::make_shared<SourceDeviceOwner>(&destructionCount)));

    QCOMPARE(fixture.resolverState->requests.size(), std::size_t(0));
    QCOMPARE(fixture.runtime->sourceUrl(), sourceUrl);
    QCOMPARE(fixture.runtime->windowTitleFileName(), QStringLiteral("clip.mp4"));
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.backend->sourceUrl, QUrl());
    QCOMPARE(fixture.backend->sourceDeviceUrl, sourceUrl);
    QVERIFY(fixture.backend->sourceDevice != nullptr);
    QCOMPARE(fixture.backend->setSourceDeviceCount, 1);
    QCOMPARE(fixture.backend->playCount, 1);
    QVERIFY(fixture.runtime->embeddedMetadata().isEmpty());
    QCOMPARE(destructionCount, 0);
}

void TestVideoDocumentRuntime::sourceDeviceOwnerLivesUntilReplacementAndDestruction()
{
    int replacedDestructionCount = 0;
    std::weak_ptr<SourceDeviceOwner> replacedOwner;
    RuntimeFixture fixture;
    const QUrl sourceUrl(QStringLiteral("zip:///home/me/videos.zip!/chapter/clip.mp4"));
    const QUrl directUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/replacement.mp4"));

    {
        auto owner = std::make_shared<SourceDeviceOwner>(&replacedDestructionCount);
        replacedOwner = owner;
        fixture.runtime->setSourceDevice(sourceUrl, makePlaybackSourceDevice(owner));
    }

    QVERIFY(!replacedOwner.expired());
    QCOMPARE(replacedDestructionCount, 0);

    fixture.runtime->setSourceUrl(directUrl);

    QVERIFY(replacedOwner.expired());
    QCOMPARE(replacedDestructionCount, 1);
    QCOMPARE(fixture.backend, nullptr);

    int destructionDestructionCount = 0;
    std::weak_ptr<SourceDeviceOwner> destructionOwner;
    {
        RuntimeFixture destructionFixture;
        auto owner = std::make_shared<SourceDeviceOwner>(&destructionDestructionCount);
        destructionOwner = owner;
        destructionFixture.runtime->setSourceDevice(sourceUrl, makePlaybackSourceDevice(owner));
        QVERIFY(!destructionOwner.expired());
        QCOMPARE(destructionDestructionCount, 0);
    }

    QVERIFY(destructionOwner.expired());
    QCOMPARE(destructionDestructionCount, 1);
}

void TestVideoDocumentRuntime::defaultBackendCallbackRetainsItsTargetAcrossReentrantReplacement()
{
    std::unique_ptr<kiriview::VideoMediaBackend> backend
        = kiriview::createDefaultVideoMediaBackend();
    std::shared_ptr<void> callbackLease = std::make_shared<char>();
    const std::weak_ptr<void> callbackLifetime = callbackLease;
    bool callbackInvoked = false;
    bool callbackTargetRetained = false;

    kiriview::VideoMediaBackendCallbacks callbacks;
    callbacks.mutedChanged
        = [callbackLease = std::move(callbackLease), backend = backend.get(),
              callbackLifetime = &callbackLifetime, callbackInvoked = &callbackInvoked,
              callbackTargetRetained = &callbackTargetRetained]() {
              kiriview::VideoMediaBackend* currentBackend = backend;
              const std::weak_ptr<void>* currentLifetime = callbackLifetime;
              bool* currentInvoked = callbackInvoked;
              bool* currentTargetRetained = callbackTargetRetained;
              currentBackend->setCallbacks({});
              *currentInvoked = true;
              *currentTargetRetained = !currentLifetime->expired();
          };
    backend->setCallbacks(std::move(callbacks));

    backend->setMuted(!backend->muted());

    QVERIFY(callbackInvoked);
    QVERIFY(callbackTargetRetained);
}

void TestVideoDocumentRuntime::sourceDevicePlaybackInvalidatesPendingResolverCompletion()
{
    RuntimeFixture fixture;
    const QUrl directUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/direct.mp4"));
    const QUrl stalePlaybackUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/stale.mp4"));
    const QUrl collectionUrl(QStringLiteral("zip:///home/me/videos.zip!/chapter/clip.mp4"));
    int destructionCount = 0;

    fixture.runtime->setSourceUrl(directUrl);
    QCOMPARE(fixture.resolverState->requests.size(), std::size_t(1));
    auto request = fixture.resolverState->requests.back();

    fixture.runtime->setSourceDevice(collectionUrl,
        makePlaybackSourceDevice(std::make_shared<SourceDeviceOwner>(&destructionCount)));
    request.resolvedCallback(kiriview::VideoPlaybackUrlResolution {
        request.operationId,
        request.sourceUrl,
        stalePlaybackUrl,
    });

    QCOMPARE(fixture.runtime->sourceUrl(), collectionUrl);
    QCOMPARE(fixture.backend->sourceUrl, QUrl());
    QCOMPARE(fixture.backend->sourceDeviceUrl, collectionUrl);
    QVERIFY(fixture.backend->sourceDevice != nullptr);
    QCOMPARE(fixture.backend->setSourceDeviceCount, 1);
    QCOMPARE(destructionCount, 0);
}

void TestVideoDocumentRuntime::staleResolverCompletionsAreIgnored()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/first.mp4"));
    const QUrl secondSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/second.mp4"));
    const QUrl stalePlaybackUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/first.mp4"));
    const QUrl currentPlaybackUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/second.mp4"));

    fixture.runtime->setSourceUrl(firstSourceUrl);
    fixture.runtime->setSourceUrl(secondSourceUrl);

    auto& firstRequest = fixture.resolverState->requests.front();
    firstRequest.resolvedCallback(kiriview::VideoPlaybackUrlResolution {
        firstRequest.operationId,
        firstRequest.sourceUrl,
        stalePlaybackUrl,
    });
    QCOMPARE(fixture.runtime->sourceUrl(), secondSourceUrl);
    QCOMPARE(fixture.backend, nullptr);

    fixture.resolveLatest(currentPlaybackUrl);
    QCOMPARE(fixture.runtime->sourceUrl(), secondSourceUrl);
    QCOMPARE(fixture.backend->sourceUrl, currentPlaybackUrl);
}

void TestVideoDocumentRuntime::loadingPublicationReentryAdmitsOnlyLatestResolver()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/first.mp4"));
    const QUrl secondSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/second.mp4"));
    bool reentered = false;
    fixture.changeHook = [&](const std::vector<kiriview::VideoDocumentChange>& changes) {
        if (reentered || !std::ranges::contains(changes, kiriview::VideoDocumentChange::SourceUrl)
            || fixture.runtime->sourceUrl() != firstSourceUrl) {
            return;
        }

        reentered = true;
        fixture.runtime->setSourceUrl(secondSourceUrl);
    };

    fixture.runtime->setSourceUrl(firstSourceUrl);

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), secondSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.resolverState->requests.size(), std::size_t(1));
    QCOMPARE(fixture.resolverState->requests.front().sourceUrl, secondSourceUrl);
}

void TestVideoDocumentRuntime::failurePublicationReentryKeepsLatestSourceLoading()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/first.mp4"));
    const QUrl secondSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/second.mp4"));
    fixture.runtime->setSourceUrl(firstSourceUrl);
    const FakeResolverState::Request firstRequest = fixture.resolverState->requests.front();

    bool reentered = false;
    fixture.changeHook = [&](const std::vector<kiriview::VideoDocumentChange>& changes) {
        if (reentered || changes.empty() || fixture.runtime->sourceUrl() != firstSourceUrl) {
            return;
        }

        reentered = true;
        fixture.runtime->setSourceUrl(secondSourceUrl);
    };

    firstRequest.failedCallback(
        firstRequest.operationId, firstRequest.sourceUrl, QStringLiteral("first failed"));

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), secondSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.runtime->errorString(), QString());
    QVERIFY(!fixture.runtime->sourceLoadFailure().has_value());
    QCOMPARE(fixture.resolverState->requests.size(), std::size_t(2));
    QCOMPARE(fixture.resolverState->requests.back().sourceUrl, secondSourceUrl);
}

void TestVideoDocumentRuntime::backendFactoryReentryRejectsSupersededBackend()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/first.mp4"));
    const QUrl firstPlaybackUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/first.mp4"));
    const QUrl secondSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/second.mp4"));
    fixture.runtime->setSourceUrl(firstSourceUrl);

    bool reentered = false;
    fixture.backendFactoryHook = [&] {
        if (reentered || fixture.runtime->sourceUrl() != firstSourceUrl) {
            return;
        }

        reentered = true;
        fixture.runtime->setSourceUrl(secondSourceUrl);
    };

    fixture.resolveLatest(firstPlaybackUrl);

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), secondSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.backend, nullptr);
    QVERIFY(fixture.runtime->embeddedMetadata().isEmpty());
    QCOMPARE(fixture.resolverState->requests.size(), std::size_t(2));
    QCOMPARE(fixture.resolverState->requests.back().sourceUrl, secondSourceUrl);
}

void TestVideoDocumentRuntime::metadataPublicationReentryRejectsSupersededBackend()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/first.mp4"));
    const QUrl firstPlaybackUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/first.mp4"));
    const QUrl secondSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/second.mp4"));
    fixture.runtime->setSourceUrl(firstSourceUrl);

    bool reentered = false;
    fixture.changeHook = [&](const std::vector<kiriview::VideoDocumentChange>& changes) {
        if (reentered
            || !std::ranges::contains(changes, kiriview::VideoDocumentChange::EmbeddedMetadata)
            || fixture.runtime->sourceUrl() != firstSourceUrl) {
            return;
        }

        reentered = true;
        fixture.runtime->setSourceUrl(secondSourceUrl);
    };

    fixture.resolveLatest(firstPlaybackUrl);

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), secondSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.backend, nullptr);
    QVERIFY(fixture.runtime->embeddedMetadata().isEmpty());
    QCOMPARE(fixture.resolverState->requests.size(), std::size_t(2));
    QCOMPARE(fixture.resolverState->requests.back().sourceUrl, secondSourceUrl);
}

void TestVideoDocumentRuntime::sourceDeviceLoadingPublicationReentryRejectsSupersededDevice()
{
    RuntimeFixture fixture;
    const QUrl collectionSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/collection.mp4"));
    const QUrl replacementSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/replacement.mp4"));
    int destructionCount = 0;
    bool reentered = false;
    fixture.changeHook = [&](const std::vector<kiriview::VideoDocumentChange>& changes) {
        if (reentered || !std::ranges::contains(changes, kiriview::VideoDocumentChange::SourceUrl)
            || fixture.runtime->sourceUrl() != collectionSourceUrl) {
            return;
        }

        reentered = true;
        fixture.runtime->setSourceUrl(replacementSourceUrl);
    };

    fixture.runtime->setSourceDevice(collectionSourceUrl,
        makePlaybackSourceDevice(std::make_shared<SourceDeviceOwner>(&destructionCount)));

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), replacementSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.backend, nullptr);
    QCOMPARE(destructionCount, 1);
    QCOMPARE(fixture.resolverState->requests.size(), std::size_t(1));
    QCOMPARE(fixture.resolverState->requests.back().sourceUrl, replacementSourceUrl);
}

void TestVideoDocumentRuntime::sourceDeviceVideoSizePublicationReentryRejectsSupersededDevice()
{
    RuntimeFixture fixture;
    const QUrl collectionSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/collection.mp4"));
    const QUrl replacementSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/replacement.mp4"));
    int destructionCount = 0;
    fixture.backendFactoryHook = [&] { fixture.backend->currentVideoSize = QSize(1280, 720); };

    bool reentered = false;
    fixture.changeHook = [&](const std::vector<kiriview::VideoDocumentChange>& changes) {
        if (reentered || !std::ranges::contains(changes, kiriview::VideoDocumentChange::VideoSize)
            || fixture.runtime->sourceUrl() != collectionSourceUrl) {
            return;
        }

        reentered = true;
        fixture.runtime->setSourceUrl(replacementSourceUrl);
    };

    fixture.runtime->setSourceDevice(collectionSourceUrl,
        makePlaybackSourceDevice(std::make_shared<SourceDeviceOwner>(&destructionCount)));

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), replacementSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.backend, nullptr);
    QCOMPARE(fixture.runtime->videoSize(), QSize());
    QCOMPARE(destructionCount, 1);
    QCOMPARE(fixture.resolverState->requests.size(), std::size_t(1));
    QCOMPARE(fixture.resolverState->requests.back().sourceUrl, replacementSourceUrl);
}

void TestVideoDocumentRuntime::initialVideoSizeGetterReplacementRejectsSupersededLoad_data()
{
    QTest::addColumn<bool>("sourceDevice");

    QTest::newRow("resolved-url") << false;
    QTest::newRow("source-device") << true;
}

void TestVideoDocumentRuntime::initialVideoSizeGetterReplacementRejectsSupersededLoad()
{
    QFETCH(bool, sourceDevice);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString playbackPath = directory.filePath(QStringLiteral("resolved.mp4"));
    QVERIFY(writeTinyMetadataMp4(playbackPath));

    RuntimeFixture fixture;
    const QUrl firstSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/first.mp4"));
    const QUrl replacementSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/replacement.mp4"));
    int sourceDeviceDestructionCount = 0;
    bool reentered = false;
    fixture.backendFactoryHook = [&] {
        fixture.backend->currentVideoSize = QSize(1280, 720);
        fixture.backend->videoSizeGetterHook = [&] {
            if (std::exchange(reentered, true)) {
                return;
            }
            fixture.runtime->setSourceUrl(replacementSourceUrl);
        };
    };

    if (sourceDevice) {
        fixture.runtime->setSourceDevice(firstSourceUrl,
            makePlaybackSourceDevice(
                std::make_shared<SourceDeviceOwner>(&sourceDeviceDestructionCount)));
    } else {
        fixture.runtime->setSourceUrl(firstSourceUrl);
        fixture.resolveLatest(QUrl::fromLocalFile(playbackPath));
    }

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), replacementSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.backend, nullptr);
    QVERIFY(fixture.runtime->embeddedMetadata().isEmpty());
    QCOMPARE(fixture.runtime->videoSize(), QSize());
    QCOMPARE(sourceDeviceDestructionCount, sourceDevice ? 1 : 0);
}

void TestVideoDocumentRuntime::loadingPublicationCanDestroyRuntimeBeforeResolverAdmission()
{
    QObject documentObject;
    auto resolverState = std::make_shared<FakeResolverState>();
    std::unique_ptr<kiriview::VideoDocumentRuntime> runtime;
    bool destroyed = false;
    runtime = std::make_unique<kiriview::VideoDocumentRuntime>(
        &documentObject,
        [&](const std::vector<kiriview::VideoDocumentChange>& changes) {
            if (destroyed
                || !std::ranges::contains(changes, kiriview::VideoDocumentChange::SourceUrl)) {
                return;
            }

            destroyed = true;
            runtime.reset();
        },
        std::make_unique<FakeVideoPlaybackUrlResolver>(resolverState));
    kiriview::VideoDocumentRuntime* runtimePointer = runtime.get();

    runtimePointer->setSourceUrl(
        QUrl(QStringLiteral("zip:///home/me/videos.zip!/destroy-owner.mp4")));

    QVERIFY(destroyed);
    QVERIFY(runtime == nullptr);
    QVERIFY(resolverState->requests.empty());
}

void TestVideoDocumentRuntime::resolverCompletionAfterRuntimeDestructionIsIgnored()
{
    QObject documentObject;
    auto resolverState = std::make_shared<FakeResolverState>();
    int factoryCallCount = 0;
    auto runtime = std::make_unique<kiriview::VideoDocumentRuntime>(&documentObject,
        kiriview::VideoDocumentRuntime::ChangeCallback {},
        std::make_unique<FakeVideoPlaybackUrlResolver>(resolverState), [&] {
            ++factoryCallCount;
            return std::make_unique<FakeVideoMediaBackend>();
        });
    const QUrl sourceUrl(QStringLiteral("zip:///home/me/videos.zip!/late.mp4"));
    runtime->setSourceUrl(sourceUrl);
    const FakeResolverState::Request request = resolverState->requests.back();

    runtime.reset();
    request.resolvedCallback(kiriview::VideoPlaybackUrlResolution {
        request.operationId,
        request.sourceUrl,
        QUrl::fromLocalFile(QStringLiteral("/tmp/late.mp4")),
    });
    request.failedCallback(request.operationId, request.sourceUrl, QStringLiteral("late failure"));

    QCOMPARE(factoryCallCount, 0);
}

void TestVideoDocumentRuntime::resolverCleanupRunsOnSourceChangeAndDestruction()
{
    auto resolverState = std::make_shared<FakeResolverState>();
    {
        QObject documentObject;
        kiriview::VideoDocumentRuntime runtime(
            &documentObject, {}, std::make_unique<FakeVideoPlaybackUrlResolver>(resolverState));

        runtime.setSourceUrl(QUrl(QStringLiteral("zip:///home/me/videos.zip!/first.mp4")));
        runtime.setSourceUrl(QUrl(QStringLiteral("zip:///home/me/videos.zip!/second.mp4")));
        QCOMPARE(resolverState->cancelCount, 2);
        QCOMPARE(resolverState->cleanupCount, 2);
    }

    QCOMPARE(resolverState->cancelCount, 3);
    QCOMPARE(resolverState->cleanupCount, 3);
}

void TestVideoDocumentRuntime::videoSizeFollowsBackendMetadata()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);

    fixture.backend->emitVideoSize(QSize(1920, 1080));
    QCOMPARE(fixture.runtime->videoSize(), QSize(1920, 1080));

    fixture.backend->emitVideoSize(QSize());
    QCOMPARE(fixture.runtime->videoSize(), QSize());
}

void TestVideoDocumentRuntime::backendScalarGetterReplacementRejectsSupersededValue_data()
{
    QTest::addColumn<int>("getter");

    QTest::newRow("has-video") << static_cast<int>(BackendScalarGetter::HasVideo);
    QTest::newRow("has-audio") << static_cast<int>(BackendScalarGetter::HasAudio);
    QTest::newRow("video-size") << static_cast<int>(BackendScalarGetter::VideoSize);
}

void TestVideoDocumentRuntime::backendScalarGetterReplacementRejectsSupersededValue()
{
    QFETCH(int, getter);

    RuntimeFixture fixture;
    const QUrl firstSourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/first.mp4"));
    const QUrl replacementSourceUrl
        = QUrl::fromLocalFile(QStringLiteral("/home/me/replacement.mp4"));
    fixture.runtime->setSourceUrl(firstSourceUrl);
    fixture.resolveLatest(firstSourceUrl);

    const BackendScalarGetter scalarGetter = static_cast<BackendScalarGetter>(getter);
    const kiriview::VideoMediaBackendCallbacks callbacks = fixture.backend->callbacks;
    bool reentered = false;
    const std::function<void()> replacementHook = [&] {
        if (std::exchange(reentered, true)) {
            return;
        }
        fixture.runtime->setSourceUrl(replacementSourceUrl);
    };
    switch (scalarGetter) {
    case BackendScalarGetter::HasVideo:
        fixture.backend->videoAvailable = true;
        fixture.backend->hasVideoGetterHook = replacementHook;
        callbacks.hasVideoChanged();
        break;
    case BackendScalarGetter::HasAudio:
        fixture.backend->audioAvailable = true;
        fixture.backend->hasAudioGetterHook = replacementHook;
        callbacks.hasAudioChanged();
        break;
    case BackendScalarGetter::VideoSize:
        fixture.backend->currentVideoSize = QSize(1280, 720);
        fixture.backend->videoSizeGetterHook = replacementHook;
        callbacks.videoSizeChanged();
        break;
    }

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), replacementSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.backend, nullptr);
    QVERIFY(!fixture.runtime->hasVideo());
    QVERIFY(!fixture.runtime->hasAudio());
    QCOMPARE(fixture.runtime->videoSize(), QSize());
}

void TestVideoDocumentRuntime::backendScalarGetterDestructionStopsBeforeCommit_data()
{
    backendScalarGetterReplacementRejectsSupersededValue_data();
}

void TestVideoDocumentRuntime::backendScalarGetterDestructionStopsBeforeCommit()
{
    QFETCH(int, getter);

    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);

    const BackendScalarGetter scalarGetter = static_cast<BackendScalarGetter>(getter);
    const kiriview::VideoMediaBackendCallbacks callbacks = fixture.backend->callbacks;
    const std::function<void()> destructionHook = [&] { fixture.runtime.reset(); };
    switch (scalarGetter) {
    case BackendScalarGetter::HasVideo:
        fixture.backend->videoAvailable = true;
        fixture.backend->hasVideoGetterHook = destructionHook;
        callbacks.hasVideoChanged();
        break;
    case BackendScalarGetter::HasAudio:
        fixture.backend->audioAvailable = true;
        fixture.backend->hasAudioGetterHook = destructionHook;
        callbacks.hasAudioChanged();
        break;
    case BackendScalarGetter::VideoSize:
        fixture.backend->currentVideoSize = QSize(1280, 720);
        fixture.backend->videoSizeGetterHook = destructionHook;
        callbacks.videoSizeChanged();
        break;
    }

    QVERIFY(fixture.runtime == nullptr);
    QCOMPARE(fixture.backend, nullptr);
}

void TestVideoDocumentRuntime::nestedBackendObservationKeepsLatestPlaybackFacts()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->currentDuration = 10000;
    fixture.backend->isSeekable = true;

    bool reentered = false;
    fixture.changeHook = [&](const std::vector<kiriview::VideoDocumentChange>& changes) {
        if (reentered || !std::ranges::contains(changes, kiriview::VideoDocumentChange::Status)) {
            return;
        }

        reentered = true;
        fixture.backend->emitDuration(20000);
    };

    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Ready);
    QCOMPARE(fixture.runtime->duration(), 20000);
    QCOMPARE(fixture.runtime->playbackControlProjection().sliderMaximumMsec, 20000);
}

void TestVideoDocumentRuntime::staleBackendCallbacksAfterSourceChangeAreIgnored()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/first.mp4"));
    const QUrl secondSourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/second.mp4"));

    fixture.runtime->setSourceUrl(firstSourceUrl);
    fixture.resolveLatest(firstSourceUrl);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(10000);
    fixture.backend->emitVideoSize(QSize(1920, 1080));
    const kiriview::VideoMediaBackendCallbacks firstSourceCallbacks = fixture.backend->callbacks;

    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Ready);
    QCOMPARE(fixture.runtime->duration(), 10000);
    QCOMPARE(fixture.runtime->videoSize(), QSize(1920, 1080));

    fixture.runtime->setSourceUrl(secondSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.runtime->duration(), 0);
    QCOMPARE(fixture.runtime->videoSize(), QSize());
    QCOMPARE(fixture.backend, nullptr);

    firstSourceCallbacks.mediaStatusChanged();
    firstSourceCallbacks.durationChanged();
    firstSourceCallbacks.videoSizeChanged();

    QCOMPARE(fixture.runtime->sourceUrl(), secondSourceUrl);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Loading);
    QCOMPARE(fixture.runtime->duration(), 0);
    QCOMPARE(fixture.runtime->videoSize(), QSize());

    fixture.resolveLatest(secondSourceUrl);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Ready);
}

void TestVideoDocumentRuntime::supersededBackendLifecycleEventsAfterReplacementAreIgnored()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/first.mp4"));
    const QUrl secondSourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/second.mp4"));

    fixture.runtime->setSourceUrl(firstSourceUrl);
    fixture.resolveLatest(firstSourceUrl);
    const kiriview::VideoMediaBackendCallbacks firstSourceCallbacks = fixture.backend->callbacks;

    fixture.runtime->setSourceUrl(secondSourceUrl);
    fixture.resolveLatest(secondSourceUrl);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(20000);
    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Ready);
    QCOMPARE(fixture.runtime->duration(), 20000);
    QVERIFY(!fixture.runtime->backendFailure().has_value());

    fixture.backend->currentDuration = 10000;
    firstSourceCallbacks.durationChanged();
    firstSourceCallbacks.errorOccurred({ kiriview::VideoMediaErrorCategory::Resource, 17,
        QStringLiteral("late first-source failure") });

    QCOMPARE(fixture.runtime->sourceUrl(), secondSourceUrl);
    QCOMPARE(fixture.runtime->duration(), 20000);
    QVERIFY(!fixture.runtime->backendFailure().has_value());
}

void TestVideoDocumentRuntime::mutedStateDispatchesBackendAndPersistsAcrossSourceChanges()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/first.mp4"));
    const QUrl secondSourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/second.mp4"));

    QVERIFY(!fixture.runtime->muted());
    QCOMPARE(fixture.backend, nullptr);

    fixture.runtime->setMuted(true);
    QVERIFY(fixture.runtime->muted());
    QCOMPARE(fixture.backend, nullptr);

    fixture.runtime->setSourceUrl(firstSourceUrl);
    fixture.resolveLatest(firstSourceUrl);
    QVERIFY(fixture.backend->isMuted);
    QCOMPARE(fixture.backend->setMutedCount, 1);

    fixture.runtime->toggleMuted();
    QVERIFY(!fixture.runtime->muted());
    QVERIFY(!fixture.backend->isMuted);
    QCOMPARE(fixture.backend->setMutedCount, 2);

    fixture.runtime->setMuted(true);
    fixture.runtime->setSourceUrl(QUrl());
    fixture.runtime->setSourceUrl(secondSourceUrl);
    fixture.resolveLatest(secondSourceUrl);

    QVERIFY(fixture.runtime->muted());
    QVERIFY(fixture.backend->isMuted);
    QCOMPARE(fixture.backend->sourceUrl, secondSourceUrl);
}

void TestVideoDocumentRuntime::muteReentryKeepsLatestIntent()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);
    const int initialSetMutedCount = fixture.backend->setMutedCount;

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || !projection.muted) {
            return;
        }

        reentered = true;
        fixture.runtime->setMuted(false);
    };

    fixture.runtime->setMuted(true);

    QVERIFY(reentered);
    QVERIFY(!fixture.runtime->muted());
    QVERIFY(!fixture.backend->isMuted);
    QCOMPARE(fixture.backend->setMutedCount, initialSetMutedCount + 1);
}

void TestVideoDocumentRuntime::playbackControlsDispatchBackendOperations()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitDuration(10000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(5000);
    const int initialStopCount = fixture.backend->stopCount;

    fixture.runtime->pause();
    QCOMPARE(fixture.backend->pauseCount, 1);
    QVERIFY(!fixture.runtime->playing());

    fixture.runtime->play();
    QCOMPARE(fixture.backend->playCount, 2);
    QVERIFY(fixture.runtime->playing());

    fixture.runtime->stop();
    QCOMPARE(fixture.backend->stopCount, initialStopCount + 1);
    QCOMPARE(fixture.backend->setPositionCount, 1);
    QCOMPARE(fixture.backend->currentPosition, 0);
    QCOMPARE(fixture.runtime->position(), 0);
    QVERIFY(!fixture.runtime->playing());
}

void TestVideoDocumentRuntime::playbackControlScrubCommitsWithoutBackendOverwrite()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(90000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(12000);

    fixture.runtime->beginPlaybackScrub();
    fixture.runtime->updatePlaybackScrub(45000);
    fixture.backend->emitPosition(13000);

    QVERIFY(fixture.runtime->playbackControlProjection().scrubbing);
    QCOMPARE(fixture.runtime->playbackControlProjection().sliderValueMsec, qint64(45000));
    const int setPositionCount = fixture.backend->setPositionCount;

    fixture.runtime->commitPlaybackScrub();

    QCOMPARE(fixture.backend->setPositionCount, setPositionCount + 1);
    QCOMPARE(fixture.backend->currentPosition, qint64(45000));
    QVERIFY(!fixture.runtime->playbackControlProjection().scrubbing);
}

void TestVideoDocumentRuntime::scrubCommitReentryDoesNotSeekReplacementSource()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/first.mp4"));
    const QUrl replacementSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/replacement.mp4"));
    fixture.runtime->setSourceUrl(firstSourceUrl);
    fixture.resolveLatest(firstSourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(90000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(12000);
    fixture.runtime->beginPlaybackScrub();
    fixture.runtime->updatePlaybackScrub(45000);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.scrubbing) {
            return;
        }

        reentered = true;
        fixture.backendFactoryHook = [&] {
            fixture.backend->currentDuration = 90000;
            fixture.backend->isSeekable = true;
        };
        fixture.runtime->setSourceDevice(
            replacementSourceUrl, makePlaybackSourceDevice(std::make_shared<int>(1)));
    };

    fixture.runtime->commitPlaybackScrub();

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), replacementSourceUrl);
    QCOMPARE(fixture.backend->setPositionCount, 0);
    QCOMPARE(fixture.backend->currentPosition, 0);
}

void TestVideoDocumentRuntime::playbackControlSeekReentryDoesNotSeekReplacementSource()
{
    RuntimeFixture fixture;
    const QUrl firstSourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/first.mp4"));
    const QUrl replacementSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/replacement.mp4"));
    fixture.runtime->setSourceUrl(firstSourceUrl);
    fixture.resolveLatest(firstSourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(90000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(12000);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection&) {
        if (reentered) {
            return;
        }

        reentered = true;
        fixture.backendFactoryHook = [&] {
            fixture.backend->currentDuration = 90000;
            fixture.backend->isSeekable = true;
        };
        fixture.runtime->setSourceDevice(
            replacementSourceUrl, makePlaybackSourceDevice(std::make_shared<int>(1)));
    };

    fixture.runtime->requestPlaybackControlSeek(45000);

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), replacementSourceUrl);
    QCOMPARE(fixture.backend->setPositionCount, 0);
    QCOMPARE(fixture.backend->currentPosition, 0);
}

void TestVideoDocumentRuntime::scrubCommitReentryPreservesSeekOrder()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(90000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(12000);
    fixture.runtime->beginPlaybackScrub();
    fixture.runtime->updatePlaybackScrub(45000);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.scrubbing || projection.sliderValueMsec != 45000) {
            return;
        }

        reentered = true;
        fixture.runtime->requestPlaybackControlSeek(60000);
    };

    fixture.runtime->commitPlaybackScrub();

    QVERIFY(reentered);
    QCOMPARE(fixture.backend->setPositionRequests,
        (std::vector<qint64> { qint64(45000), qint64(60000) }));
    QCOMPARE(fixture.backend->currentPosition, qint64(60000));
}

void TestVideoDocumentRuntime::seekProjectionReentryPreservesPositionOrder()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.sliderValueMsec != 45000) {
            return;
        }

        reentered = true;
        fixture.runtime->setPosition(60000);
    };

    fixture.runtime->requestPlaybackControlSeek(45000);

    QVERIFY(reentered);
    QCOMPARE(fixture.backend->setPositionRequests,
        (std::vector<qint64> { qint64(45000), qint64(60000) }));
    QCOMPARE(fixture.backend->currentPosition, qint64(60000));
    QCOMPARE(fixture.runtime->position(), qint64(60000));
}

void TestVideoDocumentRuntime::backendSeekReentryPreservesPositionOrder()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(90000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(12000);

    bool reentered = false;
    fixture.backend->setPositionHook = [&](qint64 positionMsec) {
        if (reentered || positionMsec != 45000) {
            return;
        }

        reentered = true;
        fixture.runtime->requestPlaybackControlSeek(60000);
    };

    fixture.runtime->requestPlaybackControlSeek(45000);

    QVERIFY(reentered);
    QCOMPARE(fixture.backend->setPositionRequests,
        (std::vector<qint64> { qint64(45000), qint64(60000) }));
    QCOMPARE(fixture.backend->currentPosition, qint64(60000));
    QCOMPARE(fixture.runtime->position(), qint64(60000));
}

void TestVideoDocumentRuntime::seekProjectionReentryQueuesStopBehindSeek()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.sliderValueMsec != 45000) {
            return;
        }

        reentered = true;
        fixture.runtime->stop();
    };

    fixture.runtime->requestPlaybackControlSeek(45000);

    QVERIFY(reentered);
    QCOMPARE(
        fixture.backend->setPositionRequests, (std::vector<qint64> { qint64(45000), qint64(0) }));
    QCOMPARE(fixture.backend->currentPosition, qint64(0));
    QCOMPARE(fixture.runtime->position(), qint64(0));
    QVERIFY(!fixture.runtime->playing());
}

void TestVideoDocumentRuntime::adjustedSeekLandingRemainsAuthoritative()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);
    fixture.backend->setPositionHook = [&](qint64 positionMsec) {
        if (positionMsec == 45000) {
            fixture.backend->currentPosition = 44000;
        }
    };

    fixture.runtime->requestPlaybackControlSeek(45000);

    QCOMPARE(fixture.backend->setPositionCount, 1);
    QCOMPARE(fixture.backend->currentPosition, qint64(44000));
    QCOMPARE(fixture.runtime->position(), qint64(44000));
    QCOMPARE(fixture.runtime->playbackControlProjection().sliderValueMsec, qint64(44000));
}

void TestVideoDocumentRuntime::seekGateRevocationDoesNotRevivePendingIntent()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.sliderValueMsec != 45000) {
            return;
        }

        reentered = true;
        fixture.backend->emitSeekable(false);
        fixture.backend->emitSeekable(true);
    };

    fixture.runtime->requestPlaybackControlSeek(45000);

    QVERIFY(reentered);
    QCOMPARE(fixture.backend->setPositionCount, 0);
    QCOMPARE(fixture.backend->currentPosition, qint64(12000));
    QCOMPARE(fixture.runtime->position(), qint64(12000));
}

void TestVideoDocumentRuntime::environmentProjectionReentryPreservesAcceptedSeek()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(90000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(12000);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.sliderValueMsec != 45000) {
            return;
        }

        reentered = true;
        fixture.runtime->reportPlaybackControlEnvironment(
            { 1280.0, 720.0, 18.0, false, false, 200, 1500 });
    };

    fixture.runtime->requestPlaybackControlSeek(45000);

    QVERIFY(reentered);
    QCOMPARE(fixture.backend->setPositionCount, 1);
    QCOMPARE(fixture.backend->currentPosition, qint64(45000));
    QCOMPARE(fixture.runtime->position(), qint64(45000));
}

void TestVideoDocumentRuntime::scrubCommitPublicationCanDestroyRuntime()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(90000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(12000);
    fixture.runtime->beginPlaybackScrub();
    fixture.runtime->updatePlaybackScrub(45000);

    bool destroyed = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (destroyed || projection.scrubbing || projection.sliderValueMsec != 45000) {
            return;
        }

        destroyed = true;
        fixture.runtime.reset();
    };
    kiriview::VideoDocumentRuntime* runtimePointer = fixture.runtime.get();

    runtimePointer->commitPlaybackScrub();

    QVERIFY(destroyed);
    QVERIFY(fixture.runtime == nullptr);
}

void TestVideoDocumentRuntime::playbackControlSeekPublicationCanDestroyRuntime()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(90000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(12000);

    bool destroyed = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (destroyed || projection.sliderValueMsec != 45000) {
            return;
        }

        destroyed = true;
        fixture.runtime.reset();
    };
    kiriview::VideoDocumentRuntime* runtimePointer = fixture.runtime.get();

    runtimePointer->requestPlaybackControlSeek(45000);

    QVERIFY(destroyed);
    QVERIFY(fixture.runtime == nullptr);
}

void TestVideoDocumentRuntime::backendSeekCallbackCanDestroyRuntime()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);

    bool destroyed = false;
    fixture.backend->setPositionHook = [&](qint64 positionMsec) {
        if (destroyed || positionMsec != 45000) {
            return;
        }

        destroyed = true;
        fixture.runtime.reset();
    };
    kiriview::VideoDocumentRuntime* runtimePointer = fixture.runtime.get();

    runtimePointer->requestPlaybackControlSeek(45000);

    QVERIFY(destroyed);
    QVERIFY(fixture.runtime == nullptr);
}

void TestVideoDocumentRuntime::playbackControlAutoHideUsesInjectedTimer()
{
    kiriview::TestSupport::ManualTimerScheduler timers;
    RuntimeFixture fixture(timers.scheduler());
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    fixture.runtime->reportPlaybackControlEnvironment(
        { 1280.0, 720.0, 18.0, false, false, 200, 1500 });
    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);

    QVERIFY(fixture.runtime->playbackControlProjection().ready);
    QVERIFY(fixture.runtime->playbackControlProjection().shown);
    QCOMPARE(timers.timerCount(), std::size_t(1));
    QVERIFY(timers.timerAt(0).active());

    timers.timerAt(0).fire();

    QVERIFY(!fixture.runtime->playbackControlProjection().shown);
}

void TestVideoDocumentRuntime::naturalPlaybackEndKeepsPresentationReadyWithoutBackendStop()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitDuration(10000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(10000);

    const int stopCountBeforeEnd = fixture.backend->stopCount;
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::EndOfMedia);

    QCOMPARE(fixture.runtime->status(), kiriview::VideoDocumentStatus::Ready);
    QVERIFY(!fixture.runtime->playing());
    QCOMPARE(fixture.runtime->position(), 10000);
    QCOMPARE(fixture.backend->currentPosition, 10000);
    QCOMPARE(fixture.backend->stopCount, stopCountBeforeEnd);
}

void TestVideoDocumentRuntime::playAfterEndOfMediaRestartsFromBeginningWhenSeekable()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitDuration(10000);
    fixture.backend->emitSeekable(true);
    fixture.backend->emitPosition(10000);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::EndOfMedia);

    fixture.runtime->play();

    QCOMPARE(fixture.backend->setPositionCount, 1);
    QCOMPARE(fixture.backend->currentPosition, 0);
    QCOMPARE(fixture.runtime->position(), 0);
    QCOMPARE(fixture.backend->playCount, 2);
    QVERIFY(fixture.runtime->playing());
}

void TestVideoDocumentRuntime::endedPlaybackRestartQueuesReentrantPause_data()
{
    QTest::addColumn<bool>("togglePlayback");
    QTest::newRow("play") << false;
    QTest::newRow("toggle") << true;
}

void TestVideoDocumentRuntime::endedPlaybackRestartQueuesReentrantPause()
{
    QFETCH(bool, togglePlayback);
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture, 10000, 10000);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::EndOfMedia);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.sliderValueMsec != 0) {
            return;
        }

        reentered = true;
        fixture.runtime->pause();
    };

    if (togglePlayback) {
        fixture.runtime->togglePlayback();
    } else {
        fixture.runtime->play();
    }

    QVERIFY(reentered);
    QVERIFY(!fixture.backend->isPlaying);
    QVERIFY(!fixture.runtime->playing());
    QCOMPARE(fixture.backend->currentPosition, qint64(0));
    QCOMPARE(fixture.runtime->position(), qint64(0));
}

void TestVideoDocumentRuntime::endedPlaybackRestartCompletesBeforeQueuedSeek()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture, 10000, 10000);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::EndOfMedia);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.sliderValueMsec != 0) {
            return;
        }

        reentered = true;
        fixture.runtime->requestPlaybackControlSeek(6000);
    };

    fixture.runtime->play();

    QVERIFY(reentered);
    QVERIFY(fixture.backend->isPlaying);
    QVERIFY(fixture.runtime->playing());
    QCOMPARE(fixture.backend->currentPosition, qint64(6000));
    QCOMPARE(fixture.runtime->position(), qint64(6000));
}

void TestVideoDocumentRuntime::endedPlaybackRestartDoesNotContinueOnReplacementBackend()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture, 10000, 10000);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::EndOfMedia);
    const QUrl replacementSourceUrl(QStringLiteral("zip:///home/me/videos.zip!/replacement.mp4"));

    bool reentered = false;
    fixture.backend->setPositionHook = [&](qint64 positionMsec) {
        if (reentered || positionMsec != 0) {
            return;
        }

        reentered = true;
        fixture.backendFactoryHook = [&] {
            fixture.backend->currentDuration = 10000;
            fixture.backend->isSeekable = true;
        };
        fixture.runtime->setSourceDevice(
            replacementSourceUrl, makePlaybackSourceDevice(std::make_shared<int>(1)));
    };

    fixture.runtime->play();

    QVERIFY(reentered);
    QCOMPARE(fixture.runtime->sourceUrl(), replacementSourceUrl);
    QCOMPARE(fixture.backend->playCount, 1);
}

void TestVideoDocumentRuntime::stopReentryReplansQueuedPositionCommand_data()
{
    QTest::addColumn<bool>("relativeSeek");
    QTest::addColumn<qint64>("expectedPositionMsec");
    QTest::newRow("absolute") << false << qint64(60000);
    QTest::newRow("relative") << true << qint64(5000);
}

void TestVideoDocumentRuntime::stopReentryReplansQueuedPositionCommand()
{
    QFETCH(bool, relativeSeek);
    QFETCH(qint64, expectedPositionMsec);
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.playing || projection.sliderValueMsec != 12000) {
            return;
        }

        reentered = true;
        if (relativeSeek) {
            fixture.runtime->seekBy(5000);
        } else {
            fixture.runtime->requestPlaybackControlSeek(60000);
        }
    };

    fixture.runtime->stop();

    QVERIFY(reentered);
    QVERIFY(!fixture.backend->isPlaying);
    QVERIFY(!fixture.runtime->playing());
    QCOMPARE(fixture.backend->currentPosition, expectedPositionMsec);
    QCOMPARE(fixture.runtime->position(), expectedPositionMsec);
}

void TestVideoDocumentRuntime::stopCompletesBeforeQueuedPlay()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);

    bool reentered = false;
    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (reentered || projection.playing || projection.sliderValueMsec != 12000) {
            return;
        }

        reentered = true;
        fixture.runtime->play();
    };

    fixture.runtime->stop();

    QVERIFY(reentered);
    QVERIFY(fixture.backend->isPlaying);
    QVERIFY(fixture.runtime->playing());
    QCOMPARE(fixture.backend->currentPosition, qint64(0));
    QCOMPARE(fixture.runtime->position(), qint64(0));
}

void TestVideoDocumentRuntime::positionCommandReentryPreservesPositionOrder_data()
{
    QTest::addColumn<bool>("relativeOuterCommand");
    QTest::newRow("set-position-then-timeline-seek") << false;
    QTest::newRow("seek-by-then-set-position") << true;
}

void TestVideoDocumentRuntime::positionCommandReentryPreservesPositionOrder()
{
    QFETCH(bool, relativeOuterCommand);
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);

    bool reentered = false;
    fixture.backend->setPositionHook = [&](qint64 positionMsec) {
        if (reentered || positionMsec != 45000) {
            return;
        }

        reentered = true;
        if (relativeOuterCommand) {
            fixture.runtime->setPosition(60000);
        } else {
            fixture.runtime->requestPlaybackControlSeek(60000);
        }
    };

    if (relativeOuterCommand) {
        fixture.runtime->seekBy(33000);
    } else {
        fixture.runtime->setPosition(45000);
    }

    QVERIFY(reentered);
    QCOMPARE(fixture.backend->setPositionRequests,
        (std::vector<qint64> { qint64(45000), qint64(60000) }));
    QCOMPARE(fixture.backend->currentPosition, qint64(60000));
    QCOMPARE(fixture.runtime->position(), qint64(60000));
    QCOMPARE(fixture.runtime->playbackControlProjection().sliderValueMsec, qint64(60000));
}

void TestVideoDocumentRuntime::playbackCommandDrainYieldsUnderSustainedReentry()
{
    RuntimeFixture fixture;
    prepareReadySeekableVideo(fixture);
    constexpr int requestCount = 512;
    int submittedCount = 1;

    fixture.projectionHook = [&](const kiriview::VideoPlaybackControlProjection& projection) {
        if (submittedCount >= requestCount
            || projection.sliderValueMsec != qint64(12000 + submittedCount)) {
            return;
        }

        ++submittedCount;
        fixture.runtime->setPosition(12000 + submittedCount);
    };

    fixture.runtime->setPosition(12001);

    QVERIFY(fixture.backend->setPositionCount > 0);
    QVERIFY(fixture.backend->setPositionCount < requestCount);
    QTRY_COMPARE_WITH_TIMEOUT(fixture.backend->setPositionCount, requestCount, 5000);
    QCOMPARE(fixture.backend->currentPosition, qint64(12000 + requestCount));
    QCOMPARE(fixture.runtime->position(), qint64(12000 + requestCount));
}

void TestVideoDocumentRuntime::seekByClampsToKnownDuration()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(10000);
    fixture.backend->emitPosition(5000);
    fixture.backend->emitSeekable(true);

    fixture.runtime->seekBy(7000);
    QCOMPARE(fixture.backend->setPositionCount, 1);
    QCOMPARE(fixture.backend->currentPosition, 10000);
    QCOMPARE(fixture.runtime->position(), 10000);

    fixture.runtime->seekBy(-20000);
    QCOMPARE(fixture.backend->setPositionCount, 2);
    QCOMPARE(fixture.backend->currentPosition, 0);
    QCOMPARE(fixture.runtime->position(), 0);
}

void TestVideoDocumentRuntime::seekByNoopsWhenNotSeekable()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    fixture.backend->emitHasVideo(true);
    fixture.backend->emitStatus(kiriview::VideoMediaStatus::Buffered);
    fixture.backend->emitDuration(10000);
    fixture.backend->emitPosition(5000);
    fixture.backend->emitSeekable(false);

    fixture.runtime->seekBy(5000);
    QCOMPARE(fixture.backend->setPositionCount, 0);
    QCOMPARE(fixture.runtime->position(), 5000);
}

void TestVideoDocumentRuntime::videoOutputDetachAndDestructionClearBackendOutput()
{
    RuntimeFixture fixture;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/home/me/clip.mp4"));
    auto* output = new QObject();

    fixture.runtime->setVideoOutputAttachment(output, {}, {});
    QCOMPARE(fixture.runtime->videoOutput(), output);
    QCOMPARE(fixture.backend, nullptr);

    fixture.runtime->setSourceUrl(sourceUrl);
    fixture.resolveLatest(sourceUrl);
    QCOMPARE(fixture.backend->videoOutput(), output);

    fixture.runtime->setVideoOutputAttachment(nullptr, {}, {});
    QCOMPARE(fixture.runtime->videoOutput(), nullptr);
    QCOMPARE(fixture.backend->videoOutput(), nullptr);

    output = new QObject();
    fixture.runtime->setVideoOutputAttachment(output, {}, {});
    delete output;

    QCOMPARE(fixture.runtime->videoOutput(), nullptr);
    QCOMPARE(fixture.backend->videoOutput(), nullptr);
}

void TestVideoDocumentRuntime::backendReplacementDuringOutputEffectConverges_data()
{
    QTest::addColumn<bool>("detach");

    QTest::newRow("attach") << false;
    QTest::newRow("detach") << true;
}

void TestVideoDocumentRuntime::backendReplacementDuringOutputEffectConverges()
{
    QFETCH(bool, detach);

    RuntimeFixture fixture;
    const QUrl firstSourceUrl
        = QUrl::fromLocalFile(QStringLiteral("/home/me/first-output-backend.mp4"));
    const QUrl replacementSourceUrl(
        QStringLiteral("zip:///home/me/videos.zip!/replacement-output-backend.mp4"));
    QObject output;
    fixture.runtime->setSourceUrl(firstSourceUrl);
    fixture.resolveLatest(firstSourceUrl);
    QVERIFY(fixture.backend != nullptr);
    if (detach) {
        fixture.runtime->setVideoOutputAttachment(&output, {}, {});
        QCOMPARE(fixture.backend->videoOutput(), &output);
    }

    FakeVideoMediaBackend* const firstBackend = fixture.backend;
    FakeVideoMediaBackend* replacementBackend = nullptr;
    bool replacementSubmitted = false;
    firstBackend->setVideoOutputHook = [&](QObject* requestedOutput) {
        const QObject* const expectedOutput = detach ? nullptr : &output;
        if (replacementSubmitted || requestedOutput != expectedOutput) {
            return;
        }
        replacementSubmitted = true;
        fixture.runtime->setSourceDevice(
            replacementSourceUrl, makePlaybackSourceDevice(std::make_shared<char>()));
        replacementBackend = fixture.backend;
    };

    fixture.runtime->setVideoOutputAttachment(detach ? nullptr : &output, {}, {});

    QVERIFY(replacementSubmitted);
    QVERIFY(replacementBackend != nullptr);
    QCOMPARE(fixture.runtime->videoOutput(), detach ? nullptr : &output);
    QCOMPARE(replacementBackend->videoOutput(), fixture.runtime->videoOutput());
}

QTEST_GUILESS_MAIN(TestVideoDocumentRuntime)

#include "tst_videodocumentruntime.moc"
