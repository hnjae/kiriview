// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagecontainernavigationcontroller.h"

#include "async/imagecallback.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "imagedocumentpagecandidaterepository.h"
#include "imagedocumentpagenavigationpolicy.h"
#include "localization/mediaentrysourceerrortext.h"
#include "location/imageurl.h"
#include "navigationlogging.h"

#include <QDebug>
#include <QString>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
QUrl parentUrlForAdjacentContainerNavigation(const QUrl& currentContainerUrl)
{
    if (currentContainerUrl.isEmpty()) {
        return {};
    }

    return kiriview::parentUrlForContainerNavigation(currentContainerUrl);
}

QString projectCandidateLoadError(const kiriview::ImageDocumentPageCandidateLoadError& error)
{
    return std::visit(
        [](const auto& detail) -> QString {
            using Error = std::decay_t<decltype(detail)>;
            if constexpr (std::is_same_v<Error, QString>) {
                return detail;
            } else if constexpr (std::is_same_v<Error, kiriview::KioOperationFailure>) {
                qCWarning(kiriviewNavigationLog).noquote()
                    << "container image candidate loading failed"
                    << "operationKind" << static_cast<int>(detail.operationKind) << "targetUrl"
                    << kiriview::diagnosticSourceReference(detail.targetUrl) << "rawErrorCode"
                    << detail.rawErrorCode.value_or(0) << "canceled" << detail.canceled << "detail"
                    << kiriview::diagnosticDetailReference(detail.diagnosticDetail) << "retryable"
                    << detail.retryable;
                return detail.userMessage;
            } else {
                qCWarning(kiriviewNavigationLog).noquote()
                    << "container image candidate loading failed" << detail;
                return kiriview::mediaEntrySourceErrorText(detail);
            }
        },
        error);
}
}

namespace kiriview {
ImageContainerNavigationController::ImageContainerNavigationController(QObject* parent,
    const ImageDocumentPageCandidateRepository& candidateRepository, Callbacks callbacks)
    : QObject(parent)
    , m_candidateRepository(candidateRepository)
    , m_callbacks(std::move(callbacks))
{
}

bool ImageContainerNavigationController::canOpenAdjacentContainer(const QUrl& currentContainerUrl)
{
    const QUrl parentUrl = parentUrlForAdjacentContainerNavigation(currentContainerUrl);
    return parentUrl.isValid() && !parentUrl.isEmpty();
}

void ImageContainerNavigationController::openAdjacentContainer(
    const QUrl& currentContainerUrl, NavigationDirection direction)
{
    const QUrl parentUrl = parentUrlForAdjacentContainerNavigation(currentContainerUrl);
    if (!parentUrl.isValid() || parentUrl.isEmpty()) {
        return;
    }
    cancel();

    const quint64 operationId = m_navigationState.startNavigation();
    m_containerListJob = m_candidateRepository.loadContainers(
        this, parentUrl,
        [this, operationId, direction, currentContainerUrl](
            const std::vector<ContainerNavigationCandidate>& candidates) {
            finishContainerNavigation(operationId, candidates, direction, currentContainerUrl);
        },
        [this, operationId, currentContainerUrl, parentUrl, direction](
            KioOperationFailure failure) {
            finishContainerNavigationListWithError(
                operationId, currentContainerUrl, parentUrl, direction, std::move(failure));
        });
}

void ImageContainerNavigationController::cancel()
{
    m_navigationState.cancel();
    m_containerListJob.cancel();
    m_firstImageJob.cancel();
}

void ImageContainerNavigationController::finishContainerNavigation(quint64 operationId,
    const std::vector<ContainerNavigationCandidate>& candidates, NavigationDirection direction,
    const QUrl& currentContainerUrl)
{
    if (!m_navigationState.acceptsNavigation(operationId)) {
        return;
    }

    if (!containerNavigationCandidateIndex(candidates, currentContainerUrl).has_value()) {
        m_navigationState.finishNavigation(operationId);
        return;
    }

    const auto target
        = adjacentContainerNavigationCandidate(candidates, currentContainerUrl, direction);
    if (!target.has_value()) {
        if (m_navigationState.finishNavigation(operationId)) {
            reportNavigationPlan(ImageDocumentPageNavigationPlan {
                ReportContainerNavigationBoundaryEffect { direction },
            });
        }
        return;
    }

    loadFirstImageFromContainerNavigation(operationId, *target);
}

void ImageContainerNavigationController::finishContainerNavigationListWithError(quint64 operationId,
    const QUrl& currentContainerUrl, const QUrl& parentUrl, NavigationDirection direction,
    KioOperationFailure failure)
{
    if (!m_navigationState.finishNavigation(operationId)) {
        return;
    }

    reportNavigationPlan(ImageDocumentPageNavigationPlan {
        ReportContainerNavigationListErrorEffect {
            ContainerNavigationListFailure {
                currentContainerUrl,
                parentUrl,
                direction,
                std::move(failure),
            },
        },
    });
}

void ImageContainerNavigationController::loadFirstImageFromContainerNavigation(
    quint64 operationId, const ContainerNavigationCandidate& container)
{
    if (!m_callbacks.resolveExternalSource) {
        finishContainerNavigationLoadWithError(
            operationId, container.url, ImageContainerOpenError::Generic, QString());
        return;
    }
    const ImageContainerOpenPlan plan = imageContainerOpenPlanForCandidate(
        container, m_callbacks.resolveExternalSource(container.url));
    if (!plan.shouldLoadCandidates()) {
        finishContainerNavigationLoadWithError(operationId, container.url, plan.error, QString());
        return;
    }

    m_firstImageJob = m_candidateRepository.loadImages(
        this, *plan.source,
        [this, operationId, scope = plan.openedCollectionScope](
            const std::vector<ImageDocumentPageCandidate>& candidates) mutable {
            finishContainerNavigationImageLoad(operationId, std::move(scope), candidates);
        },
        [this, operationId, containerUrl = container.url](
            const ImageDocumentPageCandidateLoadError& error) {
            if (!m_navigationState.finishNavigation(operationId)) {
                return;
            }
            reportNavigationPlan(
                ImageDocumentPageNavigationPlan { ReportContainerNavigationErrorEffect {
                    containerUrl,
                    ImageContainerOpenError::Generic,
                    projectCandidateLoadError(error),
                } });
        });
}

void ImageContainerNavigationController::finishContainerNavigationImageLoad(quint64 operationId,
    OpenedCollectionScopeLocation openedCollectionScope,
    const std::vector<ImageDocumentPageCandidate>& candidates)
{
    const ImageContainerOpenResult result = imageContainerOpenResultForCandidates(candidates);
    if (result.openedImage()) {
        openImageFromContainerNavigation(
            operationId, *result.target, std::move(openedCollectionScope));
        return;
    }

    finishContainerNavigationLoadWithError(
        operationId, openedCollectionScope.fileUrl(), result.error, QString());
}

void ImageContainerNavigationController::openImageFromContainerNavigation(quint64 operationId,
    const ImageDocumentPageTarget& target, OpenedCollectionScopeLocation openedCollectionScope)
{
    if (!m_navigationState.finishNavigation(operationId)) {
        return;
    }

    reportNavigationPlan(
        ImageDocumentPageNavigationPlan { OpenContainerImageDocumentPageNavigationEffect {
            target,
            std::move(openedCollectionScope),
        } });
}

void ImageContainerNavigationController::finishContainerNavigationLoadWithError(quint64 operationId,
    const QUrl& containerUrl, ContainerNavigationError error, const QString& errorString)
{
    if (!m_navigationState.finishNavigation(operationId)) {
        return;
    }

    reportNavigationPlan(ImageDocumentPageNavigationPlan { ReportContainerNavigationErrorEffect {
        containerUrl,
        error,
        errorString,
    } });
}

void ImageContainerNavigationController::reportNavigationPlan(ImageDocumentPageNavigationPlan plan)
{
    invokeIfSet(m_callbacks.navigationPlan, std::move(plan));
}
}
