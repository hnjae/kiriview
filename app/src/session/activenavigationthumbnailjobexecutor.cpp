// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailjobexecutor.h"

#include "thumbnail/thumbnailsourcekind.h"

#include <QMetaObject>
#include <QPointer>
#include <unordered_map>
#include <utility>

namespace {
kiriview::ThumbnailSourceKind thumbnailSourceKind(const QString& sourceKind)
{
    using Source = kiriview::ActiveNavigationThumbnailSourceKind;
    if (sourceKind == kiriview::activeNavigationThumbnailSourceKindIdentity(Source::DirectVideo)) {
        return kiriview::ThumbnailSourceKind::DirectVideo;
    }
    if (sourceKind
        == kiriview::activeNavigationThumbnailSourceKindIdentity(Source::ImageDocumentPageImage)) {
        return kiriview::ThumbnailSourceKind::ImageDocumentPageImage;
    }
    if (sourceKind
        == kiriview::activeNavigationThumbnailSourceKindIdentity(Source::ImageDocumentPageVideo)) {
        return kiriview::ThumbnailSourceKind::ImageDocumentPageVideo;
    }
    return kiriview::ThumbnailSourceKind::DirectImage;
}

bool usesCacheLookup(const kiriview::ThumbnailSourceAdapterPlan& plan)
{
    return plan.kind == kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile
        && !plan.localPathBytes.isEmpty();
}

bool enablesCacheInstall(const kiriview::ThumbnailSourceAdapterPlan& plan)
{
    return (plan.kind == kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile
               && !plan.localPathBytes.isEmpty())
        || (plan.kind == kiriview::ThumbnailSourceAdapterPlanKind::CacheableOpenedCollectionEntry
            && !plan.openedCollectionScope.isEmpty());
}

kiriview::ActiveNavigationThumbnailFailureKind thumbnailGenerationFailureKind(
    kiriview::ThumbnailGenerationStatus status)
{
    using FailureKind = kiriview::ActiveNavigationThumbnailFailureKind;
    using Status = kiriview::ThumbnailGenerationStatus;

    switch (status) {
    case Status::Ready:
    case Status::Failed:
        return FailureKind::GenerationFailed;
    case Status::VideoExtractionInvalidRequest:
        return FailureKind::VideoExtractionInvalidRequest;
    case Status::VideoSourceUnavailable:
        return FailureKind::VideoSourceUnavailable;
    case Status::VideoUnsupportedMedia:
        return FailureKind::VideoUnsupportedMedia;
    case Status::VideoBackendFailure:
        return FailureKind::VideoBackendFailure;
    case Status::VideoExtractionTimedOut:
        return FailureKind::VideoExtractionTimedOut;
    case Status::VideoNoRepresentativeImage:
        return FailureKind::VideoNoRepresentativeImage;
    case Status::ResourceLimitExceeded:
        return FailureKind::ResourceLimitExceeded;
    }

    return FailureKind::GenerationFailed;
}
}

namespace kiriview {
class ActiveNavigationThumbnailJobExecutor::State final
    : public std::enable_shared_from_this<ActiveNavigationThumbnailJobExecutor::State>
{
public:
    enum class PhaseKind {
        Lookup,
        Generation,
    };

    struct Record
    {
        ActiveNavigationThumbnailWorkRequest request;
        quint64 phaseToken = 0;
        PhaseKind phaseKind = PhaseKind::Lookup;
        ImageIoJob job;
        bool publicationCanceled = false;
        bool physicallyRetired = false;
    };

    State(QObject* owner, ThumbnailCacheLookupProvider lookupProvider,
        ThumbnailGenerationProvider generationProvider,
        ActiveNavigationThumbnailWorkCallback completionCallback,
        ActiveNavigationThumbnailWorkRetirementCallback retirementCallback)
        : owner(owner)
        , lookupProvider(std::move(lookupProvider))
        , generationProvider(std::move(generationProvider))
        , completionCallback(std::move(completionCallback))
        , retirementCallback(std::move(retirementCallback))
    {
    }

    bool start(ActiveNavigationThumbnailWorkRequest request)
    {
        if (!request.workId.isValid() || records.contains(request.workId.value)) {
            return false;
        }
        if (usesCacheLookup(request.sourcePlan)) {
            return startLookup(std::move(request));
        }
        return startGeneration(std::move(request));
    }

    bool cancel(ActiveNavigationThumbnailWorkId workId)
    {
        auto iterator = records.find(workId.value);
        if (iterator == records.end()) {
            return false;
        }
        iterator->second.publicationCanceled = true;
        iterator->second.job.cancel();
        iterator = records.find(workId.value);
        if (iterator != records.end() && iterator->second.physicallyRetired) {
            retirePhase(workId.value, iterator->second.phaseToken);
        }
        return true;
    }

    void cancelAll()
    {
        std::vector<ActiveNavigationThumbnailWorkId> workIds;
        workIds.reserve(records.size());
        for (const auto& entry : records) {
            workIds.push_back({ entry.first });
        }
        for (ActiveNavigationThumbnailWorkId workId : workIds) {
            cancel(workId);
        }
    }

    bool startLookup(ActiveNavigationThumbnailWorkRequest request)
    {
        if (!lookupProvider) {
            completeFailure(std::move(request),
                ActiveNavigationThumbnailFailureKind::CacheLookupProviderUnavailable, {});
            return true;
        }

        const quint64 phaseToken = nextPhaseToken++;
        const quint64 workValue = request.workId.value;
        records.emplace(workValue, Record { request, phaseToken, PhaseKind::Lookup, {} });
        ThumbnailCacheLookupRequest providerRequest {
            request.sourcePlan.localPathBytes,
            request.sourcePlan.originalIdentity,
            request.bucket,
        };
        const std::weak_ptr<State> weakState = weak_from_this();
        ImageIoJob job = lookupProvider(owner.data(), std::move(providerRequest),
            [weakState, workValue, phaseToken](ThumbnailCacheLookupResult result) mutable {
                if (const std::shared_ptr<State> state = weakState.lock()) {
                    state->finishLookup(workValue, phaseToken, std::move(result));
                }
            });
        retainReturnedJob(workValue, phaseToken, PhaseKind::Lookup, std::move(job));
        return true;
    }

    bool startGeneration(ActiveNavigationThumbnailWorkRequest request)
    {
        if (!generationProvider) {
            completeFailure(std::move(request),
                ActiveNavigationThumbnailFailureKind::GenerationProviderUnavailable, {});
            return true;
        }

        const quint64 phaseToken = nextPhaseToken++;
        const quint64 workValue = request.workId.value;
        records.emplace(workValue, Record { request, phaseToken, PhaseKind::Generation, {} });
        ThumbnailGenerationRequest providerRequest;
        providerRequest.localPathBytes = request.sourcePlan.localPathBytes;
        providerRequest.originalIdentity = request.sourcePlan.originalIdentity;
        providerRequest.openedCollectionScope = request.sourcePlan.openedCollectionScope;
        providerRequest.sourceUrl = request.sourceKey.sourceUrl;
        providerRequest.sourceLabel = request.sourceKey.row.label;
        providerRequest.sourceKind = thumbnailSourceKind(request.sourceKey.row.sourceKind);
        providerRequest.requestedBucket = request.bucket;
        providerRequest.cacheInstallEnabled = enablesCacheInstall(request.sourcePlan);
        const std::weak_ptr<State> weakState = weak_from_this();
        ImageIoJob job = generationProvider(owner.data(), std::move(providerRequest),
            [weakState, workValue, phaseToken](ThumbnailGenerationResult result) mutable {
                if (const std::shared_ptr<State> state = weakState.lock()) {
                    state->finishGeneration(workValue, phaseToken, std::move(result));
                }
            });
        retainReturnedJob(workValue, phaseToken, PhaseKind::Generation, std::move(job));
        return true;
    }

    void retainReturnedJob(
        quint64 workValue, quint64 phaseToken, PhaseKind phaseKind, ImageIoJob job)
    {
        auto iterator = records.find(workValue);
        if (iterator == records.end() || iterator->second.phaseToken != phaseToken
            || iterator->second.phaseKind != phaseKind) {
            job.cancel();
            return;
        }
        const std::weak_ptr<State> weakState = weak_from_this();
        const QPointer<QObject> guardedOwner = owner;
        job.setRetirementCallback([weakState, guardedOwner, workValue, phaseToken]() {
            QObject* target = guardedOwner.data();
            if (target == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                target,
                [weakState, workValue, phaseToken]() {
                    if (const std::shared_ptr<State> state = weakState.lock()) {
                        state->retirePhase(workValue, phaseToken);
                    }
                },
                Qt::AutoConnection);
        });
        iterator = records.find(workValue);
        if (iterator == records.end() || iterator->second.phaseToken != phaseToken
            || iterator->second.phaseKind != phaseKind) {
            job.cancel();
            return;
        }
        if (iterator->second.publicationCanceled) {
            job.cancel();
            return;
        }
        iterator->second.job = std::move(job);
    }

    void retirePhase(quint64 workValue, quint64 phaseToken)
    {
        auto iterator = records.find(workValue);
        if (iterator == records.end() || iterator->second.phaseToken != phaseToken) {
            return;
        }
        iterator->second.physicallyRetired = true;
        if (!iterator->second.publicationCanceled) {
            return;
        }
        const ActiveNavigationThumbnailWorkId workId = iterator->second.request.workId;
        records.erase(iterator);
        ActiveNavigationThumbnailWorkRetirementCallback callback = retirementCallback;
        if (callback) {
            callback(workId);
        }
    }

    void finishLookup(quint64 workValue, quint64 phaseToken, ThumbnailCacheLookupResult result)
    {
        auto iterator = records.find(workValue);
        if (iterator == records.end() || iterator->second.phaseToken != phaseToken
            || iterator->second.phaseKind != PhaseKind::Lookup
            || iterator->second.publicationCanceled) {
            return;
        }
        ActiveNavigationThumbnailWorkRequest request = iterator->second.request;
        records.erase(iterator);
        switch (result.status) {
        case ThumbnailCacheLookupStatus::Ready:
            completeReady(std::move(request), std::move(result.image));
            return;
        case ThumbnailCacheLookupStatus::Missing:
            startGeneration(std::move(request));
            return;
        case ThumbnailCacheLookupStatus::Invalid:
            completeFailure(std::move(request),
                ActiveNavigationThumbnailFailureKind::CacheLookupInvalid,
                std::move(result.errorString));
            return;
        case ThumbnailCacheLookupStatus::ResourceLimitExceeded:
            completeFailure(std::move(request),
                ActiveNavigationThumbnailFailureKind::ResourceLimitExceeded,
                std::move(result.errorString));
            return;
        case ThumbnailCacheLookupStatus::Failed:
            completeFailure(std::move(request),
                ActiveNavigationThumbnailFailureKind::CacheLookupFailed,
                std::move(result.errorString));
            return;
        }
    }

    void finishGeneration(quint64 workValue, quint64 phaseToken, ThumbnailGenerationResult result)
    {
        auto iterator = records.find(workValue);
        if (iterator == records.end() || iterator->second.phaseToken != phaseToken
            || iterator->second.phaseKind != PhaseKind::Generation
            || iterator->second.publicationCanceled) {
            return;
        }
        ActiveNavigationThumbnailWorkRequest request = iterator->second.request;
        records.erase(iterator);
        if (result.status == ThumbnailGenerationStatus::Ready) {
            std::optional<ActiveNavigationThumbnailWorkDiagnostic> diagnostic;
            if (result.diagnosticKind == ThumbnailGenerationDiagnosticKind::CacheInstallFailed) {
                diagnostic = ActiveNavigationThumbnailWorkDiagnostic {
                    ActiveNavigationThumbnailDiagnosticKind::CacheInstallFailed,
                    std::move(result.errorString),
                };
            }
            completeReady(std::move(request), std::move(result.image), std::move(diagnostic));
            return;
        }
        const ActiveNavigationThumbnailFailureKind failureKind
            = thumbnailGenerationFailureKind(result.status);
        completeFailure(std::move(request), failureKind, std::move(result.errorString));
    }

    void completeReady(ActiveNavigationThumbnailWorkRequest request, QImage image,
        std::optional<ActiveNavigationThumbnailWorkDiagnostic> diagnostic = std::nullopt)
    {
        ActiveNavigationThumbnailWorkCallback callback = completionCallback;
        if (callback) {
            callback(ActiveNavigationThumbnailWorkCompletion {
                request.workId,
                std::move(request.sourceKey),
                request.bucket,
                request.workKind,
                ActiveNavigationThumbnailReadyWorkResult {
                    std::move(image), std::move(diagnostic) },
            });
        }
    }

    void completeFailure(ActiveNavigationThumbnailWorkRequest request,
        ActiveNavigationThumbnailFailureKind failureKind, QString errorString)
    {
        ActiveNavigationThumbnailWorkCallback callback = completionCallback;
        if (callback) {
            callback(ActiveNavigationThumbnailWorkCompletion {
                request.workId,
                std::move(request.sourceKey),
                request.bucket,
                request.workKind,
                ActiveNavigationThumbnailFailedWorkResult { failureKind, std::move(errorString) },
            });
        }
    }

    QPointer<QObject> owner;
    ThumbnailCacheLookupProvider lookupProvider;
    ThumbnailGenerationProvider generationProvider;
    ActiveNavigationThumbnailWorkCallback completionCallback;
    ActiveNavigationThumbnailWorkRetirementCallback retirementCallback;
    std::unordered_map<quint64, Record> records;
    quint64 nextPhaseToken = 1;
};

ActiveNavigationThumbnailJobExecutor::ActiveNavigationThumbnailJobExecutor(QObject* owner,
    ThumbnailCacheLookupProvider lookupProvider, ThumbnailGenerationProvider generationProvider,
    ActiveNavigationThumbnailWorkCallback completionCallback,
    ActiveNavigationThumbnailWorkRetirementCallback retirementCallback)
    : m_state(
          std::make_shared<State>(owner, std::move(lookupProvider), std::move(generationProvider),
              std::move(completionCallback), std::move(retirementCallback)))
{
}

ActiveNavigationThumbnailJobExecutor::~ActiveNavigationThumbnailJobExecutor()
{
    m_state->cancelAll();
    m_state.reset();
}

bool ActiveNavigationThumbnailJobExecutor::start(ActiveNavigationThumbnailWorkRequest request)
{
    return m_state->start(std::move(request));
}

bool ActiveNavigationThumbnailJobExecutor::cancel(ActiveNavigationThumbnailWorkId workId)
{
    return m_state->cancel(workId);
}

void ActiveNavigationThumbnailJobExecutor::cancelAll() { m_state->cancelAll(); }
}
