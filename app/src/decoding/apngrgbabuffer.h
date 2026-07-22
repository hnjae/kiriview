// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APNGRGBABUFFER_H
#define KIRIVIEW_APNGRGBABUFFER_H

#include <QImage>
#include <QSize>
#include <QtGlobal>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace kiriview {
struct ApngRgbaRegion
{
    quint32 width = 0;
    quint32 height = 0;
    quint32 xOffset = 0;
    quint32 yOffset = 0;
};

class ApngRgbaBuffer final
{
public:
    static constexpr std::size_t bytesPerPixel = 4;

    bool initialize(QSize imageSize, std::size_t rowBytes);
    void clear();

    bool isValid() const;
    QSize imageSize() const;
    std::size_t rowBytes() const;
    std::span<unsigned char> bytes();
    std::span<const unsigned char> bytes() const;
    std::span<unsigned char> row(std::size_t y);
    std::span<const unsigned char> row(std::size_t y) const;

    bool contains(ApngRgbaRegion region) const;
    std::optional<std::size_t> rowOffset(quint32 x, quint32 y) const;
    std::optional<std::vector<unsigned char>> copyRegion(ApngRgbaRegion region) const;
    bool clearRegion(ApngRgbaRegion region);
    bool restoreRegion(ApngRgbaRegion region, std::span<const unsigned char> bytes);
    std::optional<QImage> imageCopy() const;

private:
    QSize m_imageSize;
    std::size_t m_rowBytes = 0;
    std::vector<unsigned char> m_bytes;
};
}

#endif
