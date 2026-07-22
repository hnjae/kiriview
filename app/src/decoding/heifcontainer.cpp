// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "heifcontainer.h"

#include <QByteArrayView>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::array<std::array<char, 4>, 13> stillImageBrands { { { 'a', 'v', 'c', 'i' },
    { 'a', 'v', 'i', 'f' }, { 'h', 'e', 'i', 'c' }, { 'h', 'e', 'i', 'f' }, { 'h', 'e', 'i', 'm' },
    { 'h', 'e', 'i', 's' }, { 'h', 'e', 'i', 'x' }, { 'h', 'e', 'j', '2' }, { 'j', '2', 'k', 'i' },
    { 'j', 'p', 'e', 'g' }, { 'm', 'i', 'f', '1' }, { 'm', 'i', 'f', '2' },
    { 'v', 'v', 'i', 'c' } } };
constexpr std::array<std::array<char, 4>, 10> imageSequenceBrands { { { 'a', 'v', 'c', 's' },
    { 'a', 'v', 'i', 's' }, { 'h', 'e', 'v', 'c' }, { 'h', 'e', 'v', 'm' }, { 'h', 'e', 'v', 's' },
    { 'h', 'e', 'v', 'x' }, { 'j', '2', 'i', 's' }, { 'j', 'p', 'g', 's' }, { 'm', 's', 'f', '1' },
    { 'v', 'v', 'i', 's' } } };

template <std::size_t Size>
bool containsBrand(const std::array<std::array<char, 4>, Size>& candidates, QByteArrayView brand)
{
    return std::ranges::any_of(candidates,
        [brand](const auto& candidate) { return brand == QByteArrayView(candidate.data(), 4); });
}

std::optional<quint32> readBigEndianU32(QByteArrayView data, qsizetype offset)
{
    if (offset < 0 || offset > data.size() - qsizetype(sizeof(quint32))) {
        return std::nullopt;
    }
    quint32 value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return qFromBigEndian(value);
}
}

namespace kiriview {
HeifBrandKind heifBrandKind(QByteArrayView brand)
{
    if (brand.size() != 4) {
        return HeifBrandKind::Unknown;
    }
    if (containsBrand(stillImageBrands, brand)) {
        return HeifBrandKind::StillImage;
    }
    return containsBrand(imageSequenceBrands, brand) ? HeifBrandKind::ImageSequence
                                                     : HeifBrandKind::Unknown;
}

bool isLikelyHeifContainer(const QByteArray& data) { return heifContainerInfo(data).isHeif(); }

bool isLikelyHeifStillImageContainer(const QByteArray& data)
{
    return heifContainerInfo(data).stillImage;
}

HeifContainerInfo heifContainerInfo(const QByteArray& data)
{
    constexpr qsizetype boxTypeOffset = 4;
    constexpr qsizetype majorBrandOffset = 8;
    constexpr qsizetype compatibleBrandsOffset = 16;
    constexpr qsizetype brandSize = 4;
    const QByteArrayView view(data);
    const std::optional<quint32> encodedSize = readBigEndianU32(view, 0);
    if (!encodedSize.has_value() || view.size() < compatibleBrandsOffset
        || view.sliced(boxTypeOffset, brandSize) != QByteArrayView("ftyp", brandSize)
        || *encodedSize < compatibleBrandsOffset || *encodedSize > quint32(view.size())) {
        return {};
    }

    HeifContainerInfo info;
    const auto recordBrand = [&info](QByteArrayView brand) {
        switch (heifBrandKind(brand)) {
        case HeifBrandKind::StillImage:
            info.stillImage = true;
            break;
        case HeifBrandKind::ImageSequence:
            info.imageSequence = true;
            break;
        case HeifBrandKind::Unknown:
            break;
        }
    };
    recordBrand(view.sliced(majorBrandOffset, brandSize));
    for (qsizetype offset = compatibleBrandsOffset; offset + brandSize <= *encodedSize;
        offset += brandSize) {
        recordBrand(view.sliced(offset, brandSize));
    }
    return info;
}
}
