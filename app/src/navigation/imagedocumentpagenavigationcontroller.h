// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGENAVIGATIONCONTROLLER_H
#define KIRIVIEW_IMAGEDOCUMENTPAGENAVIGATIONCONTROLLER_H

#include "async/imageiojob.h"
#include "imagedocumentpagecandidaterepository.h"
#include "imagedocumentpagenavigationmodel.h"
#include "imagedocumentpagenavigationplan.h"
#include "imagedocumentpagenavigationtypes.h"

#include <QObject>
#include <QUrl>
#include <functional>
#include <optional>
#include <vector>

namespace kiriview {
class ImageDocumentPageNavigationController final : public QObject
{
    Q_OBJECT
public:
    using PageNavigationCommitCallback = std::function<void(ImageDocumentPageNavigationCommit)>;
    using DeletionInProgressCallback = std::function<bool()>;

    struct Callbacks
    {
        PageNavigationCommitCallback pageNavigationCommit;
        DeletionInProgressCallback deletionInProgress;
    };

    ImageDocumentPageNavigationController(QObject* parent,
        const ImageDocumentPageCandidateRepository& candidateRepository, Callbacks callbacks);

    [[nodiscard]] int currentPageNumber() const;
    [[nodiscard]] int pageCount() const;
    [[nodiscard]] ImageDocumentPageNavigationSnapshot snapshot() const;
    [[nodiscard]] const ImageDocumentPageCandidateListSnapshot& confirmedCandidateSnapshot() const;
    [[nodiscard]] std::optional<ImageDocumentPageCandidateListContext>
    selectedPageCandidateContext() const;
    [[nodiscard]] std::optional<QUrl> urlAtPage(int pageNumber) const;
    [[nodiscard]] std::optional<ImageDocumentPageTarget> targetAtPage(int pageNumber) const;
    ImageDocumentPageSelectionResult selectPage(int pageNumber);

    void openAdjacentPage(std::optional<ImageDocumentPageCandidateListContext> context,
        NavigationDirection direction);
    void update(std::optional<ImageDocumentPageCandidateListContext> context);
    void ensureConfirmedSnapshot(const ImageDocumentPageCandidateListContext& context,
        ImageDocumentPageCandidateListSnapshotCallback callback);
    void cancelNavigation();
    void cancelUpdate();
    void clear();

private:
    void startUpdate(const ImageDocumentPageCandidateListContext& context,
        ImageDocumentPageCandidateListSnapshotCallback callback, bool reuseAnyConfirmedSnapshot);
    void finishNavigation(const std::vector<ImageDocumentPageCandidate>& candidates,
        NavigationDirection direction, const QUrl& currentUrl,
        ImageDocumentPageCandidateListSource candidateSource);
    void watchChanges(
        const ImageDocumentPageCandidateListContext& context, bool updatesNavigationState);
    void updateFromChangedCandidates(std::vector<ImageDocumentPageCandidate> candidates,
        ImageDocumentPageCandidateListSource source);
    void notifyChanged();
    void reportCommit(ImageDocumentPageNavigationCommit commit);
    ImageDocumentPageNavigationPlan recoveryPlanFromCurrentPageRemoved(
        std::vector<ImageDocumentPageCandidate> candidates,
        const ImageDocumentPageCandidateListContext& context);
    [[nodiscard]] bool deletionInProgress() const;

    const ImageDocumentPageCandidateRepository& m_candidateRepository;
    Callbacks m_callbacks;
    ImageDocumentPageNavigationModel m_model;
    ImageIoJob m_navigationListerJob;
    ImageIoJob m_refreshListerJob;
    ImageIoJob m_changesJob;
    bool m_watcherUpdatesNavigationState = false;
    quint64 m_nextNavigationOperationId = 1;
    quint64 m_activeNavigationOperationId = 0;
};
}

#endif
