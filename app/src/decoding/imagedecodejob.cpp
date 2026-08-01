// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedecodejob.h"

#include "async/imagecallback.h"
#include "imageinputclassification.h"
#include "location/sourcekey.h"
#include "rawthumbnailpreview.h"
#include "thumbnailpreview.h"

#include <QDebug>
#include <optional>
#include <utility>
#include <variant>

namespace {
bool rawEmbeddedThumbnailPreviewEligible(
    const QByteArray& data, const kiriview::ImageDecodeRequest& request)
{
    const kiriview::ImageInputClassification classification
        = kiriview::classifyImageInput(data, request.imageUrl().fileName());
    return classification.kind == kiriview::ImageInputKind::Raw;
}

bool reusableAuthoritativeSeed(
    const kiriview::StaticDisplayImagePayload& seed, const kiriview::ImageDecodeRequest& request)
{
    return seed.isAuthoritative()
        && seed.sourceIdentity == kiriview::sourceKeyForUrl(request.imageUrl()).identity
        && seed.sourceRevision == request.sourceRevision();
}

void retainSourceDataLease(
    kiriview::DecodedImageResult& result, kiriview::ImageSourceDataLease lease)
{
    kiriview::DecodedImage* image = kiriview::decodedImageResultImage(result);
    if (image == nullptr) {
        return;
    }
    std::visit(
        [lease = std::move(lease)](auto& decoded) mutable {
            using Image = std::decay_t<decltype(decoded)>;
            if constexpr (std::is_same_v<Image, kiriview::StaticDecodedImage>) {
                decoded.displayImage.sourceDataLease = std::move(lease);
            } else {
                decoded.sourceDataLease = std::move(lease);
            }
        },
        *image);
}
}

namespace kiriview {
ImageDecodeJob::ImageDecodeJob(QObject* parent)
    : ImageDecodeJob(parent, Callbacks {})
{
}

ImageDecodeJob::ImageDecodeJob(QObject* parent, Callbacks callbacks)
    : ImageDecodeJob(parent, {}, std::move(callbacks))
{
}

ImageDecodeJob::ImageDecodeJob(QObject* parent, ImageDecodeDependencies dependencies)
    : ImageDecodeJob(parent, std::move(dependencies), Callbacks {})
{
}

ImageDecodeJob::ImageDecodeJob(
    QObject* parent, ImageDecodeDependencies dependencies, Callbacks callbacks)
    : QObject(parent)
    , m_dependencies(imageDecodeDependenciesWithDefaults(std::move(dependencies)))
    , m_callbacks(std::move(callbacks))
{
}

void ImageDecodeJob::start(
    ImageDecodeRequest request, std::optional<StaticDisplayImagePayload> authoritativeSeed)
{
    cancel();
    if (request.isEmpty()) {
        qWarning().noquote() << "KiriView image decode rejected empty request";
        return;
    }
    if (!m_dependencies.dataLoader || !m_dependencies.dataDecoder) {
        qWarning().noquote() << "KiriView image decode rejected missing runtime dependency";
        return;
    }

    m_authoritativeSeed = std::move(authoritativeSeed);
    const ImageDecodeJobTicket ticket = m_state.start(std::move(request));
    m_dataLoadJob = m_dependencies.dataLoader(
        this, ticket.request,
        [this, ticket](ImageSourceData sourceData) mutable {
            if (!sourceData.lease.isManaged()) {
                sourceData.lease = m_dependencies.sourceDataBudget->startLease();
            }
            const qsizetype reservedByteCount = sourceData.lease.reservedByteCount();
            if (sourceData.data.size() > reservedByteCount
                && !sourceData.lease.tryReserve(sourceData.data.size() - reservedByteCount)) {
                ImageDecodeJobRuntimePlan errorPlan = m_state.acceptLoadError(ticket);
                const auto* errorOperation
                    = std::get_if<DeliverImageLoadErrorOperation>(&errorPlan.operation);
                if (errorOperation != nullptr) {
                    invokeIfSet(m_callbacks.loadError, errorOperation->request,
                        imageSourceDataResourceLimitDiagnostic());
                }
                return;
            }

            ImageDecodeJobRuntimePlan plan = m_state.acceptLoadedData(ticket);
            auto* operation = std::get_if<StartImageDecodeOperation>(&plan.operation);
            if (operation == nullptr) {
                return;
            }

            ImageDecodeRequest currentRequest = operation->request.withSourceRevision(
                ImageSourceRevision::fromData(sourceData.data));
            if (m_authoritativeSeed.has_value()
                && reusableAuthoritativeSeed(*m_authoritativeSeed, currentRequest)) {
                StaticDisplayImagePayload seed = std::move(*m_authoritativeSeed);
                m_authoritativeSeed.reset();
                ImageDecodeJobRuntimePlan resultPlan = m_state.acceptDecodeResult(ticket);
                if (!std::holds_alternative<DeliverImageDecodeResultOperation>(
                        resultPlan.operation)) {
                    return;
                }
                EmbeddedMetadata metadata = seed.embeddedMetadata;
                invokeIfSet(m_callbacks.decoded, std::move(currentRequest),
                    successfulDecodedImageResult(
                        StaticDecodedImage { std::move(seed), std::move(metadata) }));
                return;
            }
            m_authoritativeSeed.reset();
            startDecode(std::move(sourceData), ticket, std::move(currentRequest));
        },
        [this, ticket](const QString& errorString) {
            ImageDecodeJobRuntimePlan plan = m_state.acceptLoadError(ticket);
            const auto* operation = std::get_if<DeliverImageLoadErrorOperation>(&plan.operation);
            if (operation == nullptr) {
                return;
            }

            invokeIfSet(m_callbacks.loadError, operation->request, errorString);
        });
}

void ImageDecodeJob::cancel()
{
    m_state.cancel();
    m_dataLoadJob.cancel();
    m_thumbnailPreviewLookupJob.cancel();
    m_decodeWorkerTask.cancel();
    m_rawThumbnailPreviewWorkerTask.cancel();
    m_authoritativeSeed.reset();
}

bool ImageDecodeJob::hasActiveRequest() const { return m_state.hasActiveRequest(); }

void ImageDecodeJob::startDecode(
    ImageSourceData sourceData, ImageDecodeJobTicket ticket, ImageDecodeRequest request)
{
    startThumbnailPreviewLookup(sourceData.data, sourceData.lease, ticket, request);

    const ImageDataDecoder decoder = m_dependencies.dataDecoder;
    ImageDecodeRequest deliveredRequest = request;
    m_decodeWorkerTask = m_dependencies.workerScheduler.run(
        this,
        [decoder, sourceData = std::move(sourceData), request = std::move(request)]() mutable {
            DecodedImageResult result = decoder(sourceData.data, request);
            retainSourceDataLease(result, std::move(sourceData.lease));
            return result;
        },
        [this, ticket = std::move(ticket), request = std::move(deliveredRequest)](
            DecodedImageResult result) mutable {
            ImageDecodeJobRuntimePlan plan = m_state.acceptDecodeResult(ticket);
            if (!std::holds_alternative<DeliverImageDecodeResultOperation>(plan.operation)) {
                return;
            }

            invokeIfSet(m_callbacks.decoded, std::move(request), std::move(result));
        });
}

void ImageDecodeJob::startThumbnailPreviewLookup(const QByteArray& data,
    ImageSourceDataLease sourceDataLease, ImageDecodeJobTicket ticket,
    const ImageDecodeRequest& request)
{
    if (!m_dependencies.thumbnailPreviewLookupProvider
        || (!m_callbacks.thumbnailPreview
            && !m_dependencies.rawEmbeddedThumbnailPreviewExtractor)) {
        return;
    }

    std::optional<XdgThumbnailPreviewRequest> previewRequest
        = xdgThumbnailPreviewRequestForDecodeData(data, request);
    if (!previewRequest.has_value()) {
        return;
    }

    std::optional<ThumbnailCacheLookupRequest> lookupRequest
        = xdgThumbnailPreviewCacheLookupRequest(*previewRequest);
    if (!lookupRequest.has_value()) {
        return;
    }

    const bool rawPreviewEligible = m_dependencies.rawEmbeddedThumbnailPreviewExtractor
        && rawEmbeddedThumbnailPreviewEligible(data, request);
    ImageSourceData rawPreviewData = rawPreviewEligible
        ? ImageSourceData(data, std::move(sourceDataLease))
        : ImageSourceData();
    m_thumbnailPreviewLookupJob = m_dependencies.thumbnailPreviewLookupProvider(this,
        std::move(*lookupRequest),
        [this, ticket = std::move(ticket), previewRequest = std::move(*previewRequest), request,
            rawPreviewEligible, rawPreviewData = std::move(rawPreviewData)](
            ThumbnailCacheLookupResult lookupResult) mutable {
            ImageDecodeJobRuntimePlan plan = m_state.acceptThumbnailPreview(ticket);
            if (!std::holds_alternative<DeliverImageThumbnailPreviewOperation>(plan.operation)) {
                return;
            }

            XdgThumbnailPreviewResult previewResult
                = xdgThumbnailPreviewResult(previewRequest, std::move(lookupResult));
            if (previewResult.status == ThumbnailCacheLookupStatus::Missing && rawPreviewEligible) {
                startRawEmbeddedThumbnailPreviewValidation(
                    std::move(rawPreviewData), ticket, request);
                return;
            }
            if (previewResult.status != ThumbnailCacheLookupStatus::Ready
                || !m_callbacks.thumbnailPreview) {
                return;
            }

            std::optional<StaticDisplayImagePayload> payload
                = xdgThumbnailPreviewDisplayPayload(request, previewResult);
            if (!payload.has_value()) {
                return;
            }

            invokeIfSet(m_callbacks.thumbnailPreview, request, std::move(*payload));
        });
}

void ImageDecodeJob::startRawEmbeddedThumbnailPreviewValidation(
    ImageSourceData sourceData, ImageDecodeJobTicket ticket, const ImageDecodeRequest& request)
{
    if (!m_dependencies.rawEmbeddedThumbnailPreviewExtractor) {
        return;
    }

    const RawEmbeddedThumbnailPreviewExtractor extractor
        = m_dependencies.rawEmbeddedThumbnailPreviewExtractor;
    m_rawThumbnailPreviewWorkerTask = m_dependencies.workerScheduler.run(
        this,
        [extractor, sourceData = std::move(sourceData), request]() mutable {
            RawEmbeddedThumbnailPreviewResult result = extractor(sourceData.data, request);
            return rawEmbeddedThumbnailPreviewDisplayPayload(request, result);
        },
        [this, ticket = std::move(ticket), request](
            std::optional<StaticDisplayImagePayload> payload) mutable {
            if (!payload.has_value() || !m_callbacks.thumbnailPreview) {
                return;
            }

            ImageDecodeJobRuntimePlan plan = m_state.acceptThumbnailPreview(ticket);
            if (!std::holds_alternative<DeliverImageThumbnailPreviewOperation>(plan.operation)) {
                return;
            }

            invokeIfSet(m_callbacks.thumbnailPreview, request, std::move(*payload));
        });
}
}
