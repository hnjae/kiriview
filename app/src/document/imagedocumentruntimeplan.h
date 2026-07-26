// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTRUNTIMEPLAN_H
#define KIRIVIEW_IMAGEDOCUMENTRUNTIMEPLAN_H

#include "imagedocumentsourceloadrequest.h"
#include "imagedocumentstate.h"
#include "imagedocumenttypes.h"
#include "navigation/imagedocumentpagenavigationtypes.h"

#include <QString>
#include <QUrl>
#include <optional>
#include <variant>
#include <vector>

namespace kiriview {
struct CancelFileDeletionOperation
{
};
struct ShutdownSpreadOperation
{
};
struct ClearMediaEntrySourceOperation
{
};
struct ClearPredecodeOperation
{
};
struct CancelPredecodeOperation
{
};
struct ScheduleAdjacentImagePredecodeOperation
{
    std::optional<ImageDocumentPageTarget> target;
    int targetPageIndex = -1;
};
struct ResetRightToLeftReadingOperation
{
};
struct ClearSecondaryPageOperation
{
};
struct NotifyRightToLeftReadingChangedOperation
{
};
struct CancelPageNavigationUpdateOperation
{
};
struct CancelNavigationOperation
{
};
struct CancelContainerNavigationOperation
{
};
struct CancelAllNavigationOperation
{
};
struct ClearPageNavigationOperation
{
};
struct UpdatePageNavigationOperation
{
};
struct LoadUrlOperation
{
    ImageDocumentPageTarget target;
    OpenedCollectionScopeLocation openedCollectionScope;
};
struct LoadContainerImageOperation
{
    ImageDocumentPageTarget target;
    OpenedCollectionScopeLocation openedCollectionScope;
};
struct FinishEmptyContainerNavigationOperation
{
    QUrl containerUrl;
};
struct FinishContainerNavigationLoadWithErrorOperation
{
    QUrl containerUrl;
    QString errorString;
};
struct ReportContainerNavigationBoundaryOperation
{
    NavigationDirection direction = NavigationDirection::Next;
};
struct ReportContainerNavigationListFailureOperation
{
    ContainerNavigationListFailure failure;
};
struct LoadPageNavigationUrlOperation
{
    ImageDocumentPageTarget target;
    OpenedCollectionScopeLocation openedCollectionScope;
};
struct CancelOpenOperation
{
};
struct ClearPresentationImageOperation
{
};
struct RetireViewportPresentationOperation
{
};
struct StopPresentationPlaybackOperation
{
};
struct ClearLoadingContainerNavigationUrlOperation
{
};
struct SetLoadingContainerNavigationUrlOperation
{
    QUrl url;
};
struct SetContainerNavigationUrlOperation
{
    QUrl url;
};
struct PrepareSourceLoadOperation
{
    ImageDocumentSourceLoadRequest request;
};
struct SelectImageTargetOperation
{
    ImageDocumentSelectedTarget target;
};
struct BeginOpenOperation
{
};
struct SetErrorStringOperation
{
    QString errorString;
};
struct FinishEmptySourceLoadOperation
{
};

using ImageDocumentRuntimeOperation = std::variant<CancelFileDeletionOperation,
    ShutdownSpreadOperation, ClearMediaEntrySourceOperation, ClearPredecodeOperation,
    CancelPredecodeOperation, ScheduleAdjacentImagePredecodeOperation,
    ResetRightToLeftReadingOperation, ClearSecondaryPageOperation,
    NotifyRightToLeftReadingChangedOperation, CancelPageNavigationUpdateOperation,
    CancelNavigationOperation, CancelContainerNavigationOperation, CancelAllNavigationOperation,
    ClearPageNavigationOperation, UpdatePageNavigationOperation, LoadUrlOperation,
    LoadContainerImageOperation, FinishEmptyContainerNavigationOperation,
    FinishContainerNavigationLoadWithErrorOperation, ReportContainerNavigationBoundaryOperation,
    ReportContainerNavigationListFailureOperation, LoadPageNavigationUrlOperation,
    CancelOpenOperation, ClearPresentationImageOperation, RetireViewportPresentationOperation,
    StopPresentationPlaybackOperation, ClearLoadingContainerNavigationUrlOperation,
    SetLoadingContainerNavigationUrlOperation, SetContainerNavigationUrlOperation,
    PrepareSourceLoadOperation, SelectImageTargetOperation, BeginOpenOperation,
    SetErrorStringOperation, FinishEmptySourceLoadOperation>;

using ImageDocumentRuntimePlan = std::vector<ImageDocumentRuntimeOperation>;

struct ImageDocumentRuntimeTransaction
{
    std::vector<ImageDocumentChange> changes;
    ImageDocumentRuntimePlan plan;
};

ImageDocumentRuntimePlan imageDocumentClearImagePlan();
ImageDocumentRuntimePlan imageDocumentClearDeletedImagePlan();
ImageDocumentRuntimePlan imageDocumentShutdownPlan();
}

#endif
