// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportdecodesource.h"

#include "cache/imagebytecost.h"
#include "decoding/imagedecodelogging.h"
#include "localization/mediaentrysourceerrortext.h"
#include "rendering/imagerendering.h"

#include <ImageViewport/imagesequence.h>

#include <QMetaObject>
#include <QScopeGuard>
#include <QSizeF>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
kiriview::ImageLoadFailure loadFailure(const kiriview::ImageLoadSession& session,
    kiriview::ImageLoadFailureKind kind, QString userMessage, QString diagnosticDetail,
    kiriview::DecodedImageFailureRoute route = kiriview::DecodedImageFailureRoute::Unknown,
    kiriview::DecodedImageFailureOperation operation
    = kiriview::DecodedImageFailureOperation::Unknown,
    bool retryable = false)
{
    return {
        session.imageUrl(),
        session.id(),
        kind,
        route,
        operation,
        std::move(userMessage),
        std::move(diagnosticDetail),
        kiriview::ImageLoadFailureSeverity::Error,
        retryable,
    };
}

kiriview::ImageLoadFailure loadFailure(
    const kiriview::ImageLoadSession& session, const kiriview::DecodedImageFailure& failure)
{
    kiriview::ImageLoadFailure projected
        = loadFailure(session, kiriview::ImageLoadFailureKind::Decode, failure.errorString,
            failure.diagnosticDetail, failure.route, failure.operation, failure.retryable);
    projected.decodeCause = failure.cause;
    return projected;
}

kiriview::ImageLoadFailure loadFailure(
    const kiriview::ImageLoadSession& session, const kiriview::MediaEntrySourceError& error)
{
    qCWarning(kiriviewDecodeLog).noquote() << "collection image data loading failed" << error;
    kiriview::ImageLoadFailure failure
        = loadFailure(session, kiriview::ImageLoadFailureKind::DataLoad,
            kiriview::mediaEntrySourceErrorText(error), error.diagnosticDetail);
    failure.mediaEntrySourceError = error;
    return failure;
}

kiriview::ImageLoadFailure loadFailure(
    const kiriview::ImageLoadSession& session, const kiriview::ImageDataLoadError& error)
{
    return std::visit(
        [&session](const auto& detail) {
            using Error = std::decay_t<decltype(detail)>;
            if constexpr (std::is_same_v<Error, QString>) {
                return loadFailure(
                    session, kiriview::ImageLoadFailureKind::DataLoad, detail, detail);
            } else {
                return loadFailure(session, detail);
            }
        },
        error);
}

ImageSequenceAuthoredAnimationFacts authoredAnimationFacts(int repeatCount)
{
    ImageSequenceAuthoredAnimationFacts facts = repeatCount < 0
        ? ImageSequenceAuthoredAnimationFacts::infiniteLoop()
        : repeatCount == 0 ? ImageSequenceAuthoredAnimationFacts {}
                           : ImageSequenceAuthoredAnimationFacts::finiteLoop(repeatCount + 1);
    facts.setAutoplay(true);
    return facts;
}

int frameStartPosition(const QVector<int>& durations, int frame)
{
    if (frame <= 0) {
        return 0;
    }
    return std::ranges::fold_left(
        durations | std::views::take(frame), 0, [](int total, int duration) {
            return total > std::numeric_limits<int>::max() - duration
                ? std::numeric_limits<int>::max()
                : total + duration;
        });
}

bool containsToken(const QVector<ImageSequenceProviderRequestToken>& tokens,
    ImageSequenceProviderRequestToken token)
{
    return std::ranges::contains(tokens, token);
}

bool isAuthoritativeStaticPayload(const kiriview::StaticDisplayImagePayload& payload)
{
    return payload.isAuthoritative();
}

bool isProvisionalPreviewPayload(const kiriview::StaticDisplayImagePayload& payload)
{
    return payload.isProvisionalPreview();
}

bool isReusableAuthoritativeUpgrade(const kiriview::StaticDisplayImagePayload& candidate,
    const kiriview::StaticDisplayImagePayload& current)
{
    if (candidate.quality == kiriview::DisplayImageQuality::Exact) {
        return true;
    }
    if (current.quality == kiriview::DisplayImageQuality::Exact) {
        return false;
    }

    return candidate.image.width() >= current.image.width()
        && candidate.image.height() >= current.image.height();
}

bool hasNoAcceptedCurrentPayload(const ImageSequenceProviderDisplayDemand& demand)
{
    return demand.currentPayloadQuality() == ImageViewportPayloadQuality::Unknown
        && demand.currentPayloadExactness() == ImageViewportPayloadExactness::Unknown;
}

constexpr kiriview::TimerDuration initialDetailWaitDuration = std::chrono::milliseconds(100);

struct StaticPayloadLimits
{
    qint64 maximumWidth = ImageSequenceLimits::maximumPayloadRasterWidth();
    qint64 maximumHeight = ImageSequenceLimits::maximumPayloadRasterHeight();
    qint64 maximumBytes = ImageSequenceLimits::maximumPayloadBytes();
};

void tightenLimit(qint64& current, qint64 candidate)
{
    if (candidate >= 0) {
        current = std::min(current, candidate);
    }
}

StaticPayloadLimits staticPayloadLimits(const kiriview::ImageViewportProviderFrameRequest& request)
{
    StaticPayloadLimits limits;
    const ImageSequenceProviderDisplayDemand& demand = request.demand;
    if (demand.maximumTextureSize() >= 0) {
        tightenLimit(limits.maximumWidth, demand.maximumTextureSize());
        tightenLimit(limits.maximumHeight, demand.maximumTextureSize());
    }
    tightenLimit(limits.maximumBytes, demand.maximumPayloadBytes());
    tightenLimit(limits.maximumBytes, demand.displayByteBudget());
    return limits;
}

StaticPayloadLimits admittedStaticPayloadLimits(
    const kiriview::ImageViewportProviderFrameRequest& request)
{
    StaticPayloadLimits limits = staticPayloadLimits(request);
    tightenLimit(limits.maximumBytes, request.maximumStoreEntryBytes);
    return limits;
}

bool positiveFinite(QSizeF size)
{
    return std::isfinite(size.width()) && std::isfinite(size.height()) && size.width() > 0.0
        && size.height() > 0.0;
}

QSize sourceRasterForLongEdge(QSize sourceSize, int longEdge)
{
    if (sourceSize.isEmpty() || longEdge <= 0) {
        return {};
    }

    const bool widthIsLongEdge = sourceSize.width() >= sourceSize.height();
    const qint64 sourceLongEdge = widthIsLongEdge ? sourceSize.width() : sourceSize.height();
    const qint64 sourceShortEdge = widthIsLongEdge ? sourceSize.height() : sourceSize.width();
    const qint64 shortEdge = (sourceShortEdge * longEdge + sourceLongEdge - 1) / sourceLongEdge;
    if (shortEdge <= 0 || shortEdge > std::numeric_limits<int>::max()) {
        return {};
    }

    return widthIsLongEdge ? QSize(longEdge, static_cast<int>(shortEdge))
                           : QSize(static_cast<int>(shortEdge), longEdge);
}

bool estimatedRasterFits(QSize rasterSize, const StaticPayloadLimits& limits)
{
    if (rasterSize.isEmpty() || limits.maximumWidth <= 0 || limits.maximumHeight <= 0
        || limits.maximumBytes <= 0 || rasterSize.width() > limits.maximumWidth
        || rasterSize.height() > limits.maximumHeight) {
        return false;
    }
    const qsizetype byteCost = kiriview::estimatedRgbaByteCost(rasterSize);
    return byteCost > 0 && byteCost <= limits.maximumBytes;
}

qint64 integerPixelCoverage(qreal value)
{
    if (!std::isfinite(value) || value <= 0.0) {
        return 0;
    }
    constexpr qint64 maximumCoverage = std::numeric_limits<int>::max();
    return value >= qreal(maximumCoverage) ? maximumCoverage
                                           : static_cast<qint64>(std::ceil(value));
}

qint64 minimumLongEdgeForAxisCoverage(qint64 sourceAxis, qint64 sourceLongEdge, qint64 targetAxis)
{
    if (sourceAxis <= 0 || sourceLongEdge <= 0 || targetAxis <= 0) {
        return 0;
    }
    return (sourceLongEdge * (targetAxis - 1) / sourceAxis) + 1;
}

int desiredStaticLongEdge(QSize sourceSize, kiriview::StaticImageSourceDetailModel detailModel,
    const kiriview::ImageViewportProviderFrameRequest& request)
{
    const bool widthIsLongEdge = sourceSize.width() >= sourceSize.height();
    const qint64 sourceLongEdge = widthIsLongEdge ? sourceSize.width() : sourceSize.height();
    qint64 desiredLongEdge = sourceLongEdge;
    QSizeF targetSize = request.demand.targetDisplaySizePixels();
    int rotation = request.demand.rotationDegrees() % 360;
    if (rotation < 0) {
        rotation += 360;
    }
    if (rotation == 90 || rotation == 270) {
        targetSize.transpose();
    }
    if (positiveFinite(targetSize)) {
        const qint64 targetWidth = integerPixelCoverage(targetSize.width());
        const qint64 targetHeight = integerPixelCoverage(targetSize.height());
        desiredLongEdge = std::max(
            minimumLongEdgeForAxisCoverage(sourceSize.width(), sourceLongEdge, targetWidth),
            minimumLongEdgeForAxisCoverage(sourceSize.height(), sourceLongEdge, targetHeight));
    }
    if (detailModel == kiriview::StaticImageSourceDetailModel::FiniteRaster) {
        desiredLongEdge = std::min(desiredLongEdge, sourceLongEdge);
    }
    return static_cast<int>(
        std::clamp<qint64>(desiredLongEdge, 1, std::numeric_limits<int>::max()));
}

int largestFittingLongEdge(QSize sourceSize, int desiredLongEdge, const StaticPayloadLimits& limits)
{
    int lower = 1;
    int upper = desiredLongEdge;
    int acceptedLongEdge = 0;
    while (lower <= upper) {
        const int candidateLongEdge = lower + ((upper - lower) / 2);
        if (estimatedRasterFits(sourceRasterForLongEdge(sourceSize, candidateLongEdge), limits)) {
            acceptedLongEdge = candidateLongEdge;
            lower = candidateLongEdge + 1;
        } else {
            upper = candidateLongEdge - 1;
        }
    }
    return acceptedLongEdge;
}

enum class StaticRasterPlanOutcome {
    Ready,
    PayloadRejection,
    ResourceExhausted,
};

struct StaticRasterPlan
{
    StaticRasterPlanOutcome outcome = StaticRasterPlanOutcome::PayloadRejection;
    QSize rasterSize;
};

StaticRasterPlan plannedStaticRasterSize(QSize sourceSize,
    kiriview::StaticImageSourceDetailModel detailModel,
    const kiriview::ImageViewportProviderFrameRequest& request, bool requireExact)
{
    if (sourceSize.isEmpty()) {
        return {};
    }

    const StaticPayloadLimits publicLimits = staticPayloadLimits(request);
    const StaticPayloadLimits admittedLimits = admittedStaticPayloadLimits(request);
    const bool widthIsLongEdge = sourceSize.width() >= sourceSize.height();
    const int sourceLongEdge = widthIsLongEdge ? sourceSize.width() : sourceSize.height();
    if (requireExact) {
        if (detailModel != kiriview::StaticImageSourceDetailModel::FiniteRaster) {
            return { StaticRasterPlanOutcome::PayloadRejection, {} };
        }
        const QSize exactSize = sourceRasterForLongEdge(sourceSize, sourceLongEdge);
        if (!estimatedRasterFits(exactSize, publicLimits)) {
            return { StaticRasterPlanOutcome::PayloadRejection, {} };
        }
        return estimatedRasterFits(exactSize, admittedLimits)
            ? StaticRasterPlan { StaticRasterPlanOutcome::Ready, exactSize }
            : StaticRasterPlan { StaticRasterPlanOutcome::ResourceExhausted, {} };
    }

    const int desiredLongEdge = desiredStaticLongEdge(sourceSize, detailModel, request);
    if (largestFittingLongEdge(sourceSize, desiredLongEdge, publicLimits) <= 0) {
        return { StaticRasterPlanOutcome::PayloadRejection, {} };
    }
    const int acceptedLongEdge
        = largestFittingLongEdge(sourceSize, desiredLongEdge, admittedLimits);
    return acceptedLongEdge > 0
        ? StaticRasterPlan { StaticRasterPlanOutcome::Ready,
              sourceRasterForLongEdge(sourceSize, acceptedLongEdge) }
        : StaticRasterPlan { StaticRasterPlanOutcome::ResourceExhausted, {} };
}

enum class StaticPayloadAdmission {
    Admissible,
    Invalid,
    PublicRejected,
    PrivateRejected,
};

StaticPayloadAdmission staticPayloadAdmission(const kiriview::StaticDisplayImagePayload& payload,
    QSize expectedOriginalSize, kiriview::StaticImageSourceDetailModel expectedDetailModel,
    const kiriview::ImageViewportProviderFrameRequest& request)
{
    if (!isAuthoritativeStaticPayload(payload) || payload.originalSize != expectedOriginalSize
        || payload.sourceDetailModel != expectedDetailModel) {
        return StaticPayloadAdmission::Invalid;
    }

    const QSize rasterSize = payload.image.size();
    const StaticPayloadLimits publicLimits = staticPayloadLimits(request);
    const qsizetype byteCost = payload.image.sizeInBytes();
    const qint64 aspectDifference
        = std::abs(qint64(rasterSize.width()) * expectedOriginalSize.height()
            - qint64(rasterSize.height()) * expectedOriginalSize.width());
    const qint64 aspectRoundingTolerance
        = std::max(expectedOriginalSize.width(), expectedOriginalSize.height());
    if (rasterSize.isEmpty() || byteCost <= 0 || aspectDifference > aspectRoundingTolerance) {
        return StaticPayloadAdmission::Invalid;
    }
    if (publicLimits.maximumWidth <= 0 || publicLimits.maximumHeight <= 0
        || publicLimits.maximumBytes <= 0 || rasterSize.width() > publicLimits.maximumWidth
        || rasterSize.height() > publicLimits.maximumHeight
        || byteCost > publicLimits.maximumBytes) {
        return StaticPayloadAdmission::PublicRejected;
    }
    if (request.maximumStoreEntryBytes >= 0 && byteCost > request.maximumStoreEntryBytes) {
        return StaticPayloadAdmission::PrivateRejected;
    }
    return StaticPayloadAdmission::Admissible;
}

bool staticPayloadSatisfiesPlan(
    const kiriview::StaticDisplayImagePayload& payload, QSize targetRasterSize, bool requireExact)
{
    if (requireExact) {
        return payload.quality == kiriview::DisplayImageQuality::Exact;
    }
    return payload.image.width() >= targetRasterSize.width()
        && payload.image.height() >= targetRasterSize.height();
}

kiriview::StaticDisplayImagePayload classifiedCurrentDetailPayload(
    kiriview::StaticDisplayImagePayload payload)
{
    if (payload.quality != kiriview::DisplayImageQuality::Exact) {
        payload.quality = kiriview::DisplayImageQuality::BoundedDetail;
    }
    payload.previewOrigin = kiriview::DisplayImagePreviewOrigin::None;
    return payload;
}

kiriview::StaticDisplayImagePayload classifiedFirstDisplayPayload(
    kiriview::StaticDisplayImagePayload payload)
{
    payload.quality = kiriview::DisplayImageQuality::FirstDisplay;
    payload.previewOrigin = kiriview::DisplayImagePreviewOrigin::None;
    return payload;
}
}

namespace kiriview {
ImageViewportDecodeProviderSource::ImageViewportDecodeProviderSource(
    ImageDecodeDependencies dependencies,
    ImageViewportProvisionalPreviewPolicy provisionalPreviewPolicy,
    TimerScheduler initialDetailTimerScheduler)
    : m_dependencies(imageDecodeDependenciesWithDefaults(std::move(dependencies)))
    , m_provisionalPreviewPolicy(provisionalPreviewPolicy)
    , m_initialDetailTimerScheduler(
          timerSchedulerWithDefaults(std::move(initialDetailTimerScheduler)))
    , m_decodeJob(this, m_dependencies,
          ImageDecodeJob::Callbacks {
              [this](const ImageDecodeRequest& request, DecodedImageResult result) {
                  const std::shared_ptr<ImageViewportDecodeProviderSource> lifetime
                      = weak_from_this().lock();
                  if (lifetime != nullptr) {
                      lifetime->finishDecode(request, std::move(result));
                  }
              },
              [this](const ImageDecodeRequest& request, const ImageDataLoadError& error) {
                  const std::shared_ptr<ImageViewportDecodeProviderSource> lifetime
                      = weak_from_this().lock();
                  if (lifetime != nullptr) {
                      lifetime->finishDataLoadError(request, error);
                  }
              },
              [this](const ImageDecodeRequest& request, StaticDisplayImagePayload displayImage) {
                  const std::shared_ptr<ImageViewportDecodeProviderSource> lifetime
                      = weak_from_this().lock();
                  if (lifetime != nullptr) {
                      lifetime->finishThumbnail(request, std::move(displayImage));
                  }
              },
          })
{
}

ImageViewportDecodeProviderSource::ImageViewportDecodeProviderSource(ImageLoadSession session,
    ImageDecodeDependencies dependencies,
    std::optional<StaticDisplayImagePayload> authoritativeSeed,
    TimerScheduler initialDetailTimerScheduler)
    : ImageViewportDecodeProviderSource(std::move(dependencies),
          ImageViewportProvisionalPreviewPolicy::Allow, std::move(initialDetailTimerScheduler))
{
    static_cast<void>(resolveSession(std::move(session), std::move(authoritativeSeed)));
}

ImageViewportDecodeProviderSource::~ImageViewportDecodeProviderSource() { close(); }

bool ImageViewportDecodeProviderSource::resolveSession(
    ImageLoadSession session, std::optional<StaticDisplayImagePayload> authoritativeSeed)
{
    [[maybe_unused]] const std::shared_ptr<ImageViewportDecodeProviderSource> lifetime
        = weak_from_this().lock();
    if (m_closed || m_session.has_value() || session.id() == 0
        || session.decodeRequest().isEmpty()) {
        return false;
    }

    m_session = std::move(session);
    if (authoritativeSeed.has_value() && isAuthoritativeStaticPayload(*authoritativeSeed)) {
        m_authoritativeSeed = std::move(authoritativeSeed);
    }

    ensureDecoded();
    return true;
}

const EmbeddedMetadata& ImageViewportDecodeProviderSource::embeddedMetadata() const
{
    return m_embeddedMetadata;
}

ImageSequenceProviderMetadata ImageViewportDecodeProviderSource::constructionMetadata() const
{
    return m_metadata.value_or(ImageSequenceProviderMetadata {});
}

void ImageViewportDecodeProviderSource::requestMetadata(
    const ImageViewportProviderWorkIdentity& identity, MetadataCompletion completion)
{
    [[maybe_unused]] const std::shared_ptr<ImageViewportDecodeProviderSource> lifetime
        = weak_from_this().lock();
    if (m_closed || !completion) {
        return;
    }
    if (m_failure.has_value()) {
        completion(
            identity, ImageViewportProviderMetadataResult::failed(m_failureCause, *m_failure));
        return;
    }
    if (m_metadata.has_value()) {
        completion(identity, ImageViewportProviderMetadataResult::ready(*m_metadata));
        return;
    }
    m_pendingMetadata.push_back({ identity, std::move(completion) });
    ensureDecoded();
}

void ImageViewportDecodeProviderSource::requestFrame(
    const ImageViewportProviderWorkIdentity& identity, ImageViewportProviderFrameRequest request,
    FrameCompletion completion)
{
    [[maybe_unused]] const std::shared_ptr<ImageViewportDecodeProviderSource> lifetime
        = weak_from_this().lock();
    if (m_closed || !completion) {
        return;
    }
    if (m_failure.has_value()) {
        completion(identity, ImageViewportProviderFrameResult::failed(m_failureCause, *m_failure));
        return;
    }
    m_pendingFrames.push_back({ identity, request, std::move(completion) });
    ensureDecoded();
    publishFrames();
}

void ImageViewportDecodeProviderSource::cancel(
    const QVector<ImageSequenceProviderRequestToken>& tokens)
{
    std::erase_if(m_pendingMetadata, [&tokens](const PendingMetadata& pending) {
        return containsToken(tokens, pending.identity.requestToken);
    });
    std::erase_if(m_pendingFrames, [&tokens](const PendingFrame& pending) {
        return containsToken(tokens, pending.identity.requestToken);
    });

    std::vector<quint64> staticAttemptIds;
    for (const StaticFrameAttempt& attempt : m_staticFrameAttempts) {
        if (containsToken(tokens, attempt.pending.identity.requestToken)) {
            staticAttemptIds.push_back(attempt.id);
        }
    }
    for (quint64 attemptId : staticAttemptIds) {
        static_cast<void>(takeStaticFrameAttempt(attemptId, false));
    }

    std::vector<ImageWorkerTask> detachedTasks;
    auto unit = m_workerUnits.begin();
    while (unit != m_workerUnits.end()) {
        if (!unit->identity.has_value() || !containsToken(tokens, unit->identity->requestToken)) {
            ++unit;
            continue;
        }
        if (m_activeAnimationFrameWork.has_value()
            && m_activeAnimationFrameWork->workerUnitId == unit->id) {
            m_activeAnimationFrameWork->publishResult = false;
            ++unit;
            continue;
        }
        detachedTasks.push_back(std::move(unit->task));
        unit = m_workerUnits.erase(unit);
    }
    for (ImageWorkerTask& task : detachedTasks) {
        task.cancel();
    }
}

void ImageViewportDecodeProviderSource::close()
{
    if (m_closed) {
        return;
    }
    m_closed = true;
    m_pendingMetadata.clear();
    m_pendingFrames.clear();
    m_activeAnimationFrameWork.reset();
    m_publishingFrames = false;
    for (StaticFrameAttempt& attempt : m_staticFrameAttempts) {
        if (attempt.initialDetailTimer != nullptr) {
            attempt.initialDetailTimer->stop();
        }
    }
    m_staticFrameAttempts.clear();
    m_staticRefinementWorks.clear();
    if (m_animation.has_value() && m_animation->runtime != nullptr) {
        m_animation->runtime->close();
    }
    m_animation.reset();

    std::vector<ImageWorkerTask> detachedTasks;
    detachedTasks.reserve(m_workerUnits.size());
    for (WorkerUnit& unit : m_workerUnits) {
        detachedTasks.push_back(std::move(unit.task));
    }
    m_workerUnits.clear();

    m_decodeJob.cancel();
    for (ImageWorkerTask& task : detachedTasks) {
        task.cancel();
    }
}

void ImageViewportDecodeProviderSource::ensureDecoded()
{
    if (m_closed || !m_session.has_value() || m_decodeStarted || m_metadata.has_value()
        || m_failure.has_value() || (m_pendingMetadata.empty() && m_pendingFrames.empty())) {
        return;
    }
    m_decodeStarted = true;
    m_decodeJob.start(resolvedSession().decodeRequest(), std::move(m_authoritativeSeed));
}

void ImageViewportDecodeProviderSource::finishDecode(
    const ImageDecodeRequest& request, DecodedImageResult result)
{
    if (m_closed || !m_session.has_value() || !request.matches(resolvedSession().decodeRequest())) {
        return;
    }
    m_decodeComplete = true;
    if (!result) {
        finishFailure(ImageSequenceProviderFailureCause::Decode,
            loadFailure(resolvedSession(), result.error()));
        return;
    }
    finishDecodedImage(std::move(*result));
}

void ImageViewportDecodeProviderSource::finishDataLoadError(
    const ImageDecodeRequest& request, const ImageDataLoadError& error)
{
    if (m_closed || !m_session.has_value() || !request.matches(resolvedSession().decodeRequest())) {
        return;
    }
    m_decodeComplete = true;
    finishFailure(
        ImageSequenceProviderFailureCause::SourceAccess, loadFailure(resolvedSession(), error));
}

void ImageViewportDecodeProviderSource::finishThumbnail(
    const ImageDecodeRequest& request, StaticDisplayImagePayload displayImage)
{
    if (m_closed || m_decodeComplete || !m_session.has_value()
        || !request.matches(resolvedSession().decodeRequest()) || m_failure.has_value()
        || m_authoritativeStaticImage.has_value() || m_animation.has_value()
        || m_provisionalPreview.has_value() || !isProvisionalPreviewPayload(displayImage)) {
        return;
    }

    m_metadata = ImageSequenceProviderMetadata::still(displayImage.originalSize);
    m_provisionalPreview = std::move(displayImage);
    publishMetadata();
    publishProvisionalFrames();
}

void ImageViewportDecodeProviderSource::finishDecodedImage(DecodedImage image)
{
    m_embeddedMetadata = decodedImageEmbeddedMetadata(image);
    std::visit(
        [this](auto decoded) {
            using Image = std::decay_t<decltype(decoded)>;
            if constexpr (std::is_same_v<Image, StaticDecodedImage>) {
                finishStaticImage(std::move(decoded));
            } else if constexpr (std::is_same_v<Image, ReaderAnimationImage>) {
                const QString formatIdentifier = QString::fromLatin1(decoded.format);
                finishAnimationImage({}, std::move(decoded.firstFrame), std::move(decoded.catalog),
                    readerAnimationPlaybackRequest(std::move(decoded.data),
                        std::move(decoded.format), std::move(decoded.sourceDataLease)),
                    std::move(decoded.sourceIdentity), std::move(decoded.sourceRevision),
                    formatIdentifier);
            } else if constexpr (std::is_same_v<Image, ApngAnimationImage>) {
                finishAnimationImage(std::move(decoded.firstFrameWorkspaceHold),
                    std::move(decoded.firstFrame), std::move(decoded.catalog),
                    apngAnimationPlaybackRequest(std::move(decoded.data),
                        std::move(decoded.sourceDataLease), m_dependencies.workspaceBudget),
                    std::move(decoded.sourceIdentity), std::move(decoded.sourceRevision),
                    QStringLiteral("apng"));
            } else if constexpr (std::is_same_v<Image, WebPAnimationImage>) {
                finishAnimationImage({}, std::move(decoded.firstFrame), std::move(decoded.catalog),
                    webpAnimationPlaybackRequest(
                        std::move(decoded.data), std::move(decoded.sourceDataLease)),
                    std::move(decoded.sourceIdentity), std::move(decoded.sourceRevision),
                    QStringLiteral("webp"));
            } else if constexpr (std::is_same_v<Image, JxlAnimationImage>) {
                finishAnimationImage({}, std::move(decoded.firstFrame), std::move(decoded.catalog),
                    jxlAnimationPlaybackRequest(
                        std::move(decoded.data), std::move(decoded.sourceDataLease)),
                    std::move(decoded.sourceIdentity), std::move(decoded.sourceRevision),
                    QStringLiteral("jxl"));
            } else if constexpr (std::is_same_v<Image, HeifSequenceAnimationImage>) {
                finishAnimationImage({}, std::move(decoded.firstFrame), std::move(decoded.catalog),
                    heifSequenceAnimationPlaybackRequest(
                        std::move(decoded.data), std::move(decoded.sourceDataLease)),
                    std::move(decoded.sourceIdentity), std::move(decoded.sourceRevision),
                    QStringLiteral("heif"));
            }
        },
        std::move(image));
}

void ImageViewportDecodeProviderSource::finishStaticImage(StaticDecodedImage image)
{
    if (!isAuthoritativeStaticPayload(image.displayImage)) {
        finishFailure(ImageSequenceProviderFailureCause::Decode,
            loadFailure(resolvedSession(), ImageLoadFailureKind::Decode, QString(),
                QStringLiteral("decoded static image is not displayable")));
        return;
    }
    m_provisionalPreview.reset();
    if (m_animation.has_value() && m_animation->runtime != nullptr) {
        m_animation->runtime->close();
    }
    m_animation.reset();
    m_metadata = ImageSequenceProviderMetadata::still(image.displayImage.originalSize);
    m_authoritativeStaticImage = std::move(image.displayImage);
    publishMetadata();
    publishFrames();
}

void ImageViewportDecodeProviderSource::finishAnimationImage(
    ImageDecodeWorkspaceHold firstFrameWorkspaceHold, QImage firstFrame,
    ImageAnimationSourceCatalog catalog, ImageAnimationPlaybackRequest playbackRequest,
    QString sourceIdentity, ImageSourceRevision sourceRevision, QString formatIdentifier)
{
    m_provisionalPreview.reset();
    m_authoritativeStaticImage.reset();
    if (m_animation.has_value() && m_animation->runtime != nullptr) {
        m_animation->runtime->close();
    }
    m_animation.reset();

    ImageAnimationPlaybackSourceFactory sourceFactory
        = imageAnimationPlaybackSourceFactory(std::move(playbackRequest));
    if (firstFrame.isNull() || !catalog.isValid() || catalog.logicalSize != firstFrame.size()
        || !sourceFactory) {
        firstFrame = {};
        firstFrameWorkspaceHold = {};
        finishFailure(ImageSequenceProviderFailureCause::Decode,
            loadFailure(resolvedSession(), ImageLoadFailureKind::Decode, QString(),
                QStringLiteral("animation timing metadata is invalid"),
                DecodedImageFailureRoute::Unknown,
                DecodedImageFailureOperation::DecodeAnimationOpen));
        return;
    }

    ImageSequenceProviderMetadata metadata = ImageSequenceProviderMetadata::timedFrameList(
        catalog.logicalSize, std::move(catalog.frameDurations));
    metadata.setAuthoredAnimationFacts(authoredAnimationFacts(catalog.repeatCount));
    auto runtime = std::make_shared<AnimationSourceRuntime>(std::move(firstFrame),
        metadata.frameCount(), std::move(sourceFactory), std::move(firstFrameWorkspaceHold));
    m_metadata = metadata;
    m_animation = AnimationState {
        std::move(runtime),
        std::move(metadata),
        std::move(sourceIdentity),
        std::move(sourceRevision),
        std::move(formatIdentifier),
    };
    publishMetadata();
    publishFrames();
}

void ImageViewportDecodeProviderSource::finishFailure(
    ImageSequenceProviderFailureCause cause, ImageLoadFailure failure)
{
    m_provisionalPreview.reset();
    m_failureCause = cause;
    m_failure = std::move(failure);

    while (!m_closed && !m_pendingMetadata.empty()) {
        PendingMetadata pending = std::move(m_pendingMetadata.front());
        m_pendingMetadata.erase(m_pendingMetadata.begin());
        pending.completion(
            pending.identity, ImageViewportProviderMetadataResult::failed(cause, *m_failure));
    }
    while (!m_closed && !m_pendingFrames.empty()) {
        PendingFrame pending = std::move(m_pendingFrames.front());
        m_pendingFrames.erase(m_pendingFrames.begin());
        pending.completion(
            pending.identity, ImageViewportProviderFrameResult::failed(cause, *m_failure));
    }
}

void ImageViewportDecodeProviderSource::publishMetadata()
{
    if (!m_metadata.has_value()) {
        return;
    }

    while (!m_closed && !m_pendingMetadata.empty()) {
        PendingMetadata pending = std::move(m_pendingMetadata.front());
        m_pendingMetadata.erase(m_pendingMetadata.begin());
        pending.completion(
            pending.identity, ImageViewportProviderMetadataResult::ready(*m_metadata));
    }
}

void ImageViewportDecodeProviderSource::publishFrames()
{
    if (!m_authoritativeStaticImage.has_value() && !m_animation.has_value()) {
        publishProvisionalFrames();
        return;
    }

    if (m_animation.has_value()) {
        if (m_publishingFrames) {
            return;
        }
        m_publishingFrames = true;
        while (!m_closed && !m_pendingFrames.empty() && !m_activeAnimationFrameWork.has_value()) {
            PendingFrame frame = std::move(m_pendingFrames.front());
            m_pendingFrames.erase(m_pendingFrames.begin());
            m_activeAnimationFrameWork = ActiveAnimationFrameWork {};
            publishAnimationFrame(std::move(frame));
        }
        m_publishingFrames = false;
        return;
    }

    while (!m_closed && !m_pendingFrames.empty()) {
        PendingFrame frame = std::move(m_pendingFrames.front());
        m_pendingFrames.erase(m_pendingFrames.begin());
        publishStaticFrame(std::move(frame));
    }
}

void ImageViewportDecodeProviderSource::publishProvisionalFrames()
{
    if (m_closed || m_provisionalPreviewPolicy != ImageViewportProvisionalPreviewPolicy::Allow
        || !m_provisionalPreview.has_value() || m_authoritativeStaticImage.has_value()
        || m_animation.has_value() || m_failure.has_value()) {
        return;
    }

    while (!m_closed) {
        auto pending = std::ranges::find_if(m_pendingFrames, [](const PendingFrame& candidate) {
            return !candidate.provisionalPublished && candidate.request.frame == 0
                && hasNoAcceptedCurrentPayload(candidate.request.demand);
        });
        if (pending == m_pendingFrames.end()) {
            return;
        }

        pending->provisionalPublished = true;
        const ImageViewportProviderWorkIdentity identity = pending->identity;
        const FrameCompletion completion = pending->completion;
        completion(identity,
            ImageViewportProviderFrameResult::provisional(*m_provisionalPreview,
                ImageSequenceProviderFrameEnvelope::stillFrame(), QString()));
    }
}

void ImageViewportDecodeProviderSource::publishStaticFrame(PendingFrame pending)
{
    if (m_closed) {
        return;
    }
    if (!m_authoritativeStaticImage.has_value() || pending.request.frame != 0) {
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::failed(ImageSequenceProviderFailureCause::Decode,
                loadFailure(resolvedSession(), ImageLoadFailureKind::Decode, QString(),
                    QStringLiteral("requested static frame is unavailable"))));
        return;
    }

    const bool requireExact = pending.request.demand.exactnessPreference()
        == ImageViewportExactnessPreference::RequireExact;
    const StaticRasterPlan rasterPlan
        = plannedStaticRasterSize(m_authoritativeStaticImage->originalSize,
            m_authoritativeStaticImage->sourceDetailModel, pending.request, requireExact);
    if (rasterPlan.outcome == StaticRasterPlanOutcome::PayloadRejection) {
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::unsupported(
                ImageSequenceProviderUnsupportedCause::PayloadRejection));
        return;
    }
    if (rasterPlan.outcome == StaticRasterPlanOutcome::ResourceExhausted) {
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::failed(
                ImageSequenceProviderFailureCause::ResourceExhausted,
                loadFailure(resolvedSession(), ImageLoadFailureKind::Presentation, QString(),
                    QStringLiteral(
                        "current image detail exceeds the active display resource limits"))));
        return;
    }

    const StaticFramePlan plan {
        rasterPlan.rasterSize,
        requireExact,
    };
    if (staticPayloadAdmission(*m_authoritativeStaticImage,
            m_authoritativeStaticImage->originalSize, m_authoritativeStaticImage->sourceDetailModel,
            pending.request)
            == StaticPayloadAdmission::Admissible
        && staticPayloadSatisfiesPlan(
            *m_authoritativeStaticImage, plan.targetRasterSize, plan.requireExact)) {
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::ready(
                classifiedCurrentDetailPayload(*m_authoritativeStaticImage),
                ImageSequenceProviderFrameEnvelope::stillFrame(), QString()));
        return;
    }

    const bool initialDemand = hasNoAcceptedCurrentPayload(pending.request.demand);
    const std::shared_ptr<StaticImageDisplaySource> refinementSource
        = m_authoritativeStaticImage->refinementSource;
    if (refinementSource == nullptr || !refinementSource->supportsRasterDisplayRefinement()) {
        StaticFrameAttempt attempt {
            reserveStaticFrameAttemptId(),
            std::move(pending),
            plan,
            *m_authoritativeStaticImage,
            {},
            0,
            initialDemand,
        };
        finishStaticFrameAttempt(std::move(attempt), {}, std::nullopt, true,
            StaticFrameResolution::RefinementUnsupported);
        return;
    }

    startStaticRefinement(std::move(pending), plan, initialDemand);
}

void ImageViewportDecodeProviderSource::startStaticRefinement(
    PendingFrame pending, StaticFramePlan plan, bool initialDemand)
{
    [[maybe_unused]] const std::shared_ptr<ImageViewportDecodeProviderSource> lifetime
        = weak_from_this().lock();
    const quint64 attemptId = reserveStaticFrameAttemptId();
    const qint64 maximumReusableBytes = admittedStaticPayloadLimits(pending.request).maximumBytes;
    m_staticFrameAttempts.push_back(StaticFrameAttempt {
        attemptId,
        std::move(pending),
        plan,
        *m_authoritativeStaticImage,
        {},
        0,
        initialDemand,
    });

    auto existingWork
        = std::ranges::find_if(m_staticRefinementWorks, [&plan](const StaticRefinementWork& work) {
              return work.targetRasterSize == plan.targetRasterSize;
          });
    if (existingWork != m_staticRefinementWorks.end()) {
        existingWork->attemptIds.push_back(attemptId);
        const auto attempt = std::ranges::find_if(m_staticFrameAttempts,
            [attemptId](const StaticFrameAttempt& candidate) { return candidate.id == attemptId; });
        if (attempt != m_staticFrameAttempts.end()) {
            attempt->refinementWorkerUnitId = existingWork->workerUnitId;
        }
    } else {
        discardRetainedStaticRefinementsExcept(0);
        const StaticDisplayImagePayload basis = *m_authoritativeStaticImage;
        const std::shared_ptr<StaticImageDisplaySource> refinementSource = basis.refinementSource;
        const quint64 workerUnitId = reserveWorkerUnit();
        m_staticRefinementWorks.push_back(StaticRefinementWork {
            workerUnitId,
            plan.targetRasterSize,
            basis,
            { attemptId },
            maximumReusableBytes,
            false,
        });
        const auto attempt = std::ranges::find_if(m_staticFrameAttempts,
            [attemptId](const StaticFrameAttempt& candidate) { return candidate.id == attemptId; });
        if (attempt != m_staticFrameAttempts.end()) {
            attempt->refinementWorkerUnitId = workerUnitId;
        }

        const std::weak_ptr<ImageViewportDecodeProviderSource> weakSelf = weak_from_this();
        ImageWorkerTask task = m_dependencies.refinementScheduler.run(
            this,
            [refinementSource, targetSize = plan.targetRasterSize]() {
                return refinementSource->decodeRasterDisplayImage(targetSize);
            },
            [weakSelf, workerUnitId](const StaticImageDisplayDecodeResult& result) {
                if (const std::shared_ptr<ImageViewportDecodeProviderSource> self
                    = weakSelf.lock()) {
                    self->finishStaticRefinement(workerUnitId, result);
                }
            });
        attachWorkerTask(workerUnitId, std::move(task));
    }

    const auto attempt = std::ranges::find_if(m_staticFrameAttempts,
        [attemptId](const StaticFrameAttempt& candidate) { return candidate.id == attemptId; });
    if (attempt == m_staticFrameAttempts.end() || !attempt->initialDemand
        || attempt->plan.requireExact || !m_authoritativeStaticImage.has_value()
        || staticPayloadAdmission(*m_authoritativeStaticImage,
               m_authoritativeStaticImage->originalSize,
               m_authoritativeStaticImage->sourceDetailModel, attempt->pending.request)
            != StaticPayloadAdmission::Admissible) {
        return;
    }

    if (attempt->pending.request.demand.qualityPreference()
        == ImageViewportQualityPreference::FastFirstDisplay) {
        std::optional<StaticFrameAttempt> fallback = takeStaticFrameAttempt(attemptId, true);
        if (fallback.has_value()) {
            std::optional<StaticDisplayImagePayload> fallbackCandidate = fallback->fallbackBasis;
            finishStaticFrameAttempt(std::move(*fallback), {}, std::move(fallbackCandidate), false);
        }
        return;
    }
    scheduleInitialDetailDeadline(attemptId);
}

void ImageViewportDecodeProviderSource::finishStaticRefinement(
    quint64 workerUnitId, const StaticImageDisplayDecodeResult& result)
{
    if (!detachWorkerUnit(workerUnitId) || m_closed) {
        return;
    }
    const auto currentWork = std::ranges::find_if(
        m_staticRefinementWorks, [workerUnitId](const StaticRefinementWork& work) {
            return work.workerUnitId == workerUnitId;
        });
    if (currentWork == m_staticRefinementWorks.end()) {
        return;
    }

    StaticRefinementWork work = std::move(*currentWork);
    m_staticRefinementWorks.erase(currentWork);
    std::optional<StaticDisplayImagePayload> refinementCandidate;
    StaticFrameResolution resolution = StaticFrameResolution::CandidateSelection;
    if (result.image.isNull()) {
        resolution = StaticFrameResolution::RefinementDecodeFailure;
    } else {
        StaticDisplayImagePayload refined = work.basis;
        refined.image = displayReadyImage(result.image);
        const bool resultMatchesRequest = refined.image.size() == work.targetRasterSize;
        const bool exactForSource
            = refined.sourceDetailModel == StaticImageSourceDetailModel::FiniteRaster
            && work.targetRasterSize == refined.originalSize;
        refined.quality
            = exactForSource ? DisplayImageQuality::Exact : DisplayImageQuality::BoundedDetail;
        refined.previewOrigin = DisplayImagePreviewOrigin::None;
        refined.rasterKind = DisplayImageRasterKind::Refinement;
        if (resultMatchesRequest && isAuthoritativeStaticPayload(refined)) {
            refinementCandidate = refined;
        } else {
            resolution = StaticFrameResolution::RefinementContractViolation;
        }
        if (refinementCandidate.has_value() && m_authoritativeStaticImage.has_value()
            && refinementCandidate->image.sizeInBytes() <= work.maximumReusableBytes
            && isReusableAuthoritativeUpgrade(*refinementCandidate, *m_authoritativeStaticImage)) {
            m_authoritativeStaticImage = *refinementCandidate;
        }
    }

    for (quint64 attemptId : work.attemptIds) {
        const auto active = std::ranges::find_if(m_staticFrameAttempts,
            [attemptId](const StaticFrameAttempt& attempt) { return attempt.id == attemptId; });
        if (active == m_staticFrameAttempts.end()) {
            continue;
        }
        active->refinementWorkerUnitId = 0;
        if (active->deadlineExpired) {
            continue;
        }

        std::optional<StaticFrameAttempt> completed = takeStaticFrameAttempt(attemptId, false);
        if (completed.has_value()) {
            finishStaticFrameAttempt(
                std::move(*completed), result.diagnostics, refinementCandidate, true, resolution);
        }
        if (m_closed) {
            return;
        }
    }
}

void ImageViewportDecodeProviderSource::scheduleInitialDetailDeadline(quint64 attemptId)
{
    const std::weak_ptr<ImageViewportDecodeProviderSource> weakSelf = weak_from_this();
    std::unique_ptr<RuntimeTimerHandle> timer = m_initialDetailTimerScheduler.singleShotTimer(
        this, initialDetailWaitDuration, [weakSelf, attemptId]() {
            if (const std::shared_ptr<ImageViewportDecodeProviderSource> self = weakSelf.lock()) {
                self->markInitialDetailDeadlineExpired(attemptId);
            }
        });
    const auto attempt = std::ranges::find_if(m_staticFrameAttempts,
        [attemptId](const StaticFrameAttempt& candidate) { return candidate.id == attemptId; });
    if (attempt == m_staticFrameAttempts.end() || timer == nullptr) {
        if (timer != nullptr) {
            timer->stop();
        }
        return;
    }
    attempt->initialDetailTimer = std::move(timer);
    attempt->initialDetailTimer->start(initialDetailWaitDuration);
}

void ImageViewportDecodeProviderSource::markInitialDetailDeadlineExpired(quint64 attemptId)
{
    const auto attempt = std::ranges::find_if(m_staticFrameAttempts,
        [attemptId](const StaticFrameAttempt& candidate) { return candidate.id == attemptId; });
    if (m_closed || attempt == m_staticFrameAttempts.end() || attempt->deadlineExpired) {
        return;
    }
    if (m_authoritativeStaticImage.has_value()
        && staticPayloadAdmission(*m_authoritativeStaticImage, attempt->fallbackBasis.originalSize,
               attempt->fallbackBasis.sourceDetailModel, attempt->pending.request)
            == StaticPayloadAdmission::Admissible) {
        attempt->deadlineCandidate = *m_authoritativeStaticImage;
    } else {
        attempt->deadlineCandidate = attempt->fallbackBasis;
    }
    attempt->deadlineExpired = true;
    const std::weak_ptr<ImageViewportDecodeProviderSource> weakSelf = weak_from_this();
    const bool queued = QMetaObject::invokeMethod(
        this,
        [weakSelf, attemptId]() {
            if (const std::shared_ptr<ImageViewportDecodeProviderSource> self = weakSelf.lock()) {
                self->finishInitialDetailDeadline(attemptId);
            }
        },
        Qt::QueuedConnection);
    if (!queued) {
        attempt->deadlineExpired = false;
    }
}

void ImageViewportDecodeProviderSource::finishInitialDetailDeadline(quint64 attemptId)
{
    const auto active = std::ranges::find_if(m_staticFrameAttempts,
        [attemptId](const StaticFrameAttempt& candidate) { return candidate.id == attemptId; });
    if (m_closed || active == m_staticFrameAttempts.end() || !active->deadlineExpired) {
        return;
    }

    std::optional<StaticFrameAttempt> fallback = takeStaticFrameAttempt(attemptId, true);
    if (fallback.has_value()) {
        std::optional<StaticDisplayImagePayload> deadlineCandidate
            = std::move(fallback->deadlineCandidate);
        finishStaticFrameAttempt(std::move(*fallback), {}, std::move(deadlineCandidate), false);
    }
}

void ImageViewportDecodeProviderSource::finishStaticFrameAttempt(StaticFrameAttempt attempt,
    const StaticImageDisplayDecodeDiagnostics& diagnostics,
    std::optional<StaticDisplayImagePayload> preferredCandidate, bool considerCurrentCandidate,
    StaticFrameResolution resolution)
{
    if (m_closed) {
        return;
    }

    const QSize expectedOriginalSize = attempt.fallbackBasis.originalSize;
    const StaticImageSourceDetailModel expectedDetailModel
        = attempt.fallbackBasis.sourceDetailModel;
    const StaticDisplayImagePayload* matchingCandidate = nullptr;
    const StaticDisplayImagePayload* admissibleCandidate = nullptr;
    std::optional<StaticPayloadAdmission> preferredAdmission;
    bool publicPayloadRejected = false;
    bool privatePayloadRejected = false;
    const auto preferHigherDetail
        = [](const StaticDisplayImagePayload& candidate, const StaticDisplayImagePayload* current) {
              if (current == nullptr) {
                  return true;
              }
              const bool candidateExact = candidate.quality == DisplayImageQuality::Exact;
              const bool currentExact = current->quality == DisplayImageQuality::Exact;
              if (candidateExact != currentExact) {
                  return candidateExact;
              }
              return candidate.image.width() >= current->image.width()
                  && candidate.image.height() >= current->image.height()
                  && candidate.image.size() != current->image.size();
          };
    const auto considerCandidate
        = [&](const StaticDisplayImagePayload* candidate, bool preferred = false) {
              if (candidate == nullptr) {
                  return;
              }
              const StaticPayloadAdmission admission = staticPayloadAdmission(
                  *candidate, expectedOriginalSize, expectedDetailModel, attempt.pending.request);
              if (preferred) {
                  preferredAdmission = admission;
              }
              if (admission == StaticPayloadAdmission::PublicRejected) {
                  publicPayloadRejected = true;
                  return;
              }
              if (admission == StaticPayloadAdmission::PrivateRejected) {
                  privatePayloadRejected = true;
                  return;
              }
              if (admission != StaticPayloadAdmission::Admissible) {
                  return;
              }
              if (preferHigherDetail(*candidate, admissibleCandidate)) {
                  admissibleCandidate = candidate;
              }
              if (staticPayloadSatisfiesPlan(
                      *candidate, attempt.plan.targetRasterSize, attempt.plan.requireExact)
                  && preferHigherDetail(*candidate, matchingCandidate)) {
                  matchingCandidate = candidate;
              }
          };
    considerCandidate(preferredCandidate.has_value() ? &*preferredCandidate : nullptr, true);
    considerCandidate(considerCurrentCandidate && m_authoritativeStaticImage.has_value()
            ? &*m_authoritativeStaticImage
            : nullptr);
    considerCandidate(&attempt.fallbackBasis);

    if (matchingCandidate != nullptr) {
        attempt.pending.completion(attempt.pending.identity,
            ImageViewportProviderFrameResult::ready(
                classifiedCurrentDetailPayload(*matchingCandidate),
                ImageSequenceProviderFrameEnvelope::stillFrame(), QString()));
        return;
    }

    if (resolution == StaticFrameResolution::RefinementContractViolation) {
        const QString diagnosticDetail = diagnostics.diagnosticDetail.isEmpty()
            ? QStringLiteral("static image refinement returned an invalid current-detail payload")
            : diagnostics.diagnosticDetail;
        attempt.pending.completion(attempt.pending.identity,
            ImageViewportProviderFrameResult::failed(
                ImageSequenceProviderFailureCause::ProviderInternal,
                loadFailure(resolvedSession(), ImageLoadFailureKind::Presentation,
                    diagnostics.userMessage, diagnosticDetail)));
        return;
    }

    if (attempt.initialDemand && !attempt.plan.requireExact && admissibleCandidate != nullptr) {
        attempt.pending.completion(attempt.pending.identity,
            ImageViewportProviderFrameResult::ready(
                classifiedFirstDisplayPayload(*admissibleCandidate),
                ImageSequenceProviderFrameEnvelope::stillFrame(), QString()));
        return;
    }

    if (resolution == StaticFrameResolution::RefinementDecodeFailure) {
        const QString diagnosticDetail = diagnostics.diagnosticDetail.isEmpty()
            ? QStringLiteral("requested current-detail static frame decode failed")
            : diagnostics.diagnosticDetail;
        attempt.pending.completion(attempt.pending.identity,
            ImageViewportProviderFrameResult::failed(ImageSequenceProviderFailureCause::Decode,
                loadFailure(resolvedSession(), ImageLoadFailureKind::Decode,
                    diagnostics.userMessage, diagnosticDetail)));
        return;
    }

    if (resolution == StaticFrameResolution::RefinementUnsupported) {
        attempt.pending.completion(attempt.pending.identity,
            ImageViewportProviderFrameResult::unsupported(attempt.plan.requireExact
                    ? ImageSequenceProviderUnsupportedCause::PayloadRejection
                    : ImageSequenceProviderUnsupportedCause::UnsupportedRequest));
        return;
    }

    if (preferredAdmission == StaticPayloadAdmission::PublicRejected) {
        attempt.pending.completion(attempt.pending.identity,
            ImageViewportProviderFrameResult::unsupported(
                ImageSequenceProviderUnsupportedCause::PayloadRejection));
        return;
    }

    if (preferredAdmission == StaticPayloadAdmission::PrivateRejected) {
        attempt.pending.completion(attempt.pending.identity,
            ImageViewportProviderFrameResult::failed(
                ImageSequenceProviderFailureCause::ResourceExhausted,
                loadFailure(resolvedSession(), ImageLoadFailureKind::Presentation, QString(),
                    QStringLiteral(
                        "current image output exceeds the application display-store limit"))));
        return;
    }

    if (publicPayloadRejected) {
        attempt.pending.completion(attempt.pending.identity,
            ImageViewportProviderFrameResult::unsupported(
                ImageSequenceProviderUnsupportedCause::PayloadRejection));
        return;
    }

    if (admissibleCandidate == nullptr) {
        const ImageSequenceProviderFailureCause cause = privatePayloadRejected
            ? ImageSequenceProviderFailureCause::ResourceExhausted
            : ImageSequenceProviderFailureCause::ProviderInternal;
        const QString diagnosticDetail = privatePayloadRejected
            ? QStringLiteral("current image output exceeds the application display-store limit")
            : QStringLiteral("current image output has no valid authoritative candidate");
        attempt.pending.completion(attempt.pending.identity,
            ImageViewportProviderFrameResult::failed(cause,
                loadFailure(resolvedSession(), ImageLoadFailureKind::Presentation, QString(),
                    diagnosticDetail)));
        return;
    }

    const QString diagnosticDetail = diagnostics.diagnosticDetail.isEmpty()
        ? QStringLiteral("requested current-detail static frame is unavailable")
        : diagnostics.diagnosticDetail;
    attempt.pending.completion(attempt.pending.identity,
        ImageViewportProviderFrameResult::failed(ImageSequenceProviderFailureCause::Decode,
            loadFailure(resolvedSession(), ImageLoadFailureKind::Decode, diagnostics.userMessage,
                diagnosticDetail)));
}

quint64 ImageViewportDecodeProviderSource::reserveStaticFrameAttemptId()
{
    if (m_nextStaticFrameAttemptId == 0) {
        m_nextStaticFrameAttemptId = 1;
    }
    quint64 attemptId = m_nextStaticFrameAttemptId;
    while (std::ranges::any_of(m_staticFrameAttempts,
        [attemptId](const StaticFrameAttempt& candidate) { return candidate.id == attemptId; })) {
        ++attemptId;
        if (attemptId == 0) {
            attemptId = 1;
        }
    }
    m_nextStaticFrameAttemptId = attemptId + 1;
    if (m_nextStaticFrameAttemptId == 0) {
        m_nextStaticFrameAttemptId = 1;
    }
    return attemptId;
}

std::optional<ImageViewportDecodeProviderSource::StaticFrameAttempt>
ImageViewportDecodeProviderSource::takeStaticFrameAttempt(
    quint64 attemptId, bool retainRefinementWork)
{
    const auto attempt = std::ranges::find_if(m_staticFrameAttempts,
        [attemptId](const StaticFrameAttempt& candidate) { return candidate.id == attemptId; });
    if (attempt == m_staticFrameAttempts.end()) {
        return std::nullopt;
    }

    StaticFrameAttempt taken = std::move(*attempt);
    m_staticFrameAttempts.erase(attempt);
    if (taken.initialDetailTimer != nullptr) {
        taken.initialDetailTimer->stop();
    }

    const auto work = std::ranges::find_if(
        m_staticRefinementWorks, [&taken](const StaticRefinementWork& candidate) {
            return candidate.workerUnitId == taken.refinementWorkerUnitId;
        });
    quint64 retainedWorkerUnitId = 0;
    if (work != m_staticRefinementWorks.end()) {
        std::erase(work->attemptIds, attemptId);
        work->retainWithoutSubscribers = work->retainWithoutSubscribers || retainRefinementWork;
        if (work->attemptIds.empty() && !work->retainWithoutSubscribers) {
            const quint64 workerUnitId = work->workerUnitId;
            m_staticRefinementWorks.erase(work);
            static_cast<void>(detachWorkerUnit(workerUnitId));
        } else if (work->attemptIds.empty()) {
            retainedWorkerUnitId = work->workerUnitId;
        }
    }
    if (retainedWorkerUnitId != 0) {
        discardRetainedStaticRefinementsExcept(retainedWorkerUnitId);
    }
    return taken;
}

void ImageViewportDecodeProviderSource::discardRetainedStaticRefinementsExcept(quint64 workerUnitId)
{
    std::vector<quint64> discardedWorkerUnitIds;
    std::erase_if(m_staticRefinementWorks,
        [workerUnitId, &discardedWorkerUnitIds](const StaticRefinementWork& work) {
            if (work.workerUnitId == workerUnitId || !work.retainWithoutSubscribers
                || !work.attemptIds.empty()) {
                return false;
            }
            discardedWorkerUnitIds.push_back(work.workerUnitId);
            return true;
        });
    for (quint64 discardedWorkerUnitId : discardedWorkerUnitIds) {
        static_cast<void>(detachWorkerUnit(discardedWorkerUnitId));
    }
}

void ImageViewportDecodeProviderSource::publishAnimationFrame(PendingFrame pending)
{
    [[maybe_unused]] const std::shared_ptr<ImageViewportDecodeProviderSource> lifetime
        = weak_from_this().lock();
    if (m_closed) {
        return;
    }
    if (!m_animation.has_value() || pending.request.frame < 0
        || pending.request.frame >= m_animation->metadata.frameCount()) {
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::failed(ImageSequenceProviderFailureCause::Decode,
                loadFailure(resolvedSession(), ImageLoadFailureKind::Decode, QString(),
                    QStringLiteral("requested animation frame is unavailable"))));
        retireAnimationFrameWork();
        return;
    }
    const AnimationState animation = *m_animation;
    const int requestedFrame = pending.request.frame;
    if (requestedFrame == 0) {
        finishAnimationFrame(
            pending, animation, requestedFrame, animation.runtime->frame(requestedFrame));
        retireAnimationFrameWork();
        return;
    }

    const quint64 workerUnitId = reserveWorkerUnit(pending.identity);
    m_activeAnimationFrameWork->workerUnitId = workerUnitId;
    const std::weak_ptr<ImageViewportDecodeProviderSource> weakSelf = weak_from_this();
    ImageWorkerTask task = m_dependencies.workerScheduler.run(
        this,
        [runtime = animation.runtime, requestedFrame]() { return runtime->frame(requestedFrame); },
        [weakSelf, workerUnitId, pending = std::move(pending), animation, requestedFrame](
            AnimationSourceFrameResult result) mutable {
            const std::shared_ptr<ImageViewportDecodeProviderSource> self = weakSelf.lock();
            if (self == nullptr || !self->detachWorkerUnit(workerUnitId)) {
                return;
            }
            const bool publishResult = !self->m_closed
                && self->m_activeAnimationFrameWork.has_value()
                && self->m_activeAnimationFrameWork->workerUnitId == workerUnitId
                && self->m_activeAnimationFrameWork->publishResult;
            if (publishResult) {
                self->finishAnimationFrame(pending, animation, requestedFrame, std::move(result));
            } else {
                result = std::unexpected(AnimationSourceFrameFailure {});
            }
            if (self->m_activeAnimationFrameWork.has_value()
                && self->m_activeAnimationFrameWork->workerUnitId == workerUnitId) {
                self->retireAnimationFrameWork();
            }
        });
    attachWorkerTask(workerUnitId, std::move(task));
}

void ImageViewportDecodeProviderSource::retireAnimationFrameWork()
{
    m_activeAnimationFrameWork.reset();
    if (!m_closed && !m_publishingFrames) {
        publishFrames();
    }
}

void ImageViewportDecodeProviderSource::finishAnimationFrame(const PendingFrame& pending,
    const AnimationState& animation, int requestedFrame, AnimationSourceFrameResult result)
{
    if (m_closed) {
        return;
    }
    [[maybe_unused]] const auto releaseFirstFrameWorkspace
        = qScopeGuard([runtime = animation.runtime, requestedFrame]() {
              if (requestedFrame == 0 && runtime != nullptr) {
                  runtime->releaseRetainedFirstFrameWorkspace();
              }
          });
    if (!result.has_value() || result->image.isNull()
        || result->image.size() != animation.metadata.sourceLogicalSize().toSize()) {
        QString errorString = result.has_value()
            ? QStringLiteral("requested animation frame is unavailable")
            : std::move(result.error().errorString);
        if (errorString.isEmpty()) {
            errorString = QStringLiteral("requested animation frame is unavailable");
        }
        const ImageSequenceProviderFailureCause failureCause = !result.has_value()
                && result.error().cause == AnimationSourceFrameFailureCause::ResourceLimitExceeded
            ? ImageSequenceProviderFailureCause::ResourceExhausted
            : ImageSequenceProviderFailureCause::Decode;
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::failed(failureCause,
                loadFailure(
                    resolvedSession(), ImageLoadFailureKind::Decode, errorString, errorString)));
        return;
    }

    const QVector<int> durations = animation.metadata.frameDurations();
    StaticDisplayImagePayload payload {
        animation.sourceIdentity,
        {},
        animation.metadata.sourceLogicalSize().toSize(),
        std::move(result->image),
        DisplayImageQuality::Exact,
        {},
        {},
        DisplayImagePreviewOrigin::None,
        StaticImageSourceDetailModel::FiniteRaster,
        animation.sourceRevision,
        DisplayImageRasterKind::TimedFrame,
    };
    pending.completion(pending.identity,
        ImageViewportProviderFrameResult::ready(std::move(payload),
            ImageSequenceProviderFrameEnvelope::timedFrame(requestedFrame,
                frameStartPosition(durations, requestedFrame), durations.at(requestedFrame)),
            animation.formatIdentifier));
}

quint64 ImageViewportDecodeProviderSource::reserveWorkerUnit(
    std::optional<ImageViewportProviderWorkIdentity> identity)
{
    if (m_nextWorkerUnitId == 0) {
        m_nextWorkerUnitId = 1;
    }

    quint64 workerUnitId = m_nextWorkerUnitId;
    while (std::ranges::any_of(m_workerUnits,
        [workerUnitId](const WorkerUnit& candidate) { return candidate.id == workerUnitId; })) {
        ++workerUnitId;
        if (workerUnitId == 0) {
            workerUnitId = 1;
        }
    }

    m_nextWorkerUnitId = workerUnitId + 1;
    if (m_nextWorkerUnitId == 0) {
        m_nextWorkerUnitId = 1;
    }
    m_workerUnits.push_back({ workerUnitId, std::move(identity), {} });
    return workerUnitId;
}

void ImageViewportDecodeProviderSource::attachWorkerTask(quint64 workerUnitId, ImageWorkerTask task)
{
    const auto unit = std::ranges::find_if(m_workerUnits,
        [workerUnitId](const WorkerUnit& candidate) { return candidate.id == workerUnitId; });
    if (unit == m_workerUnits.end()) {
        task.cancel();
        return;
    }
    unit->task = std::move(task);
}

bool ImageViewportDecodeProviderSource::detachWorkerUnit(quint64 workerUnitId)
{
    const auto unit = std::ranges::find_if(m_workerUnits,
        [workerUnitId](const WorkerUnit& candidate) { return candidate.id == workerUnitId; });
    if (unit == m_workerUnits.end()) {
        return false;
    }
    m_workerUnits.erase(unit);
    return true;
}

const ImageLoadSession& ImageViewportDecodeProviderSource::resolvedSession() const
{
    Q_ASSERT(m_session.has_value());
    return *m_session;
}
}
