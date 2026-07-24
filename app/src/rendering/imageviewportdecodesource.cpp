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
}

namespace kiriview {
ImageViewportDecodeProviderSource::ImageViewportDecodeProviderSource(
    ImageLoadSession session, ImageDecodeDependencies dependencies)
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
}

void ImageViewportDecodeProviderSource::close()
{
    if (m_closed) {
        return;
    }
    m_closed = true;
    m_decodeJob.cancel();
    for (ImageWorkerTask& task : m_workerTasks) {
        task.cancel();
    }
    m_workerTasks.clear();
    m_pendingMetadata.clear();
    m_pendingFrames.clear();
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
    if (!m_closed && request.matches(m_session.decodeRequest()) && displayImage.isValid()) {
        m_metadata = ImageSequenceProviderMetadata::still(displayImage.originalSize);
        m_staticDisplayImage = std::move(displayImage);
        publishMetadata();
        publishFrames();
    }
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
    if (!image.displayImage.isValid()) {
        finishFailure(ImageSequenceProviderFailureCause::Decode,
            loadFailure(m_session, ImageLoadFailureKind::Decode, QString(),
                QStringLiteral("decoded static image is not displayable")));
        return;
    }
    m_metadata = ImageSequenceProviderMetadata::still(image.displayImage.originalSize);
    m_staticDisplayImage = std::move(image.displayImage);
    publishMetadata();
    publishFrames();
}

void ImageViewportDecodeProviderSource::finishAnimationImage(
    ImageAnimationPlaybackRequest playbackRequest, QString sourceIdentity, QString formatIdentifier)
{
    const ImageWorkerScheduler scheduler = m_dependencies.workerScheduler;
    ImageAnimationPlaybackRequest scanRequest = playbackRequest;
    m_workerTasks.push_back(scheduler.run(
        this, [scanRequest]() mutable { return scanAnimation(std::move(scanRequest)); },
        [this, playbackRequest = std::move(playbackRequest),
            sourceIdentity = std::move(sourceIdentity),
            formatIdentifier = std::move(formatIdentifier)](AnimationScanResult result) mutable {
            if (m_closed) {
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
        }));
}

void ImageViewportDecodeProviderSource::finishFailure(
    ImageSequenceProviderFailureCause cause, ImageLoadFailure failure)
{
    m_failureCause = cause;
    m_failure = std::move(failure);
    for (PendingMetadata& pending : m_pendingMetadata) {
        pending.completion(
            pending.identity, ImageViewportProviderMetadataResult::failed(cause, *m_failure));
    }
    m_pendingMetadata.clear();
    for (PendingFrame& pending : m_pendingFrames) {
        pending.completion(
            pending.identity, ImageViewportProviderFrameResult::failed(cause, *m_failure));
    }
    m_pendingFrames.clear();
}

void ImageViewportDecodeProviderSource::publishMetadata()
{
    if (!m_metadata.has_value()) {
        return;
    }
    for (PendingMetadata& pending : m_pendingMetadata) {
        pending.completion(
            pending.identity, ImageViewportProviderMetadataResult::ready(*m_metadata));
    }
    m_pendingMetadata.clear();
}

void ImageViewportDecodeProviderSource::publishFrames()
{
    if (!m_staticDisplayImage.has_value() && !m_animation.has_value()) {
        return;
    }
    std::vector<PendingFrame> pending = std::move(m_pendingFrames);
    m_pendingFrames.clear();
    for (PendingFrame& frame : pending) {
        if (m_animation.has_value()) {
            publishAnimationFrame(std::move(frame));
        } else {
            publishStaticFrame(std::move(frame));
        }
    }
}

void ImageViewportDecodeProviderSource::publishStaticFrame(PendingFrame pending)
{
    if (!m_staticDisplayImage.has_value() || pending.request.frame != 0) {
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::failed(ImageSequenceProviderFailureCause::Decode,
                loadFailure(m_session, ImageLoadFailureKind::Decode, QString(),
                    QStringLiteral("requested static frame is unavailable"))));
        return;
    }

    const bool refinementDemand
        = pending.request.demand.currentPayloadQuality() != ImageViewportPayloadQuality::Unknown;
    if (refinementDemand && !m_decodeComplete
        && m_staticDisplayImage->quality == DisplayImageQuality::ThumbnailPreview) {
        m_pendingFrames.push_back(std::move(pending));
        return;
    }
    const std::shared_ptr<StaticImageDisplaySource> refinementSource
        = m_staticDisplayImage->refinementSource;
    if (!refinementDemand || refinementSource == nullptr
        || !refinementSource->supportsRasterDisplayRefinement()) {
        pending.completion(pending.identity,
            ImageViewportProviderFrameResult::ready(*m_staticDisplayImage,
                ImageSequenceProviderFrameEnvelope::stillFrame(), QString()));
        return;
    }

    QSize targetSize = pending.request.demand.targetDisplaySizePixels().toSize();
    if (targetSize.isEmpty()) {
        targetSize = m_staticDisplayImage->originalSize;
    }
    targetSize = targetSize.boundedTo(m_staticDisplayImage->originalSize);
    const ImageWorkerScheduler scheduler = m_dependencies.workerScheduler;
    const StaticDisplayImagePayload basis = *m_staticDisplayImage;
    const ImageLoadSession session = m_session;
    m_workerTasks.push_back(scheduler.run(
        this,
        [refinementSource, targetSize]() {
            return refinementSource->decodeRasterDisplayImage(targetSize);
        },
        [pending = std::move(pending), basis, session](
            StaticImageDisplayDecodeResult result) mutable {
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
            pending.completion(pending.identity,
                ImageViewportProviderFrameResult::ready(std::move(refined),
                    ImageSequenceProviderFrameEnvelope::stillFrame(), QString()));
        }));
}

void ImageViewportDecodeProviderSource::publishAnimationFrame(PendingFrame pending)
{
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
    m_workerTasks.push_back(m_dependencies.workerScheduler.run(
        this,
        [request = animation.playbackRequest, requestedFrame]() mutable {
            return decodeAnimationFrame(std::move(request), requestedFrame);
        },
        [pending = std::move(pending), animation, requestedFrame, session](
            AnimationFrameDecodeResult result) mutable {
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
        }));
}
}
