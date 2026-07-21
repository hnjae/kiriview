// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rendering/displayimagestore.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QTest>
#include <QtGlobal>
#include <optional>

class TestDisplayImageStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void storesReusableFramePayload();
    void evictsLeastRecentlyUsedImagesByPriority();
    void frameLeasePreventsEvictionUntilReleased();
    void reusableAcquisitionRequiresExactKeyMatch();
};

namespace {
QImage testImage(const QSize& size, QColor color = Qt::red)
{
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(color);
    return image;
}

kiriview::DisplayImageEntry testEntry(
    const QSize& rasterSize, kiriview::DisplayImageRetentionPriority priority = {})
{
    return kiriview::DisplayImageEntry {
        testImage(rasterSize),
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
        {},
        QSize(rasterSize.width() * 2, rasterSize.height() * 2),
        rasterSize,
        kiriview::DisplayImageQuality::Exact,
        kiriview::DisplayImagePreviewOrigin::None,
        kiriview::DisplayedPageRole::Primary,
    };
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
        testReuseKey(QStringLiteral("leased"), QSize(4, 4)));
    QVERIFY(store.acquireFrameLease(leased));
    const QString evicted = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background),
        testReuseKey(QStringLiteral("evicted"), QSize(4, 4)));
    const QString visible = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Visible),
        testReuseKey(QStringLiteral("visible"), QSize(4, 4)));

    QVERIFY(store.entry(leased).has_value());
    QVERIFY(store.entry(evicted) == std::nullopt);
    QVERIFY(store.entry(visible).has_value());

    store.releaseFrameLease(leased);
    const QString nearby = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Nearby),
        testReuseKey(QStringLiteral("nearby"), QSize(4, 4)));

    QVERIFY(store.entry(leased) == std::nullopt);
    QVERIFY(store.entry(visible).has_value());
    QVERIFY(store.entry(nearby).has_value());
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

QTEST_GUILESS_MAIN(TestDisplayImageStore)

#include "tst_displayimagestore.moc"
