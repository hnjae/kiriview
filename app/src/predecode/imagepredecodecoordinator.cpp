// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagepredecodecoordinator.h"

#include "predecodelogging.h"
#include "predecodewindowplan.h"

#include <QDebug>
#include <QThread>
#include <cstddef>
#include <utility>
#include <vector>

namespace kiriview {
namespace {
    int defaultPredecodeThreadCount() { return QThread::idealThreadCount(); }

    DisplayedImageLocation foregroundOwnedLocationForWindow(
        const PredecodePendingSchedule& schedule)
    {
        return schedule.context.currentLocation;
    }

}

ImagePredecodeCoordinator::ImagePredecodeCoordinator(ImageDecodeDependencies decodeDependencies,
    PowerSaverProvider powerSaverProvider, qsizetype cacheByteBudget, TimerScheduler timerScheduler,
    PredecodeThreadCountProvider threadCountProvider)
    : m_threadCountProvider(
          threadCountProvider ? std::move(threadCountProvider) : defaultPredecodeThreadCount)
    , m_loadController(this, std::move(decodeDependencies), cacheByteBudget)
    , m_scheduleRuntime(
          this, m_loadController,
          [this](const PredecodePendingSchedule& schedule) {
              scheduleAdjacentImagePredecode(schedule);
          },
          []() {}, std::move(powerSaverProvider), std::move(timerScheduler))
{
}

void ImagePredecodeCoordinator::acceptForegroundSelection(const DisplayedImageLocation& location)
{
    m_loadController.retireBackgroundLoad(location);
}

void ImagePredecodeCoordinator::schedule(const Context& context)
{
    qCDebug(kiriviewPredecodeLog) << "image predecode schedule"
                                  << "url" << context.currentLocation.imageUrl()
                                  << "displayedImages" << context.displayedImages.size();
    if (context.immediate) {
        acceptForegroundSelection(context.currentLocation);
    }
    m_scheduleRuntime.schedule(context);
}

void ImagePredecodeCoordinator::setPowerSaverEnabled(bool enabled)
{
    m_scheduleRuntime.setPowerSaverEnabled(enabled);
}

bool ImagePredecodeCoordinator::powerSaverEnabled() const
{
    return m_scheduleRuntime.powerSaverEnabled();
}

void ImagePredecodeCoordinator::scheduleAdjacentImagePredecode(
    const PredecodePendingSchedule& schedule)
{
    const PredecodeWindowStartPlan plan = predecodeWindowStartPlan(PredecodeWindowPlanRequest {
        schedule.context.currentLocation,
        PredecodePolicyInput {
            predecodeSourceProfileForOpenedCollectionScope(
                schedule.context.currentLocation.openedCollectionScope(), m_threadCountProvider()),
            m_scheduleRuntime.momentumMode(),
            m_scheduleRuntime.powerSaverEnabled(),
        },
    });
    qCDebug(kiriviewPredecodeLog) << "image predecode start plan"
                                  << "generation" << schedule.generation << "url"
                                  << schedule.context.currentLocation.imageUrl() << "loadCandidates"
                                  << plan.shouldLoadCandidates() << "fallbackLocations"
                                  << plan.fallbackWindow.locations.size() << "parallelLimit"
                                  << plan.fallbackWindow.parallelLimit;
    if (!plan.shouldLoadCandidates()) {
        startPredecodeImageLoads(plan.fallbackWindow, schedule);
        return;
    }

    if (imageDocumentPageCandidateListSnapshotMatchesSource(
            schedule.context.candidateSnapshot, plan.candidateList->context.source())) {
        const ImageDocumentPageCandidateRows& candidates
            = imageDocumentPageCandidateRows(schedule.context.candidateSnapshot);
        qCDebug(kiriviewPredecodeLog)
            << "image predecode candidates reused"
            << "generation" << schedule.generation << "count" << candidates.size();
        startPredecodeImageLoads(predecodeWindowPlanForCandidates(plan, candidates), schedule);
        return;
    }

    qCDebug(kiriviewPredecodeLog) << "image predecode candidate-dependent work skipped"
                                  << "reason"
                                  << "owner-snapshot-unavailable"
                                  << "generation" << schedule.generation;
    startPredecodeImageLoads(plan.fallbackWindow, schedule);
}

void ImagePredecodeCoordinator::startPredecodeImageLoads(
    const PredecodeWindowPlan& plan, const PredecodePendingSchedule& schedule)
{
    if (!m_scheduleRuntime.accepts(schedule.generation)) {
        qCDebug(kiriviewPredecodeLog) << "image predecode window ignored"
                                      << "reason"
                                      << "stale-generation"
                                      << "generation" << schedule.generation;
        return;
    }

    qCDebug(kiriviewPredecodeLog) << "image predecode window start"
                                  << "generation" << schedule.generation << "foregroundUrl"
                                  << schedule.context.currentLocation.imageUrl() << "locations"
                                  << plan.locations.size() << "parallelLimit" << plan.parallelLimit;
    m_loadController.startWindowLoads(PredecodeLoadWindow {
        foregroundOwnedLocationForWindow(schedule),
        plan.locations,
        schedule.context.displayedImages,
        schedule.context.firstDisplayContext,
        schedule.generation,
        plan.parallelLimit,
    });
}

void ImagePredecodeCoordinator::cancel() { m_scheduleRuntime.cancel(); }

void ImagePredecodeCoordinator::clear()
{
    cancel();
    m_loadController.clear();
}

std::optional<PredecodedImage> ImagePredecodeCoordinator::findPredecodedImage(
    const DisplayedImageLocation& location) const
{
    return m_loadController.findPredecodedImage(location);
}
}
