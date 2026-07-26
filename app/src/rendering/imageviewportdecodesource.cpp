// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportdecodesource.h"

#include "presentation/imageanimationpolicy.h"

#include <ImageViewport/imagesequence.h>

#include <QSizeF>
#include <algorithm>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
struct AnimationScanResult
{
    bool ready = false;
    QSize logicalSize;
    QVector<int> durations;
    int repeatCount = 0;
    QString errorString;
};

struct AnimationFrameDecodeResult
{
    bool ready = false;
    QImage image;
    QString errorString;
};

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
    return loadFailure(session, kiriview::ImageLoadFailureKind::Decode, failure.errorString,
        failure.diagnosticDetail, failure.route, failure.operation, failure.retryable);
}

AnimationScanResult scanAnimation(kiriview::ImageAnimationPlaybackRequest request)
{
    std::unique_ptr<kiriview::ImageAnimationPlaybackSource> source
        = kiriview::makeImageAnimationPlaybackSource(std::move(request));
    if (source == nullptr) {
        return {};
    }
    kiriview::ImageAnimationPlaybackOpenResult opened = source->open();
    if (opened.status != kiriview::ImageAnimationPlaybackOpenStatus::Success
        || opened.firstFrame.isNull()) {
        return { false, {}, {}, 0, std::move(opened.errorString) };
    }

    AnimationScanResult result;
    result.logicalSize = opened.firstFrame.size();
    result.repeatCount = opened.loopCount;
    result.durations.append(kiriview::normalizedAnimationFrameDelay(opened.firstFrameDelay));
    bool hasMoreFrames = opened.sourceHasMoreFrames;
    while (hasMoreFrames && result.durations.size() < ImageSequenceLimits::maximumFrameCount()) {
        kiriview::ImageAnimationPlaybackReadResult frame = source->readNextFrame();
        switch (frame.status) {
        case kiriview::ImageAnimationPlaybackReadStatus::Frame:
            if (frame.frame.image.isNull() || frame.frame.image.size() != result.logicalSize) {
                return {};
            }
            result.durations.append(kiriview::normalizedAnimationFrameDelay(frame.frame.delay));
            hasMoreFrames = frame.sourceHasMoreFrames;
            break;
        case kiriview::ImageAnimationPlaybackReadStatus::End:
            hasMoreFrames = false;
            break;
        case kiriview::ImageAnimationPlaybackReadStatus::Error:
            result.errorString = std::move(frame.errorString);
            return result;
        }
    }
    if (hasMoreFrames || result.durations.size() < 2) {
        return result;
    }
    result.ready = true;
    return result;
}

AnimationFrameDecodeResult decodeAnimationFrame(
    kiriview::ImageAnimationPlaybackRequest request, int targetFrame)
{
    std::unique_ptr<kiriview::ImageAnimationPlaybackSource> source
        = kiriview::makeImageAnimationPlaybackSource(std::move(request));
    if (source == nullptr || targetFrame < 0) {
        return {};
    }
    kiriview::ImageAnimationPlaybackOpenResult opened = source->open();
    if (opened.status != kiriview::ImageAnimationPlaybackOpenStatus::Success
        || opened.firstFrame.isNull()) {
        return { false, {}, std::move(opened.errorString) };
    }
    if (targetFrame == 0) {
        return { true, std::move(opened.firstFrame), {} };
    }

    for (int frameIndex = 1; frameIndex <= targetFrame; ++frameIndex) {
        kiriview::ImageAnimationPlaybackReadResult frame = source->readNextFrame();
        if (frame.status == kiriview::ImageAnimationPlaybackReadStatus::Error) {
            return { false, {}, std::move(frame.errorString) };
        }
        if (frame.status != kiriview::ImageAnimationPlaybackReadStatus::Frame) {
            return {};
        }
        if (frameIndex == targetFrame) {
            return { !frame.frame.image.isNull(), std::move(frame.frame.image), {} };
        }
    }
    return {};
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
    return payload.isValid() && payload.quality != kiriview::DisplayImageQuality::ThumbnailPreview
        && payload.previewOrigin == kiriview::DisplayImagePreviewOrigin::None;
}

bool isProvisionalPreviewPayload(const kiriview::StaticDisplayImagePayload& payload)
{
    return payload.isValid() && payload.quality == kiriview::DisplayImageQuality::ThumbnailPreview
        && payload.previewOrigin != kiriview::DisplayImagePreviewOrigin::None;
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

    const qint64 candidatePixelCount = qint64(candidate.image.width()) * candidate.image.height();
    const qint64 currentPixelCount = qint64(current.image.width()) * current.image.height();
    return candidatePixelCount >= currentPixelCount;
}

bool hasNoAcceptedCurrentPayload(const ImageSequenceProviderDisplayDemand& demand)
{
    return demand.currentPayloadQuality() == ImageViewportPayloadQuality::Unknown
        && demand.currentPayloadExactness() == ImageViewportPayloadExactness::Unknown;
}
}

namespace kiriview {
ImageViewportDecodeProviderSource::ImageViewportDecodeProviderSource(ImageLoadSession session,
    ImageDecodeDependencies dependencies,
    std::optional<StaticDisplayImagePayload> authoritativeSeed)
    : m_session(std::move(session))
    , m_dependencies(imageDecodeDependenciesWithDefaults(std::move(dependencies)))
    , m_decodeJob(this, m_dependencies,
          ImageDecodeJob::Callbacks {
              [this](const ImageDecodeRequest& request, DecodedImageResult result) {
                  finishDecode(request, std::move(result));
              },
              [this](const ImageDecodeRequest& request, const QString& errorString) {
                  finishDataLoadError(request, errorString);
              },
              [this](const ImageDecodeRequest& request, StaticDisplayImagePayload displayImage) {
                  finishThumbnail(request, std::move(displayImage));
              },
          })
{
    if (!authoritativeSeed.has_value() || !isAuthoritativeStaticPayload(*authoritativeSeed)) {
        return;
    }

    m_embeddedMetadata = authoritativeSeed->embeddedMetadata;
    m_metadata = ImageSequenceProviderMetadata::still(authoritativeSeed->originalSize);
    m_authoritativeStaticImage = std::move(*authoritativeSeed);
    m_decodeStarted = true;
    m_decodeComplete = true;
}

ImageViewportDecodeProviderSource::~ImageViewportDecodeProviderSource() { close(); }

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

    std::vector<ImageWorkerTask> detachedTasks;
    auto unit = m_workerUnits.begin();
    while (unit != m_workerUnits.end()) {
        if (!unit->identity.has_value() || !containsToken(tokens, unit->identity->requestToken)) {
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
    if (m_closed || m_decodeStarted || m_metadata.has_value() || m_failure.has_value()) {
        return;
    }
    m_decodeStarted = true;
    m_decodeJob.start(m_session.decodeRequest());
}

void ImageViewportDecodeProviderSource::finishDecode(
    const ImageDecodeRequest& request, DecodedImageResult result)
{
    if (m_closed || !request.matches(m_session.decodeRequest())) {
        return;
    }
    m_decodeComplete = true;
    if (!result) {
        finishFailure(
            ImageSequenceProviderFailureCause::Decode, loadFailure(m_session, result.error()));
        return;
    }
    finishDecodedImage(std::move(*result));
}

void ImageViewportDecodeProviderSource::finishDataLoadError(
    const ImageDecodeRequest& request, const QString& errorString)
{
    if (m_closed || !request.matches(m_session.decodeRequest())) {
        return;
    }
    m_decodeComplete = true;
    finishFailure(ImageSequenceProviderFailureCause::SourceAccess,
        loadFailure(m_session, ImageLoadFailureKind::DataLoad, errorString, errorString));
}

void ImageViewportDecodeProviderSource::finishThumbnail(
    const ImageDecodeRequest& request, StaticDisplayImagePayload displayImage)
{
    if (m_closed || m_decodeComplete || !request.matches(m_session.decodeRequest())
        || m_failure.has_value() || m_authoritativeStaticImage.has_value()
        || m_animation.has_value() || m_provisionalPreview.has_value()
        || !isProvisionalPreviewPayload(displayImage)) {
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
                finishAnimationImage(
                    readerAnimationPlaybackRequest(std::move(decoded.data), decoded.format),
                    std::move(decoded.sourceIdentity), QString::fromLatin1(decoded.format));
            } else if constexpr (std::is_same_v<Image, ApngAnimationImage>) {
                finishAnimationImage(apngAnimationPlaybackRequest(std::move(decoded.data)),
                    std::move(decoded.sourceIdentity), QStringLiteral("apng"));
            } else if constexpr (std::is_same_v<Image, WebPAnimationImage>) {
                finishAnimationImage(webpAnimationPlaybackRequest(std::move(decoded.data)),
                    std::move(decoded.sourceIdentity), QStringLiteral("webp"));
            } else if constexpr (std::is_same_v<Image, JxlAnimationImage>) {
                finishAnimationImage(jxlAnimationPlaybackRequest(std::move(decoded.data)),
                    std::move(decoded.sourceIdentity), QStringLiteral("jxl"));
            } else if constexpr (std::is_same_v<Image, HeifSequenceAnimationImage>) {
                finishAnimationImage(heifSequenceAnimationPlaybackRequest(std::move(decoded.data)),
                    std::move(decoded.sourceIdentity), QStringLiteral("heif"));
            }
        },
        std::move(image));
}

void ImageViewportDecodeProviderSource::finishStaticImage(StaticDecodedImage image)
{
    if (!isAuthoritativeStaticPayload(image.displayImage)) {
        finishFailure(ImageSequenceProviderFailureCause::Decode,
            loadFailure(m_session, ImageLoadFailureKind::Decode, QString(),
                QStringLiteral("decoded static image is not displayable")));
        return;
    }
    m_provisionalPreview.reset();
    m_animation.reset();
    m_metadata = ImageSequenceProviderMetadata::still(image.displayImage.originalSize);
    m_authoritativeStaticImage = std::move(image.displayImage);
    publishMetadata();
    publishFrames();
}

void ImageViewportDecodeProviderSource::finishAnimationImage(
    ImageAnimationPlaybackRequest playbackRequest, QString sourceIdentity, QString formatIdentifier)
{
    m_provisionalPreview.reset();
    m_authoritativeStaticImage.reset();
    const ImageWorkerScheduler scheduler = m_dependencies.workerScheduler;
    ImageAnimationPlaybackRequest scanRequest = playbackRequest;
    const quint64 workerUnitId = reserveWorkerUnit();
    ImageWorkerTask task = scheduler.run(
        this, [scanRequest]() mutable { return scanAnimation(std::move(scanRequest)); },
        [this, workerUnitId, playbackRequest = std::move(playbackRequest),
            sourceIdentity = std::move(sourceIdentity),
            formatIdentifier = std::move(formatIdentifier)](AnimationScanResult result) mutable {
            if (!detachWorkerUnit(workerUnitId) || m_closed) {
                return;
            }
            if (!result.ready) {
                finishFailure(ImageSequenceProviderFailureCause::Decode,
                    loadFailure(m_session, ImageLoadFailureKind::Decode, result.errorString,
                        result.errorString.isEmpty()
                            ? QStringLiteral("animation timing metadata is invalid")
                            : result.errorString,
                        DecodedImageFailureRoute::Unknown,
                        DecodedImageFailureOperation::DecodeAnimationOpen));
                return;
            }
            ImageSequenceProviderMetadata metadata = ImageSequenceProviderMetadata::timedFrameList(
                result.logicalSize, std::move(result.durations));
            metadata.setAuthoredAnimationFacts(authoredAnimationFacts(result.repeatCount));
            m_metadata = metadata;
            m_animation = AnimationState {
                std::move(playbackRequest),
                std::move(metadata),
                std::move(sourceIdentity),
                std::move(formatIdentifier),
            };
            publishMetadata();
            publishFrames();
        });
    attachWorkerTask(workerUnitId, std::move(task));
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

    while (!m_closed && !m_pendingFrames.empty()) {
        PendingFrame frame = std::move(m_pendingFrames.front());
        m_pendingFrames.erase(m_pendingFrames.begin());
        if (m_animation.has_value()) {
            publishAnimationFrame(std::move(frame));
        } else {
            publishStaticFrame(std::move(frame));
        }
    }
}

void ImageViewportDecodeProviderSource::publishProvisionalFrames()
{
    if (m_closed || !m_provisionalPreview.has_value() || m_authoritativeStaticImage.has_value()
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
                loadFailure(m_session, ImageLoadFailureKind::Decode, QString(),
                    QStringLiteral("requested static frame is unavailable"))));
        return;
    }

    const bool refinementDemand = !hasNoAcceptedCurrentPayload(pending.request.demand);
    const std::shared_ptr<StaticImageDisplaySource> refinementSource
        = m_authoritativeStaticImage->refinementSource;
    if (!refinementDemand || refinementSource == nullptr
        || !refinementSource->supportsRasterDisplayRefinement()) {
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::ready(*m_authoritativeStaticImage,
                ImageSequenceProviderFrameEnvelope::stillFrame(), QString()));
        return;
    }

    QSize targetSize = pending.request.demand.targetDisplaySizePixels().toSize();
    if (targetSize.isEmpty()) {
        targetSize = m_authoritativeStaticImage->originalSize;
    }
    targetSize = targetSize.boundedTo(m_authoritativeStaticImage->originalSize);
    const ImageWorkerScheduler scheduler = m_dependencies.workerScheduler;
    const StaticDisplayImagePayload basis = *m_authoritativeStaticImage;
    const ImageLoadSession session = m_session;
    const quint64 workerUnitId = reserveWorkerUnit(pending.identity);
    ImageWorkerTask task = scheduler.run(
        this,
        [refinementSource, targetSize]() {
            return refinementSource->decodeRasterDisplayImage(targetSize);
        },
        [this, workerUnitId, pending = std::move(pending), basis, session](
            StaticImageDisplayDecodeResult result) mutable {
            if (!detachWorkerUnit(workerUnitId) || m_closed) {
                return;
            }
            if (result.image.isNull()) {
                pending.completion(pending.identity,
                    ImageViewportProviderFrameResult::failed(
                        ImageSequenceProviderFailureCause::Decode,
                        loadFailure(session, ImageLoadFailureKind::Decode,
                            result.diagnostics.userMessage, result.diagnostics.diagnosticDetail)));
                return;
            }
            StaticDisplayImagePayload refined = basis;
            refined.image = std::move(result.image);
            refined.quality = refined.image.size() == refined.originalSize
                ? DisplayImageQuality::Exact
                : DisplayImageQuality::BoundedDetail;
            refined.previewOrigin = DisplayImagePreviewOrigin::None;
            if (isReusableAuthoritativeUpgrade(refined, *m_authoritativeStaticImage)) {
                m_authoritativeStaticImage = refined;
            }
            pending.completion(pending.identity,
                ImageViewportProviderFrameResult::ready(std::move(refined),
                    ImageSequenceProviderFrameEnvelope::stillFrame(), QString()));
        });
    attachWorkerTask(workerUnitId, std::move(task));
}

void ImageViewportDecodeProviderSource::publishAnimationFrame(PendingFrame pending)
{
    if (m_closed) {
        return;
    }
    if (!m_animation.has_value() || pending.request.frame < 0
        || pending.request.frame >= m_animation->metadata.frameCount()) {
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::failed(ImageSequenceProviderFailureCause::Decode,
                loadFailure(m_session, ImageLoadFailureKind::Decode, QString(),
                    QStringLiteral("requested animation frame is unavailable"))));
        return;
    }
    const AnimationState animation = *m_animation;
    const int requestedFrame = pending.request.frame;
    const ImageLoadSession session = m_session;
    const quint64 workerUnitId = reserveWorkerUnit(pending.identity);
    ImageWorkerTask task = m_dependencies.workerScheduler.run(
        this,
        [request = animation.playbackRequest, requestedFrame]() mutable {
            return decodeAnimationFrame(std::move(request), requestedFrame);
        },
        [this, workerUnitId, pending = std::move(pending), animation, requestedFrame, session](
            AnimationFrameDecodeResult result) mutable {
            if (!detachWorkerUnit(workerUnitId) || m_closed) {
                return;
            }
            if (!result.ready) {
                pending.completion(pending.identity,
                    ImageViewportProviderFrameResult::failed(
                        ImageSequenceProviderFailureCause::Decode,
                        loadFailure(session, ImageLoadFailureKind::Decode, result.errorString,
                            result.errorString)));
                return;
            }
            const QVector<int> durations = animation.metadata.frameDurations();
            StaticDisplayImagePayload payload {
                animation.sourceIdentity,
                {},
                animation.metadata.sourceLogicalSize().toSize(),
                std::move(result.image),
                DisplayImageQuality::Exact,
                {},
                {},
                DisplayImagePreviewOrigin::None,
            };
            pending.completion(pending.identity,
                ImageViewportProviderFrameResult::ready(std::move(payload),
                    ImageSequenceProviderFrameEnvelope::timedFrame(requestedFrame,
                        frameStartPosition(durations, requestedFrame),
                        durations.at(requestedFrame)),
                    animation.formatIdentifier));
        });
    attachWorkerTask(workerUnitId, std::move(task));
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
}
