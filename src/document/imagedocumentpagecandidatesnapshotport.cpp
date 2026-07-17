// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidatesnapshotport.h"

#include "async/imagecallback.h"
#include "navigation/imagedocumentpagenavigationservice.h"

#include <utility>

namespace {
const kiriview::ImageDocumentPageCandidateListSnapshot& emptyCandidateSnapshot()
{
    static const kiriview::ImageDocumentPageCandidateListSnapshot snapshot;
    return snapshot;
}
}

namespace kiriview {
ImageDocumentPageCandidateSnapshotPort::ImageDocumentPageCandidateSnapshotPort(
    ImageDocumentPageNavigationService* navigationService)
    : m_navigationService(navigationService)
{
}

std::optional<ImageDocumentPageCandidateSnapshot>
ImageDocumentPageCandidateSnapshotPort::snapshot() const
{
    if (m_navigationService == nullptr) {
        return std::nullopt;
    }
    return m_navigationService->pageCandidateSnapshot();
}

const ImageDocumentPageCandidateListSnapshot&
ImageDocumentPageCandidateSnapshotPort::confirmedSnapshot() const
{
    if (m_navigationService == nullptr) {
        return emptyCandidateSnapshot();
    }
    return m_navigationService->confirmedPageCandidateSnapshot();
}

void ImageDocumentPageCandidateSnapshotPort::ensure(ImageDocumentPageCandidateListContext context,
    ImageDocumentPageCandidateListSnapshotCallback callback) const
{
    if (m_navigationService == nullptr) {
        invokeIfSet(callback, ImageDocumentPageCandidateListSnapshotResult {});
        return;
    }

    m_navigationService->ensurePageCandidateSnapshot(std::move(context), std::move(callback));
}
}
