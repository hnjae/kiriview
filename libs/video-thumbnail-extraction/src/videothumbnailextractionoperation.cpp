// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "videothumbnailextraction_p.h"

#include <QImage>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QtTypes>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace std::chrono_literals;

using kiriview::detail::VideoThumbnailBackend;
using kiriview::detail::VideoThumbnailBackendError;
using kiriview::detail::VideoThumbnailBackendFrame;
using kiriview::detail::VideoThumbnailBackendMediaFacts;
using kiriview::detail::VideoThumbnailBackendMediaStatus;
using kiriview::detail::VideoThumbnailDeadline;
using kiriview::detail::VideoThumbnailEmbeddedImages;
using kiriview::detail::VideoThumbnailExtractionJobControl;
using kiriview::detail::VideoThumbnailImageAdmission;
using kiriview::detail::VideoThumbnailImageAdmissionStatus;
using kiriview::detail::VideoThumbnailRuntimeDependencies;

constexpr auto extractionDeadline = 10s;

auto sourcePixelBytes(const QImage& image) -> qsizetype
{
    if (image.isNull()) {
        return 0;
    }
    const qsizetype bytes = image.sizeInBytes();
    if (bytes <= 0 || bytes > kiriview::VideoThumbnailExtractionLimits::maximumWorkingBytes) {
        return kiriview::VideoThumbnailExtractionLimits::maximumWorkingBytes + 1;
    }
    return bytes;
}

auto combinedSourcePixelBytes(const VideoThumbnailEmbeddedImages& images) -> qsizetype
{
    const qsizetype coverBytes = sourcePixelBytes(images.coverArt);
    const qsizetype thumbnailBytes = sourcePixelBytes(images.thumbnail);
    if (!images.coverArt.isNull() && !images.thumbnail.isNull()
        && images.coverArt.cacheKey() == images.thumbnail.cacheKey()) {
        return std::max(coverBytes, thumbnailBytes);
    }
    const qsizetype maximum = kiriview::VideoThumbnailExtractionLimits::maximumWorkingBytes;
    if (coverBytes > maximum || thumbnailBytes > maximum - coverBytes) {
        return maximum + 1;
    }
    return coverBytes + thumbnailBytes;
}

struct BackendErrorEvent
{
    VideoThumbnailBackendError error = VideoThumbnailBackendError::Other;
};

struct DeadlineExpiredEvent
{
};

using ExtractionEvent
    = std::variant<VideoThumbnailBackendMediaFacts, qint64, VideoThumbnailBackendFrame,
        VideoThumbnailEmbeddedImages, BackendErrorEvent, DeadlineExpiredEvent>;

class VideoThumbnailExtractionOperation final : public QObject
{
public:
    VideoThumbnailExtractionOperation(QObject* receiver,
        kiriview::VideoThumbnailExtractionRequest request,
        kiriview::VideoThumbnailExtractionCallback completion,
        VideoThumbnailRuntimeDependencies dependencies,
        const std::shared_ptr<VideoThumbnailExtractionJobControl>& control)
        : receiver_(receiver)
        , request_(std::move(request))
        , completion_(std::move(completion))
        , dependencies_(std::move(dependencies))
        , control_(control)
    {
        receiverDestroyed_
            = QObject::connect(receiver, &QObject::destroyed, this, [this]() { cancel(); });
    }

    VideoThumbnailExtractionOperation(const VideoThumbnailExtractionOperation&) = delete;
    auto operator=(const VideoThumbnailExtractionOperation&)
        -> VideoThumbnailExtractionOperation& = delete;
    VideoThumbnailExtractionOperation(VideoThumbnailExtractionOperation&&) = delete;
    auto operator=(VideoThumbnailExtractionOperation&&)
        -> VideoThumbnailExtractionOperation& = delete;

    ~VideoThumbnailExtractionOperation() override
    {
        retireControl();
        cleanup();
    }

    void start()
    {
        if (phase_ != Phase::Active) {
            return;
        }
        if (!kiriview::detail::isVideoThumbnailExtractionRequestValid(request_)) {
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::InvalidRequest));
            return;
        }

        backend_ = dependencies_.backendFactory ? dependencies_.backendFactory() : nullptr;
        deadline_ = dependencies_.deadlineFactory ? dependencies_.deadlineFactory() : nullptr;
        dependencies_ = {};
        if (!backend_ || !deadline_) {
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::BackendFailure));
            return;
        }

        backend_->setCallbacks({
            [this](VideoThumbnailBackendMediaFacts facts) { enqueue(facts); },
            [this](qint64 positionMsec) { enqueue(positionMsec); },
            [this](VideoThumbnailBackendFrame frame) { enqueue(std::move(frame)); },
            [this](VideoThumbnailEmbeddedImages images) { enqueue(std::move(images)); },
            [this](VideoThumbnailBackendError error, const QString&) {
                enqueue(BackendErrorEvent { error });
            },
        });
        if (phase_ != Phase::Active) {
            return;
        }
        deadline_->start(std::chrono::duration_cast<std::chrono::milliseconds>(extractionDeadline),
            [this]() { enqueue(DeadlineExpiredEvent {}); });
        if (phase_ != Phase::Active) {
            return;
        }
        backend_->setSource(request_.sourceUrl);
    }

    void cancel() noexcept
    {
        if (phase_ == Phase::Inactive) {
            return;
        }
        phase_ = Phase::Inactive;
        pendingEvents_.clear();
        terminalResult_.reset();
        completion_ = {};
        retireControl();
        cleanup();
        deleteLater();
    }

private:
    enum class Phase : std::uint8_t {
        Active,
        DeliveryPending,
        Inactive,
    };

    void enqueue(ExtractionEvent event)
    {
        if (phase_ != Phase::Active) {
            return;
        }
        pendingEvents_.push_back(std::move(event));
        if (dispatching_) {
            return;
        }

        dispatching_ = true;
        while (phase_ == Phase::Active && !pendingEvents_.empty()) {
            ExtractionEvent next = std::move(pendingEvents_.front());
            pendingEvents_.pop_front();
            dispatch(std::move(next));
        }
        if (phase_ != Phase::Active) {
            pendingEvents_.clear();
        }
        dispatching_ = false;
    }

    void dispatch(ExtractionEvent event)
    {
        std::visit(
            [this](auto&& payload) {
                using Event = std::remove_cvref_t<decltype(payload)>;
                if constexpr (std::is_same_v<Event, VideoThumbnailBackendMediaFacts>) {
                    handleMediaFacts(payload);
                } else if constexpr (std::is_same_v<Event, qint64>) {
                    handlePosition(payload);
                } else if constexpr (std::is_same_v<Event, VideoThumbnailBackendFrame>) {
                    handleFrame(std::forward<decltype(payload)>(payload));
                } else if constexpr (std::is_same_v<Event, VideoThumbnailEmbeddedImages>) {
                    handleMetadata(std::forward<decltype(payload)>(payload));
                } else if constexpr (std::is_same_v<Event, BackendErrorEvent>) {
                    finish(kiriview::detail::makeVideoThumbnailBackendFailureResult(payload.error));
                } else if constexpr (std::is_same_v<Event, DeadlineExpiredEvent>) {
                    finish(kiriview::detail::makeVideoThumbnailFailureResult(
                        kiriview::VideoThumbnailExtractionFailureCause::TimedOut));
                }
            },
            std::move(event));
    }

    void handleMediaFacts(const VideoThumbnailBackendMediaFacts& facts)
    {
        switch (facts.status) {
        case VideoThumbnailBackendMediaStatus::Pending:
            return;
        case VideoThumbnailBackendMediaStatus::EndOfMedia:
            finishNoRepresentativeImage();
            return;
        case VideoThumbnailBackendMediaStatus::Invalid:
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::BackendFailure));
            return;
        case VideoThumbnailBackendMediaStatus::Ready:
            break;
        }

        if (frameExtractionStarted_) {
            return;
        }
        frameExtractionStarted_ = true;
        if (!facts.hasVideo) {
            finishNoRepresentativeImage();
            return;
        }

        if (facts.seekable && facts.durationMsec > 0) {
            candidatePositions_ = candidatePositions(facts.durationMsec);
            if (!candidatePositions_.empty()) {
                seekToNextCandidate();
                return;
            }
        }
        awaitingFrame_ = true;
        backend_->play();
    }

    void handlePosition(qint64) { }

    void handleFrame(VideoThumbnailBackendFrame frame)
    {
        if (!frameExtractionStarted_ || !awaitingFrame_) {
            return;
        }

        const VideoThumbnailImageAdmissionStatus sizeStatus
            = kiriview::detail::admitVideoThumbnailFrameSize(frame.pixelSize);
        switch (sizeStatus) {
        case VideoThumbnailImageAdmissionStatus::Ready:
            break;
        case VideoThumbnailImageAdmissionStatus::ResourceLimit:
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::ResourceLimit));
            return;
        case VideoThumbnailImageAdmissionStatus::ConversionFailure:
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::BackendFailure));
            return;
        case VideoThumbnailImageAdmissionStatus::Missing:
            advanceAfterUnusableFrame();
            return;
        }

        if (!frame.materialize) {
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::BackendFailure));
            return;
        }
        auto materialize = std::move(frame.materialize);
        QImage image = materialize();
        materialize = {};
        if (image.isNull()) {
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::BackendFailure));
            return;
        }

        VideoThumbnailImageAdmission admitted = kiriview::detail::admitVideoThumbnailImage(
            image, request_.maximumLongEdge, sourcePixelBytes(image));
        switch (admitted.status) {
        case VideoThumbnailImageAdmissionStatus::Ready:
            finish(kiriview::detail::makeVideoThumbnailReadyResult(std::move(admitted.image)));
            return;
        case VideoThumbnailImageAdmissionStatus::ResourceLimit:
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::ResourceLimit));
            return;
        case VideoThumbnailImageAdmissionStatus::ConversionFailure:
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::BackendFailure));
            return;
        case VideoThumbnailImageAdmissionStatus::Missing:
            break;
        }

        advanceAfterUnusableFrame();
    }

    void advanceAfterUnusableFrame()
    {
        if (!candidatePositions_.empty()) {
            ++candidateIndex_;
            seekToNextCandidate();
        }
    }

    void handleMetadata(VideoThumbnailEmbeddedImages images)
    {
        const bool hasCover = !images.coverArt.isNull();
        const bool hasThumbnail = !images.thumbnail.isNull();
        VideoThumbnailImageAdmission cover = kiriview::detail::admitVideoThumbnailImage(
            images.coverArt, request_.maximumLongEdge, combinedSourcePixelBytes(images));
        if (cover.status == VideoThumbnailImageAdmissionStatus::Ready) {
            finish(kiriview::detail::makeVideoThumbnailReadyResult(std::move(cover.image)));
            return;
        }

        images.coverArt = {};
        VideoThumbnailImageAdmission thumbnail = kiriview::detail::admitVideoThumbnailImage(
            images.thumbnail, request_.maximumLongEdge, sourcePixelBytes(images.thumbnail));
        if (thumbnail.status == VideoThumbnailImageAdmissionStatus::Ready) {
            finish(kiriview::detail::makeVideoThumbnailReadyResult(std::move(thumbnail.image)));
            return;
        }

        images.thumbnail = {};
        if ((hasCover && cover.status == VideoThumbnailImageAdmissionStatus::ResourceLimit)
            || (hasThumbnail
                && thumbnail.status == VideoThumbnailImageAdmissionStatus::ResourceLimit)) {
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::ResourceLimit));
            return;
        }
        if (cover.status == VideoThumbnailImageAdmissionStatus::ConversionFailure
            || thumbnail.status == VideoThumbnailImageAdmissionStatus::ConversionFailure) {
            finish(kiriview::detail::makeVideoThumbnailFailureResult(
                kiriview::VideoThumbnailExtractionFailureCause::BackendFailure));
        }
    }

    static auto candidatePositions(qint64 durationMsec) -> std::vector<qint64>
    {
        constexpr std::array<std::pair<qint64, qint64>, 5> fractions {
            std::pair { 1, 3 },
            std::pair { 2, 3 },
            std::pair { 1, 10 },
            std::pair { 9, 10 },
            std::pair { 1, 2 },
        };
        std::vector<qint64> positions;
        positions.reserve(fractions.size());
        for (const auto& [numerator, denominator] : fractions) {
            const qint64 position = std::clamp((durationMsec / denominator) * numerator
                    + ((durationMsec % denominator) * numerator) / denominator,
                qint64(0), durationMsec);
            if (std::ranges::find(positions, position) == positions.end()) {
                positions.push_back(position);
            }
        }
        return positions;
    }

    void seekToNextCandidate()
    {
        if (phase_ != Phase::Active) {
            return;
        }
        if (candidateIndex_ >= candidatePositions_.size()) {
            finishNoRepresentativeImage();
            return;
        }
        awaitingFrame_ = true;
        backend_->pause();
        if (phase_ != Phase::Active) {
            return;
        }
        backend_->setPosition(candidatePositions_[candidateIndex_]);
        if (phase_ != Phase::Active) {
            return;
        }
        backend_->play();
    }

    void finishNoRepresentativeImage()
    {
        finish(kiriview::detail::makeVideoThumbnailFailureResult(
            kiriview::VideoThumbnailExtractionFailureCause::NoRepresentativeImage));
    }

    void finish(kiriview::VideoThumbnailExtractionResult result)
    {
        if (phase_ != Phase::Active) {
            return;
        }
        phase_ = Phase::DeliveryPending;
        terminalResult_ = std::move(result);
        pendingEvents_.clear();
        cleanup();

        QMetaObject::invokeMethod(this, [this]() { deliver(); }, Qt::QueuedConnection);
    }

    void deliver()
    {
        if (phase_ != Phase::DeliveryPending || receiver_.isNull() || !terminalResult_.has_value()
            || !completion_) {
            cancel();
            return;
        }

        auto completion = std::move(completion_);
        auto result = std::move(*terminalResult_);
        terminalResult_.reset();
        phase_ = Phase::Inactive;
        retireControl();
        QObject::disconnect(receiverDestroyed_);
        receiver_.clear();
        deleteLater();
        completion(std::move(result));
    }

    void retireControl() noexcept
    {
        if (const auto control = control_.lock()) {
            control->active = false;
            control->cancel = {};
        }
        control_.reset();
    }

    void cleanup() noexcept
    {
        if (deadline_) {
            deadline_->stop();
            retiredDeadline_ = std::move(deadline_);
        }
        if (backend_) {
            backend_->stop();
            backend_->setCallbacks({});
            retiredBackend_ = std::move(backend_);
        }
        candidatePositions_.clear();
        awaitingFrame_ = false;
    }

    QPointer<QObject> receiver_;
    kiriview::VideoThumbnailExtractionRequest request_;
    kiriview::VideoThumbnailExtractionCallback completion_;
    VideoThumbnailRuntimeDependencies dependencies_;
    std::weak_ptr<VideoThumbnailExtractionJobControl> control_;
    QMetaObject::Connection receiverDestroyed_;
    std::unique_ptr<VideoThumbnailBackend> backend_;
    std::unique_ptr<VideoThumbnailDeadline> deadline_;
    std::unique_ptr<VideoThumbnailBackend> retiredBackend_;
    std::unique_ptr<VideoThumbnailDeadline> retiredDeadline_;
    std::deque<ExtractionEvent> pendingEvents_;
    std::optional<kiriview::VideoThumbnailExtractionResult> terminalResult_;
    std::vector<qint64> candidatePositions_;
    std::size_t candidateIndex_ = 0;
    Phase phase_ = Phase::Active;
    bool dispatching_ = false;
    bool frameExtractionStarted_ = false;
    bool awaitingFrame_ = false;
};

} // namespace

namespace kiriview::detail {

void startVideoThumbnailExtractionOperation(QObject* receiver,
    VideoThumbnailExtractionRequest request, VideoThumbnailExtractionCallback completion,
    VideoThumbnailRuntimeDependencies dependencies,
    const std::shared_ptr<VideoThumbnailExtractionJobControl>& control)
{
    auto* operation = new VideoThumbnailExtractionOperation(
        receiver, std::move(request), std::move(completion), std::move(dependencies), control);
    control->cancel = [operation = QPointer<VideoThumbnailExtractionOperation>(operation)]() {
        if (operation) {
            operation->cancel();
        }
    };

    QMetaObject::invokeMethod(
        operation,
        [operation = QPointer<VideoThumbnailExtractionOperation>(operation)]() {
            if (operation) {
                operation->start();
            }
        },
        Qt::QueuedConnection);
}

} // namespace kiriview::detail
