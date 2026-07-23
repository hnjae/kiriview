// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagenavigationmodel.h"

#include "imagedocumentpagenavigationpolicy.h"

#include <cstddef>
#include <memory>
#include <utility>

namespace {
bool sameImageDocumentPageCandidates(const kiriview::ImageDocumentPageCandidateRows& left,
    const std::vector<kiriview::ImageDocumentPageCandidate>& right)
{
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left.at(index).url != right.at(index).url || left.at(index).name != right.at(index).name
            || left.at(index).kind != right.at(index).kind) {
            return false;
        }
    }

    return true;
}
}

namespace kiriview {
int ImageDocumentPageNavigationModel::currentPageNumber() const
{
    return pageNavigationCurrentPageNumber(m_state);
}

int ImageDocumentPageNavigationModel::pageCount() const { return pageNavigationPageCount(m_state); }

ImageDocumentPageNavigationSnapshot ImageDocumentPageNavigationModel::snapshot() const
{
    return ImageDocumentPageNavigationSnapshot { m_state };
}

const ImageDocumentPageCandidateListSnapshot&
ImageDocumentPageNavigationModel::confirmedCandidateSnapshot() const
{
    return m_candidateSnapshot;
}

std::optional<ImageDocumentPageCandidateListContext>
ImageDocumentPageNavigationModel::selectedPageCandidateContext() const
{
    const std::optional<QUrl> selectedUrl = urlAtPage(currentPageNumber());
    if (!selectedUrl.has_value() || !m_knownRefreshContext.has_value()) {
        return std::nullopt;
    }

    return ImageDocumentPageCandidateListContext::forSource(
        *selectedUrl, m_knownRefreshContext->source());
}

bool ImageDocumentPageNavigationModel::hasKnownSelection() const
{
    return pageNavigationHasKnownSelection(m_state);
}

std::optional<QUrl> ImageDocumentPageNavigationModel::urlAtPage(int pageNumber) const
{
    return pageNavigationUrlAtPage(m_state, pageNumber);
}

std::optional<ImageDocumentPageTarget> ImageDocumentPageNavigationModel::targetAtPage(
    int pageNumber) const
{
    return pageNavigationTargetAtPage(m_state, pageNumber);
}

std::optional<ImageDocumentPageTarget> ImageDocumentPageNavigationModel::selectPage(int pageNumber)
{
    const std::optional<std::size_t> targetIndex = pageNavigationTargetIndex(m_state, pageNumber);
    if (!targetIndex.has_value()) {
        return std::nullopt;
    }

    PageNavigationState state = m_state;
    state.currentIndex = static_cast<int>(*targetIndex);
    const ImageDocumentPageTarget target = state.targets.at(*targetIndex);
    replaceState(std::move(state));
    return target;
}

std::optional<ImageDocumentPageTarget> ImageDocumentPageNavigationModel::selectAdjacentPage(
    NavigationDirection direction)
{
    const std::optional<std::size_t> targetIndex
        = pageNavigationAdjacentTargetIndex(m_state, direction);
    if (!targetIndex.has_value()) {
        return std::nullopt;
    }

    PageNavigationState state = m_state;
    state.currentIndex = static_cast<int>(*targetIndex);
    const ImageDocumentPageTarget target = state.targets.at(*targetIndex);
    replaceState(std::move(state));
    return target;
}

bool ImageDocumentPageNavigationModel::shouldKeepExistingWatcherFor(
    const ImageDocumentPageCandidateListContext& context) const
{
    return m_knownRefreshContext.has_value()
        && sameImageDocumentPageCandidateListSource(
            m_knownRefreshContext->source(), context.source());
}

ImageDocumentPageNavigationCandidateReuseResult
ImageDocumentPageNavigationModel::reuseConfirmedCandidates(
    const ImageDocumentPageCandidateListContext& context, bool forceChanged)
{
    if (!imageDocumentPageCandidateListSnapshotMatchesSource(
            m_candidateSnapshot, context.source())) {
        return {};
    }

    m_pendingRefresh.cancel();
    m_pendingRefreshContext = context;
    m_knownRefreshContext = context;
    const bool changed = replaceState(
        pageNavigationStateForCurrentUrl(m_state, context.currentUrl()), forceChanged);
    return ImageDocumentPageNavigationCandidateReuseResult { true, changed };
}

bool ImageDocumentPageNavigationModel::reuseConfirmedCandidateSnapshot(
    const ImageDocumentPageCandidateListContext& context)
{
    if (!imageDocumentPageCandidateListSnapshotMatchesSource(
            m_candidateSnapshot, context.source())) {
        return false;
    }

    m_pendingRefresh.cancel();
    m_pendingRefreshContext = context;
    m_knownRefreshContext = context;
    return true;
}

ImageDocumentPageNavigationRefreshPlan ImageDocumentPageNavigationModel::beginRefresh(
    const ImageDocumentPageCandidateListContext& context)
{
    const quint64 refreshId = m_pendingRefresh.start();
    const bool keepKnownUrls = m_knownRefreshContext.has_value() && !m_state.targets.empty()
        && sameImageDocumentPageCandidateListSource(
            m_knownRefreshContext->source(), context.source());
    if (!keepKnownUrls) {
        m_knownRefreshContext = std::nullopt;
    }
    invalidateCandidateSnapshot(keepKnownUrls);

    m_pendingRefreshContext = context;

    bool changed = false;
    if (keepKnownUrls) {
        changed
            = replaceState(pageNavigationStateForCurrentUrl(m_state, context.currentUrl()), true);
    } else {
        changed = replaceState(PageNavigationState {});
    }

    return ImageDocumentPageNavigationRefreshPlan { changed, refreshId };
}

quint64 ImageDocumentPageNavigationModel::beginCandidateSnapshotRefresh(
    const ImageDocumentPageCandidateListContext& context)
{
    const quint64 refreshId = m_pendingRefresh.start();
    const bool keepKnownRows = m_candidateSnapshot.source.has_value()
        && sameImageDocumentPageCandidateListSource(*m_candidateSnapshot.source, context.source());
    invalidateCandidateSnapshot(keepKnownRows);
    m_pendingRefreshContext = context;
    return refreshId;
}

bool ImageDocumentPageNavigationModel::completeRefresh(
    const std::vector<ImageDocumentPageCandidate>& candidates, const QUrl& currentUrl,
    ImageDocumentPageCandidateListSource source)
{
    return completeRefresh(candidates,
        ImageDocumentPageCandidateListContext::forSource(currentUrl, std::move(source)));
}

ImageDocumentPageNavigationRefreshResult ImageDocumentPageNavigationModel::completePendingRefresh(
    const std::vector<ImageDocumentPageCandidate>& candidates, quint64 refreshId,
    ImageDocumentPageCandidateListSource source)
{
    std::optional<ImageDocumentPageCandidateListContext> context
        = acceptedPendingRefreshContext(refreshId, std::move(source));
    if (!context.has_value()) {
        return {};
    }

    m_pendingRefresh.finish(refreshId);
    return completeRefreshFromCurrentContext(candidates, *context);
}

bool ImageDocumentPageNavigationModel::completePendingCandidateSnapshotRefresh(
    const std::vector<ImageDocumentPageCandidate>& candidates, quint64 refreshId,
    ImageDocumentPageCandidateListSource source)
{
    std::optional<ImageDocumentPageCandidateListContext> context
        = acceptedPendingRefreshContext(refreshId, std::move(source));
    if (!context.has_value()) {
        return false;
    }

    m_pendingRefresh.finish(refreshId);
    finishRefresh(candidates, std::move(*context));
    return true;
}

bool ImageDocumentPageNavigationModel::failPendingRefresh(
    quint64 refreshId, ImageDocumentPageCandidateListSource source)
{
    if (!acceptedPendingRefreshContext(refreshId, std::move(source)).has_value()) {
        return false;
    }

    return m_pendingRefresh.finish(refreshId);
}

void ImageDocumentPageNavigationModel::cancelPendingRefresh()
{
    m_pendingRefresh.cancel();
    m_pendingRefreshContext.reset();
}

ImageDocumentPageNavigationRefreshResult
ImageDocumentPageNavigationModel::completeWatchedRefreshFromCurrentContext(
    const std::vector<ImageDocumentPageCandidate>& candidates,
    ImageDocumentPageCandidateListSource source)
{
    std::optional<ImageDocumentPageCandidateListContext> context
        = acceptedWatchedRefreshContext(std::move(source));
    if (!context.has_value()) {
        return {};
    }

    return completeRefreshFromCurrentContext(candidates, *context);
}

bool ImageDocumentPageNavigationModel::completeWatchedCandidateSnapshotRefresh(
    const std::vector<ImageDocumentPageCandidate>& candidates,
    ImageDocumentPageCandidateListSource source)
{
    std::optional<ImageDocumentPageCandidateListContext> context
        = acceptedWatchedRefreshContext(std::move(source));
    if (!context.has_value()) {
        return false;
    }

    finishRefresh(candidates, std::move(*context));
    return true;
}

ImageDocumentPageNavigationRefreshResult
ImageDocumentPageNavigationModel::completeRefreshFromCurrentContext(
    const std::vector<ImageDocumentPageCandidate>& candidates,
    ImageDocumentPageCandidateListContext context)
{
    const bool currentPageRemoved
        = !imageDocumentPageCandidatesContainUrl(candidates, context.currentUrl());
    const bool changed = completeRefresh(candidates, context);
    return ImageDocumentPageNavigationRefreshResult { true, changed, currentPageRemoved, context };
}

std::optional<ImageDocumentPageCandidateListContext>
ImageDocumentPageNavigationModel::acceptedPendingRefreshContext(
    quint64 refreshId, ImageDocumentPageCandidateListSource source) const
{
    if (!m_pendingRefresh.accepts(refreshId)) {
        return std::nullopt;
    }

    return acceptedWatchedRefreshContext(std::move(source));
}

std::optional<ImageDocumentPageCandidateListContext>
ImageDocumentPageNavigationModel::acceptedWatchedRefreshContext(
    ImageDocumentPageCandidateListSource source) const
{
    if (!m_pendingRefreshContext.has_value()
        || !sameImageDocumentPageCandidateListSource(m_pendingRefreshContext->source(), source)) {
        return std::nullopt;
    }

    return ImageDocumentPageCandidateListContext::forSource(
        m_pendingRefreshContext->currentUrl(), std::move(source));
}

bool ImageDocumentPageNavigationModel::completeRefresh(
    const std::vector<ImageDocumentPageCandidate>& candidates,
    ImageDocumentPageCandidateListContext context)
{
    const QUrl currentUrl = context.currentUrl();
    finishRefresh(candidates, std::move(context));
    return replaceState(
        pageNavigationStateForTargets(imageDocumentPageCandidateTargets(candidates), currentUrl));
}

void ImageDocumentPageNavigationModel::finishRefresh(
    const std::vector<ImageDocumentPageCandidate>& candidates,
    ImageDocumentPageCandidateListContext context)
{
    const ImageDocumentPageCandidateListSource source = context.source();
    const bool sameSource = m_candidateSnapshot.source.has_value()
        && sameImageDocumentPageCandidateListSource(*m_candidateSnapshot.source, source);
    const bool sameCandidates = sameImageDocumentPageCandidates(
        imageDocumentPageCandidateRows(m_candidateSnapshot), candidates);
    if (!sameSource || !sameCandidates) {
        ++m_candidateSnapshot.revision;
        m_candidateSnapshot.source = source;
        m_candidateSnapshot.candidates
            = std::make_shared<const ImageDocumentPageCandidateRows>(candidates);
    }
    m_candidateSnapshot.known = true;
    m_knownRefreshContext = std::move(context);
}

bool ImageDocumentPageNavigationModel::clear()
{
    m_pendingRefresh.cancel();
    m_knownRefreshContext = std::nullopt;
    m_pendingRefreshContext = std::nullopt;
    m_candidateSnapshot = ImageDocumentPageCandidateListSnapshot {};
    return replaceState(PageNavigationState {});
}

void ImageDocumentPageNavigationModel::invalidateCandidateSnapshot(bool keepRows)
{
    const quint64 revision = m_candidateSnapshot.revision;
    if (keepRows) {
        m_candidateSnapshot.known = false;
        return;
    }

    m_candidateSnapshot = ImageDocumentPageCandidateListSnapshot {};
    m_candidateSnapshot.revision = revision;
}

bool ImageDocumentPageNavigationModel::replaceState(PageNavigationState state, bool forceChanged)
{
    if (!forceChanged && samePageNavigationState(m_state, state)) {
        return false;
    }

    m_state = std::move(state);
    return true;
}
}
