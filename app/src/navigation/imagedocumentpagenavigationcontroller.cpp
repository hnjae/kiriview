// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagenavigationcontroller.h"

#include "async/imagecallback.h"
#include "imagedocumentpagenavigationpolicy.h"
#include "imageremovalfallback.h"
#include "navigationlogging.h"

#include <QDebug>
#include <QString>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace kiriview {
namespace {
    QString pageCandidateSourceKind(const ImageDocumentPageCandidateListSource& source)
    {
        return source.visit([](const auto& payload) -> QString {
            using Source = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Source, ImageDocumentPageCandidateListSource::Directory>) {
                return QStringLiteral("directory");
            } else {
                return QStringLiteral("openedCollection");
            }
        });
    }

    QUrl pageCandidateSourceRoot(const ImageDocumentPageCandidateListSource& source)
    {
        return source.visit([](const auto& payload) -> QUrl {
            using Source = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Source, ImageDocumentPageCandidateListSource::Directory>) {
                return payload.directoryUrl;
            } else {
                return payload.openedCollectionScope.rootUrl();
            }
        });
    }

    bool pageCandidateSourceIsOpenedCollection(const ImageDocumentPageCandidateListSource& source)
    {
        return source.visit([](const auto& payload) {
            using Source = std::decay_t<decltype(payload)>;
            return std::is_same_v<Source,
                ImageDocumentPageCandidateListSource::OpenedCollectionScope>;
        });
    }

    void logCandidateLoadError(
        const char* message, const ImageDocumentPageCandidateLoadError& error)
    {
        std::visit(
            [message](const auto& detail) {
                qCWarning(kiriviewNavigationLog).noquote() << message << detail;
            },
            error);
    }
}

ImageDocumentPageNavigationController::ImageDocumentPageNavigationController(QObject* parent,
    const ImageDocumentPageCandidateRepository& candidateRepository, Callbacks callbacks)
    : QObject(parent)
    , m_candidateRepository(candidateRepository)
    , m_callbacks(std::move(callbacks))
{
}

int ImageDocumentPageNavigationController::currentPageNumber() const
{
    return m_model.currentPageNumber();
}

int ImageDocumentPageNavigationController::pageCount() const { return m_model.pageCount(); }

ImageDocumentPageNavigationSnapshot ImageDocumentPageNavigationController::snapshot() const
{
    return m_model.snapshot();
}

const ImageDocumentPageCandidateListSnapshot&
ImageDocumentPageNavigationController::confirmedCandidateSnapshot() const
{
    return m_model.confirmedCandidateSnapshot();
}

std::optional<ImageDocumentPageCandidateListContext>
ImageDocumentPageNavigationController::selectedPageCandidateContext() const
{
    return m_model.selectedPageCandidateContext();
}

std::optional<QUrl> ImageDocumentPageNavigationController::urlAtPage(int pageNumber) const
{
    return m_model.urlAtPage(pageNumber);
}

std::optional<ImageDocumentPageTarget> ImageDocumentPageNavigationController::targetAtPage(
    int pageNumber) const
{
    return m_model.targetAtPage(pageNumber);
}

ImageDocumentPageSelectionResult ImageDocumentPageNavigationController::selectPage(int pageNumber)
{
    std::optional<ImageDocumentPageTarget> target = m_model.selectPage(pageNumber);
    const bool changed = target.has_value();
    return ImageDocumentPageSelectionResult { std::move(target), changed };
}

void ImageDocumentPageNavigationController::openAdjacentPage(
    std::optional<ImageDocumentPageCandidateListContext> context, NavigationDirection direction)
{
    cancelNavigation();

    if (m_model.hasKnownSelection()) {
        qCDebug(kiriviewNavigationLog)
            << "image document page adjacent navigation using known selection"
            << "direction" << static_cast<int>(direction) << "currentPage"
            << m_model.currentPageNumber() << "pageCount" << m_model.pageCount();
        const std::optional<ImageDocumentPageTarget> target = m_model.selectAdjacentPage(direction);
        if (target.has_value()) {
            qCDebug(kiriviewNavigationLog)
                << "image document page adjacent target selected from known model"
                << "direction" << static_cast<int>(direction) << "targetUrl" << target->url
                << "targetKind" << static_cast<int>(target->kind) << "currentPage"
                << m_model.currentPageNumber() << "pageCount" << m_model.pageCount();
            reportCommit(ImageDocumentPageNavigationCommit {
                true,
                ImageDocumentPageNavigationPlan { OpenImageDocumentPageUrlEffect { *target } },
            });
        } else {
            qCDebug(kiriviewNavigationLog)
                << "image document page adjacent navigation hit known boundary"
                << "direction" << static_cast<int>(direction) << "currentPage"
                << m_model.currentPageNumber() << "pageCount" << m_model.pageCount();
        }
        return;
    }

    if (!context.has_value()) {
        qCDebug(kiriviewNavigationLog)
            << "image document page adjacent navigation skipped without candidate context"
            << "direction" << static_cast<int>(direction);
        return;
    }

    const quint64 operationId = m_nextNavigationOperationId++;
    m_activeNavigationOperationId = operationId;
    qCDebug(kiriviewNavigationLog)
        << "image document page adjacent navigation listing candidates"
        << "operationId" << operationId << "direction" << static_cast<int>(direction)
        << "currentUrl" << context->currentUrl() << "sourceKind"
        << pageCandidateSourceKind(context->source()) << "sourceRoot"
        << pageCandidateSourceRoot(context->source());
    m_navigationListerJob = m_candidateRepository.loadImages(
        this, *context,
        [this, operationId, direction, currentUrl = context->currentUrl(),
            candidateSource = context->source()](
            const std::vector<ImageDocumentPageCandidate>& candidates) mutable {
            if (operationId != m_activeNavigationOperationId) {
                qCDebug(kiriviewNavigationLog)
                    << "image document page adjacent navigation stale candidate list ignored"
                    << "operationId" << operationId << "activeOperationId"
                    << m_activeNavigationOperationId;
                return;
            }
            m_activeNavigationOperationId = 0;
            finishNavigation(candidates, direction, currentUrl, std::move(candidateSource));
        },
        [this, operationId](const ImageDocumentPageCandidateLoadError& error) {
            if (operationId != m_activeNavigationOperationId) {
                return;
            }
            m_activeNavigationOperationId = 0;
            logCandidateLoadError("adjacent page candidate listing failed", error);
        });
}

void ImageDocumentPageNavigationController::update(
    std::optional<ImageDocumentPageCandidateListContext> context)
{
    m_refreshListerJob.cancel();

    if (!context.has_value()) {
        clear();
        return;
    }

    startUpdate(*context, {}, false);
}

void ImageDocumentPageNavigationController::ensureConfirmedSnapshot(
    const ImageDocumentPageCandidateListContext& context,
    ImageDocumentPageCandidateListSnapshotCallback callback)
{
    m_refreshListerJob.cancel();
    startUpdate(context, std::move(callback), true);
}

void ImageDocumentPageNavigationController::startUpdate(
    const ImageDocumentPageCandidateListContext& context,
    ImageDocumentPageCandidateListSnapshotCallback callback, bool reuseAnyConfirmedSnapshot)
{
    const bool ensureUpdatesNavigationState
        = reuseAnyConfirmedSnapshot && pageCandidateSourceIsOpenedCollection(context.source());
    if (reuseAnyConfirmedSnapshot) {
        const ImageDocumentPageNavigationCandidateReuseResult navigationReuse
            = ensureUpdatesNavigationState ? m_model.reuseConfirmedCandidates(context, false)
                                           : ImageDocumentPageNavigationCandidateReuseResult {};
        const bool snapshotReused = ensureUpdatesNavigationState
            ? navigationReuse.reused
            : m_model.reuseConfirmedCandidateSnapshot(context);
        if (snapshotReused) {
            if (navigationReuse.changed) {
                notifyChanged();
            }
            watchChanges(context, ensureUpdatesNavigationState);
            invokeIfSet(callback,
                ImageDocumentPageCandidateListSnapshotResult {
                    m_model.confirmedCandidateSnapshot(), true, {} });
            return;
        }
    }

    if (!reuseAnyConfirmedSnapshot && pageCandidateSourceIsOpenedCollection(context.source())) {
        const ImageDocumentPageNavigationCandidateReuseResult reuse
            = m_model.reuseConfirmedCandidates(context);
        if (reuse.reused) {
            if (reuse.changed) {
                notifyChanged();
            }
            watchChanges(context, true);
            invokeIfSet(callback,
                ImageDocumentPageCandidateListSnapshotResult {
                    m_model.confirmedCandidateSnapshot(), true, {} });
            return;
        }
    }

    const bool refreshUpdatesNavigationState
        = !reuseAnyConfirmedSnapshot || ensureUpdatesNavigationState;
    const ImageDocumentPageNavigationRefreshPlan refreshPlan = refreshUpdatesNavigationState
        ? m_model.beginRefresh(context)
        : ImageDocumentPageNavigationRefreshPlan { false,
              m_model.beginCandidateSnapshotRefresh(context) };
    if (refreshPlan.changed) {
        notifyChanged();
    }

    watchChanges(context, refreshUpdatesNavigationState);

    m_refreshListerJob = m_candidateRepository.loadImages(
        this, context,
        [this, refreshId = refreshPlan.refreshId, candidateSource = context.source(), callback,
            refreshUpdatesNavigationState](
            const std::vector<ImageDocumentPageCandidate>& candidates) mutable {
            if (!refreshUpdatesNavigationState) {
                if (!m_model.completePendingCandidateSnapshotRefresh(
                        candidates, refreshId, candidateSource)) {
                    return;
                }
            } else {
                const ImageDocumentPageNavigationRefreshResult refresh
                    = m_model.completePendingRefresh(candidates, refreshId, candidateSource);
                if (!refresh.accepted) {
                    return;
                }
                if (refresh.changed) {
                    notifyChanged();
                }
            }
            invokeIfSet(callback,
                ImageDocumentPageCandidateListSnapshotResult {
                    m_model.confirmedCandidateSnapshot(), true, {} });
        },
        [this, refreshId = refreshPlan.refreshId, candidateSource = context.source(), callback](
            ImageDocumentPageCandidateLoadError error) mutable {
            if (!m_model.failPendingRefresh(refreshId, candidateSource)) {
                return;
            }
            if (!callback) {
                logCandidateLoadError("page candidate refresh failed", error);
            }
            invokeIfSet(callback,
                ImageDocumentPageCandidateListSnapshotResult {
                    m_model.confirmedCandidateSnapshot(), false, std::move(error) });
        });
}

void ImageDocumentPageNavigationController::cancelNavigation()
{
    m_navigationListerJob.cancel();
    m_activeNavigationOperationId = 0;
}

void ImageDocumentPageNavigationController::cancelUpdate()
{
    m_refreshListerJob.cancel();
    m_changesJob.cancel();
    m_model.cancelPendingRefresh();
}

void ImageDocumentPageNavigationController::clear()
{
    m_changesJob.cancel();
    if (m_model.clear()) {
        notifyChanged();
    }
}

void ImageDocumentPageNavigationController::finishNavigation(
    const std::vector<ImageDocumentPageCandidate>& candidates, NavigationDirection direction,
    const QUrl& currentUrl, ImageDocumentPageCandidateListSource candidateSource)
{
    const std::optional<ImageDocumentPageCandidate> candidate
        = adjacentImageDocumentPageCandidate(candidates, currentUrl, direction);
    if (!candidate.has_value()) {
        qCDebug(kiriviewNavigationLog)
            << "image document page adjacent navigation listed candidates without target"
            << "direction" << static_cast<int>(direction) << "currentUrl" << currentUrl
            << "candidateCount" << static_cast<qsizetype>(candidates.size());
        return;
    }

    const ImageDocumentPageTarget target { candidate->url, candidate->kind };
    qCDebug(kiriviewNavigationLog)
        << "image document page adjacent target selected from listed candidates"
        << "direction" << static_cast<int>(direction) << "currentUrl" << currentUrl << "targetUrl"
        << target.url << "targetKind" << static_cast<int>(target.kind) << "candidateCount"
        << static_cast<qsizetype>(candidates.size()) << "sourceKind"
        << pageCandidateSourceKind(candidateSource) << "sourceRoot"
        << pageCandidateSourceRoot(candidateSource);
    const bool changed
        = m_model.completeRefresh(candidates, target.url, std::move(candidateSource));
    reportCommit(ImageDocumentPageNavigationCommit {
        changed,
        ImageDocumentPageNavigationPlan { OpenImageDocumentPageUrlEffect { target } },
    });
}

void ImageDocumentPageNavigationController::watchChanges(
    const ImageDocumentPageCandidateListContext& context, bool updatesNavigationState)
{
    m_watcherUpdatesNavigationState = updatesNavigationState;
    if (m_model.shouldKeepExistingWatcherFor(context) && m_changesJob.isActive()) {
        return;
    }

    m_changesJob.cancel();
    const ImageDocumentPageCandidateListSource& source = context.source();
    m_changesJob = m_candidateRepository.watchCandidateChanges(
        this, context,
        [this, source](std::vector<ImageDocumentPageCandidate> candidates) {
            updateFromChangedCandidates(std::move(candidates), source);
        },
        [this, context](const ImageDocumentPageCandidateLoadError& error) {
            if (!m_model.shouldKeepExistingWatcherFor(context)) {
                return;
            }
            logCandidateLoadError("page candidate watch failed", error);
        });
}

void ImageDocumentPageNavigationController::updateFromChangedCandidates(
    std::vector<ImageDocumentPageCandidate> candidates, ImageDocumentPageCandidateListSource source)
{
    if (!m_watcherUpdatesNavigationState) {
        m_model.completeWatchedCandidateSnapshotRefresh(candidates, std::move(source));
        return;
    }

    const ImageDocumentPageNavigationRefreshResult refresh
        = m_model.completeWatchedRefreshFromCurrentContext(candidates, std::move(source));
    if (!refresh.accepted) {
        return;
    }

    ImageDocumentPageNavigationPlan effects;
    if (refresh.currentPageRemoved && refresh.context.has_value()) {
        effects = recoveryPlanFromCurrentPageRemoved(std::move(candidates), *refresh.context);
    }
    if (refresh.changed || !effects.empty()) {
        reportCommit(ImageDocumentPageNavigationCommit { refresh.changed, std::move(effects) });
    }
}

void ImageDocumentPageNavigationController::notifyChanged()
{
    reportCommit(ImageDocumentPageNavigationCommit { true, {} });
}

void ImageDocumentPageNavigationController::reportCommit(ImageDocumentPageNavigationCommit commit)
{
    invokeIfSet(m_callbacks.pageNavigationCommit, std::move(commit));
}

ImageDocumentPageNavigationPlan
ImageDocumentPageNavigationController::recoveryPlanFromCurrentPageRemoved(
    std::vector<ImageDocumentPageCandidate> candidates,
    const ImageDocumentPageCandidateListContext& context)
{
    if (deletionInProgress()) {
        return {};
    }

    const ImageRemovalFallback fallback = imageRemovalFallbackForImageContext(context);
    const std::optional<ImageDocumentPageTarget> fallbackTarget
        = imageRemovalFallbackTarget(std::move(candidates), fallback);
    ImageDocumentPageNavigationPlan plan { ClearCurrentImageDocumentPageNavigationEffect {} };
    if (fallbackTarget.has_value()) {
        plan.emplace_back(OpenImageDocumentPageUrlEffect { *fallbackTarget });
    }
    return plan;
}

bool ImageDocumentPageNavigationController::deletionInProgress() const
{
    return m_callbacks.deletionInProgress && m_callbacks.deletionInProgress();
}
}
