// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "cache/imagebytecost.h"

#include "cache/imagebyteaccounting.h"

#include <QByteArray>
#include <QColorSpace>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSize>
#include <QString>
#include <QTest>
#include <Qt>
#include <cstdint>
#include <limits>

class TestImageByteCost : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void byteAccountingSaturatesProductsSumsAndQtSizes();
    void imageByteCostUsesQtImageStorageSize();
    void imageByteCostIncludesSourceScaledAncillaryData();
    void estimatedRgbaByteCostHandlesEmptyAndOverflow();
};

void TestImageByteCost::byteAccountingSaturatesProductsSumsAndQtSizes()
{
    QCOMPARE(kiriview::saturatedQtByteSize(-5), qsizetype(-5));
    QCOMPARE(kiriview::saturatedQtByteSize(std::numeric_limits<std::int64_t>::max()),
        std::numeric_limits<qsizetype>::max());
    QCOMPARE(kiriview::saturatedPositiveByteProduct(0, 10), std::int64_t(0));
    QCOMPARE(kiriview::saturatedPositiveByteProduct(-1, 10), std::int64_t(0));
    QCOMPARE(kiriview::saturatedPositiveByteProduct(10, 3), std::int64_t(30));
    QCOMPARE(kiriview::saturatedPositiveByteProduct(std::numeric_limits<std::int64_t>::max(), 2),
        std::numeric_limits<std::int64_t>::max());
    QCOMPARE(
        kiriview::saturatedQtByteProduct(std::numeric_limits<std::int64_t>::max(), std::int64_t(2)),
        std::numeric_limits<qsizetype>::max());
    QCOMPARE(kiriview::saturatedQtByteSum(10, 3), qsizetype(13));
    QCOMPARE(kiriview::saturatedQtByteSum(-5, 3), qsizetype(3));
    QCOMPARE(kiriview::saturatedQtByteSum(std::numeric_limits<qsizetype>::max(), 1),
        std::numeric_limits<qsizetype>::max());
}

void TestImageByteCost::imageByteCostUsesQtImageStorageSize()
{
    QImage image(10, 3, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);

    QCOMPARE(kiriview::imageByteCost(QImage()), qsizetype(0));
    QCOMPARE(kiriview::imageByteCost(image), image.sizeInBytes());
}

void TestImageByteCost::imageByteCostIncludesSourceScaledAncillaryData()
{
    QImage image(1, 1, QImage::Format_Indexed8);
    image.fill(Qt::transparent);
    QList<QRgb> colorTable;
    for (int component = 0; component < 256; ++component) {
        colorTable.append(qRgb(component, component, component));
    }
    image.setColorTable(colorTable);
    const QString textKey = QStringLiteral("source");
    const QString textValue(4096, QLatin1Char('x'));
    image.setText(textKey, textValue);

    QByteArray iccProfile = QColorSpace(QColorSpace::SRgb).iccProfile();
    QVERIFY(!iccProfile.isEmpty());
    const QColorSpace colorSpace = QColorSpace::fromIccProfile(iccProfile);
    QVERIFY(colorSpace.isValid());
    QCOMPARE(colorSpace.iccProfile(), iccProfile);
    image.setColorSpace(colorSpace);

    const QList<QRgb> retainedColorTable = image.colorTable();
    const QByteArray retainedIccProfile = image.colorSpace().iccProfile();
    qsizetype minimumByteCost = image.sizeInBytes();
    minimumByteCost = kiriview::saturatedQtByteSum(
        minimumByteCost, kiriview::saturatedQtByteProduct(textKey.size(), sizeof(QChar)));
    minimumByteCost = kiriview::saturatedQtByteSum(
        minimumByteCost, kiriview::saturatedQtByteProduct(textValue.size(), sizeof(QChar)));
    minimumByteCost = kiriview::saturatedQtByteSum(
        minimumByteCost, kiriview::saturatedQtByteProduct(retainedColorTable.size(), sizeof(QRgb)));
    minimumByteCost = kiriview::saturatedQtByteSum(minimumByteCost, retainedIccProfile.size());

    QVERIFY(kiriview::imageByteCost(image) > minimumByteCost);
}

void TestImageByteCost::estimatedRgbaByteCostHandlesEmptyAndOverflow()
{
    QCOMPARE(kiriview::estimatedRgbaByteCost(QSize()), qsizetype(0));
    QCOMPARE(kiriview::estimatedRgbaByteCost(QSize(10, 3)), qsizetype(120));
    QCOMPARE(kiriview::estimatedRgbaByteCost(
                 QSize(std::numeric_limits<int>::max(), std::numeric_limits<int>::max())),
        std::numeric_limits<qsizetype>::max());
}

QTEST_GUILESS_MAIN(TestImageByteCost)

#include "tst_imagebytecost.moc"
