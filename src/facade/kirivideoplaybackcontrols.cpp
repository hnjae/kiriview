// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kirivideoplaybackcontrols.h"

#include "facade/kirivideodocument.h"
#include "video/videodocumentruntime.h"

namespace {
KiriVideoPlaybackControls::TimelineKind fromTimelineKind(kiriview::VideoPlaybackTimelineKind kind)
{
    switch (kind) {
    case kiriview::VideoPlaybackTimelineKind::Unavailable:
        return KiriVideoPlaybackControls::TimelineKind::Unavailable;
    case kiriview::VideoPlaybackTimelineKind::NonSeekable:
        return KiriVideoPlaybackControls::TimelineKind::NonSeekable;
    case kiriview::VideoPlaybackTimelineKind::Seekable:
        return KiriVideoPlaybackControls::TimelineKind::Seekable;
    }
    return KiriVideoPlaybackControls::TimelineKind::Unavailable;
}
}

KiriVideoPlaybackControls::KiriVideoPlaybackControls(KiriVideoDocument& document)
    : QObject(&document)
    , m_document(document)
{
}

quint64 KiriVideoPlaybackControls::revision() const
{
    return m_document.playbackControlProjection().revision;
}

bool KiriVideoPlaybackControls::ready() const
{
    return m_document.playbackControlProjection().ready;
}

bool KiriVideoPlaybackControls::fixedMode() const
{
    return m_document.playbackControlProjection().presentationMode
        == kiriview::VideoPlaybackControlPresentationMode::Fixed;
}

bool KiriVideoPlaybackControls::reserveSpace() const
{
    return m_document.playbackControlProjection().reserveSpace;
}

bool KiriVideoPlaybackControls::shown() const
{
    return m_document.playbackControlProjection().shown;
}

bool KiriVideoPlaybackControls::autoHideEligible() const
{
    return m_document.playbackControlProjection().autoHideEligible;
}

bool KiriVideoPlaybackControls::playing() const
{
    return m_document.playbackControlProjection().playing;
}

bool KiriVideoPlaybackControls::muted() const
{
    return m_document.playbackControlProjection().muted;
}

KiriVideoPlaybackControls::TimelineKind KiriVideoPlaybackControls::timelineKind() const
{
    return fromTimelineKind(m_document.playbackControlProjection().timelineKind);
}

bool KiriVideoPlaybackControls::timelineInteractive() const
{
    return m_document.playbackControlProjection().timelineInteractive;
}

double KiriVideoPlaybackControls::sliderValueMsec() const
{
    return m_document.playbackControlProjection().sliderValueMsec;
}

double KiriVideoPlaybackControls::sliderMaximumMsec() const
{
    return m_document.playbackControlProjection().sliderMaximumMsec;
}

QString KiriVideoPlaybackControls::currentTimeText() const
{
    return m_document.playbackControlProjection().currentTimeText;
}

QString KiriVideoPlaybackControls::durationText() const
{
    return m_document.playbackControlProjection().durationText;
}

bool KiriVideoPlaybackControls::scrubbing() const
{
    return m_document.playbackControlProjection().scrubbing;
}

void KiriVideoPlaybackControls::reportEnvironment(qreal viewportWidth, qreal viewportHeight,
    qreal gridUnit, bool mobile, bool transientTouchInput, int longAnimationDurationMsec,
    int autoHideDelayMsec)
{
    m_document.reportPlaybackControlEnvironment(kiriview::VideoPlaybackControlEnvironment {
        viewportWidth,
        viewportHeight,
        gridUnit,
        mobile,
        transientTouchInput,
        longAnimationDurationMsec,
        autoHideDelayMsec,
    });
}

void KiriVideoPlaybackControls::reportInteractionActive(bool active)
{
    m_document.reportPlaybackControlInteraction(active);
}

void KiriVideoPlaybackControls::reveal() { m_document.revealPlaybackControls(); }

void KiriVideoPlaybackControls::beginScrub() { m_document.beginPlaybackScrub(); }

void KiriVideoPlaybackControls::updateScrub(qint64 positionMsec)
{
    m_document.updatePlaybackScrub(positionMsec);
}

void KiriVideoPlaybackControls::commitScrub() { m_document.commitPlaybackScrub(); }

void KiriVideoPlaybackControls::cancelScrub() { m_document.cancelPlaybackScrub(); }

void KiriVideoPlaybackControls::requestSeek(qint64 positionMsec)
{
    m_document.requestPlaybackControlSeek(positionMsec);
}

void KiriVideoPlaybackControls::togglePlayback() { m_document.togglePlayback(); }

void KiriVideoPlaybackControls::toggleMuted() { m_document.toggleMuted(); }
