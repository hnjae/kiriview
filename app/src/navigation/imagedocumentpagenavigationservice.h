// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGENAVIGATIONSERVICE_H
#define KIRIVIEW_IMAGEDOCUMENTPAGENAVIGATIONSERVICE_H

#include "imagecontainernavigationcontroller.h"
#include "imagedocumentpagecandidatelistsource.h"
#include "imagedocumentpagecandidaterepository.h"
#include "imagedocumentpagenavigationcontroller.h"
#include "imagedocumentpagenavigationplan.h"
#include "imagedocumentpagenavigationtypes.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>
#include <optional>

namespace kiriview {
class ImageDocumentPageNavigationService final : public QObject
{
    Q_OBJECT
public:
    using NavigationPlanCallback = std::function<void(ImageDocumentPageNavigationPlan)>;
    using PageNavigationCommitCallback = std::function<void(ImageDocumentPageNavigationCommit)>;
    using DeletionInProgressCallback = std::function<bool()>;

    struct Callbacks
    {
        NavigationPlanCallback navigationPlan;
        PageNavigationCommitCallback pageNavigationCommit;
        DeletionInProgressCallback deletionInProgress;
        std::function<ResolvedNavigationSource(const QUrl&)> resolveExternalSource;
    };

    ImageDocumentPageNavigationService(
        QObject* parent, ImageDocumentPageCandidateProvider candidateProvider, Callbacks callbacks);

    int currentPageNumber() const;
    int pageCount() const;
    ImageDocumentPageNavigationSnapshot pageNavigationSnapshot() const;
    std::optional<ImageDocumentPageCandidateSnapshot> pageCandidateSnapshot() const;
    const ImageDocumentPageCandidateListSnapshot& confirmedPageCandidateSnapshot() const;
    std::optional<QUrl> urlAtPage(int pageNumber) const;
    std::optional<ImageDocumentPageTarget> targetAtPage(int pageNumber) const;
    ImageDocumentPageSelectionResult selectPage(int pageNumber);

    void openAdjacentPage(std::optional<ImageDocumentPageCandidateListContext> context,
        NavigationDirection direction);
    void openAdjacentContainer(const QUrl& currentContainerUrl, NavigationDirection direction);
    void updatePageNavigation(std::optional<ImageDocumentPageCandidateListContext> context);
    void ensurePageCandidateSnapshot(ImageDocumentPageCandidateListContext context,
        ImageDocumentPageCandidateListSnapshotCallback callback);

    void cancelNavigation();
    void cancelContainerNavigation();
    void cancelPageNavigationUpdate();
    void cancelAllNavigation();
    void clearPageNavigation();

private:
    Callbacks m_callbacks;
    ImageDocumentPageCandidateRepository m_candidateRepository;
    ImageContainerNavigationController m_containerNavigation;
    ImageDocumentPageNavigationController m_pageNavigation;
};
}

#endif
