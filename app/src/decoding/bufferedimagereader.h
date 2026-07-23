// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_BUFFEREDIMAGEREADER_H
#define KIRIVIEW_BUFFEREDIMAGEREADER_H

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageIOHandler>
#include <QRect>
#include <QSize>
#include <QString>
#include <memory>

class QImageReader;

namespace kiriview {
class BufferedImageReader final
{
public:
    explicit BufferedImageReader(
        const QByteArray& data, const QByteArray& format, bool autoTransform = true);
    ~BufferedImageReader();

    explicit operator bool() const;
    [[nodiscard]] bool canRead() const;
    [[nodiscard]] bool supportsAnimation() const;
    [[nodiscard]] QSize size() const;
    [[nodiscard]] QByteArray format() const;
    [[nodiscard]] QImageIOHandler::Transformations transformation() const;
    [[nodiscard]] int nextImageDelay() const;
    [[nodiscard]] int loopCount() const;
    [[nodiscard]] QString errorString() const;

    void setScaledSize(QSize size);
    void setScaledClipRect(QRect rect);
    void setClipRect(QRect rect);
    QImage read();

private:
    QBuffer m_buffer;
    std::unique_ptr<QImageReader> m_reader;
};
}

#endif
