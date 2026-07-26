// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEOPENAPPLICATIONPLAN_H
#define KIRIVIEW_IMAGEOPENAPPLICATIONPLAN_H

#include "imagedocumentruntimeplan.h"
#include "imagedocumenttypes.h"
#include "imageloadfailure.h"
#include "location/imagelocation.h"
#include "metadata/embeddedmetadata.h"

#include <QString>
#include <QUrl>
#include <optional>

namespace kiriview {
struct ImageOpenResolvedStateDelta
{
    std::optional<ImageDocumentSelectedTarget> selectedTarget;
    std::optional<DisplayedImageLocation> displayedLocation;
    std::optional<QUrl> containerNavigationUrl;
    std::optional<bool> loading;
    std::optional<ImageDocumentStatus> status;
    std::optional<QString> errorString;
    std::optional<ImageLoadFailure> loadFailure;
    std::optional<bool> unsupportedOpenedCollectionVideo;
    std::optional<EmbeddedMetadata> embeddedMetadata;
    bool clearLoadingContainerNavigationUrl = false;
    bool advancePresentationLifecycle = false;
};

struct ImageOpenApplicationPlan
{
    ImageOpenResolvedStateDelta stateDelta;
    ImageDocumentRuntimePlan runtimePlan;
};
}

#endif
