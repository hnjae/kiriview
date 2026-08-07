// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "jxlanimationreader.h"

#include "animationtiming.h"
#include "localization/imageerrortext.h"
#include "rendering/imagerendering.h"

#include <jxl/decode.h>
#include <jxl/thread_parallel_runner.h>

#include <ImageViewport/imagesequence.h>
#include <QByteArrayView>
#include <QColorSpace>
#include <QSize>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace {
std::span<const std::uint8_t> jxlBytes(QByteArrayView data)
{
    return {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- libjxl byte API.
        reinterpret_cast<const std::uint8_t*>(data.data()),
        static_cast<std::size_t>(data.size()),
    };
}

struct JxlDecoderDeleter
{
    void operator()(JxlDecoder* decoder) const
    {
        if (decoder != nullptr) {
            JxlDecoderDestroy(decoder);
        }
    }
};

struct JxlThreadRunnerDeleter
{
    void operator()(void* runner) const
    {
        if (runner != nullptr) {
            JxlThreadParallelRunnerDestroy(runner);
        }
    }
};

using JxlDecoderPtr = std::unique_ptr<JxlDecoder, JxlDecoderDeleter>;
using JxlThreadRunnerPtr = std::unique_ptr<void, JxlThreadRunnerDeleter>;

enum class JxlReadStatus {
    Frame,
    End,
    NotAnimation,
    Error,
};

struct JxlReadResult
{
    JxlReadStatus status = JxlReadStatus::Error;
    kiriview::AnimationFrame frame;
    QString errorString;
};

QString jxlAnimationDecodeErrorString()
{
    return kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeImageAnimation);
}

kiriview::JxlAnimationOpenResult notJxlResult() { return {}; }

kiriview::JxlAnimationOpenResult notAnimationResult()
{
    kiriview::JxlAnimationOpenResult result;
    result.status = kiriview::JxlAnimationOpenStatus::NotAnimation;
    return result;
}

kiriview::JxlAnimationOpenResult errorOpenResult(QString errorString)
{
    kiriview::JxlAnimationOpenResult result;
    result.status = kiriview::JxlAnimationOpenStatus::Error;
    result.errorString = std::move(errorString);
    return result;
}

kiriview::JxlAnimationOpenResult resourceLimitOpenResult()
{
    kiriview::JxlAnimationOpenResult result;
    result.status = kiriview::JxlAnimationOpenStatus::ResourceLimitExceeded;
    result.errorString = kiriview::imageDecodeWorkspaceResourceLimitDiagnostic();
    return result;
}

kiriview::ImageAnimationSourceCatalogResult failedJxlCatalog(bool resourceLimitExceeded = false)
{
    return std::unexpected(kiriview::ImageAnimationSourceCatalogFailure {
        resourceLimitExceeded ? kiriview::imageDecodeWorkspaceResourceLimitDiagnostic()
                              : jxlAnimationDecodeErrorString(),
        resourceLimitExceeded
            ? kiriview::ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded
            : kiriview::ImageAnimationSourceCatalogFailureCause::InvalidSource,
    });
}

JxlReadResult errorReadResult(QString errorString)
{
    return JxlReadResult {
        JxlReadStatus::Error,
        {},
        std::move(errorString),
    };
}

bool isJxlData(const QByteArray& data)
{
    if (data.isEmpty()) {
        return false;
    }
    const std::span<const std::uint8_t> bytes = jxlBytes(data);
    const JxlSignature signature = JxlSignatureCheck(bytes.data(), bytes.size());
    return signature == JXL_SIG_CODESTREAM || signature == JXL_SIG_CONTAINER;
}

std::optional<QSize> imageSizeForInfo(const JxlBasicInfo& info);

JxlPixelFormat rgbaPixelFormat()
{
    return JxlPixelFormat {
        4,
        JXL_TYPE_UINT8,
        JXL_NATIVE_ENDIAN,
        0,
    };
}

std::optional<QSize> imageSizeForInfo(const JxlBasicInfo& info)
{
    if (info.xsize == 0 || info.ysize == 0
        || info.xsize > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        || info.ysize > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    return QSize(static_cast<int>(info.xsize), static_cast<int>(info.ysize));
}

std::optional<QImage> imageFromRgbaBuffer(const std::vector<std::uint8_t>& buffer, QSize size)
{
    if (buffer.empty() || size.isEmpty()) {
        return std::nullopt;
    }

    const auto width = static_cast<std::size_t>(size.width());
    const auto height = static_cast<std::size_t>(size.height());
    if (width > std::numeric_limits<std::size_t>::max() / 4U) {
        return std::nullopt;
    }
    const std::size_t rowBytes = width * 4U;
    if (height != 0 && rowBytes > std::numeric_limits<std::size_t>::max() / height) {
        return std::nullopt;
    }
    if (buffer.size() < rowBytes * height
        || rowBytes > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
        return std::nullopt;
    }

    const QImage borrowedImage(buffer.data(), size.width(), size.height(),
        static_cast<qsizetype>(rowBytes), QImage::Format_RGBA8888);
    if (borrowedImage.isNull()) {
        return std::nullopt;
    }

    QImage image = borrowedImage.copy();
    if (image.isNull()) {
        return std::nullopt;
    }
    image.setColorSpace(QColorSpace(QColorSpace::SRgb));
    QImage displayImage = kiriview::displayReadyImage(image);
    return displayImage.isNull() ? std::nullopt : std::optional<QImage>(std::move(displayImage));
}
}

namespace kiriview {
class JxlAnimationReader::Private
{
public:
    explicit Private(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
        : m_workspaceBudget(workspaceBudget != nullptr ? std::move(workspaceBudget)
                                                       : defaultImageDecodeWorkspaceBudget())
    {
    }

    ~Private() { reset(); }
    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&&) = delete;
    Private& operator=(Private&&) = delete;

    JxlAnimationOpenResult open(QByteArray inputData)
    {
        reset();
        allocationLimitExceeded.store(false, std::memory_order_relaxed);
        if (!isJxlData(inputData)) {
            return notJxlResult();
        }

        data = std::move(inputData);
        if (!initializeDecoder()) {
            const bool resourceLimitExceeded
                = allocationLimitExceeded.load(std::memory_order_relaxed);
            reset();
            if (resourceLimitExceeded) {
                allocationLimitExceeded.store(true, std::memory_order_relaxed);
                return resourceLimitOpenResult();
            }
            return errorOpenResult(jxlAnimationDecodeErrorString());
        }

        JxlReadResult firstFrame = readFrame();
        if (firstFrame.status == JxlReadStatus::NotAnimation) {
            reset();
            return notAnimationResult();
        }
        if (firstFrame.status != JxlReadStatus::Frame) {
            const bool resourceLimitExceeded
                = allocationLimitExceeded.load(std::memory_order_relaxed);
            reset();
            if (resourceLimitExceeded) {
                allocationLimitExceeded.store(true, std::memory_order_relaxed);
                return resourceLimitOpenResult();
            }
            return errorOpenResult(firstFrame.errorString.isEmpty()
                    ? jxlAnimationDecodeErrorString()
                    : firstFrame.errorString);
        }

        JxlAnimationOpenResult result;
        result.status = JxlAnimationOpenStatus::Success;
        result.firstFrame = std::move(firstFrame.frame.image);
        result.firstFrameDelay = firstFrame.frame.delay;
        result.loopCount = loopCount;
        result.sourceHasMoreFrames = true;
        result.workspaceHold = std::move(firstFrame.frame.workspaceHold);
        return result;
    }

    ImageAnimationSourceCatalogResult readSourceCatalog(QByteArray inputData)
    {
        reset();
        allocationLimitExceeded.store(false, std::memory_order_relaxed);
        if (!isJxlData(inputData)) {
            return failedJxlCatalog();
        }

        data = std::move(inputData);
        if (!initializeDecoder(JXL_DEC_BASIC_INFO | JXL_DEC_FRAME)) {
            const bool resourceLimitExceeded
                = allocationLimitExceeded.load(std::memory_order_relaxed);
            reset();
            return failedJxlCatalog(resourceLimitExceeded);
        }

        JxlBasicInfo catalogBasicInfo {};
        QSize logicalSize;
        QVector<int> durations;
        int repeatCount = 0;
        bool haveBasicInfo = false;
        bool currentFrameNeedsSkip = false;
        while (true) {
            const JxlDecoderStatus status = JxlDecoderProcessInput(decoder.get());
            if (status == JXL_DEC_BASIC_INFO) {
                const std::optional<QSize> decodedSize
                    = JxlDecoderGetBasicInfo(decoder.get(), &catalogBasicInfo) == JXL_DEC_SUCCESS
                    ? imageSizeForInfo(catalogBasicInfo)
                    : std::nullopt;
                if (!decodedSize.has_value() || catalogBasicInfo.have_animation != JXL_TRUE) {
                    reset();
                    return failedJxlCatalog();
                }
                logicalSize = *decodedSize;
                repeatCount = animationLoopCountForPlayCount(catalogBasicInfo.animation.num_loops);
                haveBasicInfo = true;
                continue;
            }
            if (status == JXL_DEC_FRAME) {
                if (!haveBasicInfo
                    || durations.size() >= ImageSequenceLimits::maximumFrameCount()) {
                    reset();
                    return failedJxlCatalog();
                }
                JxlFrameHeader header {};
                if (JxlDecoderGetFrameHeader(decoder.get(), &header) != JXL_DEC_SUCCESS) {
                    reset();
                    return failedJxlCatalog();
                }
                durations.append(normalizedAnimationFrameDelay(
                    jxlFrameDelay(header.duration, catalogBasicInfo.animation.tps_numerator,
                        catalogBasicInfo.animation.tps_denominator)));
                currentFrameNeedsSkip = true;
                continue;
            }
            if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER && currentFrameNeedsSkip) {
                if (JxlDecoderSkipCurrentFrame(decoder.get()) != JXL_DEC_SUCCESS) {
                    reset();
                    return failedJxlCatalog();
                }
                currentFrameNeedsSkip = false;
                continue;
            }
            if (status == JXL_DEC_SUCCESS) {
                ImageAnimationSourceCatalog catalog {
                    logicalSize,
                    std::move(durations),
                    repeatCount,
                };
                reset();
                return catalog.isValid() ? ImageAnimationSourceCatalogResult(std::move(catalog))
                                         : failedJxlCatalog();
            }

            const bool resourceLimitExceeded
                = allocationLimitExceeded.load(std::memory_order_relaxed);
            reset();
            return failedJxlCatalog(resourceLimitExceeded);
        }
    }

    AnimationFrameReadResult readNextFrame()
    {
        JxlReadResult result = readFrame();
        if (result.status == JxlReadStatus::Frame) {
            return std::optional<AnimationFrame>(std::move(result.frame));
        }
        if (result.status == JxlReadStatus::Error || result.status == JxlReadStatus::NotAnimation) {
            const bool resourceLimitExceeded
                = allocationLimitExceeded.load(std::memory_order_relaxed);
            QString errorString = result.errorString.isEmpty() ? jxlAnimationDecodeErrorString()
                                                               : result.errorString;
            if (resourceLimitExceeded) {
                reset();
                allocationLimitExceeded.store(true, std::memory_order_relaxed);
            }
            return std::unexpected(std::move(errorString));
        }
        return std::optional<AnimationFrame>();
    }

    [[nodiscard]] bool lastReadResourceLimitExceeded() const
    {
        return allocationLimitExceeded.load(std::memory_order_relaxed);
    }

    void reset()
    {
        decoder.reset();
        runner.reset();
        data.clear();
        basicInfo = {};
        imageSize = {};
        currentFrameDelay = 0;
        loopCount = 0;
        basicInfoAvailable = false;
        frameBuffer = std::vector<std::uint8_t> {};
        {
            const std::scoped_lock lock(allocatorMutex);
            reservedDecoderByteCount = 0;
            freeDecoderByteCount = 0;
        }
        applicationWorkspaceByteCount = 0;
        outputByteCount = 0;
        transientWorkspaceLease = {};
    }

private:
    struct alignas(std::max_align_t) AllocationHeader
    {
        std::size_t byteCount = 0;
    };

    static void* allocateDecoderMemory(void* opaque, std::size_t byteCount) noexcept
    {
        auto* self = static_cast<Private*>(opaque);
        if (self == nullptr || byteCount == 0
            || byteCount > std::numeric_limits<std::size_t>::max() - sizeof(AllocationHeader)
            || byteCount + sizeof(AllocationHeader)
                > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
            if (self != nullptr) {
                self->allocationLimitExceeded.store(true, std::memory_order_relaxed);
            }
            return nullptr;
        }

        const std::size_t allocationByteCount = sizeof(AllocationHeader) + byteCount;
        std::size_t reusedByteCount = 0;
        qsizetype additionalByteCount = 0;
        {
            const std::scoped_lock lock(self->allocatorMutex);
            reusedByteCount = std::min(allocationByteCount, self->freeDecoderByteCount);
            self->freeDecoderByteCount -= reusedByteCount;
            additionalByteCount = static_cast<qsizetype>(allocationByteCount - reusedByteCount);
            if (!self->transientWorkspaceLease.tryReserve(additionalByteCount)) {
                self->freeDecoderByteCount += reusedByteCount;
                self->allocationLimitExceeded.store(true, std::memory_order_relaxed);
                return nullptr;
            }
            self->reservedDecoderByteCount += additionalByteCount;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc) -- libjxl requires a C allocator callback.
        void* allocation = std::malloc(allocationByteCount);
        if (allocation == nullptr) {
            const std::scoped_lock lock(self->allocatorMutex);
            self->freeDecoderByteCount += reusedByteCount;
            self->reservedDecoderByteCount -= additionalByteCount;
            const bool released = self->transientWorkspaceLease.release(additionalByteCount);
            Q_ASSERT(released);
            self->allocationLimitExceeded.store(true, std::memory_order_relaxed);
            return nullptr;
        }
        auto* header = static_cast<AllocationHeader*>(allocation);
        header->byteCount = allocationByteCount;
        return header + 1;
    }

    static void freeDecoderMemory(void* opaque, void* address) noexcept
    {
        if (address == nullptr) {
            return;
        }
        auto* self = static_cast<Private*>(opaque);
        auto* header = static_cast<AllocationHeader*>(address) - 1;
        const std::size_t byteCount = header->byteCount;
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc) -- paired libjxl C allocator callback.
        std::free(header);
        if (self != nullptr) {
            const std::scoped_lock lock(self->allocatorMutex);
            self->freeDecoderByteCount += byteCount;
        }
    }

    bool initializeDecoder(int events = JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE)
    {
        transientWorkspaceLease = m_workspaceBudget->startLease();
        const JxlMemoryManager memoryManager {
            this,
            &Private::allocateDecoderMemory,
            &Private::freeDecoderMemory,
        };
        decoder = JxlDecoderPtr(JxlDecoderCreate(&memoryManager));
        if (decoder == nullptr) {
            return false;
        }

        runner = JxlThreadRunnerPtr(JxlThreadParallelRunnerCreate(
            &memoryManager, JxlThreadParallelRunnerDefaultNumWorkerThreads()));
        if (runner == nullptr
            || JxlDecoderSetParallelRunner(decoder.get(), JxlThreadParallelRunner, runner.get())
                != JXL_DEC_SUCCESS) {
            return false;
        }

        if (JxlDecoderSubscribeEvents(decoder.get(), events) != JXL_DEC_SUCCESS) {
            return false;
        }

        const std::span<const std::uint8_t> bytes = jxlBytes(data);
        if (JxlDecoderSetInput(decoder.get(), bytes.data(), bytes.size()) != JXL_DEC_SUCCESS) {
            return false;
        }
        JxlDecoderCloseInput(decoder.get());
        return true;
    }

    JxlReadResult readFrame()
    {
        if (decoder == nullptr) {
            return errorReadResult(jxlAnimationDecodeErrorString());
        }

        while (true) {
            const JxlDecoderStatus status = JxlDecoderProcessInput(decoder.get());
            switch (status) {
            case JXL_DEC_BASIC_INFO:
                if (!readBasicInfo()) {
                    return errorReadResult(jxlAnimationDecodeErrorString());
                }
                if (basicInfo.have_animation != JXL_TRUE) {
                    return JxlReadResult { JxlReadStatus::NotAnimation, {}, {} };
                }
                break;
            case JXL_DEC_FRAME:
                if (!readFrameHeader()) {
                    return errorReadResult(jxlAnimationDecodeErrorString());
                }
                break;
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
                if (!setImageOutBuffer()) {
                    return errorReadResult(jxlAnimationDecodeErrorString());
                }
                break;
            case JXL_DEC_FULL_IMAGE:
                if (ImageDecodeWorkspaceLease outputLease
                    = m_workspaceBudget->startLeaseForOperation(
                        transientWorkspaceLease.reservedByteCount());
                    outputLease.tryReserve(outputByteCount)) {
                    std::optional<QImage> image = imageFromRgbaBuffer(frameBuffer, imageSize);
                    if (!image.has_value()) {
                        allocationLimitExceeded.store(true, std::memory_order_relaxed);
                        return errorReadResult(jxlAnimationDecodeErrorString());
                    }
                    return JxlReadResult {
                        JxlReadStatus::Frame,
                        AnimationFrame {
                            std::move(*image), currentFrameDelay, outputLease.sharedHold() },
                        {},
                    };
                }
                allocationLimitExceeded.store(true, std::memory_order_relaxed);
                return errorReadResult(imageDecodeWorkspaceResourceLimitDiagnostic());
            case JXL_DEC_SUCCESS:
                return JxlReadResult { JxlReadStatus::End, {}, {} };
            case JXL_DEC_ERROR:
            case JXL_DEC_NEED_MORE_INPUT:
            default:
                return errorReadResult(jxlAnimationDecodeErrorString());
            }
        }
    }

    bool readBasicInfo()
    {
        if (JxlDecoderGetBasicInfo(decoder.get(), &basicInfo) != JXL_DEC_SUCCESS) {
            return false;
        }
        std::optional<QSize> decodedSize = imageSizeForInfo(basicInfo);
        if (!decodedSize.has_value()) {
            return false;
        }

        imageSize = *decodedSize;
        loopCount = animationLoopCountForPlayCount(basicInfo.animation.num_loops);
        basicInfoAvailable = true;
        return true;
    }

    bool readFrameHeader()
    {
        if (!basicInfoAvailable) {
            return false;
        }

        JxlFrameHeader frameHeader;
        if (JxlDecoderGetFrameHeader(decoder.get(), &frameHeader) != JXL_DEC_SUCCESS) {
            return false;
        }
        currentFrameDelay = jxlFrameDelay(frameHeader.duration, basicInfo.animation.tps_numerator,
            basicInfo.animation.tps_denominator);
        return true;
    }

    bool setImageOutBuffer()
    {
        if (!basicInfoAvailable) {
            return false;
        }

        const JxlPixelFormat format = rgbaPixelFormat();
        std::size_t bufferSize = 0;
        if (JxlDecoderImageOutBufferSize(decoder.get(), &format, &bufferSize) != JXL_DEC_SUCCESS) {
            return false;
        }
        if (bufferSize == 0
            || bufferSize > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
            return false;
        }

        const std::optional<qsizetype> outputByteCount
            = checkedImageDecodeWorkspaceByteCount(imageSize, 4, 1);
        const std::optional<qsizetype> workspaceByteCount
            = checkedImageDecodeWorkspaceByteCount(imageSize, 4, 2);
        if (!outputByteCount.has_value() || !workspaceByteCount.has_value()
            || std::cmp_not_equal(*outputByteCount, bufferSize)) {
            allocationLimitExceeded.store(true, std::memory_order_relaxed);
            return false;
        }
        if (applicationWorkspaceByteCount == 0) {
            if (!transientWorkspaceLease.tryReserve(*workspaceByteCount)) {
                allocationLimitExceeded.store(true, std::memory_order_relaxed);
                return false;
            }
            try {
                frameBuffer.resize(bufferSize);
            } catch (const std::bad_alloc&) {
                const bool released = transientWorkspaceLease.release(*workspaceByteCount);
                Q_ASSERT(released);
                allocationLimitExceeded.store(true, std::memory_order_relaxed);
                return false;
            }
            applicationWorkspaceByteCount = *workspaceByteCount;
            this->outputByteCount = *outputByteCount;
        } else if (frameBuffer.size() != bufferSize) {
            return false;
        }
        return JxlDecoderSetImageOutBuffer(
                   decoder.get(), &format, frameBuffer.data(), frameBuffer.size())
            == JXL_DEC_SUCCESS;
    }

    std::shared_ptr<ImageDecodeWorkspaceBudget> m_workspaceBudget;
    ImageDecodeWorkspaceLease transientWorkspaceLease;
    qsizetype applicationWorkspaceByteCount = 0;
    qsizetype outputByteCount = 0;
    std::atomic_bool allocationLimitExceeded = false;
    std::mutex allocatorMutex;
    qsizetype reservedDecoderByteCount = 0;
    std::size_t freeDecoderByteCount = 0;
    QByteArray data;
    JxlDecoderPtr decoder;
    JxlThreadRunnerPtr runner;
    JxlBasicInfo basicInfo = {};
    QSize imageSize;
    int currentFrameDelay = 0;
    int loopCount = 0;
    bool basicInfoAvailable = false;
    std::vector<std::uint8_t> frameBuffer;
};

JxlAnimationReader::JxlAnimationReader()
    : JxlAnimationReader(defaultImageDecodeWorkspaceBudget())
{
}

JxlAnimationReader::JxlAnimationReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
    : d(std::make_unique<Private>(std::move(workspaceBudget)))
{
}

JxlAnimationReader::~JxlAnimationReader() = default;

JxlAnimationReader::JxlAnimationReader(JxlAnimationReader&&) noexcept = default;

JxlAnimationReader& JxlAnimationReader::operator=(JxlAnimationReader&&) noexcept = default;

JxlAnimationOpenResult JxlAnimationReader::open(QByteArray data)
{
    return d->open(std::move(data));
}

ImageAnimationSourceCatalogResult JxlAnimationReader::readSourceCatalog(QByteArray data)
{
    return d->readSourceCatalog(std::move(data));
}

AnimationFrameReadResult JxlAnimationReader::readNextFrame() { return d->readNextFrame(); }

bool JxlAnimationReader::lastReadResourceLimitExceeded() const
{
    return d->lastReadResourceLimitExceeded();
}

void JxlAnimationReader::close() { (*d).reset(); }
}
