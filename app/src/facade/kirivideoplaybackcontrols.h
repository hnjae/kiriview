// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIRIVIDEOPLAYBACKCONTROLS_H
#define KIRIVIEW_KIRIVIDEOPLAYBACKCONTROLS_H

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QtQml/qqmlregistration.h>

class KiriVideoDocument;

class KiriVideoPlaybackControls final : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KiriVideoPlaybackControls)
    QML_UNCREATABLE("KiriVideoPlaybackControls is owned by KiriVideoDocument")

    Q_PROPERTY(quint64 revision READ revision NOTIFY projectionChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY projectionChanged)
    Q_PROPERTY(bool fixedMode READ fixedMode NOTIFY projectionChanged)
    Q_PROPERTY(bool reserveSpace READ reserveSpace NOTIFY projectionChanged)
    Q_PROPERTY(bool shown READ shown NOTIFY projectionChanged)
    Q_PROPERTY(bool autoHideEligible READ autoHideEligible NOTIFY projectionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY projectionChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY projectionChanged)
    Q_PROPERTY(TimelineKind timelineKind READ timelineKind NOTIFY projectionChanged)
    Q_PROPERTY(bool timelineInteractive READ timelineInteractive NOTIFY projectionChanged)
    Q_PROPERTY(double sliderValueMsec READ sliderValueMsec NOTIFY projectionChanged)
    Q_PROPERTY(double sliderMaximumMsec READ sliderMaximumMsec NOTIFY projectionChanged)
    Q_PROPERTY(QString currentTimeText READ currentTimeText NOTIFY projectionChanged)
    Q_PROPERTY(QString durationText READ durationText NOTIFY projectionChanged)
    Q_PROPERTY(bool scrubbing READ scrubbing NOTIFY projectionChanged)

public:
    enum class TimelineKind {
        Unavailable,
        NonSeekable,
        Seekable,
    };
    Q_ENUM(TimelineKind)

    explicit KiriVideoPlaybackControls(KiriVideoDocument& document);

    quint64 revision() const;
    bool ready() const;
    bool fixedMode() const;
    bool reserveSpace() const;
    bool shown() const;
    bool autoHideEligible() const;
    bool playing() const;
    bool muted() const;
    TimelineKind timelineKind() const;
    bool timelineInteractive() const;
    double sliderValueMsec() const;
    double sliderMaximumMsec() const;
    QString currentTimeText() const;
    QString durationText() const;
    bool scrubbing() const;

    Q_INVOKABLE void reportEnvironment(qreal viewportWidth, qreal viewportHeight, qreal gridUnit,
        bool mobile, bool transientTouchInput, int longAnimationDurationMsec,
        int autoHideDelayMsec);
    Q_INVOKABLE void reportInteractionActive(bool active);
    Q_INVOKABLE void reveal();
    Q_INVOKABLE void beginScrub();
    Q_INVOKABLE void updateScrub(qint64 positionMsec);
    Q_INVOKABLE void commitScrub();
    Q_INVOKABLE void cancelScrub();
    Q_INVOKABLE void requestSeek(qint64 positionMsec);
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void toggleMuted();

Q_SIGNALS:
    void projectionChanged();

private:
    KiriVideoDocument& m_document;
};

#endif
