// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/svgworkerlimits.h"

#include "kiriview/src/support/svgrenderer.cxx.h"

#include <QByteArray>
#include <QFile>
#include <QtEndian>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sys/resource.h>

namespace {
constexpr int workerRuntimeFailureExitCode = 5;

bool installAddressSpaceLimit()
{
    const auto limit = static_cast<rlim_t>(kiriview::svgWorkerAddressSpaceByteLimit);
    const rlimit limits { limit, limit };
    return ::setrlimit(RLIMIT_AS, &limits) == 0;
}

QByteArray readInput()
{
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) {
        return {};
    }
    return input.readAll();
}

rust::Slice<const std::uint8_t> rustBytes(const QByteArray& bytes)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- CXX byte bridge.
    return { reinterpret_cast<const std::uint8_t*>(bytes.constData()),
        static_cast<std::size_t>(bytes.size()) };
}

bool writeAll(const void* data, std::size_t size)
{
    return size == 0 || std::fwrite(data, 1, size, stdout) == size;
}

int intrinsicSize(const QByteArray& input)
{
    kiriview::RustSvgOperationStatus parseStatus = kiriview::RustSvgOperationStatus::Success;
    const kiriview::RustSvgImageSize size
        = kiriview::rustSvgIntrinsicSize(rustBytes(input), parseStatus);
    if (parseStatus == kiriview::RustSvgOperationStatus::DecodeError) {
        return kiriview::svgWorkerDecodeErrorExitCode;
    }
    if (parseStatus == kiriview::RustSvgOperationStatus::ResourceExhausted) {
        return kiriview::svgWorkerResourceExhaustedExitCode;
    }
    if (parseStatus != kiriview::RustSvgOperationStatus::Success || !size.valid || size.width <= 0
        || size.height <= 0) {
        return workerRuntimeFailureExitCode;
    }

    uchar encoded[8] {};
    qToBigEndian<qint32>(size.width, encoded);
    qToBigEndian<qint32>(size.height, encoded + 4);
    return writeAll(encoded, sizeof(encoded)) ? EXIT_SUCCESS : workerRuntimeFailureExitCode;
}

bool parsePositiveInt(const char* text, int* value)
{
    if (text == nullptr || *text == '\0' || value == nullptr) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed <= 0
        || parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

int render(const QByteArray& input, const char* widthText, const char* heightText)
{
    int width = 0;
    int height = 0;
    if (!parsePositiveInt(widthText, &width) || !parsePositiveInt(heightText, &height)) {
        return workerRuntimeFailureExitCode;
    }
    const std::uint64_t expectedByteCount
        = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4;
    if (expectedByteCount > static_cast<std::uint64_t>(std::numeric_limits<qsizetype>::max())) {
        return workerRuntimeFailureExitCode;
    }

    kiriview::RustSvgOperationStatus renderStatus = kiriview::RustSvgOperationStatus::Success;
    const rust::Vec<std::uint8_t> bytes
        = kiriview::rustRenderSvgImage(rustBytes(input), width, height, renderStatus);
    if (renderStatus == kiriview::RustSvgOperationStatus::DecodeError) {
        return kiriview::svgWorkerDecodeErrorExitCode;
    }
    if (renderStatus == kiriview::RustSvgOperationStatus::ResourceExhausted) {
        return kiriview::svgWorkerResourceExhaustedExitCode;
    }
    if (renderStatus != kiriview::RustSvgOperationStatus::Success) {
        return workerRuntimeFailureExitCode;
    }
    if (bytes.size() != static_cast<std::size_t>(expectedByteCount)) {
        return workerRuntimeFailureExitCode;
    }
    return writeAll(bytes.data(), bytes.size()) ? EXIT_SUCCESS : workerRuntimeFailureExitCode;
}
}

int main(int argumentCount, char* arguments[])
{
    if (!installAddressSpaceLimit() || argumentCount < 2) {
        return workerRuntimeFailureExitCode;
    }

    const QByteArray input = readInput();
    if (input.isEmpty()) {
        return kiriview::svgWorkerDecodeErrorExitCode;
    }
    if (argumentCount == 2 && std::strcmp(arguments[1], "intrinsic") == 0) {
        return intrinsicSize(input);
    }
    if (argumentCount == 4 && std::strcmp(arguments[1], "render") == 0) {
        return render(input, arguments[2], arguments[3]);
    }
    return workerRuntimeFailureExitCode;
}
