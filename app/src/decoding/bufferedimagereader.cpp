// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "bufferedimagereader.h"

#include "imagedecodeworkspace.h"

#include <QIODevice>
#include <QImageReader>
#include <limits>
#include <memory>

namespace kiriview {
std::optional<qsizetype> qImageReaderGifTransientWorkspaceByteCount(QSize logicalSize)
{
    // Qt's GIF handler may retain a canvas and disposal backing while producing
    // an output-conversion source. Keep fixed decoder state outside that pixel term.
    constexpr qsizetype fixedDecoderHeadroomByteCount = qsizetype { 1 } * 1024 * 1024;
    const std::optional<qsizetype> pixelByteCount
        = checkedImageDecodeWorkspaceByteCount(logicalSize, 8, 3);
    if (!pixelByteCount.has_value()
        || *pixelByteCount
            > std::numeric_limits<qsizetype>::max() - fixedDecoderHeadroomByteCount) {
        return std::nullopt;
    }
    return *pixelByteCount + fixedDecoderHeadroomByteCount;
}

BufferedImageReader::BufferedImageReader(
    const QByteArray& data, const QByteArray& format, bool autoTransform)
{
    if (format.isEmpty()) {
        return;
    }

    m_buffer.setData(data);
    if (!m_buffer.open(QIODevice::ReadOnly)) {
        return;
    }

    m_reader = std::make_unique<QImageReader>(&m_buffer, format);
    m_reader->setAutoTransform(autoTransform);
}

BufferedImageReader::~BufferedImageReader() = default;

BufferedImageReader::operator bool() const { return m_reader != nullptr; }

bool BufferedImageReader::canRead() const { return m_reader != nullptr && m_reader->canRead(); }

bool BufferedImageReader::supportsAnimation() const
{
    return m_reader != nullptr && m_reader->supportsAnimation();
}

bool BufferedImageReader::supportsOption(QImageIOHandler::ImageOption option) const
{
    return m_reader != nullptr && m_reader->supportsOption(option);
}

QSize BufferedImageReader::size() const { return m_reader == nullptr ? QSize() : m_reader->size(); }

QImage::Format BufferedImageReader::imageFormat() const
{
    return m_reader == nullptr ? QImage::Format_Invalid : m_reader->imageFormat();
}

QByteArray BufferedImageReader::format() const
{
    return m_reader == nullptr ? QByteArray() : m_reader->format();
}

QImageIOHandler::Transformations BufferedImageReader::transformation() const
{
    return m_reader == nullptr ? QImageIOHandler::TransformationNone : m_reader->transformation();
}

int BufferedImageReader::nextImageDelay() const
{
    return m_reader == nullptr ? -1 : m_reader->nextImageDelay();
}

int BufferedImageReader::loopCount() const
{
    return m_reader == nullptr ? 0 : m_reader->loopCount();
}

QString BufferedImageReader::errorString() const
{
    return m_reader == nullptr ? QString() : m_reader->errorString();
}

void BufferedImageReader::setScaledSize(QSize size)
{
    if (m_reader != nullptr) {
        m_reader->setScaledSize(size);
    }
}

void BufferedImageReader::setScaledClipRect(QRect rect)
{
    if (m_reader != nullptr) {
        m_reader->setScaledClipRect(rect);
    }
}

void BufferedImageReader::setClipRect(QRect rect)
{
    if (m_reader != nullptr) {
        m_reader->setClipRect(rect);
    }
}

QImage BufferedImageReader::read() { return m_reader == nullptr ? QImage() : m_reader->read(); }
}
