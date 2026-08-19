// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imageanimationplaybacksource.h"

#include "decoding/apnganimationreader.h"
#include "decoding/bufferedimagereader.h"
#include "decoding/heifsequencereader.h"
#include "decoding/imageanimationsourcecatalog.h"
#include "decoding/imagerendering.h"
#include "decoding/jxlanimationreader.h"
#include "decoding/webpanimationreader.h"

#include <QImage>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace {
std::optional<qsizetype> checkedByteSum(qsizetype left, qsizetype right)
{
    if (left < 0 || right < 0 || left > std::numeric_limits<qsizetype>::max() - right) {
        return std::nullopt;
    }
    return left + right;
}

std::optional<kiriview::ImageAnimationPlaybackWorkspacePlan> readerWorkspacePlan(
    const kiriview::ReaderAnimationPlaybackRequest&, QSize logicalSize)
{
    const std::optional<qsizetype> transientByteCount
        = kiriview::qImageReaderGifTransientWorkspaceByteCount(logicalSize);
    const std::optional<qsizetype> outputByteCount
        = kiriview::checkedImageDecodeWorkspaceByteCount(logicalSize, 4, 1);
    if (!transientByteCount.has_value() || !outputByteCount.has_value()) {
        return std::nullopt;
    }
    return kiriview::ImageAnimationPlaybackWorkspacePlan {
        0,
        *transientByteCount,
        *outputByteCount,
    };
}

std::optional<kiriview::ImageAnimationPlaybackWorkspacePlan> apngWorkspacePlan(
    const kiriview::ApngAnimationPlaybackRequest& request, QSize logicalSize)
{
    const kiriview::ApngAnimationWorkspacePlanResult planning
        = kiriview::planApngAnimationOpen(request.data);
    if (planning.status != kiriview::ApngOpenStatus::Success
        || planning.plan.canvasSize != logicalSize) {
        return std::nullopt;
    }
    return kiriview::ImageAnimationPlaybackWorkspacePlan {
        0,
        planning.plan.transientByteCount,
        planning.plan.firstFrameOutputByteCount,
    };
}

std::optional<kiriview::ImageAnimationPlaybackWorkspacePlan> webPWorkspacePlan(
    const kiriview::WebPAnimationPlaybackRequest& request, QSize logicalSize)
{
    const kiriview::WebPAnimationWorkspacePlanResult planning
        = kiriview::planWebPAnimationOpen(request.data);
    if (planning.status != kiriview::WebPAnimationOpenStatus::Success
        || planning.plan.canvasSize != logicalSize) {
        return std::nullopt;
    }
    return kiriview::ImageAnimationPlaybackWorkspacePlan {
        0,
        planning.plan.transientByteCount,
        planning.plan.firstFrameOutputByteCount,
    };
}

std::optional<kiriview::ImageAnimationPlaybackWorkspacePlan> jxlWorkspacePlan(
    const kiriview::JxlAnimationPlaybackRequest&, QSize logicalSize)
{
    const std::optional<qsizetype> outputByteCount
        = kiriview::checkedImageDecodeWorkspaceByteCount(logicalSize, 4, 1);
    const std::optional<qsizetype> openPeakByteCount
        = kiriview::jxlAnimationOpenWorkspaceByteCount(logicalSize);
    if (!outputByteCount.has_value() || !openPeakByteCount.has_value()
        || *openPeakByteCount <= *outputByteCount) {
        return std::nullopt;
    }
    return kiriview::ImageAnimationPlaybackWorkspacePlan {
        0,
        *openPeakByteCount - *outputByteCount,
        *outputByteCount,
    };
}

std::optional<kiriview::ImageAnimationPlaybackWorkspacePlan> heifWorkspacePlan(
    const kiriview::HeifSequenceAnimationPlaybackRequest&, QSize logicalSize)
{
    const std::optional<kiriview::HeifSequenceWorkspacePlan> planning
        = kiriview::heifSequenceWorkspacePlan(logicalSize);
    if (!planning.has_value()) {
        return std::nullopt;
    }
    return kiriview::ImageAnimationPlaybackWorkspacePlan {
        0,
        planning->transientByteCount,
        planning->outputByteCount,
    };
}

std::optional<kiriview::ImageAnimationPlaybackWorkspacePlan> emptyWorkspacePlan(
    std::monostate, QSize)
{
    return std::nullopt;
}

kiriview::ImageAnimationPlaybackOpenResult playbackOpenResult(QImage firstFrame,
    int firstFrameDelay, int loopCount, bool sourceHasMoreFrames,
    kiriview::ImageDecodeWorkspaceHold workspaceHold = {})
{
    kiriview::ImageAnimationPlaybackOpenResult result;
    result.status = kiriview::ImageAnimationPlaybackOpenStatus::Success;
    result.workspaceHold = std::move(workspaceHold);
    result.firstFrame = std::move(firstFrame);
    result.firstFrameDelay = firstFrameDelay;
    result.loopCount = loopCount;
    result.sourceHasMoreFrames = sourceHasMoreFrames;
    return result;
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
    ReaderAnimationPlaybackSource(QByteArray data, QByteArray format,
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget)
        : m_data(std::move(data))
        , m_format(std::move(format))
        , m_workspaceBudget(workspaceBudget != nullptr
                  ? std::move(workspaceBudget)
                  : kiriview::defaultImageDecodeWorkspaceBudget())
    {
    }

    ~ReaderAnimationPlaybackSource() override { reset(); }
    Q_DISABLE_COPY_MOVE(ReaderAnimationPlaybackSource)

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        return openWithBudget(m_workspaceBudget);
    }

    kiriview::ImageAnimationPlaybackOpenResult openAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return openWithBudget(std::move(operationBudget));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        return readNextFrameWithBudget(m_workspaceBudget);
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return readNextFrameWithBudget(operationBudget);
    }

    [[nodiscard]] bool restartable() const override { return true; }

private:
    kiriview::ImageAnimationPlaybackOpenResult openWithBudget(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget)
    {
        reset();

        if (operationBudget == nullptr) {
            return playbackOpenResourceLimit(
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
        }
        m_readerWorkspaceBudget = std::move(operationBudget);

        kiriview::ImageAnimationSourceCatalogResult catalog
            = kiriview::readImageAnimationSourceCatalog(kiriview::readerAnimationPlaybackRequest(
                m_data, m_format, {}, m_readerWorkspaceBudget));
        if (!catalog.has_value()) {
            return catalog.error().cause
                    == kiriview::ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded
                ? playbackOpenResourceLimit(std::move(catalog.error().errorString))
                : playbackOpenError(std::move(catalog.error().errorString));
        }

        const std::optional<qsizetype> outputByteCount
            = kiriview::checkedImageDecodeWorkspaceByteCount(catalog->logicalSize, 4, 1);
        const std::optional<qsizetype> workspaceByteCount
            = kiriview::qImageReaderGifTransientWorkspaceByteCount(catalog->logicalSize);
        m_transientWorkspaceLease
            = kiriview::ImageDecodeWorkspaceDetail::startLease(*m_readerWorkspaceBudget);
        if (!outputByteCount.has_value() || !workspaceByteCount.has_value()
            || !kiriview::ImageDecodeWorkspaceDetail::tryReserve(
                m_transientWorkspaceLease, *workspaceByteCount)) {
            m_transientWorkspaceLease = {};
            return playbackOpenResourceLimit(
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
        }
        m_outputByteCount = *outputByteCount;

        auto reader = std::make_unique<kiriview::BufferedImageReader>(m_data, m_format);
        if (!*reader) {
            resetWorkspaceAfterReaderRelease(std::move(reader));
            return playbackOpenError(
                QStringLiteral("Qt image reader could not open the animation data"));
        }

        kiriview::ImageDecodeWorkspaceLease outputLease
            = outputWorkspaceLease(m_readerWorkspaceBudget);
        if (!kiriview::ImageDecodeWorkspaceDetail::tryReserve(outputLease, m_outputByteCount)) {
            resetWorkspaceAfterReaderRelease(std::move(reader));
            return playbackOpenResourceLimit(
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
        }

        QImage decodedFirstFrame = reader->read();
        if (decodedFirstFrame.isNull()) {
            const QString errorString = reader->errorString();
            resetWorkspaceAfterReaderRelease(std::move(reader));
            return playbackOpenError(errorString);
        }
        QImage firstFrame = kiriview::displayReadyImage(decodedFirstFrame);
        if (firstFrame.isNull()) {
            decodedFirstFrame = {};
            resetWorkspaceAfterReaderRelease(std::move(reader));
            return playbackOpenResourceLimit(
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
        }
        if (firstFrame.size() != catalog->logicalSize) {
            decodedFirstFrame = {};
            firstFrame = {};
            resetWorkspaceAfterReaderRelease(std::move(reader));
            return playbackOpenError(QStringLiteral("animation source catalog size mismatch"));
        }

        const int firstFrameDelay = catalog->frameDurations.constFirst();
        const int loopCount = catalog->repeatCount;
        m_logicalSize = catalog->logicalSize;
        m_frameDurations = std::move(catalog->frameDurations);
        m_nextFrameIndex = 1;
        m_reader = std::move(reader);
        return playbackOpenResult(std::move(firstFrame), firstFrameDelay, loopCount,
            m_nextFrameIndex < m_frameDurations.size(), outputLease.sharedHold());
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameWithBudget(
        const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& operationBudget)
    {
        if (m_reader == nullptr || m_nextFrameIndex >= m_frameDurations.size()) {
            return playbackReadEnd();
        }

        if (operationBudget == nullptr) {
            reset();
            return playbackReadResourceLimit(
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
        }

        kiriview::ImageDecodeWorkspaceLease outputLease = outputWorkspaceLease(operationBudget);
        if (!kiriview::ImageDecodeWorkspaceDetail::tryReserve(outputLease, m_outputByteCount)) {
            reset();
            return playbackReadResourceLimit(
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
        }

        QImage decodedFrame = m_reader->read();
        if (decodedFrame.isNull()) {
            const QString errorString = m_reader->errorString();
            reset();
            return playbackReadError(errorString);
        }
        QImage frame = kiriview::displayReadyImage(decodedFrame);
        if (frame.isNull()) {
            decodedFrame = {};
            reset();
            return playbackReadResourceLimit(
                kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
        }
        if (frame.size() != m_logicalSize) {
            decodedFrame = {};
            frame = {};
            reset();
            return playbackReadError(QStringLiteral("animation frame size mismatch"));
        }

        const int frameDelay = m_frameDurations.at(m_nextFrameIndex);
        ++m_nextFrameIndex;
        return playbackReadFrame(
            kiriview::AnimationFrame {
                std::move(frame),
                frameDelay,
                outputLease.sharedHold(),
            },
            m_nextFrameIndex < m_frameDurations.size());
    }

    [[nodiscard]] kiriview::ImageDecodeWorkspaceLease outputWorkspaceLease(
        const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& operationBudget) const
    {
        return kiriview::ImageDecodeWorkspaceDetail::startLeaseForOperation(
            *operationBudget, m_transientWorkspaceLease.reservedByteCount());
    }

    void resetWorkspaceAfterReaderRelease(
        std::unique_ptr<kiriview::BufferedImageReader> reader = {})
    {
        reader.reset();
        m_reader.reset();
        m_logicalSize = {};
        m_frameDurations.clear();
        m_nextFrameIndex = 0;
        m_outputByteCount = 0;
        m_transientWorkspaceLease = {};
        m_readerWorkspaceBudget.reset();
    }

    void reset() { resetWorkspaceAfterReaderRelease(); }

    QByteArray m_data;
    QByteArray m_format;
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> m_workspaceBudget;
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> m_readerWorkspaceBudget;
    kiriview::ImageDecodeWorkspaceLease m_transientWorkspaceLease;
    QSize m_logicalSize;
    QVector<int> m_frameDurations;
    int m_nextFrameIndex = 0;
    qsizetype m_outputByteCount = 0;
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
        return openWithBudget(m_workspaceBudget);
    }

    kiriview::ImageAnimationPlaybackOpenResult openAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return openWithBudget(std::move(operationBudget));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        return readNextFrameWithBudget({});
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return readNextFrameWithBudget(std::move(operationBudget));
    }

    [[nodiscard]] bool restartable() const override { return true; }

private:
    kiriview::ImageAnimationPlaybackOpenResult openWithBudget(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget)
    {
        if (operationBudget == nullptr) {
            operationBudget = m_workspaceBudget;
        }
        m_reader = std::make_unique<kiriview::ApngAnimationReader>(operationBudget);
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
            return playbackOpenError(QStringLiteral("APNG reader rejected the animation data"));
        }

        m_reader.reset();
        return playbackOpenError(QStringLiteral("APNG reader returned an invalid open status"));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameWithBudget(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget)
    {
        if (m_reader == nullptr) {
            return playbackReadEnd();
        }

        kiriview::AnimationFrameReadResult frame = operationBudget == nullptr
            ? m_reader->readNextFrame()
            : m_reader->readNextFrame(std::move(operationBudget));
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

    QByteArray m_data;
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> m_workspaceBudget;
    std::unique_ptr<kiriview::ApngAnimationReader> m_reader;
};

class WebPAnimationPlaybackSource final : public kiriview::ImageAnimationPlaybackSource
{
public:
    WebPAnimationPlaybackSource(
        QByteArray data, std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget)
        : m_data(std::move(data))
        , m_workspaceBudget(std::move(workspaceBudget))
    {
    }

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        return openWithBudget(m_workspaceBudget);
    }

    kiriview::ImageAnimationPlaybackOpenResult openAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return openWithBudget(std::move(operationBudget));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        return readNextFrameWithBudget({});
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return readNextFrameWithBudget(operationBudget);
    }

    [[nodiscard]] bool restartable() const override { return true; }

private:
    kiriview::ImageAnimationPlaybackOpenResult openWithBudget(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget)
    {
        if (operationBudget == nullptr) {
            operationBudget = m_workspaceBudget;
        }
        m_reader = std::make_unique<kiriview::WebPAnimationReader>(operationBudget);
        kiriview::WebPAnimationOpenResult openResult = m_reader->open(m_data);
        switch (openResult.status) {
        case kiriview::WebPAnimationOpenStatus::Success:
            return playbackOpenResult(std::move(openResult.firstFrame), openResult.firstFrameDelay,
                openResult.loopCount, openResult.sourceHasMoreFrames,
                std::move(openResult.workspaceHold));
        case kiriview::WebPAnimationOpenStatus::Error:
            m_reader.reset();
            return playbackOpenError(openResult.errorString);
        case kiriview::WebPAnimationOpenStatus::ResourceLimitExceeded:
            m_reader.reset();
            return playbackOpenResourceLimit(openResult.errorString);
        case kiriview::WebPAnimationOpenStatus::NotWebP:
        case kiriview::WebPAnimationOpenStatus::NotAnimation:
            m_reader.reset();
            return playbackOpenError(QStringLiteral("WebP reader rejected the animation data"));
        }

        m_reader.reset();
        return playbackOpenError(QStringLiteral("WebP reader returned an invalid open status"));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameWithBudget(
        const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& operationBudget)
    {
        if (m_reader == nullptr) {
            return playbackReadEnd();
        }

        kiriview::AnimationFrameReadResult frame = operationBudget == nullptr
            ? m_reader->readNextFrame()
            : m_reader->readNextFrame(operationBudget);
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

    QByteArray m_data;
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> m_workspaceBudget;
    std::unique_ptr<kiriview::WebPAnimationReader> m_reader;
};

class JxlAnimationPlaybackSource final : public kiriview::ImageAnimationPlaybackSource
{
public:
    JxlAnimationPlaybackSource(
        QByteArray data, std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget)
        : m_data(std::move(data))
        , m_workspaceBudget(std::move(workspaceBudget))
    {
    }

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        return openWithBudget(m_workspaceBudget);
    }

    kiriview::ImageAnimationPlaybackOpenResult openAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return openWithBudget(std::move(operationBudget));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        return readNextFrameWithBudget({});
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return readNextFrameWithBudget(operationBudget);
    }

    [[nodiscard]] bool restartable() const override { return true; }

private:
    kiriview::ImageAnimationPlaybackOpenResult openWithBudget(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget)
    {
        if (operationBudget == nullptr) {
            operationBudget = m_workspaceBudget;
        }
        m_reader = std::make_unique<kiriview::JxlAnimationReader>(operationBudget);
        kiriview::JxlAnimationOpenResult openResult = m_reader->open(m_data);
        switch (openResult.status) {
        case kiriview::JxlAnimationOpenStatus::Success:
            return playbackOpenResult(std::move(openResult.firstFrame), openResult.firstFrameDelay,
                openResult.loopCount, openResult.sourceHasMoreFrames,
                std::move(openResult.workspaceHold));
        case kiriview::JxlAnimationOpenStatus::Error:
            m_reader.reset();
            return playbackOpenError(openResult.errorString);
        case kiriview::JxlAnimationOpenStatus::ResourceLimitExceeded:
            m_reader.reset();
            return playbackOpenResourceLimit(openResult.errorString);
        case kiriview::JxlAnimationOpenStatus::NotJxl:
        case kiriview::JxlAnimationOpenStatus::NotAnimation:
            m_reader.reset();
            return playbackOpenError(QStringLiteral("JPEG XL reader rejected the animation data"));
        }

        m_reader.reset();
        return playbackOpenError(QStringLiteral("JPEG XL reader returned an invalid open status"));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameWithBudget(
        const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& operationBudget)
    {
        if (m_reader == nullptr) {
            return playbackReadEnd();
        }

        kiriview::AnimationFrameReadResult frame = operationBudget == nullptr
            ? m_reader->readNextFrame()
            : m_reader->readNextFrame(operationBudget);
        if (!frame) {
            return m_reader->lastReadResourceLimitExceeded()
                ? playbackReadResourceLimit(std::move(frame.error()))
                : playbackReadError(std::move(frame.error()));
        }
        if (frame->has_value()) {
            return playbackReadFrame(std::move(**frame), true);
        }
        return playbackReadEnd();
    }

    QByteArray m_data;
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> m_workspaceBudget;
    std::unique_ptr<kiriview::JxlAnimationReader> m_reader;
};

class HeifSequenceAnimationPlaybackSource final : public kiriview::ImageAnimationPlaybackSource
{
public:
    HeifSequenceAnimationPlaybackSource(QByteArray data,
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget,
        qsizetype retainedInputWorkspaceByteCount)
        : m_data(std::move(data))
        , m_workspaceBudget(std::move(workspaceBudget))
        , m_retainedInputWorkspaceByteCount(retainedInputWorkspaceByteCount)
    {
    }

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        return openWithBudget(m_workspaceBudget);
    }

    kiriview::ImageAnimationPlaybackOpenResult openAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return openWithBudget(std::move(operationBudget));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        return readNextFrameWithBudget({});
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameAdmitted(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget) override
    {
        return readNextFrameWithBudget(operationBudget);
    }

    [[nodiscard]] bool restartable() const override { return false; }

private:
    kiriview::ImageAnimationPlaybackOpenResult openWithBudget(
        std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> operationBudget)
    {
        if (operationBudget == nullptr) {
            operationBudget = m_workspaceBudget;
        }
        m_reader = std::make_unique<kiriview::HeifSequenceReader>(
            operationBudget, m_retainedInputWorkspaceByteCount);
        const kiriview::HeifSequenceOpenResult openResult = m_reader->open(m_data);
        if (openResult.status != kiriview::HeifSequenceOpenStatus::Success) {
            m_reader.reset();
            const QString errorString = openResult.errorString.isEmpty()
                ? kiriview::heifSequenceDecodeErrorString()
                : openResult.errorString;
            return openResult.status == kiriview::HeifSequenceOpenStatus::ResourceLimitExceeded
                ? playbackOpenResourceLimit(errorString)
                : playbackOpenError(errorString);
        }

        kiriview::AnimationFrameReadResult firstFrame = m_reader->readNextFrame();
        if (!firstFrame || !firstFrame->has_value()) {
            const bool resourceLimitExceeded = m_reader->lastReadResourceLimitExceeded();
            m_reader.reset();
            const QString errorString
                = firstFrame ? kiriview::heifSequenceDecodeErrorString() : firstFrame.error();
            return resourceLimitExceeded ? playbackOpenResourceLimit(errorString)
                                         : playbackOpenError(errorString);
        }

        kiriview::AnimationFrame decodedFirstFrame = std::move(**firstFrame);
        return playbackOpenResult(std::move(decodedFirstFrame.image), decodedFirstFrame.delay, 0,
            true, std::move(decodedFirstFrame.workspaceHold));
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrameWithBudget(
        const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& operationBudget)
    {
        if (m_reader == nullptr) {
            return playbackReadEnd();
        }

        kiriview::AnimationFrameReadResult frame = operationBudget == nullptr
            ? m_reader->readNextFrame()
            : m_reader->readNextFrame(operationBudget);
        if (!frame) {
            return m_reader->lastReadResourceLimitExceeded()
                ? playbackReadResourceLimit(std::move(frame.error()))
                : playbackReadError(std::move(frame.error()));
        }
        if (frame->has_value()) {
            return playbackReadFrame(std::move(**frame), true);
        }
        return playbackReadEnd();
    }

    QByteArray m_data;
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> m_workspaceBudget;
    qsizetype m_retainedInputWorkspaceByteCount = 0;
    std::unique_ptr<kiriview::HeifSequenceReader> m_reader;
};

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makeReaderPlaybackSource(
    kiriview::ReaderAnimationPlaybackRequest request)
{
    return std::make_unique<ReaderAnimationPlaybackSource>(
        std::move(request.data), std::move(request.format), std::move(request.workspaceBudget));
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
    return std::make_unique<WebPAnimationPlaybackSource>(
        std::move(request.data), std::move(request.workspaceBudget));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makeJxlPlaybackSource(
    kiriview::JxlAnimationPlaybackRequest request)
{
    return std::make_unique<JxlAnimationPlaybackSource>(
        std::move(request.data), std::move(request.workspaceBudget));
}

std::unique_ptr<kiriview::ImageAnimationPlaybackSource> makeHeifSequencePlaybackSource(
    kiriview::HeifSequenceAnimationPlaybackRequest request)
{
    return std::make_unique<HeifSequenceAnimationPlaybackSource>(std::move(request.data),
        std::move(request.workspaceBudget), request.retainedInputWorkspaceByteCount);
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
bool ImageAnimationPlaybackWorkspacePlan::isValid() const
{
    const std::optional<qsizetype> openPeak = openPeakByteCount();
    return retainedInputByteCount >= 0 && persistentDecoderByteCount > 0 && frameOutputByteCount > 0
        && openPeak.has_value() && checkedByteSum(retainedInputByteCount, *openPeak).has_value();
}

std::optional<qsizetype> ImageAnimationPlaybackWorkspacePlan::openPeakByteCount() const
{
    return checkedByteSum(persistentDecoderByteCount, frameOutputByteCount);
}

ImageAnimationPlaybackOpenResult ImageAnimationPlaybackSource::openAdmitted(
    std::shared_ptr<ImageDecodeWorkspaceBudget>) // NOLINT(performance-unnecessary-value-param)
{
    return open();
}

ImageAnimationPlaybackReadResult ImageAnimationPlaybackSource::readNextFrameAdmitted(
    std::shared_ptr<ImageDecodeWorkspaceBudget>) // NOLINT(performance-unnecessary-value-param)
{
    return readNextFrame();
}

void ImageAnimationPlaybackSource::retainSourceDataLease(ImageSourceDataLease sourceDataLease)
{
    m_sourceDataLease = std::move(sourceDataLease);
}

void ImageAnimationPlaybackSource::retainInputWorkspace(ImageDecodeWorkspaceHold inputWorkspaceHold)
{
    m_inputWorkspaceHold = std::move(inputWorkspaceHold);
}

std::unique_ptr<ImageAnimationPlaybackSource> makeImageAnimationPlaybackSource(
    ImageAnimationPlaybackRequest request)
{
    ImageSourceDataLease sourceDataLease = std::move(request.sourceDataLease);
    ImageDecodeWorkspaceHold inputWorkspaceHold = std::move(request.inputWorkspaceHold);
    std::unique_ptr<ImageAnimationPlaybackSource> source = std::visit(
        [](auto&& playbackRequest) {
            return makePlaybackSource(std::forward<decltype(playbackRequest)>(playbackRequest));
        },
        std::move(request.payload));
    if (source != nullptr) {
        source->retainSourceDataLease(std::move(sourceDataLease));
        source->retainInputWorkspace(std::move(inputWorkspaceHold));
    }
    return source;
}

std::optional<ImageAnimationPlaybackWorkspacePlan> imageAnimationPlaybackWorkspacePlan(
    const ImageAnimationPlaybackRequest& request, QSize logicalSize)
{
    if (!request.isValid() || logicalSize.isEmpty()) {
        return std::nullopt;
    }
    std::optional<ImageAnimationPlaybackWorkspacePlan> plan = std::visit(
        [logicalSize](const auto& playbackRequest) {
            if constexpr (std::is_same_v<std::decay_t<decltype(playbackRequest)>, std::monostate>) {
                return emptyWorkspacePlan(playbackRequest, logicalSize);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(playbackRequest)>,
                                     ReaderAnimationPlaybackRequest>) {
                return readerWorkspacePlan(playbackRequest, logicalSize);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(playbackRequest)>,
                                     ApngAnimationPlaybackRequest>) {
                return apngWorkspacePlan(playbackRequest, logicalSize);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(playbackRequest)>,
                                     WebPAnimationPlaybackRequest>) {
                return webPWorkspacePlan(playbackRequest, logicalSize);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(playbackRequest)>,
                                     JxlAnimationPlaybackRequest>) {
                return jxlWorkspacePlan(playbackRequest, logicalSize);
            } else {
                return heifWorkspacePlan(playbackRequest, logicalSize);
            }
        },
        request.payload);
    if (!plan.has_value()) {
        return std::nullopt;
    }
    plan->retainedInputByteCount = request.inputWorkspaceHold.reservedByteCount();
    return plan->isValid() ? plan : std::nullopt;
}
}
