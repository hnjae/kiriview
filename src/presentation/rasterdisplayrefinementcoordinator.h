// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_RASTERDISPLAYREFINEMENTCOORDINATOR_H
#define KIRIVIEW_RASTERDISPLAYREFINEMENTCOORDINATOR_H

#include "async/imageasyncticket.h"
#include "async/imageworkerscheduler.h"
#include "presentation/imagepresentationruntime.h"
#include "rendering/imagerendercontext.h"
#include "rendering/rasterdisplaybucketpolicy.h"
#include "rendering/staticimage.h"

#include <QImageIOHandler>
#include <QSize>
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
struct RasterDisplayRefinementCacheKey
{
    QString displayScopeIdentity;
    QString sourceIdentity;
    QImageIOHandler::Transformations imageReaderTransformations
        = QImageIOHandler::TransformationNone;
    QSize originalSize;
    DisplayedPageRole pageRole = DisplayedPageRole::Primary;
    bool resolutionIndependent = false;
    DisplayImageQuality quality = DisplayImageQuality::Exact;
    RasterDisplayBucketKey bucketKey;

    friend bool operator==(
        const RasterDisplayRefinementCacheKey& left, const RasterDisplayRefinementCacheKey& right)
    {
        return left.displayScopeIdentity == right.displayScopeIdentity
            && left.sourceIdentity == right.sourceIdentity
            && left.imageReaderTransformations == right.imageReaderTransformations
            && left.originalSize == right.originalSize && left.pageRole == right.pageRole
            && left.resolutionIndependent == right.resolutionIndependent
            && left.quality == right.quality && left.bucketKey == right.bucketKey;
    }
};

class RasterDisplayRefinementCoordinator final
{
public:
    using AcceptedCallback
        = std::function<void(StaticDisplayImagePayload, const ImageDocumentRenderContext&)>;

    RasterDisplayRefinementCoordinator(QObject* context, qsizetype displayImageByteBudget,
        ImageWorkerScheduler workerScheduler, AcceptedCallback acceptedCallback);
    ~RasterDisplayRefinementCoordinator();

    void request(StaticDisplayImagePayload currentDisplay,
        const ImagePresentationRenderProjection& projection, quint64 displaySourceRevision);
    void cancel();

    Q_DISABLE_COPY_MOVE(RasterDisplayRefinementCoordinator)

private:
    struct CachedRefinement
    {
        RasterDisplayRefinementCacheKey cacheKey;
        StaticDisplayImagePayload displayImage;
    };

    struct InFlightRefinement
    {
        RasterDisplayRefinementCacheKey cacheKey;
        RasterDisplayRefinementDemandKey demandKey;
        quint64 ticket = 0;
        std::shared_ptr<std::atomic_bool> startCanceled;
    };

    bool promoteCachedRefinement(const RasterDisplayRefinementCacheKey& cacheKey,
        const ImageDocumentRenderContext& renderContext);
    void retainCachedRefinement(const RasterDisplayRefinementCacheKey& cacheKey,
        const StaticDisplayImagePayload& displayImage);
    std::optional<RasterDisplayRefinementDemandKey> attachInFlightRefinement(
        const RasterDisplayRefinementCacheKey& cacheKey,
        RasterDisplayRefinementDemandKey demandKey);
    void retainInFlightRefinement(const RasterDisplayRefinementCacheKey& cacheKey,
        const RasterDisplayRefinementDemandKey& demandKey, quint64 ticket,
        std::shared_ptr<std::atomic_bool> startCanceled);
    std::optional<RasterDisplayRefinementDemandKey> takeInFlightRefinement(
        const RasterDisplayRefinementCacheKey& cacheKey, quint64 ticket);

    QObject* m_context = nullptr;
    qsizetype m_displayImageByteBudget = 0;
    ImageWorkerScheduler m_workerScheduler;
    AcceptedCallback m_acceptedCallback;
    std::optional<RasterDisplayRefinementDemandKey> m_activeDemand;
    std::vector<CachedRefinement> m_cachedRefinements;
    std::vector<InFlightRefinement> m_inFlightRefinements;
    ImageAsyncTicket m_ticket;
};
}

#endif
