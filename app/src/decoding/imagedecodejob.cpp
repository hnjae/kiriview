// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedecodejob.h"

#include "async/imagecallback.h"
#include "imageinputclassification.h"
#include "location/sourcekey.h"
#include "rawthumbnailpreview.h"
#include "system/kiooperationfailure.h"
#include "thumbnailpreview.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <Qt>
#include <memory>
#include <mutex>
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

}

namespace kiriview {
namespace {
    struct DeleteDecodeRetirementRelayLater final
    {
        void operator()(QObject* relay) const
        {
            if (relay != nullptr && !QCoreApplication::closingDown()) {
                relay->deleteLater();
            }
        }
    };

    class ImageDecodeRetirementBarrier;

    class ImageDecodeRetirementTicket final
    {
    public:
        explicit ImageDecodeRetirementTicket(std::shared_ptr<ImageDecodeRetirementBarrier> barrier)
            : m_barrier(std::move(barrier))
        {
        }
        ~ImageDecodeRetirementTicket();
        Q_DISABLE_COPY_MOVE(ImageDecodeRetirementTicket)

    private:
        std::shared_ptr<ImageDecodeRetirementBarrier> m_barrier;
    };

    class ImageDecodeRetirementBarrier final
        : public std::enable_shared_from_this<ImageDecodeRetirementBarrier>
    {
    public:
        using Relay = std::shared_ptr<QObject>;

        ImageDecodeRetirementBarrier(QThread* ownerThread, ImageDecodeRequest request,
            ImageDecodeJob::RetiredCallback retired)
            : m_request(std::move(request))
            , m_retired(std::move(retired))
        {
            Relay relay(new QObject, DeleteDecodeRetirementRelayLater {});
            if (ownerThread != nullptr && relay->moveToThread(ownerThread)) {
                m_relay = std::move(relay);
            }
        }

        std::shared_ptr<ImageDecodeRetirementTicket> openTicket()
        {
            {
                const std::scoped_lock lock(m_mutex);
                ++m_ticketCount;
            }
            return std::make_shared<ImageDecodeRetirementTicket>(shared_from_this());
        }

        void retireTicket()
        {
            ImageDecodeJob::RetiredCallback retired;
            ImageDecodeRequest request;
            Relay relay;
            {
                const std::scoped_lock lock(m_mutex);
                if (m_ticketCount == 0) {
                    return;
                }
                --m_ticketCount;
                if (m_ticketCount != 0 || m_delivered) {
                    return;
                }
                m_delivered = true;
                retired = std::move(m_retired);
                request = m_request;
                relay = m_relay;
            }
            if (!retired) {
                return;
            }
            QObject* relayObject = relay.get();
            if (relayObject == nullptr
                || !QMetaObject::invokeMethod(
                    relayObject,
                    [relay = std::move(relay), retired = std::move(retired),
                        request = std::move(request)]() mutable { retired(request); },
                    Qt::QueuedConnection)) {
                qWarning().noquote()
                    << "KiriView image decode retirement delivery could not be queued";
            }
        }

    private:
        std::mutex m_mutex;
        ImageDecodeRequest m_request;
        ImageDecodeJob::RetiredCallback m_retired;
        Relay m_relay;
        qsizetype m_ticketCount = 0;
        bool m_delivered = false;
    };

    ImageDecodeRetirementTicket::~ImageDecodeRetirementTicket() { m_barrier->retireTicket(); }
}

class ImageDecodeJobRun final
{
public:
    ImageDecodeJobRun(QThread* ownerThread, ImageDecodeRequest request,
        ImageDecodeWorkspacePriority priority, ImageDecodeJob::RetiredCallback retired)
        : request(std::move(request))
        , priority(priority)
        , barrier(std::make_shared<ImageDecodeRetirementBarrier>(
              ownerThread, this->request, std::move(retired)))
        , rootTicket(barrier->openTicket())
    {
    }

    [[nodiscard]] std::shared_ptr<ImageDecodeRetirementTicket> openTicket() const
    {
        return barrier->openTicket();
    }

    void finishLogical()
    {
        const std::scoped_lock lock(m_mutex);
        rootTicket.reset();
    }

    ImageDecodeRequest request;
    ImageDecodeWorkspacePriority priority = ImageDecodeWorkspacePriority::Interactive;

private:
    std::shared_ptr<ImageDecodeRetirementBarrier> barrier;
    std::shared_ptr<ImageDecodeRetirementTicket> rootTicket;
    std::mutex m_mutex;
};

namespace {
    ImageDecodeWorkspacePriority thumbnailPreviewPriority(
        const std::shared_ptr<ImageDecodeJobRun>& run)
    {
        return run != nullptr && run->priority == ImageDecodeWorkspacePriority::Speculative
            ? ImageDecodeWorkspacePriority::Speculative
            : ImageDecodeWorkspacePriority::Demanded;
    }

    struct ThumbnailPreviewPlanningResult
    {
        std::optional<XdgThumbnailPreviewRequest> previewRequest;
        ImageSourceData sourceData;
    };

    struct RawEmbeddedThumbnailPreviewWorkerResult
    {
        std::optional<StaticDisplayImagePayload> payload;
        ImageSourceData sourceData;
    };
}

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

ImageDecodeJob::~ImageDecodeJob() { cancel(); }

void ImageDecodeJob::start(ImageDecodeRequest request,
    std::optional<StaticDisplayImagePayload> authoritativeSeed,
    ImageDecodeWorkspacePriority priority)
{
    cancel();
    if (request.isEmpty()) {
        qWarning().noquote() << "KiriView image decode rejected empty request";
        return;
    }
    if (!m_dependencies.dataLoader || !m_dependencies.dataPlanner
        || m_dependencies.workspaceBudget == nullptr) {
        qWarning().noquote() << "KiriView image decode rejected missing runtime dependency";
        return;
    }

    m_authoritativeSeed = std::move(authoritativeSeed);
    const ImageDecodeJobTicket ticket = m_state.start(std::move(request));
    const std::shared_ptr<ImageDecodeJobRun> run = std::make_shared<ImageDecodeJobRun>(
        thread(), ticket.request, priority, m_callbacks.retired);
    m_run = run;
    const QPointer<ImageDecodeJob> guardedJob(this);
    const std::shared_ptr<ImageDecodeRetirementTicket> loadRetirement = run->openTicket();
    const ImageDataLoader dataLoader = m_dependencies.dataLoader;
    ImageIoJob loadJob = dataLoader(
        this, ticket.request,
        [guardedJob, run, ticket](ImageSourceData sourceData) mutable {
            ImageDecodeJob* job = guardedJob.data();
            if (job == nullptr || job->m_run != run) {
                return;
            }
            if (!sourceData.lease.isManaged()) {
                sourceData.lease = job->m_dependencies.sourceDataBudget->startLease();
            }
            const qsizetype reservedByteCount = sourceData.lease.reservedByteCount();
            if (sourceData.data.size() > reservedByteCount
                && !sourceData.lease.tryReserve(sourceData.data.size() - reservedByteCount)) {
                ImageDecodeJobRuntimePlan errorPlan = job->m_state.acceptLoadError(ticket);
                const auto* errorOperation
                    = std::get_if<DeliverImageLoadErrorOperation>(&errorPlan.operation);
                if (errorOperation != nullptr) {
                    run->finishLogical();
                    const LoadErrorCallback loadError = job->m_callbacks.loadError;
                    invokeIfSet(loadError, errorOperation->request,
                        ImageDataLoadError { kioOperationResourceLimitFailure(
                            KioOperationKind::ImageDataRead, errorOperation->request.imageUrl(),
                            imageSourceDataResourceLimitDiagnostic()) });
                }
                return;
            }

            ImageDecodeJobRuntimePlan plan = job->m_state.acceptLoadedData(ticket);
            auto* operation = std::get_if<StartImageDecodeOperation>(&plan.operation);
            if (operation == nullptr) {
                return;
            }

            ImageDecodeRequest currentRequest = operation->request.withSourceRevision(
                ImageSourceRevision::fromData(sourceData.data));
            if (job->m_authoritativeSeed.has_value()
                && reusableAuthoritativeSeed(*job->m_authoritativeSeed, currentRequest)) {
                StaticDisplayImagePayload seed = std::move(*job->m_authoritativeSeed);
                job->m_authoritativeSeed.reset();
                ImageDecodeJobRuntimePlan resultPlan = job->m_state.acceptDecodeResult(ticket);
                if (!std::holds_alternative<DeliverImageDecodeResultOperation>(
                        resultPlan.operation)) {
                    return;
                }
                EmbeddedMetadata metadata = seed.embeddedMetadata;
                run->finishLogical();
                const DecodedCallback decoded = job->m_callbacks.decoded;
                invokeIfSet(decoded, std::move(currentRequest),
                    successfulDecodedImageResult(
                        StaticDecodedImage { std::move(seed), std::move(metadata) }));
                return;
            }
            job->m_authoritativeSeed.reset();
            job->startDecode(std::move(sourceData), ticket, std::move(currentRequest));
        },
        [guardedJob, run, ticket](ImageDataLoadError error) mutable {
            ImageDecodeJob* job = guardedJob.data();
            if (job == nullptr || job->m_run != run) {
                return;
            }
            ImageDecodeJobRuntimePlan plan = job->m_state.acceptLoadError(ticket);
            const auto* operation = std::get_if<DeliverImageLoadErrorOperation>(&plan.operation);
            if (operation == nullptr) {
                return;
            }

            run->finishLogical();
            const LoadErrorCallback loadError = job->m_callbacks.loadError;
            invokeIfSet(loadError, operation->request, std::move(error));
        });
    loadJob.setRetirementCallback([loadRetirement]() { });
    if (ImageDecodeJob* job = guardedJob.data(); job != nullptr && job->m_run == run) {
        job->m_ioJobs.push_back(std::move(loadJob));
    }
}

void ImageDecodeJob::cancel()
{
    m_state.cancel();
    std::vector<ImageIoJob> ioJobs = std::move(m_ioJobs);
    std::vector<ImageWorkerTask> workerTasks = std::move(m_workerTasks);
    ImageDecodeWorkspaceAdmission workspaceAdmission = std::move(m_workspaceAdmission);
    ImageDecodeWorkspaceAdmission previewWorkspaceAdmission
        = std::move(m_previewWorkspaceAdmission);
    std::shared_ptr<ImageDecodeJobRun> run = std::move(m_run);
    m_authoritativeSeed.reset();

    for (ImageIoJob& job : ioJobs) {
        job.cancel();
    }
    for (ImageWorkerTask& task : workerTasks) {
        task.cancel();
    }
    workspaceAdmission.cancel();
    previewWorkspaceAdmission.cancel();
    if (run != nullptr) {
        run->finishLogical();
    }
}

bool ImageDecodeJob::hasActiveRequest() const { return m_state.hasActiveRequest(); }

void ImageDecodeJob::startDecode(
    ImageSourceData sourceData, ImageDecodeJobTicket ticket, ImageDecodeRequest request)
{
    const std::shared_ptr<ImageDecodeJobRun> run = m_run;
    if (run == nullptr) {
        return;
    }
    const QPointer<ImageDecodeJob> guardedJob(this);
    startThumbnailPreviewLookup(sourceData, ticket, request);

    ImageDecodeJob* job = guardedJob.data();
    if (job == nullptr || job->m_run != run || !job->m_state.acceptsDecodeWork(ticket)) {
        return;
    }
    const ImageDataDecodePlanner planner = job->m_dependencies.dataPlanner;
    const ImageWorkerScheduler workerScheduler = job->m_dependencies.workerScheduler;
    const ImageDecodeWorkspacePriority priority = run->priority;
    ImageDecodeRequest plannerRequest = request;
    ImageDecodeRequest completionRequest = std::move(request);
    ImageDecodeJobTicket completionTicket = std::move(ticket);
    const std::shared_ptr<ImageDecodeRetirementTicket> planningRetirement = run->openTicket();
    ImageWorkerTask planningTask = workerScheduler.run(
        job,
        [planner, sourceData = std::move(sourceData), request = std::move(plannerRequest),
            priority]() mutable { return planner(std::move(sourceData), request, priority); },
        [guardedJob, ticket = std::move(completionTicket), request = std::move(completionRequest)](
            PreparedImageDecodeResult result) mutable {
            ImageDecodeJob* job = guardedJob.data();
            if (job == nullptr || !job->m_state.acceptsDecodeWork(ticket)) {
                return;
            }
            job->processPreparedResult(std::move(result), std::move(ticket), std::move(request));
        });
    planningTask.setRetirementCallback([planningRetirement]() { });
    if (ImageDecodeJob* activeJob = guardedJob.data();
        activeJob != nullptr && activeJob->m_run == run) {
        activeJob->m_workerTasks.push_back(std::move(planningTask));
    }
}

void ImageDecodeJob::processPreparedResult(
    PreparedImageDecodeResult result, ImageDecodeJobTicket ticket, ImageDecodeRequest request)
{
    if (auto* decoded = std::get_if<DecodedImageResult>(&result)) {
        deliverDecodeResult(std::move(*decoded), ticket, std::move(request));
        return;
    }

    auto* prepared = std::get_if<std::unique_ptr<PreparedImageDecodeWork>>(&result);
    if (prepared == nullptr || *prepared == nullptr) {
        deliverDecodeResult(
            failedDecodedImageResult(QStringLiteral("image decode planning failed")), ticket,
            std::move(request));
        return;
    }
    requestPreparedAdmission(std::move(*prepared), std::move(ticket), std::move(request));
}

void ImageDecodeJob::requestPreparedAdmission(std::unique_ptr<PreparedImageDecodeWork> prepared,
    ImageDecodeJobTicket ticket, ImageDecodeRequest request)
{
    if (prepared == nullptr || m_run == nullptr) {
        return;
    }
    const DecodedImageFailure hardLimitFailure = prepared->hardLimitFailure();
    const ImageDecodeWorkspaceAdmissionRequest admissionRequest = prepared->admissionRequest();
    const std::shared_ptr<ImageDecodeJobRun> run = m_run;
    const std::shared_ptr<ImageDecodeRetirementTicket> admissionRetirement = run->openTicket();
    const QPointer<ImageDecodeJob> guardedJob(this);
    std::expected<ImageDecodeWorkspaceAdmission, ImageDecodeWorkspaceAdmissionFailure> admission
        = m_dependencies.workspaceBudget->requestAdmission(this, admissionRequest,
            [guardedJob, admissionRetirement, prepared = std::move(prepared), ticket, request](
                ImageDecodeWorkspaceLease lease) mutable {
                ImageDecodeJob* job = guardedJob.data();
                if (job == nullptr || !job->m_state.acceptsDecodeWork(ticket)) {
                    return;
                }
                job->startPreparedExecution(
                    std::move(prepared), std::move(lease), std::move(ticket), std::move(request));
            });
    if (!admission.has_value()) {
        deliverDecodeResult(failedDecodedImageResult(hardLimitFailure), ticket, std::move(request));
        return;
    }
    m_workspaceAdmission = std::move(*admission);
}

void ImageDecodeJob::startPreparedExecution(std::unique_ptr<PreparedImageDecodeWork> prepared,
    ImageDecodeWorkspaceLease lease, ImageDecodeJobTicket ticket, ImageDecodeRequest request)
{
    if (prepared == nullptr || m_run == nullptr) {
        return;
    }
    const std::shared_ptr<ImageDecodeJobRun> run = m_run;
    const ImageWorkerScheduler workerScheduler = m_dependencies.workerScheduler;
    const QPointer<ImageDecodeJob> guardedJob(this);
    const std::shared_ptr<ImageDecodeRetirementTicket> producerRetirement = run->openTicket();
    ImageWorkerTask producerTask = workerScheduler.run(
        this,
        [prepared = std::move(prepared), lease = std::move(lease)]() mutable {
            return std::move(*prepared).execute(std::move(lease));
        },
        [guardedJob, ticket = std::move(ticket), request = std::move(request)](
            PreparedImageDecodeResult result) mutable {
            ImageDecodeJob* job = guardedJob.data();
            if (job == nullptr || !job->m_state.acceptsDecodeWork(ticket)) {
                return;
            }
            job->processPreparedResult(std::move(result), std::move(ticket), std::move(request));
        });
    producerTask.setRetirementCallback([producerRetirement]() { });
    if (ImageDecodeJob* job = guardedJob.data(); job != nullptr && job->m_run == run) {
        job->m_workerTasks.push_back(std::move(producerTask));
    }
}

void ImageDecodeJob::deliverDecodeResult(
    DecodedImageResult result, const ImageDecodeJobTicket& ticket, ImageDecodeRequest request)
{
    ImageDecodeJobRuntimePlan plan = m_state.acceptDecodeResult(ticket);
    if (!std::holds_alternative<DeliverImageDecodeResultOperation>(plan.operation)) {
        return;
    }

    m_previewWorkspaceAdmission.cancel();
    m_previewWorkspaceAdmission = {};
    const std::shared_ptr<ImageDecodeJobRun> run = m_run;
    if (run != nullptr) {
        run->finishLogical();
    }
    const DecodedCallback decoded = m_callbacks.decoded;
    invokeIfSet(decoded, std::move(request), std::move(result));
}

void ImageDecodeJob::startThumbnailPreviewLookup(
    ImageSourceData sourceData, ImageDecodeJobTicket ticket, const ImageDecodeRequest& request)
{
    if (!m_dependencies.thumbnailPreviewLookupProvider || !m_callbacks.thumbnailPreview) {
        return;
    }

    const ImageDecodeWorkspacePriority priority = thumbnailPreviewPriority(m_run);
    const std::optional<ImageDecodeWorkspaceAdmissionRequest> admissionRequest
        = xdgThumbnailPreviewPlanningAdmissionRequest(sourceData.data, request, priority);
    if (!admissionRequest.has_value()) {
        return;
    }
    if (admissionRequest->additionalPeakByteCount == 0) {
        std::optional<XdgThumbnailPreviewRequest> previewRequest
            = admittedXdgThumbnailPreviewRequestForDecodeData(sourceData.data, request, {});
        if (previewRequest.has_value()) {
            startThumbnailPreviewCacheLookup(
                std::move(*previewRequest), std::move(sourceData), std::move(ticket), request);
        }
        return;
    }

    const std::shared_ptr<ImageDecodeJobRun> run = m_run;
    if (run == nullptr) {
        return;
    }
    const std::shared_ptr<ImageDecodeRetirementTicket> admissionRetirement = run->openTicket();
    const QPointer<ImageDecodeJob> guardedJob(this);
    std::expected<ImageDecodeWorkspaceAdmission, ImageDecodeWorkspaceAdmissionFailure> admission
        = m_dependencies.workspaceBudget->requestAdmission(this, *admissionRequest,
            [guardedJob, admissionRetirement, sourceData = std::move(sourceData), ticket, request](
                ImageDecodeWorkspaceLease workspaceLease) mutable {
                ImageDecodeJob* job = guardedJob.data();
                if (job == nullptr || !job->m_state.acceptsDecodeWork(ticket)) {
                    return;
                }
                job->startThumbnailPreviewPlanning(
                    std::move(sourceData), std::move(workspaceLease), std::move(ticket), request);
            });
    if (!admission.has_value()) {
        return;
    }
    m_previewWorkspaceAdmission = std::move(*admission);
}

void ImageDecodeJob::startThumbnailPreviewPlanning(ImageSourceData sourceData,
    ImageDecodeWorkspaceLease workspaceLease, ImageDecodeJobTicket ticket,
    const ImageDecodeRequest& request)
{
    const std::shared_ptr<ImageDecodeJobRun> run = m_run;
    if (run == nullptr) {
        return;
    }
    const ImageSourceDataLease retirementSourceDataLease = sourceData.lease;
    const ImageWorkerScheduler workerScheduler = m_dependencies.workerScheduler;
    const QPointer<ImageDecodeJob> guardedJob(this);
    const std::shared_ptr<ImageDecodeRetirementTicket> planningRetirement = run->openTicket();
    ImageWorkerTask planningTask = workerScheduler.run(
        this,
        [sourceData = std::move(sourceData), workspaceLease = std::move(workspaceLease),
            request]() mutable {
            std::optional<XdgThumbnailPreviewRequest> previewRequest
                = admittedXdgThumbnailPreviewRequestForDecodeData(
                    sourceData.data, request, std::move(workspaceLease));
            return ThumbnailPreviewPlanningResult {
                std::move(previewRequest),
                std::move(sourceData),
            };
        },
        [guardedJob, ticket = std::move(ticket), request](
            ThumbnailPreviewPlanningResult result) mutable {
            ImageDecodeJob* job = guardedJob.data();
            if (job == nullptr || !job->m_state.acceptsDecodeWork(ticket)
                || !result.previewRequest.has_value()) {
                return;
            }
            job->startThumbnailPreviewCacheLookup(std::move(*result.previewRequest),
                std::move(result.sourceData), std::move(ticket), request);
        });
    planningTask.setRetirementCallback([planningRetirement, retirementSourceDataLease]() { });
    if (ImageDecodeJob* job = guardedJob.data(); job != nullptr && job->m_run == run) {
        job->m_workerTasks.push_back(std::move(planningTask));
    }
}

void ImageDecodeJob::startThumbnailPreviewCacheLookup(XdgThumbnailPreviewRequest previewRequest,
    ImageSourceData sourceData, ImageDecodeJobTicket ticket, const ImageDecodeRequest& request)
{
    std::optional<ThumbnailCacheLookupRequest> lookupRequest
        = xdgThumbnailPreviewCacheLookupRequest(previewRequest);
    if (!lookupRequest.has_value()) {
        return;
    }
    lookupRequest->workspacePriority = thumbnailPreviewPriority(m_run);

    const bool rawPreviewEligible = m_dependencies.rawEmbeddedThumbnailPreviewExtractor
        && rawEmbeddedThumbnailPreviewEligible(sourceData.data, request);
    const ThumbnailCacheLookupProvider lookupProvider
        = m_dependencies.thumbnailPreviewLookupProvider;
    const ImageSourceDataLease retirementSourceDataLease = sourceData.lease;
    const std::shared_ptr<ImageDecodeJobRun> run = m_run;
    if (!lookupProvider || run == nullptr) {
        return;
    }
    const std::shared_ptr<ImageDecodeRetirementTicket> lookupRetirement = run->openTicket();
    const QPointer<ImageDecodeJob> guardedJob(this);
    ImageIoJob lookupJob = lookupProvider(this, std::move(*lookupRequest),
        [guardedJob, lookupRetirement, ticket = std::move(ticket),
            previewRequest = std::move(previewRequest), request, rawPreviewEligible,
            sourceData = std::move(sourceData)](ThumbnailCacheLookupResult lookupResult) mutable {
            ImageDecodeJob* job = guardedJob.data();
            if (job == nullptr) {
                return;
            }
            ImageDecodeJobRuntimePlan plan = job->m_state.acceptThumbnailPreview(ticket);
            if (!std::holds_alternative<DeliverImageThumbnailPreviewOperation>(plan.operation)) {
                return;
            }

            XdgThumbnailPreviewResult previewResult
                = xdgThumbnailPreviewResult(previewRequest, std::move(lookupResult));
            if (previewResult.status == ThumbnailCacheLookupStatus::Missing && rawPreviewEligible) {
                job->requestRawEmbeddedThumbnailPreviewAdmission(
                    std::move(sourceData), ticket, request);
                return;
            }
            const ThumbnailPreviewCallback thumbnailPreview = job->m_callbacks.thumbnailPreview;
            if (previewResult.status != ThumbnailCacheLookupStatus::Ready || !thumbnailPreview) {
                return;
            }

            std::optional<StaticDisplayImagePayload> payload
                = xdgThumbnailPreviewDisplayPayload(request, previewResult);
            if (!payload.has_value()) {
                return;
            }

            invokeIfSet(thumbnailPreview, request, std::move(*payload));
        });
    lookupJob.setRetirementCallback([lookupRetirement, retirementSourceDataLease]() { });
    if (ImageDecodeJob* job = guardedJob.data(); job != nullptr && job->m_run == run) {
        job->m_ioJobs.push_back(std::move(lookupJob));
    }
}

void ImageDecodeJob::requestRawEmbeddedThumbnailPreviewAdmission(
    ImageSourceData sourceData, ImageDecodeJobTicket ticket, const ImageDecodeRequest& request)
{
    if (!m_dependencies.rawEmbeddedThumbnailPreviewExtractor || m_run == nullptr) {
        return;
    }

    const ImageDecodeWorkspaceAdmissionRequest admissionRequest {
        rawEmbeddedThumbnailPreviewWorkspaceByteCount(),
        0,
        thumbnailPreviewPriority(m_run),
    };
    const std::shared_ptr<ImageDecodeJobRun> run = m_run;
    const std::shared_ptr<ImageDecodeRetirementTicket> admissionRetirement = run->openTicket();
    const QPointer<ImageDecodeJob> guardedJob(this);
    std::expected<ImageDecodeWorkspaceAdmission, ImageDecodeWorkspaceAdmissionFailure> admission
        = m_dependencies.workspaceBudget->requestAdmission(this, admissionRequest,
            [guardedJob, admissionRetirement, sourceData = std::move(sourceData), ticket, request](
                ImageDecodeWorkspaceLease workspaceLease) mutable {
                ImageDecodeJob* job = guardedJob.data();
                if (job == nullptr || !job->m_state.acceptsDecodeWork(ticket)) {
                    return;
                }
                job->startRawEmbeddedThumbnailPreviewValidation(
                    std::move(sourceData), std::move(workspaceLease), std::move(ticket), request);
            });
    if (!admission.has_value()) {
        return;
    }
    m_previewWorkspaceAdmission = std::move(*admission);
}

void ImageDecodeJob::startRawEmbeddedThumbnailPreviewValidation(ImageSourceData sourceData,
    ImageDecodeWorkspaceLease workspaceLease, ImageDecodeJobTicket ticket,
    const ImageDecodeRequest& request)
{
    if (!m_dependencies.rawEmbeddedThumbnailPreviewExtractor) {
        return;
    }

    const RawEmbeddedThumbnailPreviewExtractor extractor
        = m_dependencies.rawEmbeddedThumbnailPreviewExtractor;
    const ImageWorkerScheduler workerScheduler = m_dependencies.workerScheduler;
    const std::shared_ptr<ImageDecodeJobRun> run = m_run;
    if (run == nullptr) {
        return;
    }
    const ImageSourceDataLease retirementSourceDataLease = sourceData.lease;
    const std::shared_ptr<ImageDecodeRetirementTicket> previewRetirement = run->openTicket();
    const QPointer<ImageDecodeJob> guardedJob(this);
    ImageWorkerTask previewTask = workerScheduler.run(
        this,
        [extractor, sourceData = std::move(sourceData), workspaceLease = std::move(workspaceLease),
            request]() mutable {
            RawEmbeddedThumbnailPreviewResult result
                = extractor(sourceData.data, request, std::move(workspaceLease));
            return RawEmbeddedThumbnailPreviewWorkerResult {
                rawEmbeddedThumbnailPreviewDisplayPayload(request, result),
                std::move(sourceData),
            };
        },
        [guardedJob, ticket = std::move(ticket), request](
            RawEmbeddedThumbnailPreviewWorkerResult result) mutable {
            ImageDecodeJob* job = guardedJob.data();
            if (job == nullptr || !result.payload.has_value()) {
                return;
            }

            const ThumbnailPreviewCallback thumbnailPreview = job->m_callbacks.thumbnailPreview;
            if (!thumbnailPreview) {
                return;
            }
            ImageDecodeJobRuntimePlan plan = job->m_state.acceptThumbnailPreview(ticket);
            if (!std::holds_alternative<DeliverImageThumbnailPreviewOperation>(plan.operation)) {
                return;
            }

            invokeIfSet(thumbnailPreview, request, std::move(*result.payload));
        });
    previewTask.setRetirementCallback([previewRetirement, retirementSourceDataLease]() { });
    if (ImageDecodeJob* job = guardedJob.data(); job != nullptr && job->m_run == run) {
        job->m_workerTasks.push_back(std::move(previewTask));
    }
}
}
