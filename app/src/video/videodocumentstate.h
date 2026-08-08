// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEODOCUMENTSTATE_H
#define KIRIVIEW_VIDEODOCUMENTSTATE_H

#include "video/videobackendfailure.h"
#include "video/videodocumenttypes.h"

#include "metadata/embeddedmetadata.h"

#include <QSize>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <optional>
#include <vector>

namespace kiriview {
class VideoDocumentState final
{
public:
    using ChangeCallback = std::function<void(const std::vector<VideoDocumentChange>&)>;

    explicit VideoDocumentState(ChangeCallback changeCallback = {});

    [[nodiscard]] const QUrl& sourceUrl() const;
    [[nodiscard]] VideoDocumentStatus status() const;
    [[nodiscard]] const QString& errorString() const;
    [[nodiscard]] const std::optional<VideoSourceLoadFailure>& sourceLoadFailure() const;
    [[nodiscard]] const std::optional<VideoBackendFailure>& backendFailure() const;
    [[nodiscard]] const QString& windowTitleFileName() const;
    [[nodiscard]] bool hasVideo() const;
    [[nodiscard]] bool hasAudio() const;
    [[nodiscard]] QSize videoSize() const;
    [[nodiscard]] bool zoomPercentKnown() const;
    [[nodiscard]] int zoomPercent() const;
    [[nodiscard]] const EmbeddedMetadata& embeddedMetadata() const;

    void resetForClearedSource();
    void resetForSourceLoad(const QUrl& sourceUrl);
    void setSourceLoadFailure(VideoSourceLoadFailure failure);
    void setBackendFailure(VideoBackendFailure failure);
    void setStatusAndClearFailure(VideoDocumentStatus status);
    void setHasVideo(bool hasVideo);
    void setHasAudio(bool hasAudio);
    void setVideoSize(QSize size);
    void setZoomPercent(std::optional<int> zoomPercent);
    void applyVideoOutputProjection(std::optional<int> zoomPercent, bool videoOutputChanged);
    void setEmbeddedMetadata(EmbeddedMetadata metadata);

    void publish(VideoDocumentChange change);
    void publish(const std::vector<VideoDocumentChange>& changes);

private:
    void appendIfSourceUrlChanged(std::vector<VideoDocumentChange>& changes, const QUrl& sourceUrl);
    void appendIfStatusChanged(
        std::vector<VideoDocumentChange>& changes, VideoDocumentStatus status);
    void appendIfErrorStringChanged(
        std::vector<VideoDocumentChange>& changes, const QString& errorString);
    void appendIfWindowTitleFileNameChanged(
        std::vector<VideoDocumentChange>& changes, const QString& fileName);
    void appendIfHasVideoChanged(std::vector<VideoDocumentChange>& changes, bool hasVideo);
    void appendIfHasAudioChanged(std::vector<VideoDocumentChange>& changes, bool hasAudio);
    void appendIfVideoSizeChanged(std::vector<VideoDocumentChange>& changes, QSize size);
    void appendIfZoomPercentKnownChanged(std::vector<VideoDocumentChange>& changes, bool known);
    void appendIfZoomPercentChanged(std::vector<VideoDocumentChange>& changes, int zoomPercent);
    void appendZoomPercentChanges(
        std::vector<VideoDocumentChange>& changes, std::optional<int> zoomPercent);

    ChangeCallback m_changeCallback;
    QUrl m_sourceUrl;
    VideoDocumentStatus m_status = VideoDocumentStatus::Null;
    QString m_errorString;
    std::optional<VideoSourceLoadFailure> m_sourceLoadFailure;
    std::optional<VideoBackendFailure> m_backendFailure;
    QString m_windowTitleFileName;
    bool m_hasVideo = false;
    bool m_hasAudio = false;
    QSize m_videoSize;
    bool m_zoomPercentKnown = false;
    int m_zoomPercent = 0;
    EmbeddedMetadata m_embeddedMetadata;
};
}

#endif
