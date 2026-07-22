// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/avifcompatibility.h"

#include <QtEndian>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

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

    qsizetype bodyOffset() const { return offset + headerSize; }
    qsizetype endOffset() const { return offset + size; }
    bool isType(const char (&expected)[5]) const
    {
        return std::equal(kind.begin(), kind.end(), expected);
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
    QByteArray entries;
};

template <typename T> std::optional<T> readBigEndian(QByteArrayView data, qsizetype offset)
{
    if (offset < 0 || offset > data.size() - static_cast<qsizetype>(sizeof(T))) {
        return std::nullopt;
    }
    return qFromBigEndian<T>(reinterpret_cast<const uchar*>(data.data() + offset));
}

template <typename T> bool writeBigEndian(QByteArray& data, qsizetype offset, T value)
{
    if (offset < 0 || offset > data.size() - static_cast<qsizetype>(sizeof(T))) {
        return false;
    }
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
    std::copy_n(data.data() + offset + 4, 4, box.kind.begin());
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

std::optional<std::vector<BoxHeader>> childBoxes(
    QByteArrayView data, qsizetype offset, qsizetype end)
{
    std::vector<BoxHeader> boxes;
    while (offset < end) {
        const std::optional<BoxHeader> box = readBox(data, offset, end);
        if (!box.has_value()) {
            return std::nullopt;
        }
        boxes.push_back(*box);
        offset = box->endOffset();
    }
    return boxes;
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
    std::copy_n(data.data() + box.bodyOffset(), 4, full.versionAndFlags.begin());
    return full;
}

bool hasAvifBrand(QByteArrayView data, const std::vector<BoxHeader>& boxes)
{
    for (const BoxHeader& box : boxes) {
        if (!box.isType("ftyp") || box.size < box.headerSize + 8) {
            continue;
        }
        for (qsizetype offset = box.bodyOffset(); offset <= box.endOffset() - 4; offset += 4) {
            const QByteArrayView brand = data.sliced(offset, 4);
            if (brand == QByteArrayView("avif", 4) || brand == QByteArrayView("avis", 4)) {
                return true;
            }
        }
    }
    return false;
}

std::optional<std::vector<BoxHeader>> metaChildren(QByteArrayView data, BoxHeader meta)
{
    const std::optional<FullBox> full = readFullBox(data, meta, "meta", 0);
    return full.has_value() ? childBoxes(data, full->payloadOffset, meta.endOffset())
                            : std::nullopt;
}

std::vector<std::vector<BoxHeader>> iprpGroups(
    QByteArrayView data, const std::vector<BoxHeader>& meta)
{
    std::vector<std::vector<BoxHeader>> groups;
    for (const BoxHeader& box : meta) {
        if (box.isType("iprp")) {
            if (std::optional<std::vector<BoxHeader>> children
                = childBoxes(data, box.bodyOffset(), box.endOffset())) {
                groups.push_back(std::move(*children));
            }
        }
    }
    return groups;
}

bool hasAlphaProperty(QByteArrayView data, const std::vector<BoxHeader>& meta)
{
    for (const std::vector<BoxHeader>& iprp : iprpGroups(data, meta)) {
        for (const BoxHeader& box : iprp) {
            if (!box.isType("ipco")) {
                continue;
            }
            const std::optional<std::vector<BoxHeader>> properties
                = childBoxes(data, box.bodyOffset(), box.endOffset());
            if (!properties.has_value()) {
                continue;
            }
            for (const BoxHeader& property : *properties) {
                if (property.isType("auxC")
                    && data.sliced(property.bodyOffset(), property.size - property.headerSize)
                            .indexOf(QByteArrayView("alpha", 5))
                        >= 0) {
                    return true;
                }
            }
        }
    }
    return false;
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

std::optional<quint32> primaryItemId(QByteArrayView data, const std::vector<BoxHeader>& meta)
{
    for (const BoxHeader& box : meta) {
        const std::optional<FullBox> pitm = readFullBox(data, box, "pitm", 0);
        if (!pitm.has_value()) {
            continue;
        }
        const qsizetype size = pitm->versionAndFlags[0] == 0 ? 2
            : pitm->versionAndFlags[0] == 1                  ? 4
                                                             : 0;
        if (size != 0 && pitm->payloadOffset <= box.endOffset() - size) {
            return readItemId(data, pitm->payloadOffset, size);
        }
    }
    return std::nullopt;
}

bool patchAuxiliaryReferences(
    QByteArray& data, const std::vector<BoxHeader>& meta, quint32 primaryId)
{
    bool changed = false;
    for (const BoxHeader& box : meta) {
        const std::optional<FullBox> iref = readFullBox(data, box, "iref", 0);
        if (!iref.has_value()) {
            continue;
        }
        const qsizetype idSize = iref->versionAndFlags[0] == 1 ? 4 : 2;
        qsizetype offset = iref->payloadOffset;
        while (offset < box.endOffset()) {
            const std::optional<BoxHeader> reference = readBox(data, offset, box.endOffset());
            if (!reference.has_value()) {
                break;
            }
            qsizetype cursor = reference->bodyOffset();
            if (cursor > reference->endOffset() - idSize - 2) {
                break;
            }
            cursor += idSize;
            const std::optional<quint16> count = readBigEndian<quint16>(data, cursor);
            if (!count.has_value()) {
                break;
            }
            cursor += 2;
            if (reference->isType("auxl")) {
                for (quint16 index = 0; index < *count && cursor <= reference->endOffset() - idSize;
                    ++index, cursor += idSize) {
                    if (readItemId(data, cursor, idSize) == 0
                        && writeItemId(data, cursor, idSize, primaryId)) {
                        changed = true;
                    }
                }
            }
            offset = reference->endOffset();
        }
    }
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
        data.sliced(full->payloadOffset + 4, box.endOffset() - full->payloadOffset - 4)
            .toByteArray() };
}

bool writeIpma(QByteArray& target, std::array<char, 4> flags, quint32 count, QByteArrayView entries)
{
    if (target.size() != ipmaEntriesOffset + entries.size()
        || target.size() > std::numeric_limits<quint32>::max()
        || !writeBigEndian<quint32>(target, 0, static_cast<quint32>(target.size()))) {
        return false;
    }
    std::memcpy(target.data() + 4, "ipma", 4);
    std::ranges::copy(flags, target.data() + boxHeaderSize);
    if (!writeBigEndian<quint32>(target, boxHeaderSize + fullBoxFieldsSize, count)) {
        return false;
    }
    std::ranges::copy(entries, target.data() + ipmaEntriesOffset);
    return true;
}

bool mergeIpma(QByteArray& data, const IpmaBox& first, const IpmaBox& second)
{
    if (first.versionAndFlags != second.versionAndFlags
        || first.box.endOffset() != second.box.offset
        || first.entryCount > std::numeric_limits<quint32>::max() - second.entryCount) {
        return false;
    }
    const QByteArray entries = first.entries + second.entries;
    const qsizetype mergedSize = ipmaEntriesOffset + entries.size();
    const qsizetype available = first.box.size + second.box.size;
    if (available - mergedSize != ipmaEntriesOffset) {
        return false;
    }
    QByteArray replacement(mergedSize, '\0');
    if (!writeIpma(
            replacement, first.versionAndFlags, first.entryCount + second.entryCount, entries)) {
        return false;
    }
    std::array<char, 4> alternate = first.versionAndFlags;
    alternate[0] = alternate[0] == 0 ? 1 : 0;
    QByteArray empty(ipmaEntriesOffset, '\0');
    if (!writeIpma(empty, alternate, 0, {})) {
        return false;
    }
    replacement += empty;
    if (replacement.size() != available || first.box.offset > data.size() - available) {
        return false;
    }
    std::ranges::copy(replacement, data.begin() + first.box.offset);
    return true;
}

bool patchDuplicateIpma(QByteArray& data, const std::vector<BoxHeader>& meta)
{
    bool changed = false;
    for (const std::vector<BoxHeader>& iprp : iprpGroups(data, meta)) {
        std::optional<IpmaBox> previous;
        for (const BoxHeader& box : iprp) {
            const std::optional<IpmaBox> current = readIpma(data, box);
            if (!current.has_value()) {
                previous.reset();
            } else if (previous.has_value() && mergeIpma(data, *previous, *current)) {
                changed = true;
                previous.reset();
            } else {
                previous = current;
            }
        }
    }
    return changed;
}
}

namespace kiriview {
QByteArray avifDataWithCompatibilityFixes(QByteArrayView data)
{
    const std::optional<std::vector<BoxHeader>> top = childBoxes(data, 0, data.size());
    if (!top.has_value() || !hasAvifBrand(data, *top)) {
        return data.toByteArray();
    }
    QByteArray fixed = data.toByteArray();
    bool changed = false;
    for (const BoxHeader& box : *top) {
        if (!box.isType("meta")) {
            continue;
        }
        const std::optional<std::vector<BoxHeader>> meta = metaChildren(data, box);
        if (!meta.has_value() || !hasAlphaProperty(data, *meta)) {
            continue;
        }
        const std::optional<quint32> primary = primaryItemId(data, *meta);
        if (primary.has_value() && *primary != 0) {
            changed = patchAuxiliaryReferences(fixed, *meta, *primary) || changed;
        }
        changed = patchDuplicateIpma(fixed, *meta) || changed;
    }
    return changed ? fixed : data.toByteArray();
}
}
