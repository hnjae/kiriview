// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILPROJECTION_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILPROJECTION_H

#include "navigation/imagedocumentpagecandidatelistsource.h"
#include "session/activenavigationprojection.h"
#include "session/directmedianavigationcandidatesnapshot.h"

#include <QString>
#include <QUrl>
#include <optional>
#include <vector>

namespace kiriview {
enum class ActiveNavigationThumbnailKind {
    Image,
    Video,
};

enum class ActiveNavigationThumbnailSourceKind {
    DirectImage,
    DirectVideo,
    ImageDocumentPageImage,
    ImageDocumentPageVideo,
};

enum class ActiveNavigationThumbnailResultStatus {
    NoResult,
    Pending,
    Ready,
    Unsupported,
    Failed,
};

struct ActiveNavigationThumbnailRow
{
    int number = 0;
    QUrl url;
    QString label;
    ActiveNavigationThumbnailKind kind = ActiveNavigationThumbnailKind::Image;
    ActiveNavigationThumbnailSourceKind sourceKind
        = ActiveNavigationThumbnailSourceKind::DirectImage;
    bool current = false;
    quint64 sourceFreshness = 0;
};

struct ActiveNavigationThumbnailRowSetIdentity
{
    ActiveNavigationSourceKind sourceKind = ActiveNavigationSourceKind::None;
    std::optional<DirectMediaScope> directMediaSource;
    std::optional<ImageDocumentPageCandidateListSource> imageDocumentPageSource;
    quint64 candidateRevision = 0;
    int count = 0;
    bool known = false;
};

QString activeNavigationThumbnailPageKindIdentity(ActiveNavigationThumbnailKind kind);
QString activeNavigationThumbnailSourceKindIdentity(ActiveNavigationThumbnailSourceKind sourceKind);
bool sameActiveNavigationThumbnailRowSetIdentity(
    const ActiveNavigationThumbnailRowSetIdentity& left,
    const ActiveNavigationThumbnailRowSetIdentity& right);
std::optional<ActiveNavigationThumbnailRowSetIdentity> activeNavigationThumbnailRowSetIdentity(
    ActiveNavigationSourceKind sourceKind, ActiveNavigationSnapshot navigation,
    const DirectMediaNavigationCandidateSnapshot& directMediaNavigationCandidateSnapshot,
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot);
std::vector<ActiveNavigationThumbnailRow> projectActiveNavigationThumbnailRows(
    ActiveNavigationSourceKind sourceKind, ActiveNavigationSnapshot navigation,
    const DirectMediaNavigationCandidateSnapshot& directMediaNavigationCandidateSnapshot,
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot);
}

#endif
