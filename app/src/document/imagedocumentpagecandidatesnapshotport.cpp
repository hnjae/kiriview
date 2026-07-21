// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidatesnapshotport.h"

#include "async/imagecallback.h"
#include "navigation/imagedocumentpagenavigationservice.h"

#include <utility>

namespace kiriview {
ImageDocumentPageCandidateSnapshotPort::ImageDocumentPageCandidateSnapshotPort(
    ImageDocumentPageNavigationService* navigationService)
    : m_navigationService(navigationService)
{
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
