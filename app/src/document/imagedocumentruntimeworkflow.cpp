// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentruntimeworkflow.h"

#include "archive/mediaentrysourcestore.h"
#include "async/imagecallback.h"
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
#include <optional>
#include <utility>

namespace {
kiriview::ImageDocumentRuntimeOperations runtimeOperations(
    kiriview::ImageDocumentRuntimeWorkflowPorts ports)
{
    kiriview::ImageDocumentRuntimeOperations operations;
    operations.lifecycle.cancelFileDeletion = [ports]() {
        if (ports.deletionController != nullptr) {
            ports.deletionController->cancel();
        }
    };
    operations.lifecycle.shutdownSpread = [ports]() {
        if (ports.spreadController != nullptr) {
            ports.spreadController->shutdown();
        }
    };
    operations.mediaEntrySource.clear = [ports]() {
        if (ports.mediaEntrySourceStore != nullptr) {
            ports.mediaEntrySourceStore->clear();
        }
    };
    operations.predecode.clearPredecode = [ports]() {
        if (ports.predecodeController != nullptr) {
            ports.predecodeController->clear();
        }
    };
    operations.predecode.cancelPredecode = [ports]() {
        if (ports.predecodeController != nullptr) {
            ports.predecodeController->cancel();
        }
    };
    operations.predecode.scheduleAdjacentImagePredecode
        = [ports](const kiriview::ScheduleAdjacentImagePredecodeOperation& operation) {
              if (ports.predecodeController != nullptr && ports.spreadController != nullptr) {
                  std::optional<kiriview::DisplayedPredecodeImage> secondaryImage
                      = ports.spreadController->secondaryDisplayedPredecodeImage();
                  qCDebug(kiriviewPredecodeLog)
                      << "runtime scheduling adjacent image predecode"
                      << "hasExplicitTarget" << operation.target.has_value() << "targetUrl"
                      << (operation.target.has_value() ? operation.target->url : QUrl())
                      << "targetKind"
                      << (operation.target.has_value() ? static_cast<int>(operation.target->kind)
                                                       : -1)
                      << "targetPageIndex" << operation.targetPageIndex << "hasSecondaryImage"
                      << (secondaryImage.has_value() && secondaryImage->hasLocation());
                  if (operation.target.has_value()) {
                      ports.predecodeController->scheduleImageNavigationTargetPredecode(
                          *operation.target, operation.targetPageIndex, std::move(secondaryImage));
                      return;
                  }

                  ports.predecodeController->scheduleAdjacentImagePredecode(
                      std::move(secondaryImage));
              }
          };
    operations.spread.resetRightToLeftReading = [ports]() {
        if (ports.spreadController != nullptr) {
            ports.spreadController->resetRightToLeftReading();
        }
    };
    operations.spread.clearSecondaryPage = [ports]() {
        if (ports.spreadController != nullptr) {
            ports.spreadController->clearSecondaryPage();
        }
    };
    operations.spread.notifyRightToLeftReadingChanged = [ports]() {
        if (ports.spreadController != nullptr) {
            ports.spreadController->notifyRightToLeftReadingChanged();
        }
    };
    operations.navigation.cancelPageNavigationUpdate = [ports]() {
        if (ports.navigationController != nullptr) {
            ports.navigationController->cancelPageNavigationUpdate();
        }
    };
    operations.navigation.cancelNavigation = [ports]() {
        if (ports.navigationController != nullptr) {
            ports.navigationController->cancelNavigation();
        }
    };
    operations.navigation.cancelContainerNavigation = [ports]() {
        if (ports.navigationController != nullptr) {
            ports.navigationController->cancelContainerNavigation();
        }
    };
    operations.navigation.cancelAllNavigation = [ports]() {
        if (ports.navigationController != nullptr) {
            ports.navigationController->cancelAllNavigation();
        }
    };
    operations.navigation.clearPageNavigation = [ports]() {
        if (ports.navigationController != nullptr) {
            ports.navigationController->clearPageNavigation();
        }
    };
    operations.navigation.updatePageNavigation = [ports]() {
        if (ports.navigationController != nullptr) {
            ports.navigationController->updatePageNavigation();
        }
    };
    operations.navigation.loadUrl = [ports](const kiriview::ImageDocumentPageTarget& target,
                                        const kiriview::OpenedCollectionScopeLocation& scope) {
        if (ports.loadSource) {
            ports.loadSource(
                kiriview::ImageDocumentSourceLoadRequest::fromSameScopePageTarget(target, scope));
        }
    };
    operations.navigation.loadContainerImage
        = [ports](const kiriview::ImageDocumentPageTarget& target,
              const kiriview::OpenedCollectionScopeLocation& scope) {
              if (ports.loadSource && !scope.isEmpty()) {
                  ports.loadSource(
                      kiriview::ImageDocumentSourceLoadRequest::fromContainerTarget(target, scope));
              }
          };
    operations.navigation.finishEmptyContainerNavigation = [ports](const QUrl& containerUrl) {
        if (ports.openController != nullptr) {
            ports.openController->finishContainerNavigationWithEmptyContainer(containerUrl);
        }
    };
    operations.navigation.finishContainerNavigationLoadWithError = [ports](const QUrl& containerUrl,
                                                                       const QString& errorString) {
        if (ports.openController != nullptr) {
            ports.openController->finishContainerNavigationLoadWithError(containerUrl, errorString);
        }
    };
    operations.navigation.reportContainerNavigationBoundary
        = [ports](kiriview::NavigationDirection direction) {
              if (ports.containerNavigationBoundaryReached) {
                  ports.containerNavigationBoundaryReached(
                      kiriview::containerNavigationBoundaryFeedbackText(direction));
              }
          };
    operations.navigation.reportContainerNavigationListFailure
        = [](const kiriview::ContainerNavigationListFailure& failure) {
              qCDebug(kiriviewNavigationLog)
                  << "container navigation listing failed"
                  << "currentContainerUrl" << failure.currentContainerUrl << "parentUrl"
                  << failure.parentUrl << "direction" << static_cast<int>(failure.direction)
                  << "kind" << static_cast<int>(failure.kind) << "detail"
                  << failure.diagnosticDetail;
          };
    operations.navigation.loadPageNavigationUrl =
        [ports](const kiriview::ImageDocumentPageTarget& target,
            const kiriview::OpenedCollectionScopeLocation& scope) {
            qCDebug(kiriviewNavigationLog)
                << "runtime loading page navigation target"
                << "targetUrl" << target.url << "targetKind" << static_cast<int>(target.kind);
            if (ports.loadSource) {
                ports.loadSource(kiriview::ImageDocumentSourceLoadRequest::fromSameScopePageTarget(
                    target, scope));
            }
        };
    operations.open.cancelOpen = [ports]() {
        if (ports.openController != nullptr) {
            ports.openController->cancel();
        }
    };
    operations.open.clearPresentationImage = [ports]() {
        if (ports.state != nullptr) {
            ports.state->clearDisplayedImageLocation();
        }
        kiriview::invokeIfSet(ports.clearViewportTarget);
        if (ports.spreadController != nullptr) {
            ports.spreadController->clearPrimaryPageSlot();
        }
    };
    operations.open.retireViewportPresentation = [ports]() {
        kiriview::invokeIfSet(ports.clearViewportTarget);
        if (ports.spreadController != nullptr) {
            ports.spreadController->clearPrimaryPageSlot();
        }
    };
    operations.open.stopPresentationPlayback
        = [ports]() { kiriview::invokeIfSet(ports.stopViewportPlayback); };
    operations.sourceLoad.clearLoadingContainerNavigationUrl = [ports]() {
        if (ports.state != nullptr) {
            ports.state->clearLoadingContainerNavigationUrl();
        }
    };
    operations.sourceLoad.setLoadingContainerNavigationUrl = [ports](const QUrl& url) {
        if (ports.state != nullptr) {
            ports.state->setLoadingContainerNavigationUrl(url);
        }
    };
    operations.sourceLoad.setContainerNavigationUrl = [ports](const QUrl& url) {
        if (ports.state != nullptr) {
            ports.state->setContainerNavigationUrl(url);
        }
    };
    operations.sourceLoad.prepareSourceLoad
        = [ports](const kiriview::ImageDocumentSourceLoadRequest& request) {
              if (ports.openController != nullptr) {
                  ports.openController->prepareSourceLoad(request);
              }
              if (ports.mediaEntrySourceStore != nullptr && ports.state != nullptr) {
                  ports.mediaEntrySourceStore->prepareForOpenedCollectionScope(
                      kiriview::openedCollectionScopeForImageDocumentSourceLoad(request));
              }
          };
    operations.open.selectImageTarget
        = [ports](const kiriview::SelectImageTargetOperation& operation) {
              if (ports.state != nullptr) {
                  ports.state->setSelectedTarget(operation.target);
              }
          };
    operations.sourceLoad.beginOpen = [ports]() {
        if (ports.openController != nullptr) {
            ports.openController->open();
        }
    };
    operations.open.setErrorString = [ports](const QString& errorString) {
        if (ports.state != nullptr) {
            ports.state->setErrorString(errorString);
        }
    };
    operations.open.finishEmptySourceLoad = [ports]() {
        if (ports.openController != nullptr) {
            ports.openController->finishEmptySourceLoad();
        }
    };
    return operations;
}
}

namespace kiriview {
ImageDocumentRuntimeWorkflow::ImageDocumentRuntimeWorkflow(
    ImageDocumentRuntimeOperations operations)
    : m_executor(std::move(operations))
{
}

ImageDocumentRuntimeWorkflow::ImageDocumentRuntimeWorkflow(ImageDocumentRuntimeWorkflowPorts ports)
    : ImageDocumentRuntimeWorkflow(runtimeOperations(std::move(ports)))
{
}

void ImageDocumentRuntimeWorkflow::dispatchPlan(const ImageDocumentRuntimePlan& plan)
{
    m_executor.dispatchPlan(plan);
}

void ImageDocumentRuntimeWorkflow::shutdownRuntime() { m_executor.shutdownRuntime(); }
}
