// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTDELETIONFALLBACKCONTROLLER_H
#define KIRIVIEW_IMAGEDOCUMENTDELETIONFALLBACKCONTROLLER_H

#include "async/imageasyncoperationstate.h"
#include "async/imageiojob.h"
#include "imagedocumentruntimeplan.h"
#include "navigation/imagedocumentpagecandidaterepository.h"
#include "navigation/imageremovalfallback.h"

#include <QtGlobal>
#include <functional>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
class ImageDocumentDeletionFallbackController final
{
public:
    using RuntimePlanCallback = std::function<void(ImageDocumentRuntimePlan)>;

    ImageDocumentDeletionFallbackController(QObject* parent,
        ImageDocumentPageCandidateProvider candidateProvider,
        RuntimePlanCallback runtimePlanCallback,
        std::function<ResolvedNavigationSource(const QUrl&)> resolveExternalSource);
    ~ImageDocumentDeletionFallbackController();

    void open(const ImageRemovalFallbackPlan& fallbackPlan);
    void cancel();

private:
    void openFallbackPlan(quint64 operationId, NoImageRemovalFallback);
    void openFallbackPlan(quint64 operationId, const ImageRemovalFallback& fallback);
    void openFallbackPlan(quint64 operationId, const ComicBookRemovalFallback& fallback);
    void openComicBookFallbackCandidate(quint64 operationId,
        const std::optional<ContainerNavigationCandidate>& candidate,
        const std::optional<ContainerNavigationCandidate>& fallbackCandidate);
    void loadComicBookFallbackImage(quint64 operationId,
        const ContainerNavigationCandidate& candidate,
        const std::optional<ContainerNavigationCandidate>& fallbackCandidate);
    void finishComicBookFallbackImageLoad(quint64 operationId,
        OpenedCollectionScopeLocation openedCollectionScope,
        const std::optional<ContainerNavigationCandidate>& fallbackCandidate,
        std::vector<ImageDocumentPageCandidate> candidates);
    void failComicBookFallbackImageLoad(
        quint64 operationId, const std::optional<ContainerNavigationCandidate>& fallbackCandidate);
    void reportRuntimePlan(ImageDocumentRuntimePlan plan);

    QObject* m_parent = nullptr;
    ImageDocumentPageCandidateRepository m_candidateRepository;
    RuntimePlanCallback m_runtimePlanCallback;
    std::function<ResolvedNavigationSource(const QUrl&)> m_resolveExternalSource;
    ImageIoJob m_job;
    ImageAsyncOperationState m_operation;
};
}

#endif
