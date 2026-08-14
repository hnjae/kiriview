// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEITEMS_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATEITEMS_H

#include "async/directorylistingjob.h"
#include "directmedianavigationmodel.h"
#include "imagedocumentpagenavigationtypes.h"

#include <expected>
#include <vector>

namespace kiriview {
enum class ImageDocumentPageCandidateAdmissionFailure {
    ResourceLimitExceeded,
    ScopeViolation,
};

using ImageDocumentPageCandidateAdmissionResult
    = std::expected<std::vector<ImageDocumentPageCandidate>,
        ImageDocumentPageCandidateAdmissionFailure>;
using DirectMediaNavigationCandidateAdmissionResult
    = std::expected<std::vector<DirectMediaNavigationCandidate>,
        ImageDocumentPageCandidateAdmissionFailure>;
using ContainerNavigationCandidateAdmissionResult
    = std::expected<std::vector<ContainerNavigationCandidate>,
        ImageDocumentPageCandidateAdmissionFailure>;

[[nodiscard]] ImageDocumentPageCandidateAdmissionResult imageDocumentPageNavigationCandidates(
    const QUrl& directoryUrl, const DirectoryItemList& items,
    SiblingCandidateAdmissionLimits limits = defaultSiblingCandidateAdmissionLimits());
[[nodiscard]] DirectMediaNavigationCandidateAdmissionResult directMediaNavigationCandidates(
    const QUrl& directoryUrl, const DirectoryItemList& items,
    SiblingCandidateAdmissionLimits limits = defaultSiblingCandidateAdmissionLimits());
[[nodiscard]] ContainerNavigationCandidateAdmissionResult containerNavigationCandidates(
    const QUrl& directoryUrl, const DirectoryItemList& items,
    SiblingCandidateAdmissionLimits limits = defaultSiblingCandidateAdmissionLimits());
bool imageDocumentPageCandidatesBelongToDirectoryScope(
    const std::vector<ImageDocumentPageCandidate>& candidates, const QUrl& directoryUrl);
bool containerNavigationCandidatesBelongToDirectoryScope(
    const std::vector<ContainerNavigationCandidate>& candidates, const QUrl& directoryUrl);
}

#endif
