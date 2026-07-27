// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kirivideodocument.h"

#include "video/videodocumentruntime.h"

#include <QPointer>
#include <QScopeGuard>
#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

namespace {
KiriVideoDocument::Status fromVideoDocumentStatus(kiriview::VideoDocumentStatus status)
{
    switch (status) {
    case kiriview::VideoDocumentStatus::Null:
        return KiriVideoDocument::Status::Null;
    case kiriview::VideoDocumentStatus::Loading:
        return KiriVideoDocument::Status::Loading;
    case kiriview::VideoDocumentStatus::Ready:
        return KiriVideoDocument::Status::Ready;
    case kiriview::VideoDocumentStatus::Error:
        return KiriVideoDocument::Status::Error;
    }

    return KiriVideoDocument::Status::Null;
}

kiriview::VideoDocumentPublicSignalOperations publicSignalOperations(KiriVideoDocument& document)
{
    kiriview::VideoDocumentPublicSignalOperations operations;
    operations.sourceUrlChanged = [&document]() { Q_EMIT document.sourceUrlChanged(); };
    operations.statusChanged = [&document]() { Q_EMIT document.statusChanged(); };
    operations.errorStringChanged = [&document]() { Q_EMIT document.errorStringChanged(); };
    operations.windowTitleFileNameChanged
        = [&document]() { Q_EMIT document.windowTitleFileNameChanged(); };
    operations.hasVideoChanged = [&document]() { Q_EMIT document.hasVideoChanged(); };
    operations.hasAudioChanged = [&document]() { Q_EMIT document.hasAudioChanged(); };
    operations.videoSizeChanged = [&document]() { Q_EMIT document.videoSizeChanged(); };
    operations.zoomPercentKnownChanged
        = [&document]() { Q_EMIT document.zoomPercentKnownChanged(); };
    operations.zoomPercentChanged = [&document]() { Q_EMIT document.zoomPercentChanged(); };
    operations.videoOutputChanged = [&document]() { Q_EMIT document.videoOutputChanged(); };
    operations.embeddedMetadataChanged
        = [&document]() { Q_EMIT document.embeddedMetadataChanged(); };
    operations.playbackControlProjectionChanged
        = [&document]() { Q_EMIT document.playbackControls()->projectionChanged(); };
    return operations;
}

std::vector<kiriview::VideoDocumentPublicSignal> mergePublicSignals(
    std::vector<kiriview::VideoDocumentPublicSignal> preferred,
    const std::vector<kiriview::VideoDocumentPublicSignal>& fallback)
{
    for (kiriview::VideoDocumentPublicSignal signal : fallback) {
        if (!std::ranges::contains(preferred, signal)) {
            preferred.push_back(signal);
        }
    }

    const auto sessionSnapshot
        = std::ranges::find(preferred, kiriview::VideoDocumentPublicSignal::SessionSnapshot);
    if (sessionSnapshot != preferred.end() && sessionSnapshot != preferred.begin()) {
        std::rotate(preferred.begin(), sessionSnapshot, std::next(sessionSnapshot));
    }
    return preferred;
}
}

KiriVideoDocument::KiriVideoDocument(QObject* parent)
    : KiriVideoDocument(kiriview::TimerScheduler {}, kiriview::VideoMediaBackendFactory {}, parent)
{
}

KiriVideoDocument::KiriVideoDocument(
    kiriview::TimerScheduler playbackControlTimerScheduler, QObject* parent)
    : KiriVideoDocument(
          std::move(playbackControlTimerScheduler), kiriview::VideoMediaBackendFactory {}, parent)
{
}

KiriVideoDocument::KiriVideoDocument(kiriview::TimerScheduler playbackControlTimerScheduler,
    kiriview::VideoMediaBackendFactory videoMediaBackendFactory, QObject* parent)
    : QObject(parent)
{
    m_playbackControls = new KiriVideoPlaybackControls(*this);
    m_runtime = std::make_unique<kiriview::VideoDocumentRuntime>(
        this,
        [this](const std::vector<kiriview::VideoDocumentChange>& changes) {
            handleDocumentChanges(changes);
        },
        std::unique_ptr<kiriview::VideoPlaybackUrlResolver>(), std::move(videoMediaBackendFactory),
        std::move(playbackControlTimerScheduler),
        [this](const kiriview::VideoPlaybackControlProjection& projection) {
            const qint64 videoDuration = static_cast<qint64>(projection.sliderMaximumMsec);
            const bool actionStateChanged = !m_playbackControlActionStateKnown
                || m_videoSeekable != projection.timelineInteractive
                || m_videoDuration != videoDuration;
            m_playbackControlActionStateKnown = true;
            m_videoSeekable = projection.timelineInteractive;
            m_videoDuration = videoDuration;
            std::vector<kiriview::VideoDocumentPublicSignal> signals;
            if (actionStateChanged) {
                signals.push_back(kiriview::VideoDocumentPublicSignal::SessionSnapshot);
            }
            signals.push_back(kiriview::VideoDocumentPublicSignal::PlaybackControlProjection);
            enqueuePublicSignals(std::move(signals));
        });
}

KiriVideoDocument::~KiriVideoDocument() = default;

QUrl KiriVideoDocument::sourceUrl() const { return m_runtime->sourceUrl(); }

void KiriVideoDocument::setSourceUrl(const QUrl& sourceUrl) { m_runtime->setSourceUrl(sourceUrl); }

void KiriVideoDocument::setSourceDevice(
    const QUrl& sourceUrl, kiriview::VideoPlaybackSourceDevice sourceDevice)
{
    m_runtime->setSourceDevice(sourceUrl, std::move(sourceDevice));
}

KiriVideoDocument::Status KiriVideoDocument::status() const
{
    return fromVideoDocumentStatus(m_runtime->status());
}

QString KiriVideoDocument::errorString() const { return m_runtime->errorString(); }

QString KiriVideoDocument::windowTitleFileName() const { return m_runtime->windowTitleFileName(); }

qint64 KiriVideoDocument::duration() const { return m_runtime->duration(); }

qint64 KiriVideoDocument::position() const { return m_runtime->position(); }

bool KiriVideoDocument::playing() const { return m_runtime->playing(); }

bool KiriVideoDocument::seekable() const { return m_runtime->seekable(); }

bool KiriVideoDocument::hasVideo() const { return m_runtime->hasVideo(); }

bool KiriVideoDocument::hasAudio() const { return m_runtime->hasAudio(); }

QSize KiriVideoDocument::videoSize() const { return m_runtime->videoSize(); }

bool KiriVideoDocument::zoomPercentKnown() const { return m_runtime->zoomPercentKnown(); }

int KiriVideoDocument::zoomPercent() const { return m_runtime->zoomPercent(); }

bool KiriVideoDocument::muted() const { return m_runtime->muted(); }

QObject* KiriVideoDocument::videoOutput() const { return m_runtime->videoOutput(); }

KiriVideoPlaybackControls* KiriVideoDocument::playbackControls() const
{
    return m_playbackControls;
}

const kiriview::EmbeddedMetadata& KiriVideoDocument::embeddedMetadata() const
{
    return m_runtime->embeddedMetadata();
}

void KiriVideoDocument::setVideoOutputAttachment(
    QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect)
{
    m_runtime->setVideoOutputAttachment(videoOutput, contentRect, sourceRect);
}

void KiriVideoDocument::runWithPublicSignalsSuppressed(const std::function<void()>& effect)
{
    ++m_publicSignalSuppressionDepth;
    const QPointer<KiriVideoDocument> owner(this);
    [[maybe_unused]] const auto restoreSuppression = qScopeGuard([owner]() {
        if (owner != nullptr) {
            --owner->m_publicSignalSuppressionDepth;
        }
    });
    if (effect) {
        effect();
    }
}

void KiriVideoDocument::play() { m_runtime->play(); }

void KiriVideoDocument::pause() { m_runtime->pause(); }

void KiriVideoDocument::stop() { m_runtime->stop(); }

void KiriVideoDocument::togglePlayback() { m_runtime->togglePlayback(); }

void KiriVideoDocument::setMuted(bool muted) { m_runtime->setMuted(muted); }

void KiriVideoDocument::toggleMuted() { m_runtime->toggleMuted(); }

void KiriVideoDocument::setPosition(qint64 position) { m_runtime->setPosition(position); }

void KiriVideoDocument::seekBy(qint64 deltaMilliseconds) { m_runtime->seekBy(deltaMilliseconds); }

const kiriview::VideoPlaybackControlProjection& KiriVideoDocument::playbackControlProjection() const
{
    return m_runtime->playbackControlProjection();
}

void KiriVideoDocument::reportPlaybackControlEnvironment(
    kiriview::VideoPlaybackControlEnvironment environment)
{
    m_runtime->reportPlaybackControlEnvironment(environment);
}

void KiriVideoDocument::reportPlaybackControlInteraction(bool active)
{
    m_runtime->reportPlaybackControlInteraction(active);
}

void KiriVideoDocument::revealPlaybackControls() { m_runtime->revealPlaybackControls(); }

void KiriVideoDocument::beginPlaybackScrub() { m_runtime->beginPlaybackScrub(); }

void KiriVideoDocument::updatePlaybackScrub(qint64 positionMsec)
{
    m_runtime->updatePlaybackScrub(positionMsec);
}

void KiriVideoDocument::commitPlaybackScrub() { m_runtime->commitPlaybackScrub(); }

void KiriVideoDocument::cancelPlaybackScrub() { m_runtime->cancelPlaybackScrub(); }

void KiriVideoDocument::requestPlaybackControlSeek(qint64 positionMsec)
{
    m_runtime->requestPlaybackControlSeek(positionMsec);
}

void KiriVideoDocument::handleDocumentChanges(
    const std::vector<kiriview::VideoDocumentChange>& changes)
{
    enqueuePublicSignals(kiriview::videoDocumentPublicationSignalsForChanges(changes));
}

void KiriVideoDocument::enqueuePublicSignals(
    std::vector<kiriview::VideoDocumentPublicSignal> signals)
{
    if (m_publicSignalSuppressionDepth != 0) {
        return;
    }
    m_pendingPublicSignals = mergePublicSignals(std::move(signals), m_pendingPublicSignals);
    if (m_publicSignalDispatchActive) {
        return;
    }
    drainPublicSignals();
}

void KiriVideoDocument::drainPublicSignals()
{
    const QPointer<KiriVideoDocument> owner(this);
    m_publicSignalDispatchActive = true;
    kiriview::VideoDocumentPublicSignalOperations operations = publicSignalOperations(*this);
    operations.sessionSnapshotChanged = [this]() { Q_EMIT documentSessionSnapshotChanged(); };
    const kiriview::VideoDocumentPublicSignalEmitter emitter(std::move(operations));

    while (!m_pendingPublicSignals.empty()) {
        const kiriview::VideoDocumentPublicSignal signal = m_pendingPublicSignals.front();
        m_pendingPublicSignals.erase(m_pendingPublicSignals.begin());
        emitter.emitSignal(signal);
        if (owner.isNull()) {
            return;
        }
    }
    m_publicSignalDispatchActive = false;
}
