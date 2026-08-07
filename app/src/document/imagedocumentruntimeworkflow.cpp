// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentruntimeworkflow.h"

#include "archive/mediaentrysourcestore.h"
#include "async/imagecallback.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "imagedocumentdeletioncontroller.h"
#include "imagedocumentnavigationcontroller.h"
#include "imagedocumentpredecodecontroller.h"
#include "imagedocumentsourceloadrequest.h"
#include "imagedocumentsourceloadscope.h"
#include "imagedocumentstate.h"
#include "imageopencontroller.h"
#include "localization/activenavigationboundarytext.h"
#include "navigation/navigationlogging.h"
#include "predecode/predecodelogging.h"
#include "presentation/imagespreadpresentationcontroller.h"

#include <QDebug>
#include <QtGlobal>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
template <typename> inline constexpr bool alwaysFalse = false;
}

namespace kiriview {
ImageDocumentRuntimeWorkflow::ImageDocumentRuntimeWorkflow(ImageDocumentRuntimeWorkflowPorts ports)
    : m_ports(std::move(ports))
{
    if (!m_ports.clearViewportTarget || !m_ports.stopViewportPlayback || !m_ports.loadSource) {
        qFatal("Image-document runtime workflow requires all command callbacks");
    }
}

void ImageDocumentRuntimeWorkflow::dispatchPlan(const ImageDocumentRuntimePlan& plan)
{
    for (const ImageDocumentRuntimeOperation& operation : plan) {
        dispatchOperation(operation);
    }
}

void ImageDocumentRuntimeWorkflow::dispatchPlanWhile(
    const ImageDocumentRuntimePlan& plan, const ContinuePlanCallback& shouldContinue)
{
    for (const ImageDocumentRuntimeOperation& operation : plan) {
        if (!shouldContinue()) {
            return;
        }
        dispatchOperation(operation);
    }
}

void ImageDocumentRuntimeWorkflow::dispatchOperation(const ImageDocumentRuntimeOperation& operation)
{
    std::visit(
        [this](const auto& payload) {
            using Operation = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Operation, CancelFileDeletionOperation>) {
                m_ports.deletionController.cancel();
            } else if constexpr (std::is_same_v<Operation, ShutdownSpreadOperation>) {
                m_ports.spreadController.shutdown();
            } else if constexpr (std::is_same_v<Operation, ClearMediaEntrySourceOperation>) {
                if (m_ports.mediaEntrySourceStore != nullptr) {
                    m_ports.mediaEntrySourceStore->clear();
                }
            } else if constexpr (std::is_same_v<Operation, ClearPredecodeOperation>) {
                m_ports.predecodeController.clear();
            } else if constexpr (std::is_same_v<Operation, CancelPredecodeOperation>) {
                m_ports.predecodeController.cancel();
            } else if constexpr (std::is_same_v<Operation,
                                     ScheduleAdjacentImagePredecodeOperation>) {
                std::optional<DisplayedPredecodeImage> secondaryImage
                    = m_ports.spreadController.secondaryDisplayedPredecodeImage();
                qCDebug(kiriviewPredecodeLog)
                    << "runtime scheduling adjacent image predecode"
                    << "hasExplicitTarget" << payload.target.has_value() << "targetUrl"
                    << diagnosticSourceReference(
                           payload.target.has_value() ? payload.target->url : QUrl())
                    << "targetKind"
                    << (payload.target.has_value() ? static_cast<int>(payload.target->kind) : -1)
                    << "targetPageIndex" << payload.targetPageIndex << "hasSecondaryImage"
                    << (secondaryImage.has_value() && secondaryImage->hasLocation());
                if (payload.target.has_value()) {
                    m_ports.predecodeController.scheduleImageNavigationTargetPredecode(
                        *payload.target, payload.targetPageIndex, std::move(secondaryImage));
                } else {
                    m_ports.predecodeController.scheduleAdjacentImagePredecode(
                        std::move(secondaryImage));
                }
            } else if constexpr (std::is_same_v<Operation, ResetRightToLeftReadingOperation>) {
                m_ports.spreadController.resetRightToLeftReading();
            } else if constexpr (std::is_same_v<Operation, ClearSecondaryPageOperation>) {
                m_ports.spreadController.clearSecondaryPage();
            } else if constexpr (std::is_same_v<Operation,
                                     NotifyRightToLeftReadingChangedOperation>) {
                m_ports.spreadController.notifyRightToLeftReadingChanged();
            } else if constexpr (std::is_same_v<Operation, CancelPageNavigationUpdateOperation>) {
                m_ports.navigationController.cancelPageNavigationUpdate();
            } else if constexpr (std::is_same_v<Operation, CancelNavigationOperation>) {
                m_ports.navigationController.cancelNavigation();
            } else if constexpr (std::is_same_v<Operation, CancelContainerNavigationOperation>) {
                m_ports.navigationController.cancelContainerNavigation();
            } else if constexpr (std::is_same_v<Operation, CancelAllNavigationOperation>) {
                m_ports.navigationController.cancelAllNavigation();
            } else if constexpr (std::is_same_v<Operation, ClearPageNavigationOperation>) {
                m_ports.navigationController.clearPageNavigation();
            } else if constexpr (std::is_same_v<Operation, UpdatePageNavigationOperation>) {
                m_ports.navigationController.updatePageNavigation();
            } else if constexpr (std::is_same_v<Operation, LoadUrlOperation>) {
                m_ports.loadSource(ImageDocumentSourceLoadRequest::fromSameScopePageTarget(
                    payload.target, payload.openedCollectionScope));
            } else if constexpr (std::is_same_v<Operation, LoadContainerImageOperation>) {
                if (payload.openedCollectionScope.isEmpty()) {
                    qFatal("Container image load operation requires an opened collection scope");
                }
                m_ports.loadSource(ImageDocumentSourceLoadRequest::fromContainerTarget(
                    payload.target, payload.openedCollectionScope));
            } else if constexpr (std::is_same_v<Operation,
                                     FinishEmptyContainerNavigationOperation>) {
                m_ports.openController.finishContainerNavigationWithEmptyContainer(
                    payload.containerUrl);
            } else if constexpr (std::is_same_v<Operation,
                                     FinishContainerNavigationLoadWithErrorOperation>) {
                m_ports.openController.finishContainerNavigationLoadWithError(
                    payload.containerUrl, payload.errorString);
            } else if constexpr (std::is_same_v<Operation,
                                     ReportContainerNavigationBoundaryOperation>) {
                invokeIfSet(m_ports.containerNavigationBoundaryReached,
                    containerNavigationBoundaryFeedbackText(payload.direction));
            } else if constexpr (std::is_same_v<Operation,
                                     ReportContainerNavigationListFailureOperation>) {
                qCDebug(kiriviewNavigationLog)
                    << "container navigation listing failed"
                    << "currentContainerUrl"
                    << diagnosticSourceReference(payload.failure.currentContainerUrl) << "parentUrl"
                    << diagnosticSourceReference(payload.failure.parentUrl) << "direction"
                    << static_cast<int>(payload.failure.direction) << "kind"
                    << static_cast<int>(payload.failure.kind) << "detail"
                    << diagnosticDetailReference(payload.failure.diagnosticDetail);
            } else if constexpr (std::is_same_v<Operation, LoadPageNavigationUrlOperation>) {
                qCDebug(kiriviewNavigationLog)
                    << "runtime loading page navigation target"
                    << "targetUrl" << diagnosticSourceReference(payload.target.url) << "targetKind"
                    << static_cast<int>(payload.target.kind);
                m_ports.loadSource(ImageDocumentSourceLoadRequest::fromSameScopePageTarget(
                    payload.target, payload.openedCollectionScope));
            } else if constexpr (std::is_same_v<Operation, CancelOpenOperation>) {
                m_ports.openController.cancel();
            } else if constexpr (std::is_same_v<Operation, ClearPresentationImageOperation>) {
                m_ports.state.clearDisplayedImageLocation();
                m_ports.clearViewportTarget();
                m_ports.spreadController.clearPrimaryPageSlot();
            } else if constexpr (std::is_same_v<Operation, RetireViewportPresentationOperation>) {
                m_ports.clearViewportTarget();
                m_ports.spreadController.clearPrimaryPageSlot();
            } else if constexpr (std::is_same_v<Operation, StopPresentationPlaybackOperation>) {
                m_ports.stopViewportPlayback();
            } else if constexpr (std::is_same_v<Operation,
                                     ClearLoadingContainerNavigationUrlOperation>) {
                m_ports.state.clearLoadingContainerNavigationUrl();
            } else if constexpr (std::is_same_v<Operation,
                                     SetLoadingContainerNavigationUrlOperation>) {
                m_ports.state.setLoadingContainerNavigationUrl(payload.url);
            } else if constexpr (std::is_same_v<Operation, SetContainerNavigationUrlOperation>) {
                m_ports.state.setContainerNavigationUrl(payload.url);
            } else if constexpr (std::is_same_v<Operation, PrepareSourceLoadOperation>) {
                m_ports.openController.prepareSourceLoad(payload.request);
                if (m_ports.mediaEntrySourceStore != nullptr) {
                    m_ports.mediaEntrySourceStore->prepareForOpenedCollectionScope(
                        openedCollectionScopeForImageDocumentSourceLoad(payload.request));
                }
            } else if constexpr (std::is_same_v<Operation, SelectImageTargetOperation>) {
                m_ports.state.setSelectedTarget(payload.target);
            } else if constexpr (std::is_same_v<Operation, BeginOpenOperation>) {
                m_ports.openController.open();
            } else if constexpr (std::is_same_v<Operation, SetErrorStringOperation>) {
                m_ports.state.setErrorString(payload.errorString);
            } else if constexpr (std::is_same_v<Operation, FinishEmptySourceLoadOperation>) {
                m_ports.openController.finishEmptySourceLoad();
            } else {
                static_assert(alwaysFalse<Operation>, "Unhandled image document runtime operation");
            }
        },
        operation);
}

void ImageDocumentRuntimeWorkflow::shutdownRuntime() { dispatchPlan(imageDocumentShutdownPlan()); }
}
