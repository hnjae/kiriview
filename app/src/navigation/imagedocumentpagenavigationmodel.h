// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGENAVIGATIONMODEL_H
#define KIRIVIEW_IMAGEDOCUMENTPAGENAVIGATIONMODEL_H

#include "async/imageasyncoperationstate.h"
#include "imagedocumentpagecandidatelistsource.h"
#include "imagedocumentpagenavigationtypes.h"

#include <QUrl>
#include <optional>
#include <vector>

namespace kiriview {
struct ImageDocumentPageNavigationRefreshResult
{
    bool accepted = false;
    bool changed = false;
    bool currentPageRemoved = false;
    std::optional<ImageDocumentPageCandidateListContext> context;
};

struct ImageDocumentPageNavigationRefreshPlan
{
    bool changed = false;
    quint64 refreshId = 0;
};

struct ImageDocumentPageNavigationCandidateReuseResult
{
    bool reused = false;
    bool changed = false;
};

class ImageDocumentPageNavigationModel
{
public:
    int currentPageNumber() const;
    int pageCount() const;
    ImageDocumentPageNavigationSnapshot snapshot() const;
    const ImageDocumentPageCandidateListSnapshot& confirmedCandidateSnapshot() const;
    bool hasKnownSelection() const;
    std::optional<QUrl> urlAtPage(int pageNumber) const;
    std::optional<ImageDocumentPageTarget> targetAtPage(int pageNumber) const;
    std::optional<ImageDocumentPageTarget> selectPage(int pageNumber);
    std::optional<ImageDocumentPageTarget> selectAdjacentPage(NavigationDirection direction);

    bool shouldKeepExistingWatcherFor(const ImageDocumentPageCandidateListContext& context) const;
    ImageDocumentPageNavigationCandidateReuseResult reuseConfirmedCandidates(
        const ImageDocumentPageCandidateListContext& context, bool forceChanged = true);
    bool reuseConfirmedCandidateSnapshot(const ImageDocumentPageCandidateListContext& context);
    ImageDocumentPageNavigationRefreshPlan beginRefresh(
        const ImageDocumentPageCandidateListContext& context);
    quint64 beginCandidateSnapshotRefresh(const ImageDocumentPageCandidateListContext& context);
    bool completeRefresh(const std::vector<ImageDocumentPageCandidate>& candidates,
        const QUrl& currentUrl, ImageDocumentPageCandidateListSource source);
    ImageDocumentPageNavigationRefreshResult completePendingRefresh(
        const std::vector<ImageDocumentPageCandidate>& candidates, quint64 refreshId,
        ImageDocumentPageCandidateListSource source);
    bool completePendingCandidateSnapshotRefresh(
        const std::vector<ImageDocumentPageCandidate>& candidates, quint64 refreshId,
        ImageDocumentPageCandidateListSource source);
    bool failPendingRefresh(quint64 refreshId, ImageDocumentPageCandidateListSource source);
    void cancelPendingRefresh();
    ImageDocumentPageNavigationRefreshResult completeWatchedRefreshFromCurrentContext(
        const std::vector<ImageDocumentPageCandidate>& candidates,
        ImageDocumentPageCandidateListSource source);
    bool completeWatchedCandidateSnapshotRefresh(
        const std::vector<ImageDocumentPageCandidate>& candidates,
        ImageDocumentPageCandidateListSource source);
    bool clear();

private:
    ImageDocumentPageNavigationRefreshResult completeRefreshFromCurrentContext(
        const std::vector<ImageDocumentPageCandidate>& candidates,
        ImageDocumentPageCandidateListContext context);
    std::optional<ImageDocumentPageCandidateListContext> acceptedPendingRefreshContext(
        quint64 refreshId, ImageDocumentPageCandidateListSource source) const;
    std::optional<ImageDocumentPageCandidateListContext> acceptedWatchedRefreshContext(
        ImageDocumentPageCandidateListSource source) const;
    bool completeRefresh(const std::vector<ImageDocumentPageCandidate>& candidates,
        ImageDocumentPageCandidateListContext context);
    void finishRefresh(const std::vector<ImageDocumentPageCandidate>& candidates,
        ImageDocumentPageCandidateListContext context);
    void invalidateCandidateSnapshot(bool keepRows);
    bool replaceState(PageNavigationState state, bool forceChanged = false);

    PageNavigationState m_state;
    std::optional<ImageDocumentPageCandidateListContext> m_knownRefreshContext;
    std::optional<ImageDocumentPageCandidateListContext> m_pendingRefreshContext;
    ImageDocumentPageCandidateListSnapshot m_candidateSnapshot;
    ImageAsyncOperationState m_pendingRefresh;
};
}

#endif
