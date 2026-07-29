// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediapredecodecoordinator.h"

#include "mediapredecodescheduleplan.h"
#include "mediapredecodewindowplan.h"
#include "predecodelogging.h"

#include <QDebug>
#include <memory>
#include <optional>
#include <utility>

namespace kiriview {
namespace {
    DisplayedImageLocation primaryDisplayedLocationForWindow(
        const PredecodePendingSchedule& schedule)
    {
        return schedule.context.currentLocation;
    }

}

MediaPredecodeCoordinator::MediaPredecodeCoordinator(MediaPredecodeDependencies dependencies)
    : m_loadController(this, std::move(dependencies.imageDecode), dependencies.cacheByteBudget)
    , m_scheduleRuntime(
          this, m_loadController,
          [this](const PredecodePendingSchedule& schedule) { startPredecodeWindow(schedule); }, {},
          std::move(dependencies.powerSaver), std::move(dependencies.timerScheduler))
{
}

void MediaPredecodeCoordinator::schedule(Context context)
{
    qCDebug(kiriviewPredecodeLog)
        << "media predecode schedule"
        << "url" << context.currentUrl << "candidates"
        << directMediaNavigationCandidateRows(context.candidateSnapshot).size() << "displayedImages"
        << context.displayedImages.size();
    MediaPredecodeSchedulePlan plan = mediaPredecodeSchedulePlan(MediaPredecodeScheduleRequest {
        context.currentUrl,
        std::move(context.activeScope),
        std::move(context.candidateSnapshot),
        std::move(context.displayedImages),
        context.firstDisplayContext,
        context.immediate,
    });
    if (!plan.shouldSchedule()) {
        qCDebug(kiriviewPredecodeLog) << "media predecode schedule ignored"
                                      << "reason"
                                      << "invalid-current-url";
    }
    if (plan.context.immediate) {
        m_loadController.retireBackgroundLoad(plan.context.currentLocation);
    }
    m_scheduleRuntime.schedule(plan.context);
}

void MediaPredecodeCoordinator::cacheDisplayedImages(
    const std::vector<DisplayedPredecodeImage>& images)
{
    m_loadController.cacheDisplayedImages(images);
}

void MediaPredecodeCoordinator::setPowerSaverEnabled(bool enabled)
{
    m_scheduleRuntime.setPowerSaverEnabled(enabled);
}

bool MediaPredecodeCoordinator::powerSaverEnabled() const
{
    return m_scheduleRuntime.powerSaverEnabled();
}

void MediaPredecodeCoordinator::startPredecodeWindow(const PredecodePendingSchedule& schedule)
{
    if (!m_scheduleRuntime.accepts(schedule.generation)) {
        qCDebug(kiriviewPredecodeLog) << "media predecode window ignored"
                                      << "reason"
                                      << "stale-generation"
                                      << "generation" << schedule.generation;
        return;
    }

    const DirectMediaNavigationCandidateSnapshot* candidateSnapshot
        = mediaPredecodeScheduleCandidateSnapshot(schedule);
    const MediaPredecodeEligibilitySnapshot* eligibility
        = mediaPredecodeScheduleEligibility(schedule);
    if (candidateSnapshot == nullptr || eligibility == nullptr) {
        qCDebug(kiriviewPredecodeLog) << "media predecode window ignored"
                                      << "reason"
                                      << "missing-payload"
                                      << "generation" << schedule.generation;
        return;
    }

    const PredecodeWindowPlan plan = mediaPredecodeWindowPlan(*eligibility, policyInput());
    qCDebug(kiriviewPredecodeLog) << "media predecode window start"
                                  << "generation" << schedule.generation << "primaryUrl"
                                  << schedule.context.currentLocation.imageUrl() << "candidates"
                                  << directMediaNavigationCandidateRows(*candidateSnapshot).size()
                                  << "stillLocations" << plan.locations.size() << "parallelLimit"
                                  << plan.parallelLimit;
    m_loadController.startWindowLoads(PredecodeLoadWindow {
        primaryDisplayedLocationForWindow(schedule),
        plan.locations,
        schedule.context.displayedImages,
        schedule.context.firstDisplayContext,
        schedule.generation,
        plan.parallelLimit,
    });
}

PredecodePolicyInput MediaPredecodeCoordinator::policyInput() const
{
    return PredecodePolicyInput {
        directMediaPredecodeSourceProfile(),
        m_scheduleRuntime.momentumMode(),
        m_scheduleRuntime.powerSaverEnabled(),
    };
}

void MediaPredecodeCoordinator::cancel() { m_scheduleRuntime.cancel(); }

void MediaPredecodeCoordinator::clear()
{
    cancel();
    m_loadController.clear();
}

std::optional<PredecodedImage> MediaPredecodeCoordinator::findPredecodedImage(
    const DisplayedImageLocation& location) const
{
    return m_loadController.findPredecodedImage(location);
}
}
