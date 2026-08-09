// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEITEMS_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEITEMS_H

#include "directmedianavigationmodel.h"
#include "imagedocumentpagenavigationtypes.h"

#include <KFileItem>
#include <QtGlobal>
#include <expected>
#include <vector>

namespace kiriview {
struct ImageDocumentPageCandidateAdmissionLimits
{
    qsizetype maximumEntryCount = 0;
    qsizetype maximumRetainedIdentityCodeUnitCount = 0;
};

enum class ImageDocumentPageCandidateAdmissionFailure {
    ResourceLimitExceeded,
};

using ImageDocumentPageCandidateAdmissionResult
    = std::expected<std::vector<ImageDocumentPageCandidate>,
        ImageDocumentPageCandidateAdmissionFailure>;
using DirectMediaNavigationCandidateAdmissionResult
    = std::expected<std::vector<DirectMediaNavigationCandidate>,
        ImageDocumentPageCandidateAdmissionFailure>;

[[nodiscard]] ImageDocumentPageCandidateAdmissionLimits
defaultImageDocumentPageCandidateAdmissionLimits();
[[nodiscard]] ImageDocumentPageCandidateAdmissionResult imageDocumentPageNavigationCandidates(
    const KFileItemList& items,
    ImageDocumentPageCandidateAdmissionLimits limits
    = defaultImageDocumentPageCandidateAdmissionLimits());
[[nodiscard]] DirectMediaNavigationCandidateAdmissionResult directMediaNavigationCandidates(
    const KFileItemList& items,
    ImageDocumentPageCandidateAdmissionLimits limits
    = defaultImageDocumentPageCandidateAdmissionLimits());
std::vector<ContainerNavigationCandidate> containerNavigationCandidates(const KFileItemList& items);
}

#endif
