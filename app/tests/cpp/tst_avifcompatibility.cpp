// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/avifcompatibility.h"

#include <QObject>
#include <QTest>
#include <QtEndian>
#include <algorithm>

namespace {
QByteArray box(const char (&kind)[5], const QByteArray& body)
{
    QByteArray data(8, '\0');
    qToBigEndian<quint32>(
        static_cast<quint32>(data.size() + body.size()), reinterpret_cast<uchar*>(data.data()));
    std::copy_n(kind, 4, data.data() + 4);
    data += body;
    return data;
}

QByteArray fullBox(const char (&kind)[5], char version, const QByteArray& body)
{
    QByteArray fullBody(4, '\0');
    fullBody[0] = version;
    fullBody += body;
    return box(kind, fullBody);
}

QByteArray avif(const QList<QByteArray>& metaChildren)
{
    QByteArray ftypBody("avif", 4);
    ftypBody += QByteArray(4, '\0');
    ftypBody += QByteArray("avif", 4);
    QByteArray metaBody(4, '\0');
    for (const QByteArray& child : metaChildren) {
        metaBody += child;
    }
    return box("ftyp", ftypBody) + box("meta", metaBody);
}

QByteArray alphaIprp(const QList<QByteArray>& associationBoxes = {})
{
    QByteArray body = box(
        "ipco", box("auxC", QByteArrayLiteral("urn:mpeg:mpegB:cicp:systems:auxiliary:alpha")));
    for (const QByteArray& association : associationBoxes) {
        body += association;
    }
    return box("iprp", body);
}
}

class TestAvifCompatibility : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void patchesZeroAuxiliaryReference();
    void mergesAdjacentAssociationBoxesWithoutChangingSize();
};

void TestAvifCompatibility::patchesZeroAuxiliaryReference()
{
    QByteArray itemId(2, '\0');
    qToBigEndian<quint16>(7, reinterpret_cast<uchar*>(itemId.data()));
    const QByteArray reference = box("auxl", QByteArray::fromHex("000200010000"));
    const QByteArray data
        = avif({ fullBox("pitm", 0, itemId), alphaIprp(), fullBox("iref", 0, reference) });

    const QByteArray fixed = kiriview::avifDataWithCompatibilityFixes(data);
    QVERIFY(fixed.contains(QByteArray::fromHex("000200010007")));
    QCOMPARE(fixed.size(), data.size());
}

void TestAvifCompatibility::mergesAdjacentAssociationBoxesWithoutChangingSize()
{
    const QByteArray first = fullBox("ipma", 0, QByteArray::fromHex("00000001010203"));
    const QByteArray second = fullBox("ipma", 0, QByteArray::fromHex("000000010405"));
    const QByteArray data = avif({ alphaIprp({ first, second }) });

    const QByteArray fixed = kiriview::avifDataWithCompatibilityFixes(data);
    QCOMPARE(fixed.size(), data.size());
    QVERIFY(fixed.contains(QByteArray::fromHex("000000020102030405")));
}

QTEST_GUILESS_MAIN(TestAvifCompatibility)

#include "tst_avifcompatibility.moc"
