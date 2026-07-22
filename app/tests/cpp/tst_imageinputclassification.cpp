// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imageinputclassification.h"

#include <QObject>
#include <QTest>
#include <QtEndian>
#include <algorithm>

namespace {
QByteArray pngChunk(const char (&kind)[5], const QByteArray& body)
{
    QByteArray chunk(4, '\0');
    qToBigEndian<quint32>(
        static_cast<quint32>(body.size()), reinterpret_cast<uchar*>(chunk.data()));
    chunk.append(kind, 4);
    chunk.append(body);
    chunk.append(QByteArray(4, '\0'));
    return chunk;
}

QByteArray pngData(bool animated)
{
    QByteArray data("\x89PNG\r\n\x1a\n", 8);
    data += pngChunk("IHDR", QByteArray(13, '\0'));
    if (animated) {
        data += pngChunk("acTL", QByteArray(8, '\0'));
    }
    data += pngChunk("IDAT", QByteArray(1, '\0'));
    return data;
}

QByteArray ftyp(const char (&brand)[5])
{
    QByteArray data(20, '\0');
    qToBigEndian<quint32>(20, reinterpret_cast<uchar*>(data.data()));
    std::copy_n("ftyp", 4, data.data() + 4);
    std::copy_n(brand, 4, data.data() + 8);
    std::copy_n(brand, 4, data.data() + 16);
    return data;
}
}

class TestImageInputClassification : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void contentSelectsRasterAndAnimatedRoutes();
    void bmffBrandsSelectSpecializedRoutes();
    void extensionHintsApplyOnlyAfterContentSignatures();
};

void TestImageInputClassification::contentSelectsRasterAndAnimatedRoutes()
{
    const auto png = kiriview::classifyImageInput(pngData(false), QStringLiteral("image.raw"));
    QCOMPARE(png.kind, kiriview::ImageInputKind::QtRaster);
    QCOMPARE(png.qtFormat, kiriview::QtRasterFormat::Png);

    const auto apng = kiriview::classifyImageInput(pngData(true), QStringLiteral("image.png"));
    QCOMPARE(apng.kind, kiriview::ImageInputKind::Apng);

    const auto jpeg
        = kiriview::classifyImageInput(QByteArray("\xff\xd8\xff rest", 8), QStringLiteral("x"));
    QCOMPARE(jpeg.qtFormat, kiriview::QtRasterFormat::Jpeg);
}

void TestImageInputClassification::bmffBrandsSelectSpecializedRoutes()
{
    const auto avif = kiriview::classifyImageInput(ftyp("avif"), QStringLiteral("image.bin"));
    QCOMPARE(avif.kind, kiriview::ImageInputKind::HeifFamily);
    QCOMPARE(avif.dataSource, kiriview::ImageDecodeDataSource::AvifCompatible);

    QCOMPARE(kiriview::classifyImageInput(ftyp("crx "), QStringLiteral("image.bin")).kind,
        kiriview::ImageInputKind::Raw);
    QCOMPARE(kiriview::classifyImageInput(ftyp("jxl "), QStringLiteral("image.bin")).qtFormat,
        kiriview::QtRasterFormat::Jxl);
}

void TestImageInputClassification::extensionHintsApplyOnlyAfterContentSignatures()
{
    QCOMPARE(kiriview::classifyImageInput(
                 QByteArrayLiteral("<?xml?><svg/>"), QStringLiteral("image.bin"))
                 .kind,
        kiriview::ImageInputKind::Svg);
    QCOMPARE(kiriview::classifyImageInput(QByteArrayLiteral("unknown"), QStringLiteral("photo.NEF"))
                 .kind,
        kiriview::ImageInputKind::Raw);
    QCOMPARE(kiriview::classifyImageInput(QByteArrayLiteral("unknown"), QStringLiteral("image.bin"))
                 .kind,
        kiriview::ImageInputKind::Unknown);
}

QTEST_GUILESS_MAIN(TestImageInputClassification)

#include "tst_imageinputclassification.moc"
