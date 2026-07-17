// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATESNAPSHOTPORT_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATESNAPSHOTPORT_H

#include "navigation/imagedocumentpagecandidatelistsource.h"

#include <optional>

namespace kiriview {
class ImageDocumentPageNavigationService;

class ImageDocumentPageCandidateSnapshotPort final
{
public:
    explicit ImageDocumentPageCandidateSnapshotPort(
        ImageDocumentPageNavigationService* navigationService = nullptr);

    std::optional<ImageDocumentPageCandidateSnapshot> snapshot() const;
    const ImageDocumentPageCandidateListSnapshot& confirmedSnapshot() const;
    void ensure(ImageDocumentPageCandidateListContext context,
        ImageDocumentPageCandidateListSnapshotCallback callback) const;

private:
    ImageDocumentPageNavigationService* m_navigationService = nullptr;
};
}

#endif
