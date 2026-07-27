// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTRUNTIMEWORKFLOW_H
#define KIRIVIEW_IMAGEDOCUMENTRUNTIMEWORKFLOW_H

#include "imagedocumentruntimeplan.h"

#include <QString>
#include <functional>

namespace kiriview {
class MediaEntrySourceStore;
class ImageDocumentDeletionController;
class ImageDocumentNavigationController;
class ImageDocumentPredecodeController;
class ImageDocumentState;
class ImageDocumentSourceLoadRequest;
class ImageOpenController;
class ImageSpreadPresentationController;

struct ImageDocumentRuntimeWorkflowPorts
{
    ImageDocumentState& state;
    MediaEntrySourceStore* mediaEntrySourceStore = nullptr;
    ImageDocumentDeletionController& deletionController;
    std::function<void()> clearViewportTarget;
    std::function<void()> stopViewportPlayback;
    ImageOpenController& openController;
    ImageDocumentPredecodeController& predecodeController;
    ImageSpreadPresentationController& spreadController;
    ImageDocumentNavigationController& navigationController;
    std::function<void(const ImageDocumentSourceLoadRequest&)> loadSource;
    std::function<void(const QString&)> containerNavigationBoundaryReached;
};

class ImageDocumentRuntimeWorkflow final
{
public:
    using ContinuePlanCallback = std::function<bool()>;

    explicit ImageDocumentRuntimeWorkflow(ImageDocumentRuntimeWorkflowPorts ports);

    void dispatchPlan(const ImageDocumentRuntimePlan& plan);
    void dispatchPlanWhile(
        const ImageDocumentRuntimePlan& plan, const ContinuePlanCallback& shouldContinue);
    void shutdownRuntime();

private:
    void dispatchOperation(const ImageDocumentRuntimeOperation& operation);

    ImageDocumentRuntimeWorkflowPorts m_ports;
};
}

#endif
