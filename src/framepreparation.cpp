#include "framepreparation_p.h"

#include "imageviewporthelpers_p.h"
#include "timingintervals_p.h"

#include <utility>

using namespace ImageViewportInternal;

namespace {

FramePreparation::ProviderFrameAdmissionResult providerFrameRejection(
    FramePreparation::ProviderFrameAdmissionResult::Cause cause,
    ImageViewport::RequestStatus status, QString diagnostic)
{
    return {
        cause,
        status,
        ImageViewport::RequestReason::PayloadRejection,
        std::move(diagnostic),
    };
}

FramePreparation::ProviderFrameAdmissionResult providerFrameError(
    FramePreparation::ProviderFrameAdmissionResult::Cause cause, QString diagnostic)
{
    return providerFrameRejection(
        cause, ImageViewport::RequestStatus::Error, std::move(diagnostic));
}

} // namespace

bool FramePreparation::ProviderFrameAdmissionResult::accepted() const
{
    return cause == Cause::Accepted;
}

bool FramePreparation::validateProviderStillMetadata(const ImageSequenceProviderMetadata& metadata)
{
    if (!metadata.isStill() || !metadata.isValid()) {
        return false;
    }

    const QSizeF size = metadata.logicalSize();
    if (!isAdmittedLogicalSizeComponent(size.width(), ImageSequenceLimits::maximumLogicalWidth())
        || !isAdmittedLogicalSizeComponent(
            size.height(), ImageSequenceLimits::maximumLogicalHeight())) {
        return false;
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    return width * height <= ImageSequenceLimits::maximumPixelsPerFrame();
}

bool FramePreparation::validateProviderTimedMetadata(const ImageSequenceProviderMetadata& metadata)
{
    if (!metadata.isTimedFrameList() || !metadata.isValid()) {
        return false;
    }

    const QSizeF size = metadata.logicalSize();
    if (!isAdmittedLogicalSizeComponent(size.width(), ImageSequenceLimits::maximumLogicalWidth())
        || !isAdmittedLogicalSizeComponent(
            size.height(), ImageSequenceLimits::maximumLogicalHeight())) {
        return false;
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width * height > ImageSequenceLimits::maximumPixelsPerFrame()) {
        return false;
    }

    const QVector<int> durations = metadata.frameDurations();
    if (durations.isEmpty()
        || durations.size() > ImageSequenceLimits::maximumTimedListFrameCount()) {
        return false;
    }

    qint64 totalDuration = 0;
    for (int duration : durations) {
        if (duration <= 0 || duration > ImageSequenceLimits::maximumFrameDuration()) {
            return false;
        }
        totalDuration += duration;
        if (totalDuration > ImageSequenceLimits::maximumTotalSequenceDuration()) {
            return false;
        }
    }

    return true;
}

FramePreparation::ProviderFrameAdmissionResult FramePreparation::admitProviderFrame(
    ImageFrame* frame, ImageSequenceProviderFrameMetadata metadata, const ProviderFrameState& state)
{
    using Cause = ProviderFrameAdmissionResult::Cause;

    if (!frame || !frame->isValid()) {
        return providerFrameError(
            Cause::InvalidFramePayload, QStringLiteral("provider frame payload is invalid"));
    }
    if (!state.metadataReady) {
        return providerFrameError(
            Cause::MetadataNotReady, QStringLiteral("provider frame metadata is not ready"));
    }
    if (frame->logicalSize() != state.logicalSize) {
        return providerFrameError(
            Cause::LogicalSizeMismatch, QStringLiteral("provider frame logical size mismatch"));
    }
    if (frame->payloadByteSize() <= 0) {
        return providerFrameError(Cause::InvalidPayloadByteSize,
            QStringLiteral("provider frame payload byte size is invalid"));
    }
    if (frame->payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
        return providerFrameRejection(Cause::PayloadTooLarge,
            ImageViewport::RequestStatus::Unsupported,
            QStringLiteral("provider frame payload exceeds maximumPayloadBytesPerFrame"));
    }
    if (!metadata.isValid()) {
        return providerFrameError(
            Cause::InvalidFrameMetadata, QStringLiteral("provider frame metadata is invalid"));
    }

    if (state.timedMetadata) {
        if (!metadata.isTimedFrame() || metadata.frame() != state.currentFrame) {
            if (!metadata.isTimedFrame()) {
                return providerFrameError(Cause::InvalidFrameMetadata,
                    QStringLiteral("provider frame metadata is invalid"));
            }
            return providerFrameError(Cause::ResolvedFrameMismatch,
                QStringLiteral("provider frame resolved frame mismatch"));
        }
        if (metadata.frameStartPosition()
            != providerFrameStartPosition(state.frameDurations, state.currentFrame)) {
            return providerFrameError(Cause::FrameStartMismatch,
                QStringLiteral("provider frame start position mismatch"));
        }
        if (metadata.frameDuration() != -1
            && metadata.frameDuration() != state.frameDurations.at(state.currentFrame)) {
            return providerFrameError(
                Cause::FrameDurationMismatch, QStringLiteral("provider frame duration mismatch"));
        }
        return {};
    }

    if (!metadata.isStillFrame()) {
        return providerFrameError(
            Cause::InvalidFrameMetadata, QStringLiteral("provider frame metadata is invalid"));
    }
    if (metadata.frame() != 0) {
        return providerFrameError(
            Cause::ResolvedFrameMismatch, QStringLiteral("provider frame resolved frame mismatch"));
    }
    return {};
}

int FramePreparation::providerFrameStartPosition(const QVector<int>& frameDurations, int frame)
{
    return TimingIntervals::fromFrameDurations(frameDurations).frameStartPosition(frame);
}

int FramePreparation::providerFrameIndexForPosition(
    const QVector<int>& frameDurations, int position)
{
    return TimingIntervals::fromFrameDurations(frameDurations).frameIndexForPosition(position);
}

int FramePreparation::totalDuration(const QVector<int>& frameDurations)
{
    return TimingIntervals::fromFrameDurations(frameDurations).totalDuration();
}

QString FramePreparation::boundedDiagnostic(QString diagnostic, QString fallback)
{
    QString selected = plainTextDiagnostic(redactDiagnosticDetails(
        diagnostic.isEmpty() ? std::move(fallback) : std::move(diagnostic)));
    const auto scalars = selected.toUcs4();
    const int maximumLength = ImageSequenceLimits::maximumDiagnosticStringLength();
    if (scalars.size() <= maximumLength) {
        return selected;
    }
    QString bounded;
    bounded.reserve(selected.size());
    for (int i = 0; i < maximumLength; ++i) {
        const char32_t scalar = static_cast<char32_t>(scalars.at(i));
        bounded += QString::fromUcs4(&scalar, 1);
    }
    return bounded;
}
