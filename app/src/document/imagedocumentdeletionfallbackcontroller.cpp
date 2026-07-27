// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentdeletionfallbackcontroller.h"

#include "async/imagecallback.h"
#include "navigation/imagecontaineropenplan.h"

#include <utility>
#include <variant>
#include <vector>

namespace kiriview {
ImageDocumentDeletionFallbackController::ImageDocumentDeletionFallbackController(QObject* parent,
    ImageDocumentPageCandidateProvider candidateProvider, RuntimePlanCallback runtimePlanCallback,
    std::function<ResolvedNavigationSource(const QUrl&)> resolveExternalSource)
    : m_parent(parent)
    , m_candidateRepository(std::move(candidateProvider))
    , m_runtimePlanCallback(std::move(runtimePlanCallback))
    , m_resolveExternalSource(std::move(resolveExternalSource))
{
}

ImageDocumentDeletionFallbackController::~ImageDocumentDeletionFallbackController() { cancel(); }

void ImageDocumentDeletionFallbackController::open(const ImageRemovalFallbackPlan& fallbackPlan)
{
    const quint64 operationId = m_operation.start();
    m_jobRequest.cancel();
    m_job.cancel();
    if (!m_operation.accepts(operationId)) {
        return;
    }
    std::visit([this, operationId](const auto& plan) { openFallbackPlan(operationId, plan); },
        fallbackPlan);
}

void ImageDocumentDeletionFallbackController::cancel()
{
    m_operation.cancel();
    m_jobRequest.cancel();
    m_job.cancel();
}

void ImageDocumentDeletionFallbackController::openFallbackPlan(
    quint64 operationId, NoImageRemovalFallback)
{
    m_operation.finish(operationId);
}

void ImageDocumentDeletionFallbackController::openFallbackPlan(
    quint64 operationId, const ImageRemovalFallback& fallback)
{
    const quint64 requestId = beginJobRequest(operationId);
    if (requestId == 0) {
        return;
    }
    ImageIoJob startedJob = m_candidateRepository.loadImages(
        m_parent, fallback.imageContext,
        [this, operationId, requestId, fallback](
            std::vector<ImageDocumentPageCandidate> candidates) {
            if (!claimJobRequest(operationId, requestId)) {
                return;
            }

            const std::optional<ImageDocumentPageTarget> fallbackTarget
                = imageRemovalFallbackTarget(std::move(candidates), fallback);
            if (!m_operation.finish(operationId)) {
                return;
            }
            if (fallbackTarget.has_value()) {
                reportRuntimePlan(ImageDocumentRuntimePlan { LoadUrlOperation {
                    *fallbackTarget, fallback.imageContext.openedCollectionScope() } });
            }
        },
        [this, operationId, requestId](const QString&) {
            if (claimJobRequest(operationId, requestId)) {
                static_cast<void>(m_operation.finish(operationId));
            }
        });
    retainJobIfCurrent(operationId, requestId, std::move(startedJob));
}

void ImageDocumentDeletionFallbackController::openFallbackPlan(
    quint64 operationId, const ComicBookRemovalFallback& fallback)
{
    if (!fallback.candidateDirectoryUrl.isValid() || fallback.candidateDirectoryUrl.isEmpty()) {
        m_operation.finish(operationId);
        return;
    }

    const quint64 requestId = beginJobRequest(operationId);
    if (requestId == 0) {
        return;
    }
    ImageIoJob startedJob = m_candidateRepository.loadContainers(
        m_parent, fallback.candidateDirectoryUrl,
        [this, operationId, requestId, fallback](
            std::vector<ContainerNavigationCandidate> candidates) {
            if (!claimJobRequest(operationId, requestId)) {
                return;
            }

            const ComicBookRemovalFallbackCandidates fallbackCandidates
                = comicBookRemovalFallbackCandidates(std::move(candidates), fallback);
            openComicBookFallbackCandidate(
                operationId, fallbackCandidates.preferred, fallbackCandidates.fallback);
        },
        [this, operationId, requestId](const QString&) {
            if (claimJobRequest(operationId, requestId)) {
                static_cast<void>(m_operation.finish(operationId));
            }
        });
    retainJobIfCurrent(operationId, requestId, std::move(startedJob));
}

quint64 ImageDocumentDeletionFallbackController::beginJobRequest(quint64 operationId)
{
    if (!m_operation.accepts(operationId)) {
        return 0;
    }

    const quint64 requestId = m_jobRequest.start();
    m_job.cancel();
    if (!m_operation.accepts(operationId) || !m_jobRequest.accepts(requestId)) {
        return 0;
    }
    return requestId;
}

bool ImageDocumentDeletionFallbackController::claimJobRequest(
    quint64 operationId, quint64 requestId)
{
    return m_operation.accepts(operationId) && m_jobRequest.finish(requestId);
}

void ImageDocumentDeletionFallbackController::retainJobIfCurrent(
    quint64 operationId, quint64 requestId, ImageIoJob job)
{
    if (!m_operation.accepts(operationId) || !m_jobRequest.accepts(requestId)) {
        job.cancel();
        return;
    }
    m_job = std::move(job);
}

void ImageDocumentDeletionFallbackController::openComicBookFallbackCandidate(quint64 operationId,
    const std::optional<ContainerNavigationCandidate>& candidate,
    const std::optional<ContainerNavigationCandidate>& fallbackCandidate)
{
    if (!m_operation.accepts(operationId)) {
        return;
    }

    if (!candidate.has_value()) {
        if (fallbackCandidate.has_value()) {
            openComicBookFallbackCandidate(operationId, fallbackCandidate, std::nullopt);
            return;
        }
        m_operation.finish(operationId);
        return;
    }

    loadComicBookFallbackImage(operationId, *candidate, fallbackCandidate);
}

void ImageDocumentDeletionFallbackController::loadComicBookFallbackImage(quint64 operationId,
    const ContainerNavigationCandidate& candidate,
    const std::optional<ContainerNavigationCandidate>& fallbackCandidate)
{
    if (!m_resolveExternalSource) {
        failComicBookFallbackImageLoad(operationId, fallbackCandidate);
        return;
    }
    const ResolvedNavigationSource source = m_resolveExternalSource(candidate.url);
    if (!m_operation.accepts(operationId)) {
        return;
    }
    const ImageContainerOpenPlan plan = imageContainerOpenPlanForCandidate(candidate, source);
    if (!plan.shouldLoadCandidates()) {
        failComicBookFallbackImageLoad(operationId, fallbackCandidate);
        return;
    }

    const quint64 requestId = beginJobRequest(operationId);
    if (requestId == 0) {
        return;
    }
    ImageIoJob startedJob = m_candidateRepository.loadImages(
        m_parent, *plan.source,
        [this, operationId, requestId, scope = plan.openedCollectionScope, fallbackCandidate](
            const std::vector<ImageDocumentPageCandidate>& candidates) {
            finishComicBookFallbackImageLoad(
                operationId, requestId, scope, fallbackCandidate, candidates);
        },
        [this, operationId, requestId, fallbackCandidate](const QString&) {
            if (claimJobRequest(operationId, requestId)) {
                failComicBookFallbackImageLoad(operationId, fallbackCandidate);
            }
        });
    retainJobIfCurrent(operationId, requestId, std::move(startedJob));
}

void ImageDocumentDeletionFallbackController::finishComicBookFallbackImageLoad(quint64 operationId,
    quint64 requestId, OpenedCollectionScopeLocation openedCollectionScope,
    const std::optional<ContainerNavigationCandidate>& fallbackCandidate,
    const std::vector<ImageDocumentPageCandidate>& candidates)
{
    if (!claimJobRequest(operationId, requestId)) {
        return;
    }

    const ImageContainerOpenResult result = imageContainerOpenResultForCandidates(candidates);
    if (!result.openedImage()) {
        failComicBookFallbackImageLoad(operationId, fallbackCandidate);
        return;
    }

    m_operation.finish(operationId);
    reportRuntimePlan(ImageDocumentRuntimePlan {
        LoadContainerImageOperation { *result.target, std::move(openedCollectionScope) } });
}

void ImageDocumentDeletionFallbackController::failComicBookFallbackImageLoad(
    quint64 operationId, const std::optional<ContainerNavigationCandidate>& fallbackCandidate)
{
    if (!m_operation.accepts(operationId)) {
        return;
    }

    openComicBookFallbackCandidate(operationId, fallbackCandidate, std::nullopt);
}

void ImageDocumentDeletionFallbackController::reportRuntimePlan(ImageDocumentRuntimePlan plan)
{
    invokeIfSet(m_runtimePlanCallback, std::move(plan));
}
}
