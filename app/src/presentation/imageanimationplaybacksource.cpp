// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imageanimationplaybacksource.h"

#include "decoding/apnganimationreader.h"
#include "decoding/bufferedimagereader.h"
#include "decoding/heifsequencereader.h"
#include "decoding/jxlanimationreader.h"
#include "decoding/webpanimationreader.h"
#include "localization/imageerrortext.h"

#include <QImage>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace {
kiriview::ImageAnimationPlaybackOpenResult playbackOpenResult(QImage firstFrame,
    int firstFrameDelay, int loopCount, bool sourceHasMoreFrames,
    kiriview::ImageDecodeWorkspaceHold workspaceHold = {})
{
    return kiriview::ImageAnimationPlaybackOpenResult {
        kiriview::ImageAnimationPlaybackOpenStatus::Success,
        std::move(workspaceHold),
        std::move(firstFrame),
        firstFrameDelay,
        loopCount,
        sourceHasMoreFrames,
        {},
    };
}

kiriview::ImageAnimationPlaybackOpenResult playbackOpenError(QString errorString)
{
    kiriview::ImageAnimationPlaybackOpenResult result;
    result.errorString = std::move(errorString);
    return result;
}

kiriview::ImageAnimationPlaybackOpenResult playbackOpenResourceLimit(QString errorString)
{
    kiriview::ImageAnimationPlaybackOpenResult result;
    result.status = kiriview::ImageAnimationPlaybackOpenStatus::ResourceLimitExceeded;
    result.errorString = std::move(errorString);
    return result;
}

kiriview::ImageAnimationPlaybackReadResult playbackReadFrame(
    kiriview::AnimationFrame frame, bool sourceHasMoreFrames)
{
    return kiriview::ImageAnimationPlaybackReadResult {
        kiriview::ImageAnimationPlaybackReadStatus::Frame,
        std::move(frame),
        sourceHasMoreFrames,
        {},
    };
}

kiriview::ImageAnimationPlaybackReadResult playbackReadEnd()
{
    return kiriview::ImageAnimationPlaybackReadResult {
        kiriview::ImageAnimationPlaybackReadStatus::End,
        {},
        false,
        {},
    };
}

kiriview::ImageAnimationPlaybackReadResult playbackReadError(QString errorString)
{
    return kiriview::ImageAnimationPlaybackReadResult {
        kiriview::ImageAnimationPlaybackReadStatus::Error,
        {},
        false,
        std::move(errorString),
    };
}

kiriview::ImageAnimationPlaybackReadResult playbackReadResourceLimit(QString errorString)
{
    return kiriview::ImageAnimationPlaybackReadResult {
        kiriview::ImageAnimationPlaybackReadStatus::ResourceLimitExceeded,
        {},
        false,
        std::move(errorString),
    };
}

class ReaderAnimationPlaybackSource final : public kiriview::ImageAnimationPlaybackSource
{
public:
    ReaderAnimationPlaybackSource(QByteArray data, QByteArray format)
        : m_data(std::move(data))
        , m_format(std::move(format))
    {
    }

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        m_reader.reset();

        auto reader = std::make_unique<kiriview::BufferedImageReader>(m_data, m_format);
        if (!*reader) {
            return playbackOpenError(
                kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData));
        }

        if (!reader->canRead()) {
            return playbackOpenError(reader->errorString());
        }

        QImage firstFrame = reader->read();
        if (firstFrame.isNull()) {
            return playbackOpenError(reader->errorString());
        }

        const int firstFrameDelay = reader->nextImageDelay();
        const int loopCount = reader->loopCount();
        const bool sourceHasMoreFrames = reader->canRead();
        m_reader = std::move(reader);
        return playbackOpenResult(
            std::move(firstFrame), firstFrameDelay, loopCount, sourceHasMoreFrames);
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        if (m_reader == nullptr || !m_reader->canRead()) {
            return playbackReadEnd();
        }

        QImage frame = m_reader->read();
        if (frame.isNull()) {
            return playbackReadError(m_reader->errorString());
        }

        return playbackReadFrame(
            kiriview::AnimationFrame {
                std::move(frame),
                m_reader->nextImageDelay(),
            },
            m_reader->canRead());
    }

    [[nodiscard]] bool restartable() const override { return true; }

private:
    QByteArray m_data;
    QByteArray m_format;
    std::unique_ptr<kiriview::BufferedImageReader> m_reader;
};

class ApngAnimationPlaybackSource final : public kiriview::ImageAnimationPlaybackSource
{
public:
    ApngAnimationPlaybackSource(
        QByteArray data, std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget)
        : m_data(std::move(data))
        , m_workspaceBudget(std::move(workspaceBudget))
    {
    }

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        m_reader = std::make_unique<kiriview::ApngAnimationReader>(m_workspaceBudget);
        kiriview::ApngOpenResult openResult = m_reader->open(m_data);
        switch (openResult.status) {
        case kiriview::ApngOpenStatus::Success:
            return playbackOpenResult(std::move(openResult.firstFrame), openResult.firstFrameDelay,
                openResult.loopCount, openResult.frameCount > 1,
                std::move(openResult.workspaceHold));
        case kiriview::ApngOpenStatus::Error:
            m_reader.reset();
            return playbackOpenError(openResult.errorString);
        case kiriview::ApngOpenStatus::ResourceLimitExceeded:
            m_reader.reset();
            return playbackOpenResourceLimit(openResult.errorString);
        case kiriview::ApngOpenStatus::NotApng:
            m_reader.reset();
            return playbackOpenError(
                kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeApngAnimation));
        }

        m_reader.reset();
        return playbackOpenError(
            kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeApngAnimation));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        if (m_reader == nullptr) {
            return playbackReadEnd();
        }

        kiriview::AnimationFrameReadResult frame = m_reader->readNextFrame();
        if (!frame) {
            return m_reader->lastReadResourceLimitExceeded()
                ? playbackReadResourceLimit(std::move(frame.error()))
                : playbackReadError(std::move(frame.error()));
        }
        if (frame->has_value()) {
            return playbackReadFrame(std::move(**frame), m_reader->hasMoreFrames());
        }
        return playbackReadEnd();
    }

    [[nodiscard]] bool restartable() const override { return true; }

private:
    QByteArray m_data;
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> m_workspaceBudget;
    std::unique_ptr<kiriview::ApngAnimationReader> m_reader;
};

class WebPAnimationPlaybackSource final : public kiriview::ImageAnimationPlaybackSource
{
public:
    explicit WebPAnimationPlaybackSource(QByteArray data)
        : m_data(std::move(data))
    {
    }

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        m_reader = std::make_unique<kiriview::WebPAnimationReader>();
        kiriview::WebPAnimationOpenResult openResult = m_reader->open(m_data);
        switch (openResult.status) {
        case kiriview::WebPAnimationOpenStatus::Success:
            return playbackOpenResult(std::move(openResult.firstFrame), openResult.firstFrameDelay,
                openResult.loopCount, openResult.sourceHasMoreFrames);
        case kiriview::WebPAnimationOpenStatus::Error:
            m_reader.reset();
            return playbackOpenError(openResult.errorString);
        case kiriview::WebPAnimationOpenStatus::NotWebP:
        case kiriview::WebPAnimationOpenStatus::NotAnimation:
            m_reader.reset();
            return playbackOpenError(
                kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeImageAnimation));
        }

        m_reader.reset();
        return playbackOpenError(
            kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeImageAnimation));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        if (m_reader == nullptr) {
            return playbackReadEnd();
        }

        kiriview::AnimationFrameReadResult frame = m_reader->readNextFrame();
        if (!frame) {
            return playbackReadError(std::move(frame.error()));
        }
        if (frame->has_value()) {
            return playbackReadFrame(std::move(**frame), m_reader->hasMoreFrames());
        }
        return playbackReadEnd();
    }

    [[nodiscard]] bool restartable() const override { return true; }

private:
    QByteArray m_data;
    std::unique_ptr<kiriview::WebPAnimationReader> m_reader;
};

class JxlAnimationPlaybackSource final : public kiriview::ImageAnimationPlaybackSource
{
public:
    explicit JxlAnimationPlaybackSource(QByteArray data)
        : m_data(std::move(data))
    {
    }

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        m_reader = std::make_unique<kiriview::JxlAnimationReader>();
        kiriview::JxlAnimationOpenResult openResult = m_reader->open(m_data);
        switch (openResult.status) {
        case kiriview::JxlAnimationOpenStatus::Success:
            return playbackOpenResult(std::move(openResult.firstFrame), openResult.firstFrameDelay,
                openResult.loopCount, openResult.sourceHasMoreFrames);
        case kiriview::JxlAnimationOpenStatus::Error:
            m_reader.reset();
            return playbackOpenError(openResult.errorString);
        case kiriview::JxlAnimationOpenStatus::NotJxl:
        case kiriview::JxlAnimationOpenStatus::NotAnimation:
            m_reader.reset();
            return playbackOpenError(
                kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeImageAnimation));
        }

        m_reader.reset();
        return playbackOpenError(
            kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeImageAnimation));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        if (m_reader == nullptr) {
            return playbackReadEnd();
        }

        kiriview::AnimationFrameReadResult frame = m_reader->readNextFrame();
        if (!frame) {
            return playbackReadError(std::move(frame.error()));
        }
        if (frame->has_value()) {
            return playbackReadFrame(std::move(**frame), true);
        }
        return playbackReadEnd();
    }

    [[nodiscard]] bool restartable() const override { return true; }

private:
    QByteArray m_data;
    std::unique_ptr<kiriview::JxlAnimationReader> m_reader;
};

class HeifSequenceAnimationPlaybackSource final : public kiriview::ImageAnimationPlaybackSource
{
public:
    explicit HeifSequenceAnimationPlaybackSource(QByteArray data)
        : m_data(std::move(data))
    {
    }

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        m_reader = std::make_unique<kiriview::HeifSequenceReader>();
        const kiriview::HeifSequenceOpenResult openResult = m_reader->open(m_data);
        if (openResult.status != kiriview::HeifSequenceOpenStatus::Success) {
            m_reader.reset();
            return playbackOpenError(openResult.errorString.isEmpty()
                    ? kiriview::heifSequenceDecodeErrorString()
                    : openResult.errorString);
        }

        kiriview::AnimationFrameReadResult firstFrame = m_reader->readNextFrame();
        if (!firstFrame || !firstFrame->has_value()) {
            m_reader.reset();
            return playbackOpenError(
                firstFrame ? kiriview::heifSequenceDecodeErrorString() : firstFrame.error());
        }

        kiriview::AnimationFrame decodedFirstFrame = std::move(**firstFrame);
        return playbackOpenResult(
            std::move(decodedFirstFrame.image), decodedFirstFrame.delay, 0, true);
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        if (m_reader == nullptr) {
            return playbackReadEnd();
        }

        kiriview::AnimationFrameReadResult frame = m_reader->readNextFrame();
        if (!frame) {
            return playbackReadError(std::move(frame.error()));
        }
        if (frame->has_value()) {
            return playbackReadFrame(std::move(**frame), true);
        }
        return playbackReadEnd();
    }

    [[nodiscard]] bool restartable() const override { return false; }

private:
    QByteArray m_data;
    std::unique_ptr<kiriview::HeifSequenceReader> m_reader;
};

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makeReaderPlaybackSource(
    kiriview::ReaderAnimationPlaybackRequest request)
{
    return std::make_unique<ReaderAnimationPlaybackSource>(
        std::move(request.data), std::move(request.format));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makeApngPlaybackSource(
    kiriview::ApngAnimationPlaybackRequest request)
{
    return std::make_unique<ApngAnimationPlaybackSource>(
        std::move(request.data), std::move(request.workspaceBudget));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makeWebPPlaybackSource(
    kiriview::WebPAnimationPlaybackRequest request)
{
    return std::make_unique<WebPAnimationPlaybackSource>(std::move(request.data));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makeJxlPlaybackSource(
    kiriview::JxlAnimationPlaybackRequest request)
{
    return std::make_unique<JxlAnimationPlaybackSource>(std::move(request.data));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makeHeifSequencePlaybackSource(
    kiriview::HeifSequenceAnimationPlaybackRequest request)
{
    return std::make_unique<HeifSequenceAnimationPlaybackSource>(std::move(request.data));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makePlaybackSource(std::monostate)
{
    return {};
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makePlaybackSource(
    kiriview::ReaderAnimationPlaybackRequest request)
{
    return makeReaderPlaybackSource(std::move(request));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makePlaybackSource(
    kiriview::ApngAnimationPlaybackRequest request)
{
    return makeApngPlaybackSource(std::move(request));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makePlaybackSource(
    kiriview::WebPAnimationPlaybackRequest request)
{
    return makeWebPPlaybackSource(std::move(request));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makePlaybackSource(
    kiriview::JxlAnimationPlaybackRequest request)
{
    return makeJxlPlaybackSource(std::move(request));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makePlaybackSource(
    kiriview::HeifSequenceAnimationPlaybackRequest request)
{
    return makeHeifSequencePlaybackSource(std::move(request));
}
}

namespace kiriview {
void ImageAnimationPlaybackSource::retainSourceDataLease(ImageSourceDataLease sourceDataLease)
{
    m_sourceDataLease = std::move(sourceDataLease);
}

std::unique_ptr<ImageAnimationPlaybackSource> makeImageAnimationPlaybackSource(
    ImageAnimationPlaybackRequest request)
{
    ImageSourceDataLease sourceDataLease = std::move(request.sourceDataLease);
    std::unique_ptr<ImageAnimationPlaybackSource> source = std::visit(
        [](auto&& playbackRequest) {
            return makePlaybackSource(std::forward<decltype(playbackRequest)>(playbackRequest));
        },
        std::move(request.payload));
    if (source != nullptr) {
        source->retainSourceDataLease(std::move(sourceDataLease));
    }
    return source;
}
}
