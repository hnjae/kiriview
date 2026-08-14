// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/avifcompatibility.h"

#include <QtEndian>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace {
constexpr qsizetype boxHeaderSize = 8;
constexpr qsizetype fullBoxFieldsSize = 4;
constexpr qsizetype ipmaEntriesOffset = boxHeaderSize + fullBoxFieldsSize + 4;

struct BoxHeader
{
    qsizetype offset = 0;
    qsizetype size = 0;
    qsizetype headerSize = 0;
    std::array<char, 4> kind {};

    [[nodiscard]] qsizetype bodyOffset() const { return offset + headerSize; }
    [[nodiscard]] qsizetype endOffset() const { return offset + size; }
    [[nodiscard]] bool isType(const char (&expected)[5]) const
    {
        return std::ranges::equal(kind, std::string_view(expected, kind.size()));
    }
};

struct FullBox
{
    BoxHeader box;
    std::array<char, 4> versionAndFlags {};
    qsizetype payloadOffset = 0;
};

struct IpmaBox
{
    BoxHeader box;
    std::array<char, 4> versionAndFlags {};
    quint32 entryCount = 0;
    QByteArrayView entries;
};

template <typename T> std::optional<T> readBigEndian(QByteArrayView data, qsizetype offset)
{
    if (offset < 0 || offset > data.size() - static_cast<qsizetype>(sizeof(T))) {
        return std::nullopt;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- Qt byte API.
    return qFromBigEndian<T>(reinterpret_cast<const uchar*>(data.data() + offset));
}

template <typename T> bool writeBigEndian(QByteArray& data, qsizetype offset, T value)
{
    if (offset < 0 || offset > data.size() - static_cast<qsizetype>(sizeof(T))) {
        return false;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- Qt byte API.
    qToBigEndian<T>(value, reinterpret_cast<uchar*>(data.data() + offset));
    return true;
}

std::optional<BoxHeader> readBox(QByteArrayView data, qsizetype offset, qsizetype end)
{
    if (offset < 0 || end < offset || end > data.size() || end - offset < boxHeaderSize) {
        return std::nullopt;
    }
    const std::optional<quint32> smallSize = readBigEndian<quint32>(data, offset);
    if (!smallSize.has_value()) {
        return std::nullopt;
    }
    BoxHeader box { offset, static_cast<qsizetype>(*smallSize), boxHeaderSize, {} };
    std::ranges::copy_n(data.data() + offset + 4, 4, box.kind.begin());
    if (*smallSize == 1) {
        const std::optional<quint64> largeSize
            = readBigEndian<quint64>(data, offset + boxHeaderSize);
        if (!largeSize.has_value()
            || *largeSize > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
            return std::nullopt;
        }
        box.headerSize = 16;
        box.size = static_cast<qsizetype>(*largeSize);
    } else if (*smallSize == 0) {
        box.size = end - offset;
    }
    if (box.size < box.headerSize || box.size > end - offset) {
        return std::nullopt;
    }
    return box;
}

template <typename Visitor>
bool forEachChildBox(QByteArrayView data, qsizetype offset, qsizetype end, const Visitor& visitor)
{
    while (offset < end) {
        const std::optional<BoxHeader> box = readBox(data, offset, end);
        if (!box.has_value()) {
            return false;
        }
        visitor(*box);
        offset = box->endOffset();
    }
    return offset == end;
}

bool validChildBoxes(QByteArrayView data, qsizetype offset, qsizetype end)
{
    return forEachChildBox(data, offset, end, [](BoxHeader) { });
}

std::optional<FullBox> readFullBox(
    QByteArrayView data, BoxHeader box, const char (&kind)[5], qsizetype minimumPayload)
{
    if (!box.isType(kind) || box.bodyOffset() > box.endOffset() - fullBoxFieldsSize) {
        return std::nullopt;
    }
    FullBox full { box, {}, box.bodyOffset() + fullBoxFieldsSize };
    if (full.payloadOffset > box.endOffset() - minimumPayload) {
        return std::nullopt;
    }
    std::ranges::copy_n(data.data() + box.bodyOffset(), 4, full.versionAndFlags.begin());
    return full;
}

bool hasAvifBrand(QByteArrayView data)
{
    bool found = false;
    const bool valid = forEachChildBox(data, 0, data.size(), [&](const BoxHeader& box) {
        if (!box.isType("ftyp") || box.size < box.headerSize + 8) {
            return;
        }
        for (qsizetype offset = box.bodyOffset(); offset <= box.endOffset() - 4; offset += 4) {
            const QByteArrayView brand = data.sliced(offset, 4);
            if (brand == QByteArrayView("avif", 4) || brand == QByteArrayView("avis", 4)) {
                found = true;
            }
        }
    });
    return valid && found;
}

std::optional<FullBox> metaFullBox(QByteArrayView data, BoxHeader meta)
{
    const std::optional<FullBox> full = readFullBox(data, meta, "meta", 0);
    if (!full.has_value() || !validChildBoxes(data, full->payloadOffset, meta.endOffset())) {
        return std::nullopt;
    }
    return full;
}

template <typename Visitor>
void forEachIprpGroup(QByteArrayView data, const FullBox& meta, const Visitor& visitor)
{
    (void)forEachChildBox(
        data, meta.payloadOffset, meta.box.endOffset(), [&](const BoxHeader& box) {
            if (box.isType("iprp") && validChildBoxes(data, box.bodyOffset(), box.endOffset())) {
                visitor(box);
            }
        });
}

bool hasAlphaProperty(QByteArrayView data, const FullBox& meta)
{
    bool found = false;
    forEachIprpGroup(data, meta, [&](const BoxHeader& iprp) {
        (void)forEachChildBox(data, iprp.bodyOffset(), iprp.endOffset(), [&](const BoxHeader& box) {
            if (!box.isType("ipco") || !validChildBoxes(data, box.bodyOffset(), box.endOffset())) {
                return;
            }
            (void)forEachChildBox(
                data, box.bodyOffset(), box.endOffset(), [&](const BoxHeader& property) {
                    if (property.isType("auxC")
                        && data.sliced(property.bodyOffset(), property.size - property.headerSize)
                                .indexOf(QByteArrayView("alpha", 5))
                            >= 0) {
                        found = true;
                    }
                });
        });
    });
    return found;
}

std::optional<quint32> readItemId(QByteArrayView data, qsizetype offset, qsizetype size)
{
    if (size == 2) {
        const std::optional<quint16> value = readBigEndian<quint16>(data, offset);
        return value.has_value() ? std::optional<quint32>(*value) : std::nullopt;
    }
    return size == 4 ? readBigEndian<quint32>(data, offset) : std::nullopt;
}

bool writeItemId(QByteArray& data, qsizetype offset, qsizetype size, quint32 value)
{
    if (size == 2) {
        return value <= std::numeric_limits<quint16>::max()
            && writeBigEndian<quint16>(data, offset, static_cast<quint16>(value));
    }
    return size == 4 && writeBigEndian<quint32>(data, offset, value);
}

std::optional<quint32> primaryItemId(QByteArrayView data, const FullBox& meta)
{
    std::optional<quint32> primary;
    (void)forEachChildBox(
        data, meta.payloadOffset, meta.box.endOffset(), [&](const BoxHeader& box) {
            if (primary.has_value()) {
                return;
            }
            const std::optional<FullBox> pitm = readFullBox(data, box, "pitm", 0);
            if (!pitm.has_value()) {
                return;
            }
            const qsizetype size = pitm->versionAndFlags[0] == 0 ? 2
                : pitm->versionAndFlags[0] == 1                  ? 4
                                                                 : 0;
            if (size != 0 && pitm->payloadOffset <= box.endOffset() - size) {
                primary = readItemId(data, pitm->payloadOffset, size);
            }
        });
    return primary;
}

bool patchAuxiliaryReferences(
    QByteArray& target, QByteArrayView source, const FullBox& meta, quint32 primaryId)
{
    bool changed = false;
    (void)forEachChildBox(
        source, meta.payloadOffset, meta.box.endOffset(), [&](const BoxHeader& box) {
            const std::optional<FullBox> iref = readFullBox(source, box, "iref", 0);
            if (!iref.has_value()) {
                return;
            }
            const qsizetype idSize = iref->versionAndFlags[0] == 1 ? 4 : 2;
            qsizetype offset = iref->payloadOffset;
            while (offset < box.endOffset()) {
                const std::optional<BoxHeader> reference = readBox(source, offset, box.endOffset());
                if (!reference.has_value()) {
                    break;
                }
                qsizetype cursor = reference->bodyOffset();
                if (cursor > reference->endOffset() - idSize - 2) {
                    break;
                }
                cursor += idSize;
                const std::optional<quint16> count = readBigEndian<quint16>(source, cursor);
                if (!count.has_value()) {
                    break;
                }
                cursor += 2;
                if (reference->isType("auxl")) {
                    for (quint16 index = 0;
                        index < *count && cursor <= reference->endOffset() - idSize;
                        ++index, cursor += idSize) {
                        if (readItemId(source, cursor, idSize) == 0
                            && writeItemId(target, cursor, idSize, primaryId)) {
                            changed = true;
                        }
                    }
                }
                offset = reference->endOffset();
            }
        });
    return changed;
}

std::optional<IpmaBox> readIpma(QByteArrayView data, BoxHeader box)
{
    const std::optional<FullBox> full = readFullBox(data, box, "ipma", 4);
    if (!full.has_value() || box.headerSize != boxHeaderSize) {
        return std::nullopt;
    }
    const std::optional<quint32> count = readBigEndian<quint32>(data, full->payloadOffset);
    if (!count.has_value()) {
        return std::nullopt;
    }
    return IpmaBox { box, full->versionAndFlags, *count,
        data.sliced(full->payloadOffset + 4, box.endOffset() - full->payloadOffset - 4) };
}

bool writeIpmaHeader(QByteArray& target, qsizetype offset, qsizetype size,
    const std::array<char, 4>& flags, quint32 count)
{
    if (size < ipmaEntriesOffset || std::cmp_greater(size, std::numeric_limits<quint32>::max())
        || offset < 0 || offset > target.size() - size
        || !writeBigEndian<quint32>(target, offset, static_cast<quint32>(size))) {
        return false;
    }
    std::memcpy(target.data() + offset + 4, "ipma", 4);
    std::ranges::copy(flags, target.data() + offset + boxHeaderSize);
    return writeBigEndian<quint32>(target, offset + boxHeaderSize + fullBoxFieldsSize, count);
}

bool mergeIpma(QByteArray& target, const IpmaBox& first, const IpmaBox& second)
{
    if (first.versionAndFlags != second.versionAndFlags
        || first.box.endOffset() != second.box.offset
        || first.entryCount > std::numeric_limits<quint32>::max() - second.entryCount
        || first.entries.size() > std::numeric_limits<qsizetype>::max() - second.entries.size()) {
        return false;
    }
    const qsizetype entriesSize = first.entries.size() + second.entries.size();
    const qsizetype mergedSize = ipmaEntriesOffset + entriesSize;
    const qsizetype available = first.box.size + second.box.size;
    if (available - mergedSize != ipmaEntriesOffset
        || first.box.offset > target.size() - available) {
        return false;
    }

    const qsizetype secondEntriesTarget
        = first.box.offset + ipmaEntriesOffset + first.entries.size();
    std::memmove(target.data() + secondEntriesTarget, second.entries.data(),
        static_cast<std::size_t>(second.entries.size()));
    if (!writeIpmaHeader(target, first.box.offset, mergedSize, first.versionAndFlags,
            first.entryCount + second.entryCount)) {
        return false;
    }
    std::array<char, 4> alternate = first.versionAndFlags;
    alternate[0] = alternate[0] == 0 ? 1 : 0;
    return writeIpmaHeader(target, first.box.offset + mergedSize, ipmaEntriesOffset, alternate, 0);
}

bool patchDuplicateIpma(QByteArray& target, QByteArrayView source, const FullBox& meta)
{
    bool changed = false;
    forEachIprpGroup(source, meta, [&](const BoxHeader& iprp) {
        std::optional<IpmaBox> previous;
        (void)forEachChildBox(
            source, iprp.bodyOffset(), iprp.endOffset(), [&](const BoxHeader& box) {
                const std::optional<IpmaBox> current = readIpma(source, box);
                if (!current.has_value()) {
                    previous.reset();
                } else if (previous.has_value() && mergeIpma(target, *previous, *current)) {
                    changed = true;
                    previous.reset();
                } else {
                    previous = current;
                }
            });
    });
    return changed;
}
}

namespace kiriview {
std::optional<qsizetype> avifCompatibilityWorkspaceByteCost(qsizetype sourceByteCount)
{
    return sourceByteCount < 0 ? std::nullopt : std::optional<qsizetype>(sourceByteCount);
}

AvifCompatibleData avifDataWithCompatibilityFixes(const QByteArray& data)
{
    const QByteArrayView source(data);
    if (!validChildBoxes(source, 0, source.size()) || !hasAvifBrand(source)) {
        return { data, AvifCompatibleDataStorage::Original };
    }

    QByteArray fixed(data.size(), Qt::Uninitialized);
    if (!data.isEmpty()) {
        std::memcpy(fixed.data(), data.constData(), static_cast<std::size_t>(data.size()));
    }
    bool changed = false;
    (void)forEachChildBox(source, 0, source.size(), [&](const BoxHeader& box) {
        if (!box.isType("meta")) {
            return;
        }
        const std::optional<FullBox> meta = metaFullBox(source, box);
        if (!meta.has_value() || !hasAlphaProperty(source, *meta)) {
            return;
        }
        const std::optional<quint32> primary = primaryItemId(source, *meta);
        if (primary.has_value() && *primary != 0) {
            changed = patchAuxiliaryReferences(fixed, source, *meta, *primary) || changed;
        }
        changed = patchDuplicateIpma(fixed, source, *meta) || changed;
    });
    return changed
        ? AvifCompatibleData { std::move(fixed), AvifCompatibleDataStorage::OwnedReplacement }
        : AvifCompatibleData { data, AvifCompatibleDataStorage::Original };
}
}
