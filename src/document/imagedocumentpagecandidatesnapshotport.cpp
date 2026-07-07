// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidatesnapshotport.h"

#include "navigation/imagedocumentpagenavigationservice.h"

namespace {
const kiriview::ImageDocumentPageCandidateListSnapshot& emptyCandidateSnapshot()
{
    static const kiriview::ImageDocumentPageCandidateListSnapshot snapshot;
    return snapshot;
}
}

namespace kiriview {
ImageDocumentPageCandidateSnapshotPort::ImageDocumentPageCandidateSnapshotPort(
    const ImageDocumentPageNavigationService* navigationService)
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
}
