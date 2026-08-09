// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "cache/imagebytecost.h"

#include "cache/imagebyteaccounting.h"

#include <QByteArray>
#include <QColorSpace>
#include <QList>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <limits>

namespace kiriview {
namespace {
    // QImage exposes text contents but not its associative-container allocation. Keep a
    // conservative per-entry allowance for the node, two string headers, terminators, and allocator
    // bookkeeping.
    constexpr qsizetype imageTextEntryAccountingOverhead = 256;

    qsizetype stringStorageByteCost(const QString& value)
    {
        const qsizetype retainedCodeUnits = std::max(value.size(), value.capacity());
        return saturatedQtByteSum(
            saturatedQtByteProduct(retainedCodeUnits, sizeof(QChar)), sizeof(QChar));
    }
}

qsizetype imageByteCost(const QImage& image)
{
    if (image.isNull()) {
        return 0;
    }

    qsizetype byteCost = image.sizeInBytes();
    const QList<QRgb> colorTable = image.colorTable();
    byteCost = saturatedQtByteSum(byteCost,
        saturatedQtByteProduct(std::max(colorTable.size(), colorTable.capacity()), sizeof(QRgb)));
    const QByteArray iccProfile = image.colorSpace().iccProfile();
    byteCost = saturatedQtByteSum(byteCost, std::max(iccProfile.size(), iccProfile.capacity()));
    const QStringList textKeys = image.textKeys();
    byteCost = saturatedQtByteSum(
        byteCost, saturatedQtByteProduct(textKeys.size(), imageTextEntryAccountingOverhead));
    for (const QString& key : textKeys) {
        byteCost = saturatedQtByteSum(byteCost, stringStorageByteCost(key));
        byteCost = saturatedQtByteSum(byteCost, stringStorageByteCost(image.text(key)));
    }
    return byteCost;
}

qsizetype estimatedRgbaByteCost(QSize size)
{
    if (size.isEmpty()) {
        return 0;
    }
    constexpr qsizetype bytesPerPixel = 4;
    const qsizetype width = size.width();
    const qsizetype height = size.height();
    if (width > std::numeric_limits<qsizetype>::max() / height / bytesPerPixel) {
        return std::numeric_limits<qsizetype>::max();
    }
    return width * height * bytesPerPixel;
}
}
