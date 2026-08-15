// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodeloadcontroller.h"

#include "decoding/decodedimageresult.h"
#include "decoding/imagedecodejob.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "predecodelogging.h"

#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <optional>
#include <utility>
#include <variant>

namespace kiriview {
PredecodeLoadController::PredecodeLoadController(
    QObject* parent, ImageDecodeDependencies decodeDependencies, qsizetype cacheByteBudget)
    : m_parent(parent)
    , m_decodeDependencies(imageDecodeDependenciesWithDefaults(std::move(decodeDependencies)))
    , m_loadState(cacheByteBudget)
{
}

PredecodeLoadController::~PredecodeLoadController() { m_lifetime.reset(); }

void PredecodeLoadController::cacheDisplayedImages(
    const std::vector<DisplayedPredecodeImage>& images)
{
    m_loadState.cacheDisplayedImages(images);
}

void PredecodeLoadController::clearWindow() { m_loadState.clearWindow(); }

void PredecodeLoadController::startWindowLoads(const PredecodeLoadWindow& window)
{
    qCDebug(kiriviewPredecodeLog) << "predecode controller start window"
                                  << "generation" << window.generation << "foregroundUrl"
                                  << diagnosticSourceReference(
                                         window.foregroundOwnedLocation.imageUrl())
                                  << "locations" << window.locations.size() << "parallelLimit"
                                  << window.parallelLimit;
    m_loadState.startWindow(window, m_activeDecodes.activeLoads());
    startNextLoads();
}

void PredecodeLoadController::retireBackgroundLoad(const DisplayedImageLocation& location)
{
    m_loadState.retireBackgroundLoad(location);
    m_activeDecodes.cancelLocation(location);
}

void PredecodeLoadController::reclaimDisplayOutputAliases()
{
    m_loadState.reclaimDisplayOutputAliases();
}

void PredecodeLoadController::supersedeBackgroundWindow() { m_loadState.cancelBackgroundWork(); }

void PredecodeLoadController::startNextLoads()
{
    while (true) {
        std::optional<PredecodeLoadStart> load
            = m_loadState.takeNextLoad(m_activeDecodes.activeLoads());
        if (!load.has_value()) {
            return;
        }
        if (!startLoad(std::move(*load))) {
            return;
        }
    }
}

bool PredecodeLoadController::startLoad(PredecodeLoadStart load)
{
    if (load.request.isEmpty()) {
        qCDebug(kiriviewPredecodeLog) << "predecode decode job skipped"
                                      << "reason"
                                      << "empty-request";
        return false;
    }

    auto* decodeJob = new ImageDecodeJob(m_parent, m_decodeDependencies,
        ImageDecodeJob::Callbacks {
            [this](const ImageDecodeRequest& request, const DecodedImageResult& result) {
                finishDecode(request, result);
            },
            [this](const ImageDecodeRequest& request, const ImageDataLoadError& error) {
                finishLoadError(request, error);
            },
            {},
            [owner = QPointer<QObject>(m_parent), lifetime = std::weak_ptr(m_lifetime), this](
                const ImageDecodeRequest& request) {
                if (owner == nullptr || lifetime.expired()) {
                    return;
                }
                QMetaObject::invokeMethod(
                    owner,
                    [lifetime, this, request]() {
                        if (lifetime.expired()) {
                            return;
                        }
                        finishRetirement(request);
                    },
                    Qt::QueuedConnection);
            },
        });
    const ImageDecodeRequest request = load.request;
    if (!m_activeDecodes.add(std::move(load.request), std::move(load.workKey), decodeJob)) {
        qCDebug(kiriviewPredecodeLog) << "predecode decode job skipped"
                                      << "reason"
                                      << "active-store-rejected"
                                      << "generation" << request.id() << "url"
                                      << diagnosticSourceReference(request.imageUrl());
        decodeJob->deleteLater();
        return false;
    }

    qCDebug(kiriviewPredecodeLog) << "predecode decode job start"
                                  << "generation" << request.id() << "url"
                                  << diagnosticSourceReference(request.imageUrl()) << "activeLoads"
                                  << m_activeDecodes.size();
    decodeJob->start(
        request, std::move(load.authoritativeSeed), ImageDecodeWorkspacePriority::Speculative);
    return true;
}

void PredecodeLoadController::finishLoadError(
    const ImageDecodeRequest& request, const ImageDataLoadError& error)
{
    std::optional<PredecodeRetiringDecode> active = m_activeDecodes.beginRetirement(request);
    if (!active.has_value()) {
        qCDebug(kiriviewPredecodeLog) << "predecode load error ignored"
                                      << "reason"
                                      << "inactive-request"
                                      << "generation" << request.id() << "url"
                                      << diagnosticSourceReference(request.imageUrl());
        return;
    }
    m_loadState.completeWork(active->workKey);

    std::visit(
        [&request](const auto& detail) {
            qCDebug(kiriviewPredecodeLog).noquote()
                << "predecode load error"
                << "generation" << request.id() << "url"
                << diagnosticSourceReference(request.imageUrl()) << "error" << detail;
        },
        error);
}

void PredecodeLoadController::finishDecode(
    const ImageDecodeRequest& request, const DecodedImageResult& result)
{
    std::optional<PredecodeRetiringDecode> active = m_activeDecodes.beginRetirement(request);
    if (!active.has_value()) {
        qCDebug(kiriviewPredecodeLog) << "predecode decode result ignored"
                                      << "reason"
                                      << "inactive-request"
                                      << "generation" << request.id() << "url"
                                      << diagnosticSourceReference(request.imageUrl());
        return;
    }
    m_loadState.completeWork(active->workKey);
    const ImageDecodeRequest& activeRequest = active->request;

    const auto* failure = decodedImageResultFailure(result);
    if (failure != nullptr) {
        qCDebug(kiriviewPredecodeLog) << "predecode decode failed"
                                      << "generation" << activeRequest.id() << "url"
                                      << diagnosticSourceReference(activeRequest.imageUrl())
                                      << "error" << diagnosticDetailReference(failure->errorString);
        return;
    }

    const auto* staticImage = decodedImageResultImageAs<StaticDecodedImage>(result);
    if (staticImage != nullptr) {
        StaticDisplayImagePayload displayImage = staticImage->displayImage;
        qCDebug(kiriviewPredecodeLog)
            << "predecode decode finished"
            << "generation" << activeRequest.id() << "url"
            << diagnosticSourceReference(activeRequest.imageUrl()) << "originalSize"
            << displayImage.originalSize << "rasterSize" << displayImage.image.size() << "byteCost"
            << displayImage.byteCost();
        m_loadState.cacheDecodedImage(
            activeRequest, std::move(displayImage), staticImage->embeddedMetadata);
    } else {
        qCDebug(kiriviewPredecodeLog) << "predecode decoded non-static image ignored"
                                      << "generation" << activeRequest.id() << "url"
                                      << diagnosticSourceReference(activeRequest.imageUrl());
    }
}

void PredecodeLoadController::finishRetirement(const ImageDecodeRequest& request)
{
    if (!m_activeDecodes.retire(request)) {
        qCDebug(kiriviewPredecodeLog) << "predecode retirement ignored"
                                      << "reason"
                                      << "inactive-request"
                                      << "generation" << request.id() << "url"
                                      << diagnosticSourceReference(request.imageUrl());
        return;
    }

    m_loadState.reconcileWindow(m_activeDecodes.activeLoads());
    startNextLoads();
}

void PredecodeLoadController::cancelBackgroundWork()
{
    qCDebug(kiriviewPredecodeLog) << "predecode controller cancel background"
                                  << "activeLoads" << m_activeDecodes.size();
    m_activeDecodes.cancel();
    m_loadState.cancelBackgroundWork();
}

void PredecodeLoadController::clear()
{
    qCDebug(kiriviewPredecodeLog) << "predecode controller clear";
    m_activeDecodes.cancel();
    m_loadState.clear();
}

std::optional<PredecodedImage> PredecodeLoadController::findPredecodedImage(
    const DisplayedImageLocation& location) const
{
    return m_loadState.findPredecodedImage(location);
}
}
