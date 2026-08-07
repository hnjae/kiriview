// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rendering/displayimagestore.h"

#include "decoding/imagesourcerevision.h"

#include <QByteArrayView>
#include <QColor>
#include <QColorSpace>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QTest>
#include <QtGlobal>
#include <memory>
#include <optional>

class TestDisplayImageStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void storesReusableFramePayload();
    void evictsLeastRecentlyUsedImagesByPriority();
    void frameLeasePreventsEvictionUntilReleased();
    void outputAdmissionRetiresAfterStoredPixels();
    void failedReservationPreservesExternallyAdmittedEntry();
    void entryAliasRetainsAdmissionAndPixelsAfterStoreDestruction();
    void reusableAcquisitionRequiresExactKeyMatch();
    void reusableAcquisitionDistinguishesFreshnessAndAuthoredRasterIdentity();
};

namespace {
QImage testImage(const QSize& size, QColor color = Qt::red)
{
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(color);
    return image;
}

kiriview::DisplayImageEntry testEntry(const QSize& rasterSize,
    kiriview::DisplayImageRetentionPriority priority = {}, QColor color = Qt::red)
{
    return kiriview::DisplayImageEntry {
        testImage(rasterSize, color),
        QSize(rasterSize.width() * 2, rasterSize.height() * 2),
        rasterSize,
        kiriview::DisplayImageQuality::Exact,
        priority,
    };
}

kiriview::DisplayImageReuseKey testReuseKey(
    const QString& sourceIdentity = QStringLiteral("file:///tmp/image.png"),
    QSize rasterSize = QSize(8, 4))
{
    return kiriview::DisplayImageReuseKey {
        sourceIdentity,
        sourceIdentity,
        kiriview::ImageSourceRevision::fromData(QByteArrayView("current-content")),
        kiriview::DisplayImageRasterIdentity::authoritativeStill(),
        {},
        QSize(rasterSize.width() * 2, rasterSize.height() * 2),
        rasterSize,
        kiriview::DisplayImageQuality::Exact,
        kiriview::DisplayImagePreviewOrigin::None,
        kiriview::DisplayedPageRole::Primary,
    };
}

kiriview::DisplayImageReuseKey testTimedFrameReuseKey(int authoredFrame)
{
    kiriview::DisplayImageReuseKey key
        = testReuseKey(QStringLiteral("animated-source"), QSize(4, 4));
    key.rasterIdentity = kiriview::DisplayImageRasterIdentity::timedFrame(authoredFrame);
    return key;
}
}

void TestDisplayImageStore::storesReusableFramePayload()
{
    kiriview::DisplayImageStore store(1024);
    kiriview::DisplayImageReuseKey reuseKey = testReuseKey();
    reuseKey.imageReaderTransformations = QImageIOHandler::TransformationRotate90;

    const QString id = store.acquireReusable(testEntry(QSize(8, 4)), reuseKey);
    QVERIFY(!id.isEmpty());

    const std::optional<kiriview::DisplayImageStoreEntry> stored = store.entry(id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->image.size(), QSize(8, 4));
    QCOMPARE(stored->originalSize, QSize(16, 8));
    QCOMPARE(stored->rasterSize, QSize(8, 4));
    QCOMPARE(stored->imageReaderTransformations, QImageIOHandler::TransformationRotate90);
    QCOMPARE(stored->quality, kiriview::DisplayImageQuality::Exact);
    QCOMPARE(stored->byteCost, qsizetype(128));
    QCOMPARE(store.byteCost(), qsizetype(128));
    QCOMPARE(store.size(), qsizetype(1));
}

void TestDisplayImageStore::evictsLeastRecentlyUsedImagesByPriority()
{
    kiriview::DisplayImageStore store(128);

    const QString background = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background),
        testReuseKey(QStringLiteral("background"), QSize(4, 4)));
    const QString nearby = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Nearby),
        testReuseKey(QStringLiteral("nearby"), QSize(4, 4)));
    const QString visible = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Visible),
        testReuseKey(QStringLiteral("visible"), QSize(4, 4)));

    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(!background.isEmpty());
    QVERIFY(store.entry(background) == std::nullopt);
    QVERIFY(store.entry(nearby).has_value());
    QVERIFY(store.entry(visible).has_value());
}

void TestDisplayImageStore::frameLeasePreventsEvictionUntilReleased()
{
    kiriview::DisplayImageStore store(128);

    const QString leased = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background),
        testTimedFrameReuseKey(0));
    QVERIFY(store.acquireFrameLease(leased));
    const QString evicted = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background),
        testTimedFrameReuseKey(1));
    const QString visible = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Visible),
        testTimedFrameReuseKey(2));

    QVERIFY(store.entry(leased).has_value());
    QVERIFY(store.entry(evicted) == std::nullopt);
    QVERIFY(store.entry(visible).has_value());

    store.releaseFrameLease(leased);
    const QString nearby = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Nearby),
        testTimedFrameReuseKey(3));

    QVERIFY(store.entry(leased) == std::nullopt);
    QVERIFY(store.entry(visible).has_value());
    QVERIFY(store.entry(nearby).has_value());
}

void TestDisplayImageStore::outputAdmissionRetiresAfterStoredPixels()
{
    struct PixelRetirementProbe
    {
        std::weak_ptr<kiriview::DisplayImageOutputAdmission> outputAdmission;
        uchar* pixels = nullptr;
        bool* admissionAliveAtPixelRetirement = nullptr;
    };

    kiriview::DisplayImageStore store(64);
    std::shared_ptr<kiriview::DisplayImageOutputAdmission> outputAdmission
        = store.reserveOutput(64);
    QVERIFY(outputAdmission != nullptr);
    const std::weak_ptr<kiriview::DisplayImageOutputAdmission> weakAdmission = outputAdmission;
    bool admissionAliveAtPixelRetirement = false;
    auto* pixels = new uchar[64];
    auto* probe = new PixelRetirementProbe {
        weakAdmission,
        pixels,
        &admissionAliveAtPixelRetirement,
    };
    QImage producedImage(
        pixels, 4, 4, 16, QImage::Format_RGBA8888,
        [](void* data) {
            auto* retired = static_cast<PixelRetirementProbe*>(data);
            *retired->admissionAliveAtPixelRetirement = !retired->outputAdmission.expired();
            delete[] retired->pixels;
            delete retired;
        },
        probe);
    producedImage.fill(Qt::red);

    const QString produced = store.acquireReusable(
        kiriview::DisplayImageEntry {
            producedImage,
            QSize(4, 4),
            QSize(4, 4),
            kiriview::DisplayImageQuality::Exact,
            kiriview::DisplayImageRetentionPriority::Background,
        },
        testTimedFrameReuseKey(0), std::move(outputAdmission));
    QVERIFY(!produced.isEmpty());
    producedImage = {};
    QCOMPARE(store.byteCost(), qsizetype(64));

    const QString replacement = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Visible),
        testTimedFrameReuseKey(1));

    QVERIFY(!replacement.isEmpty());
    QVERIFY(store.entry(produced) == std::nullopt);
    QVERIFY(admissionAliveAtPixelRetirement);
    QVERIFY(weakAdmission.expired());
    QCOMPARE(store.byteCost(), qsizetype(64));
}

void TestDisplayImageStore::failedReservationPreservesExternallyAdmittedEntry()
{
    kiriview::DisplayImageStore store(64);
    std::shared_ptr<kiriview::DisplayImageOutputAdmission> outputAdmission
        = store.reserveOutput(64);
    QVERIFY(outputAdmission != nullptr);

    const QString stored = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background),
        testTimedFrameReuseKey(0), outputAdmission);
    QVERIFY(!stored.isEmpty());

    QVERIFY(store.reserveOutput(64) == nullptr);
    QVERIFY(store.entry(stored).has_value());
    QCOMPARE(store.byteCost(), qsizetype(64));
}

void TestDisplayImageStore::entryAliasRetainsAdmissionAndPixelsAfterStoreDestruction()
{
    struct PixelRetirementProbe
    {
        std::weak_ptr<kiriview::DisplayImageOutputAdmission> outputAdmission;
        uchar* pixels = nullptr;
        bool* pixelsRetired = nullptr;
        bool* admissionAliveAtPixelRetirement = nullptr;
    };

    auto store = std::make_unique<kiriview::DisplayImageStore>(64);
    std::shared_ptr<kiriview::DisplayImageOutputAdmission> outputAdmission
        = store->reserveOutput(64);
    QVERIFY(outputAdmission != nullptr);
    const std::weak_ptr<kiriview::DisplayImageOutputAdmission> weakAdmission = outputAdmission;
    bool pixelsRetired = false;
    bool admissionAliveAtPixelRetirement = false;
    auto* pixels = new uchar[64];
    auto* probe = new PixelRetirementProbe {
        weakAdmission,
        pixels,
        &pixelsRetired,
        &admissionAliveAtPixelRetirement,
    };
    QImage producedImage(
        pixels, 4, 4, 16, QImage::Format_RGBA8888,
        [](void* data) {
            auto* retired = static_cast<PixelRetirementProbe*>(data);
            *retired->pixelsRetired = true;
            *retired->admissionAliveAtPixelRetirement = !retired->outputAdmission.expired();
            delete[] retired->pixels;
            delete retired;
        },
        probe);
    producedImage.fill(Qt::red);
    producedImage.setColorSpace(QColorSpace(QColorSpace::SRgb));
    producedImage.setDevicePixelRatio(2.0);
    producedImage.setOffset(QPoint(3, 5));
    producedImage.setText(QStringLiteral("source"), QStringLiteral("retirement-probe"));
    const uchar* const producedPixels = producedImage.constBits();

    const QString id = store->acquireReusable(
        kiriview::DisplayImageEntry {
            producedImage,
            QSize(4, 4),
            QSize(4, 4),
            kiriview::DisplayImageQuality::Exact,
            kiriview::DisplayImageRetentionPriority::Visible,
        },
        testTimedFrameReuseKey(0), std::move(outputAdmission));
    QVERIFY(!id.isEmpty());
    std::optional<kiriview::DisplayImageStoreEntry> stored = store->entry(id);
    QVERIFY(stored.has_value());
    QVERIFY(stored->image.constBits() == producedPixels);
    QCOMPARE(stored->image.colorSpace(), QColorSpace(QColorSpace::SRgb));
    QCOMPARE(stored->image.devicePixelRatio(), 2.0);
    QCOMPARE(stored->image.offset(), QPoint(3, 5));
    QCOMPARE(stored->image.text(QStringLiteral("source")), QStringLiteral("retirement-probe"));
    producedImage = {};

    QVERIFY(store->reserveOutput(64) == nullptr);
    QVERIFY(store->entry(id).has_value());
    store.reset();

    QVERIFY(!pixelsRetired);
    QVERIFY(!weakAdmission.expired());
    QCOMPARE(stored->image.pixelColor(0, 0), QColor(Qt::red));
    stored.reset();

    QVERIFY(pixelsRetired);
    QVERIFY(admissionAliveAtPixelRetirement);
    QVERIFY(weakAdmission.expired());
}

void TestDisplayImageStore::reusableAcquisitionRequiresExactKeyMatch()
{
    kiriview::DisplayImageStore store(1024);
    const kiriview::DisplayImageReuseKey firstKey = testReuseKey(QStringLiteral("page-a"));
    kiriview::DisplayImageReuseKey otherLocationKey = firstKey;
    otherLocationKey.locationIdentity = QStringLiteral("other-scope");
    kiriview::DisplayImageReuseKey otherSourceKey = firstKey;
    otherSourceKey.sourceIdentity = QStringLiteral("page-b");

    const QString first = store.acquireReusable(testEntry(QSize(8, 4)), firstKey);
    const QString reused = store.acquireReusable(testEntry(QSize(8, 4)), firstKey);
    const QString otherLocation = store.acquireReusable(testEntry(QSize(8, 4)), otherLocationKey);
    const QString otherSource = store.acquireReusable(testEntry(QSize(8, 4)), otherSourceKey);

    QCOMPARE(reused, first);
    QVERIFY(!otherLocation.isEmpty());
    QVERIFY(!otherSource.isEmpty());
    QVERIFY(first != otherLocation);
    QVERIFY(first != otherSource);
    QCOMPARE(store.size(), qsizetype(3));
}

void TestDisplayImageStore::reusableAcquisitionDistinguishesFreshnessAndAuthoredRasterIdentity()
{
    kiriview::DisplayImageStore store(4096);
    const kiriview::DisplayImageReuseKey stillKey = testReuseKey();
    kiriview::DisplayImageReuseKey newerContentKey = stillKey;
    newerContentKey.sourceRevision
        = kiriview::ImageSourceRevision::fromData(QByteArrayView("newer-content"));
    kiriview::DisplayImageReuseKey firstFrameKey = stillKey;
    firstFrameKey.rasterIdentity = kiriview::DisplayImageRasterIdentity::timedFrame(0);
    kiriview::DisplayImageReuseKey secondFrameKey = stillKey;
    secondFrameKey.rasterIdentity = kiriview::DisplayImageRasterIdentity::timedFrame(1);
    kiriview::DisplayImageReuseKey refinementKey = stillKey;
    refinementKey.rasterIdentity = kiriview::DisplayImageRasterIdentity::refinement();

    const QString still = store.acquireReusable(testEntry(QSize(8, 4), {}, Qt::red), stillKey);
    const QString reused = store.acquireReusable(testEntry(QSize(8, 4), {}, Qt::yellow), stillKey);
    const QString newer
        = store.acquireReusable(testEntry(QSize(8, 4), {}, Qt::blue), newerContentKey);
    const QString firstFrame
        = store.acquireReusable(testEntry(QSize(8, 4), {}, Qt::green), firstFrameKey);
    const QString secondFrame
        = store.acquireReusable(testEntry(QSize(8, 4), {}, Qt::cyan), secondFrameKey);
    const QString refinement
        = store.acquireReusable(testEntry(QSize(8, 4), {}, Qt::magenta), refinementKey);

    QCOMPARE(reused, still);
    QVERIFY(newer != still);
    QVERIFY(firstFrame != still);
    QVERIFY(firstFrame != secondFrame);
    QVERIFY(refinement != still);
    QCOMPARE(store.entry(still)->image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(store.entry(newer)->image.pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(store.entry(firstFrame)->image.pixelColor(0, 0), QColor(Qt::green));
    QCOMPARE(store.entry(secondFrame)->image.pixelColor(0, 0), QColor(Qt::cyan));
    QCOMPARE(store.entry(refinement)->image.pixelColor(0, 0), QColor(Qt::magenta));
    QCOMPARE(store.size(), qsizetype(5));
}

QTEST_GUILESS_MAIN(TestDisplayImageStore)

#include "tst_displayimagestore.moc"
