// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rendering/qimagereaderdisplaysource.h"

#include <QBuffer>
#include <QByteArrayList>
#include <QColor>
#include <QImage>
#include <QImageWriter>
#include <QObject>
#include <QTest>
#include <Qt>
#include <memory>

namespace {
QByteArray encodedImageData(const QImage& image, const QByteArray& format, QString* errorString)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, format);
    if (!writer.write(image)) {
        if (errorString != nullptr) {
            *errorString = writer.errorString();
        }
        return {};
    }
    return data;
}

QByteArray pngData()
{
    QImage image(4, 4, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    image.setPixelColor(0, 0, Qt::red);
    image.setPixelColor(3, 3, Qt::blue);

    return encodedImageData(image, QByteArrayLiteral("png"), nullptr);
}

bool imageWriterSupports(const QByteArray& format)
{
    const QByteArrayList formats = QImageWriter::supportedImageFormats();
    return formats.contains(format) || formats.contains(format.toUpper());
}

QByteArray jpegWriterFormat()
{
    return imageWriterSupports(QByteArrayLiteral("jpg")) ? QByteArrayLiteral("jpg")
                                                         : QByteArrayLiteral("jpeg");
}

void verifyDisplayFailure(const kiriview::StaticImageDisplayDecodeResult& result)
{
    QVERIFY(result.image.isNull());
    QVERIFY(!result.diagnostics.userMessage.isEmpty());
    QVERIFY(!result.diagnostics.diagnosticDetail.isEmpty());
}
}

class TestQImageReaderDisplaySource : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sourceDecodesBlockingAndRasterDisplayImages();
    void failedDisplayDecodePreservesDiagnostics();
    void jpegSourceDecodesFirstDisplayToViewport();
    void jpegSourceSkipsFirstDisplayWhenImageFitsViewport();
    void pngSourceLeavesFirstDisplayNotImplemented();
};

void TestQImageReaderDisplaySource::sourceDecodesBlockingAndRasterDisplayImages()
{
    QString errorString;
    std::shared_ptr<kiriview::QImageReaderDisplaySource> source
        = kiriview::QImageReaderDisplaySource::open(
            pngData(), QByteArrayLiteral("png"), &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));
    QCOMPARE(source->imageSize(), QSize(4, 4));

    const kiriview::StaticImageDisplayDecodeResult preview = source->decodeBlockingDisplayImage(2);
    QVERIFY2(!preview.image.isNull(), qPrintable(preview.diagnostics.userMessage));
    QCOMPARE(preview.image.size(), QSize(2, 2));

    const kiriview::StaticImageDisplayDecodeResult raster
        = source->decodeRasterDisplayImage(QSize(4, 4));
    QVERIFY2(!raster.image.isNull(), qPrintable(raster.diagnostics.userMessage));
    QCOMPARE(raster.image.size(), QSize(4, 4));
    QCOMPARE(raster.image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(raster.image.pixelColor(3, 3), QColor(Qt::blue));
}

void TestQImageReaderDisplaySource::failedDisplayDecodePreservesDiagnostics()
{
    kiriview::QImageReaderDisplaySource source(QByteArrayLiteral("not image data"),
        QByteArrayLiteral("png"), QSize(4, 4), kiriview::StaticImageReaderTransform {});

    const kiriview::StaticImageDisplayDecodeResult rasterResult
        = source.decodeRasterDisplayImage(QSize(2, 2));
    verifyDisplayFailure(rasterResult);

    const kiriview::StaticImageDisplayDecodeResult blockingResult
        = source.decodeBlockingDisplayImage(2);
    verifyDisplayFailure(blockingResult);
}

void TestQImageReaderDisplaySource::jpegSourceDecodesFirstDisplayToViewport()
{
    if (!imageWriterSupports(QByteArrayLiteral("jpg"))
        && !imageWriterSupports(QByteArrayLiteral("jpeg"))) {
        QSKIP("Qt JPEG image writer is unavailable");
    }

    QImage image(1600, 1200, QImage::Format_RGB32);
    image.fill(Qt::red);
    QString errorString;
    const QByteArray data = encodedImageData(image, jpegWriterFormat(), &errorString);
    QVERIFY2(!data.isEmpty(), qPrintable(errorString));

    std::shared_ptr<kiriview::QImageReaderDisplaySource> source
        = kiriview::QImageReaderDisplaySource::open(data, QByteArrayLiteral("jpeg"), &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));

    const kiriview::StaticImageFirstDisplayDecodeResult decoded = source->decodeFirstDisplayImage(
        kiriview::ImageFirstDisplayDecodeContext { QSize(400, 300) });
    const kiriview::FirstDisplayImageDecodeResult& result = decoded.firstDisplay;

    QCOMPARE(result.status, kiriview::FirstDisplayImageDecodeStatus::Ready);
    QVERIFY2(!result.image.isNull(), qPrintable(decoded.diagnostics.userMessage));
    QCOMPARE(result.image.size(), QSize(400, 300));
    QVERIFY(result.image.size() != source->imageSize());
}

void TestQImageReaderDisplaySource::jpegSourceSkipsFirstDisplayWhenImageFitsViewport()
{
    if (!imageWriterSupports(QByteArrayLiteral("jpg"))
        && !imageWriterSupports(QByteArrayLiteral("jpeg"))) {
        QSKIP("Qt JPEG image writer is unavailable");
    }

    QImage image(200, 100, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QString errorString;
    const QByteArray data = encodedImageData(image, jpegWriterFormat(), &errorString);
    QVERIFY2(!data.isEmpty(), qPrintable(errorString));

    std::shared_ptr<kiriview::QImageReaderDisplaySource> source
        = kiriview::QImageReaderDisplaySource::open(data, QByteArrayLiteral("jpeg"), &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));

    const kiriview::FirstDisplayImageDecodeResult result
        = source
              ->decodeFirstDisplayImage(
                  kiriview::ImageFirstDisplayDecodeContext { QSize(400, 300) })
              .firstDisplay;

    QCOMPARE(result.status, kiriview::FirstDisplayImageDecodeStatus::NotImplemented);
    QVERIFY(result.image.isNull());
}

void TestQImageReaderDisplaySource::pngSourceLeavesFirstDisplayNotImplemented()
{
    QString errorString;
    std::shared_ptr<kiriview::QImageReaderDisplaySource> source
        = kiriview::QImageReaderDisplaySource::open(
            pngData(), QByteArrayLiteral("png"), &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));

    const kiriview::FirstDisplayImageDecodeResult result
        = source->decodeFirstDisplayImage(kiriview::ImageFirstDisplayDecodeContext { QSize(2, 2) })
              .firstDisplay;
    QCOMPARE(result.status, kiriview::FirstDisplayImageDecodeStatus::NotImplemented);

    const kiriview::StaticImageDisplayDecodeResult blockingDisplay
        = source->decodeBlockingDisplayImage(2);
    QVERIFY2(!blockingDisplay.image.isNull(), qPrintable(blockingDisplay.diagnostics.userMessage));
    QCOMPARE(blockingDisplay.image.size(), QSize(2, 2));
}

QTEST_GUILESS_MAIN(TestQImageReaderDisplaySource)

#include "tst_qimagereaderdisplaysource.moc"
