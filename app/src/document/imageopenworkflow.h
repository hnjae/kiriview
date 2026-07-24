// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEOPENWORKFLOW_H
#define KIRIVIEW_IMAGEOPENWORKFLOW_H

#include "imagedocumentruntimeplan.h"
#include "imagedocumentsourceloadrequest.h"
#include "imageloadtypes.h"
#include "imageopenapplicationplan.h"
#include "location/imagelocation.h"
#include "metadata/embeddedmetadata.h"

namespace kiriview {
struct ImageDocumentSourceLoadSnapshot
{
    QUrl currentSourceUrl;
    OpenedCollectionScopeLocation displayedOpenedCollectionScope;
    bool rightToLeftReadingEnabled = false;
};

struct ImageOpenBeginSourceLoadSnapshot
{
    bool hasImage = false;
    bool hasLoadingContainerNavigationTarget = false;
    bool sameScopePageNavigation = false;
};

struct ImageOpenSuccessfulImageLoadSnapshot
{
    bool hasRequestContainerNavigationTarget = false;
};

namespace ImageOpenWorkflow {
    ImageDocumentRuntimePlan sourceLoadPlan(const ImageDocumentSourceLoadSnapshot& snapshot,
        const ImageDocumentSourceLoadRequest& request);
    ImageOpenApplicationPlan beginSourceLoadPlan(ImageOpenBeginSourceLoadSnapshot snapshot);
    ImageOpenApplicationPlan finishEmptySourceLoadPlan();
    ImageOpenApplicationPlan resolveSourceImagePlan(const ImageLoadSession& session);
    ImageOpenApplicationPlan finishUnsupportedOpenedCollectionVideoLoadPlan(
        const ImageLoadSession& session);
    ImageOpenApplicationPlan finishPlayableOpenedCollectionVideoLoadPlan(
        const ImageLoadSession& session);
    ImageOpenApplicationPlan finishSuccessfulImageLoadPlan(
        ImageOpenSuccessfulImageLoadSnapshot snapshot, const ImageLoadSession& session);
    ImageOpenApplicationPlan finishSuccessfulImageLoadPlan(
        ImageOpenSuccessfulImageLoadSnapshot snapshot, const ImageLoadSession& session,
        EmbeddedMetadata metadata);
    ImageOpenApplicationPlan finishLoadWithErrorPlan(
        const ImageLoadSession& session, ImageLoadFailure failure);
    ImageOpenApplicationPlan finishContainerNavigationLoadWithErrorPlan(
        ImageDocumentSelectedTarget selectedTarget, const QString& errorString);
}
}

#endif
