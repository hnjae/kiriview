#include "framepreparation_p.h"

#include "imageviewporthelpers_p.h"

using namespace ImageViewportInternal;

bool FramePreparation::validateProviderStillMetadata(const ImageSequenceProviderMetadata &metadata)
{
    if (!metadata.isStill() || !metadata.isValid()) {
        return false;
    }

    const QSizeF size = metadata.logicalSize();
    if (!isAdmittedLogicalSizeComponent(size.width(), ImageSequenceLimits::maximumLogicalWidth())
        || !isAdmittedLogicalSizeComponent(size.height(), ImageSequenceLimits::maximumLogicalHeight())) {
        return false;
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    return width * height <= ImageSequenceLimits::maximumPixelsPerFrame();
}

bool FramePreparation::validateProviderTimedMetadata(const ImageSequenceProviderMetadata &metadata)
{
    if (!metadata.isTimedFrameList() || !metadata.isValid()) {
        return false;
    }

    const QSizeF size = metadata.logicalSize();
    if (!isAdmittedLogicalSizeComponent(size.width(), ImageSequenceLimits::maximumLogicalWidth())
        || !isAdmittedLogicalSizeComponent(size.height(), ImageSequenceLimits::maximumLogicalHeight())) {
        return false;
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width * height > ImageSequenceLimits::maximumPixelsPerFrame()) {
        return false;
    }

    const QVector<int> durations = metadata.frameDurations();
    if (durations.isEmpty() || durations.size() > ImageSequenceLimits::maximumTimedListFrameCount()) {
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

bool FramePreparation::exceedsPayloadLimit(const ImageFrame *frame)
{
    return frame && frame->payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame();
}

bool FramePreparation::validateProviderFrame(ImageFrame *frame, const ImageSequenceProviderFrameMetadata &metadata, const ProviderFrameState &state)
{
    if (!frame
        || !frame->isValid()
        || !state.metadataReady
        || frame->logicalSize() != state.logicalSize
        || frame->payloadByteSize() <= 0
        || frame->payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame()
        || !metadata.isValid()) {
        return false;
    }

    if (state.timedMetadata) {
        if (!metadata.isTimedFrame() || metadata.frame() != state.currentFrame) {
            return false;
        }
        if (metadata.frameStartPosition() != providerFrameStartPosition(state.frameDurations, state.currentFrame)) {
            return false;
        }
        if (metadata.frameDuration() != -1 && metadata.frameDuration() != state.frameDurations.at(state.currentFrame)) {
            return false;
        }
        return true;
    }

    return metadata.isStillFrame() && metadata.frame() == 0;
}

int FramePreparation::providerFrameStartPosition(const QVector<int> &frameDurations, int frame)
{
    if (frameDurations.isEmpty() || frame < 0 || frame >= frameDurations.size()) {
        return -1;
    }

    int position = 0;
    for (int index = 0; index < frame; ++index) {
        position += frameDurations.at(index);
    }
    return position;
}

int FramePreparation::providerFrameIndexForPosition(const QVector<int> &frameDurations, int position)
{
    const int duration = totalDuration(frameDurations);
    if (frameDurations.isEmpty() || position < 0 || position > duration) {
        return -1;
    }
    if (position == duration) {
        return frameDurations.size() - 1;
    }

    int frameStart = 0;
    for (int index = 0; index < frameDurations.size(); ++index) {
        const int frameEnd = frameStart + frameDurations.at(index);
        if (position >= frameStart && position < frameEnd) {
            return index;
        }
        frameStart = frameEnd;
    }

    return -1;
}

int FramePreparation::totalDuration(const QVector<int> &frameDurations)
{
    int total = 0;
    for (int duration : frameDurations) {
        total += duration;
    }
    return total;
}

QString FramePreparation::boundedDiagnostic(const QString &diagnostic, const QString &fallback)
{
    const QString selected = plainTextDiagnostic(redactDiagnosticDetails(diagnostic.isEmpty() ? fallback : diagnostic));
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
