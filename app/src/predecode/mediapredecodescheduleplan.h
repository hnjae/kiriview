// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAPREDECODESCHEDULEPLAN_H
#define KIRIVIEW_MEDIAPREDECODESCHEDULEPLAN_H

#include "predecode/mediapredecodeeligibility.h"
#include "predecodeschedulestate.h"
#include "session/directmedianavigationcandidatesnapshot.h"

#include <QUrl>
#include <QtGlobal>
#include <vector>

namespace kiriview {
struct MediaPredecodeSchedulePayload final : PredecodeSchedulePayload
{
public:
    MediaPredecodeSchedulePayload() = default;
    ~MediaPredecodeSchedulePayload() override = default;

    DirectMediaNavigationCandidateSnapshot directMediaNavigationCandidateSnapshot;
    MediaPredecodeEligibilitySnapshot eligibleImages;
    Q_DISABLE_COPY(MediaPredecodeSchedulePayload)
};

struct MediaPredecodeScheduleRequest
{
    QUrl currentUrl;
    DirectMediaNavigationCandidateSnapshot candidateSnapshot;
    std::vector<DisplayedPredecodeImage> displayedImages;
    ImageFirstDisplayDecodeContext firstDisplayContext;
    bool immediate = false;
};

struct MediaPredecodeSchedulePlan
{
    PredecodeScheduleContext context;

    [[nodiscard]] bool shouldSchedule() const;
};

MediaPredecodeSchedulePlan mediaPredecodeSchedulePlan(MediaPredecodeScheduleRequest request);
const DirectMediaNavigationCandidateSnapshot* mediaPredecodeScheduleCandidateSnapshot(
    const PredecodePendingSchedule& schedule);
const MediaPredecodeEligibilitySnapshot* mediaPredecodeScheduleEligibility(
    const PredecodePendingSchedule& schedule);
}

#endif
