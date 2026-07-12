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

std::optional<ImageDocumentPageCandidateSnapshot>
ImageDocumentPageNavigationController::candidateSnapshot() const
{
    return m_model.candidateSnapshot();
}

const ImageDocumentPageCandidateListSnapshot&
ImageDocumentPageNavigationController::confirmedCandidateSnapshot() const
{
    return m_model.confirmedCandidateSnapshot();
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

std::optional<ImageDocumentPageTarget> ImageDocumentPageNavigationController::selectPage(
    int pageNumber)
{
    const std::optional<ImageDocumentPageTarget> target = m_model.selectPage(pageNumber);
    if (target.has_value()) {
        notifyChanged();
    }
    return target;
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
            notifyChanged();
            reportNavigationPlan(ImageDocumentPageNavigationPlan { OpenImageDocumentPageUrlEffect {
                *target,
            } });
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
            std::vector<ImageDocumentPageCandidate> candidates) mutable {
            if (operationId != m_activeNavigationOperationId) {
                qCDebug(kiriviewNavigationLog)
                    << "image document page adjacent navigation stale candidate list ignored"
                    << "operationId" << operationId << "activeOperationId"
                    << m_activeNavigationOperationId;
                return;
            }
            m_activeNavigationOperationId = 0;
            finishNavigation(
                std::move(candidates), direction, currentUrl, std::move(candidateSource));
        },
        [](const QString&) {});
}

void ImageDocumentPageNavigationController::update(
    std::optional<ImageDocumentPageCandidateListContext> context)
{
    m_refreshListerJob.cancel();

    if (!context.has_value()) {
        clear();
        return;
    }

    if (pageCandidateSourceIsOpenedCollection(context->source())) {
        const ImageDocumentPageNavigationCandidateReuseResult reuse
            = m_model.reuseConfirmedCandidates(*context);
        if (reuse.reused) {
            if (reuse.changed) {
                notifyChanged();
            }
            watchChanges(*context);
            return;
        }
    }

    const ImageDocumentPageNavigationRefreshPlan refreshPlan = m_model.beginRefresh(*context);
    if (refreshPlan.changed) {
        notifyChanged();
    }

    watchChanges(*context);

    m_refreshListerJob = m_candidateRepository.loadImages(
        this, *context,
        [this, refreshId = refreshPlan.refreshId, candidateSource = context->source()](
            std::vector<ImageDocumentPageCandidate> candidates) {
            const ImageDocumentPageNavigationRefreshResult refresh
                = m_model.completePendingRefresh(candidates, refreshId, candidateSource);
            if (refresh.accepted && refresh.changed) {
                notifyChanged();
            }
        },
        [](const QString&) {});
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
}

void ImageDocumentPageNavigationController::clear()
{
    m_changesJob.cancel();
    if (m_model.clear()) {
        notifyChanged();
    }
}

void ImageDocumentPageNavigationController::finishNavigation(
    std::vector<ImageDocumentPageCandidate> candidates, NavigationDirection direction,
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
    if (m_model.completeRefresh(candidates, target.url, std::move(candidateSource))) {
        notifyChanged();
    }
    reportNavigationPlan(
        ImageDocumentPageNavigationPlan { OpenImageDocumentPageUrlEffect { target } });
}

void ImageDocumentPageNavigationController::watchChanges(
    const ImageDocumentPageCandidateListContext& context)
{
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
        [](const QString&) {});
}

void ImageDocumentPageNavigationController::updateFromChangedCandidates(
    std::vector<ImageDocumentPageCandidate> candidates, ImageDocumentPageCandidateListSource source)
{
    const ImageDocumentPageNavigationRefreshResult refresh
        = m_model.completeWatchedRefreshFromCurrentContext(candidates, std::move(source));
    if (!refresh.accepted) {
        return;
    }

    if (refresh.changed) {
        notifyChanged();
    }

    if (refresh.currentPageRemoved && refresh.context.has_value()) {
        recoverFromCurrentPageRemoved(std::move(candidates), *refresh.context);
    }
}

void ImageDocumentPageNavigationController::notifyChanged()
{
    invokeIfSet(m_callbacks.pageNavigationChanged);
}

void ImageDocumentPageNavigationController::reportNavigationPlan(
    ImageDocumentPageNavigationPlan plan)
{
    invokeIfSet(m_callbacks.navigationPlan, std::move(plan));
}

void ImageDocumentPageNavigationController::recoverFromCurrentPageRemoved(
    std::vector<ImageDocumentPageCandidate> candidates,
    ImageDocumentPageCandidateListContext context)
{
    if (deletionInProgress()) {
        return;
    }

    const ImageRemovalFallback fallback = imageRemovalFallbackForImageContext(context);
    const std::optional<ImageDocumentPageTarget> fallbackTarget
        = imageRemovalFallbackTarget(std::move(candidates), fallback);
    ImageDocumentPageNavigationPlan plan { ClearCurrentImageDocumentPageNavigationEffect {} };
    if (fallbackTarget.has_value()) {
        plan.push_back(OpenImageDocumentPageUrlEffect { *fallbackTarget });
    }
    reportNavigationPlan(std::move(plan));
}

bool ImageDocumentPageNavigationController::deletionInProgress() const
{
    return m_callbacks.deletionInProgress && m_callbacks.deletionInProgress();
}
}
