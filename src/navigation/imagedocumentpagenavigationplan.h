// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGENAVIGATIONPLAN_H
#define KIRIVIEW_IMAGEDOCUMENTPAGENAVIGATIONPLAN_H

#include "imagecontaineropenplan.h"
#include "imagedocumentpagenavigationtypes.h"

#include <QString>
#include <QUrl>
#include <optional>
#include <variant>
#include <vector>

namespace kiriview {
using ContainerNavigationError = ImageContainerOpenError;

struct OpenImageDocumentPageUrlEffect
{
    ImageDocumentPageTarget target;
};

struct OpenContainerImageDocumentPageNavigationEffect
{
    ImageDocumentPageTarget target;
    OpenedCollectionScopeLocation openedCollectionScope;
};

struct ReportContainerNavigationErrorEffect
{
    QUrl containerUrl;
    ContainerNavigationError error = ContainerNavigationError::Generic;
    QString errorString;
};

struct ReportContainerNavigationBoundaryEffect
{
    NavigationDirection direction = NavigationDirection::Next;
};

struct ReportContainerNavigationListErrorEffect
{
    ContainerNavigationListFailure failure;
};

struct ClearCurrentImageDocumentPageNavigationEffect
{
};

using ImageDocumentPageNavigationEffect
    = std::variant<OpenImageDocumentPageUrlEffect, OpenContainerImageDocumentPageNavigationEffect,
        ReportContainerNavigationErrorEffect, ReportContainerNavigationBoundaryEffect,
        ReportContainerNavigationListErrorEffect, ClearCurrentImageDocumentPageNavigationEffect>;

using ImageDocumentPageNavigationPlan = std::vector<ImageDocumentPageNavigationEffect>;

struct ImageDocumentPageNavigationCommit
{
    bool pageNavigationChanged = false;
    ImageDocumentPageNavigationPlan effects;
};

struct ImageDocumentPageSelectionResult
{
    std::optional<ImageDocumentPageTarget> target;
    bool pageNavigationChanged = false;
};
}

#endif
