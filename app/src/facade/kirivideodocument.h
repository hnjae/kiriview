// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIRIVIDEODOCUMENT_H
#define KIRIVIEW_KIRIVIDEODOCUMENT_H

#include "facade/kirivideoplaybackcontrols.h"
#include "facade/videodocumentpublicsignals.h"
#include "metadata/embeddedmetadata.h"
#include "video/videoplaybackcontrolruntime.h"
#include "video/videoplaybacksource.h"

#include <QObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <memory>
#include <vector>

namespace kiriview {
struct KiriVideoDocumentComposition;
class VideoDocumentRuntime;
}

class KiriDocumentSession;

class KiriVideoDocument : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KiriVideoDocument)
    QML_UNCREATABLE("KiriVideoDocument is owned by KiriDocumentSession")

    Q_PROPERTY(QUrl sourceUrl READ sourceUrl NOTIFY sourceUrlChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(
        QString windowTitleFileName READ windowTitleFileName NOTIFY windowTitleFileNameChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY hasAudioChanged)
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
    Q_PROPERTY(bool zoomPercentKnown READ zoomPercentKnown NOTIFY zoomPercentKnownChanged)
    Q_PROPERTY(int zoomPercent READ zoomPercent NOTIFY zoomPercentChanged)
    Q_PROPERTY(QObject* videoOutput READ videoOutput NOTIFY videoOutputChanged)
    Q_PROPERTY(KiriVideoPlaybackControls* playbackControls READ playbackControls CONSTANT)

public:
    enum class Status {
        Null,
        Loading,
        Ready,
        Error,
    };
    Q_ENUM(Status)

    explicit KiriVideoDocument(QObject* parent = nullptr);
    ~KiriVideoDocument() override;
    Q_DISABLE_COPY_MOVE(KiriVideoDocument)

    [[nodiscard]] QUrl sourceUrl() const;
    [[nodiscard]] Status status() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QString windowTitleFileName() const;
    [[nodiscard]] qint64 duration() const;
    [[nodiscard]] qint64 position() const;
    [[nodiscard]] bool playing() const;
    [[nodiscard]] bool seekable() const;
    [[nodiscard]] bool hasVideo() const;
    [[nodiscard]] bool hasAudio() const;
    [[nodiscard]] QSize videoSize() const;
    [[nodiscard]] bool zoomPercentKnown() const;
    [[nodiscard]] int zoomPercent() const;
    [[nodiscard]] bool muted() const;
    [[nodiscard]] QObject* videoOutput() const;
    [[nodiscard]] KiriVideoPlaybackControls* playbackControls() const;
    [[nodiscard]] const kiriview::EmbeddedMetadata& embeddedMetadata() const;
    void play();
    void pause();
    void stop();
    void togglePlayback();
    void setMuted(bool muted);
    void toggleMuted();
    void setPosition(qint64 position);
    void seekBy(qint64 deltaMilliseconds);

Q_SIGNALS:
    void sourceUrlChanged();
    void statusChanged();
    void errorStringChanged();
    void windowTitleFileNameChanged();
    void hasVideoChanged();
    void hasAudioChanged();
    void videoSizeChanged();
    void zoomPercentKnownChanged();
    void zoomPercentChanged();
    void videoOutputChanged();
    void embeddedMetadataChanged();

private:
    friend class KiriDocumentSession;
    friend class KiriVideoPlaybackControls;

    explicit KiriVideoDocument(kiriview::KiriVideoDocumentComposition composition, QObject* parent);

    Q_SIGNAL void documentSessionSnapshotChanged();

    void setSourceUrl(const QUrl& sourceUrl);
    void setSourceDevice(const QUrl& sourceUrl, kiriview::VideoPlaybackSourceDevice sourceDevice);
    void setVideoOutputAttachment(
        QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect);
    void runWithPublicSignalsSuppressed(const std::function<void()>& effect);
    void handleDocumentChanges(const std::vector<kiriview::VideoDocumentChange>& changes);
    void enqueuePublicSignals(std::vector<kiriview::VideoDocumentPublicSignal> signals);
    void drainPublicSignals();
    [[nodiscard]] const kiriview::VideoPlaybackControlProjection& playbackControlProjection() const;
    void reportPlaybackControlEnvironment(kiriview::VideoPlaybackControlEnvironment environment);
    void reportPlaybackControlInteraction(bool active);
    void revealPlaybackControls();
    void beginPlaybackScrub();
    void updatePlaybackScrub(qint64 positionMsec);
    void commitPlaybackScrub();
    void cancelPlaybackScrub();
    void requestPlaybackControlSeek(qint64 positionMsec);

    KiriVideoPlaybackControls* m_playbackControls = nullptr;
    std::unique_ptr<kiriview::VideoDocumentRuntime> m_runtime;
    bool m_playbackControlActionStateKnown = false;
    bool m_videoSeekable = false;
    qint64 m_videoDuration = 0;
    std::vector<kiriview::VideoDocumentPublicSignal> m_pendingPublicSignals;
    bool m_publicSignalDispatchActive = false;
    quint64 m_publicSignalSuppressionDepth = 0;
};

#endif
