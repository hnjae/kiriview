// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "heiftilingplan.h"

#include <algorithm>
#include <limits>

namespace kiriview {
std::optional<HeifTileGrid> heifTileGridForTiling(const heif_image_tiling& tiling)
{
    constexpr auto maximum = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
    if (tiling.num_columns == 0 || tiling.num_rows == 0 || tiling.tile_width == 0
        || tiling.tile_height == 0 || tiling.num_columns > maximum || tiling.num_rows > maximum
        || tiling.tile_width > maximum || tiling.tile_height > maximum
        || (tiling.num_columns == 1 && tiling.num_rows == 1)) {
        return std::nullopt;
    }
    return HeifTileGrid { static_cast<int>(tiling.num_columns), static_cast<int>(tiling.num_rows),
        static_cast<int>(tiling.tile_width), static_cast<int>(tiling.tile_height) };
}

std::vector<HeifTileDecodeRegion> heifTileDecodeRegions(HeifTileGrid grid, QRect sourceRect)
{
    if (sourceRect.isEmpty() || grid.columns <= 0 || grid.rows <= 0 || grid.tileWidth <= 0
        || grid.tileHeight <= 0) {
        return {};
    }
    const int firstTileX = std::max(0, sourceRect.x() / grid.tileWidth);
    const int firstTileY = std::max(0, sourceRect.y() / grid.tileHeight);
    const int lastTileX = std::min(grid.columns - 1, sourceRect.right() / grid.tileWidth);
    const int lastTileY = std::min(grid.rows - 1, sourceRect.bottom() / grid.tileHeight);
    if (firstTileX > lastTileX || firstTileY > lastTileY) {
        return {};
    }
    std::vector<HeifTileDecodeRegion> regions;
    regions.reserve(static_cast<std::size_t>(lastTileX - firstTileX + 1)
        * static_cast<std::size_t>(lastTileY - firstTileY + 1));
    for (int tileY = firstTileY; tileY <= lastTileY; ++tileY) {
        for (int tileX = firstTileX; tileX <= lastTileX; ++tileX) {
            const qint64 targetX = qint64(tileX) * grid.tileWidth - sourceRect.x();
            const qint64 targetY = qint64(tileY) * grid.tileHeight - sourceRect.y();
            regions.push_back({ tileX, tileY,
                QPoint(static_cast<int>(std::clamp(targetX, qint64(std::numeric_limits<int>::min()),
                           qint64(std::numeric_limits<int>::max()))),
                    static_cast<int>(std::clamp(targetY, qint64(std::numeric_limits<int>::min()),
                        qint64(std::numeric_limits<int>::max())))) });
        }
    }
    return regions;
}
}
