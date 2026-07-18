// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rendering/displayimagestore.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QTest>
#include <QUrl>
#include <QtGlobal>
#include <array>
#include <future>
#include <optional>
#include <vector>

class TestDisplayImageStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void storesImagesAndMetadata();
    void urlShapeUsesNeverReusedIds();
    void providerReportsOriginalSizeAndMissesReturnEmpty();
    void providerReturnsStoredRasterForRequestedSize();
    void evictsLeastRecentlyUsedImagesByPriority();
    void pinLeasesPreventEvictionAndReleaseDefersRemoval();
    void allPinKindsPreventEvictionAndReleaseDefersRemoval();
    void reusableAcquisitionReturnsBufferedEntryForMatchingKey();
    void reusableAcquisitionClearsDeferredReleaseForAllPinKinds();
    void reusableAcquisitionRequiresExactKeyMatch();
    void indexedLookupStatsStayConstantForIdAndReusableLookups();
    void indexesStayConsistentAcrossReuseEvictionReleaseAndClear();
    void providerRequestsAreThreadSafeReads();
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
        QStringLiteral("file:///tmp/image.png"),
        kiriview::DisplayedPageRole::Primary,
        kiriview::DisplayImageQuality::Exact,
        priority,
        42,
        QStringLiteral("test-entry"),
    };
}

kiriview::DisplayImageReuseKey testReuseKey(
    const QString& sourceIdentity = QStringLiteral("file:///tmp/image.png"))
{
    return kiriview::DisplayImageReuseKey {
        sourceIdentity,
        sourceIdentity,
        {},
        QSize(16, 8),
        QSize(8, 4),
        kiriview::DisplayImageQuality::Exact,
        kiriview::DisplayImagePreviewOrigin::None,
        kiriview::DisplayedPageRole::Primary,
    };
}

std::array<kiriview::DisplayImagePinKind, 5> allPinKinds()
{
    return {
        kiriview::DisplayImagePinKind::Visible,
        kiriview::DisplayImagePinKind::StaleRetained,
        kiriview::DisplayImagePinKind::PendingLoad,
        kiriview::DisplayImagePinKind::FrameRetention,
        kiriview::DisplayImagePinKind::BufferedDisplay,
    };
}

void assertStoreIndexesConsistent(const kiriview::DisplayImageStore& store)
{
    const kiriview::DisplayImageStoreDebugStats stats = store.debugStats();
    QCOMPARE(stats.entryCount, store.size());
    QCOMPARE(stats.byteCost, store.byteCost());
    QCOMPARE(stats.idIndexEntryCount, stats.entryCount);
    QVERIFY(stats.reuseIndexEntryCount <= stats.entryCount);
    QVERIFY(stats.evictionIndexEntryCount <= stats.entryCount);
}
}

void TestDisplayImageStore::storesImagesAndMetadata()
{
    kiriview::DisplayImageStore store(1024);

    const QString id = store.insert(testEntry(QSize(8, 4)));
    QVERIFY(!id.isEmpty());

    const std::optional<kiriview::DisplayImageStoreEntry> stored = store.entry(id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->id, id);
    QCOMPARE(stored->image.size(), QSize(8, 4));
    QCOMPARE(stored->originalSize, QSize(16, 8));
    QCOMPARE(stored->rasterSize, QSize(8, 4));
    QCOMPARE(stored->sourceIdentity, QStringLiteral("file:///tmp/image.png"));
    QCOMPARE(stored->pageRole, kiriview::DisplayedPageRole::Primary);
    QCOMPARE(stored->quality, kiriview::DisplayImageQuality::Exact);
    QCOMPARE(stored->priority, kiriview::DisplayImageRetentionPriority::Nearby);
    QCOMPARE(stored->generation, quint64(42));
    QCOMPARE(stored->debugLabel, QStringLiteral("test-entry"));
    QCOMPARE(stored->byteCost, qsizetype(128));
    QCOMPARE(store.byteCost(), qsizetype(128));
    QCOMPARE(store.size(), qsizetype(1));
}

void TestDisplayImageStore::urlShapeUsesNeverReusedIds()
{
    kiriview::DisplayImageStore store(1024);

    const QString first = store.insert(testEntry(QSize(4, 4)));
    store.release(first);
    const QString second = store.insert(testEntry(QSize(4, 4)));

    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY(first != second);
    QCOMPARE(kiriview::displayImageSourceForId(second),
        QUrl(QStringLiteral("image://kiriview-images/%1").arg(second)));
    QVERIFY(kiriview::displayImageSourceForId({}).isEmpty());
}

void TestDisplayImageStore::providerReportsOriginalSizeAndMissesReturnEmpty()
{
    auto store = std::make_shared<kiriview::DisplayImageStore>(1024);
    const QString id = store->insert(testEntry(QSize(8, 4)));
    kiriview::DisplayImageProvider provider(store);

    QSize originalSize;
    const QImage image = provider.requestImage(id, &originalSize, {});
    QCOMPARE(image.size(), QSize(8, 4));
    QCOMPARE(originalSize, QSize(16, 8));

    QSize missSize(1, 1);
    QVERIFY(provider.requestImage(QStringLiteral("missing"), &missSize, {}).isNull());
    QCOMPARE(missSize, QSize());
}

void TestDisplayImageStore::providerReturnsStoredRasterForRequestedSize()
{
    auto store = std::make_shared<kiriview::DisplayImageStore>(32768);
    const QString id = store->insert(testEntry(QSize(80, 40)));
    kiriview::DisplayImageProvider provider(store);

    QSize originalSize;
    QCOMPARE(provider.requestImage(id, &originalSize, QSize(40, 40)).size(), QSize(80, 40));
    QCOMPARE(originalSize, QSize(160, 80));
    QCOMPARE(provider.requestImage(id, nullptr, QSize(20, 0)).size(), QSize(80, 40));
    QCOMPARE(provider.requestImage(id, nullptr, QSize(0, 10)).size(), QSize(80, 40));
    QCOMPARE(provider.requestImage(id, nullptr, QSize(0, 0)).size(), QSize(80, 40));
    QCOMPARE(provider.requestImage(id, nullptr, QSize(-20, 10)).size(), QSize(80, 40));
    QCOMPARE(provider.requestImage(id, nullptr, QSize(160, 80)).size(), QSize(80, 40));
    QCOMPARE(provider.requestImage(id, nullptr, QSize(80, 40)).size(), QSize(80, 40));
}

void TestDisplayImageStore::evictsLeastRecentlyUsedImagesByPriority()
{
    kiriview::DisplayImageStore store(128);

    const QString background
        = store.insert(testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background));
    const QString nearby
        = store.insert(testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Nearby));
    const QString visible
        = store.insert(testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Visible));

    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(!background.isEmpty());
    QVERIFY(store.entry(background) == std::nullopt);
    QVERIFY(store.entry(nearby).has_value());
    QVERIFY(store.entry(visible).has_value());
}

void TestDisplayImageStore::pinLeasesPreventEvictionAndReleaseDefersRemoval()
{
    kiriview::DisplayImageStore store(128);

    const QString pinned
        = store.insert(testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background));
    QVERIFY(store.acquirePinLease(pinned, kiriview::DisplayImagePinKind::Visible));
    const QString newer
        = store.insert(testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background));
    const QString newest
        = store.insert(testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background));

    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(store.entry(pinned).has_value());
    QVERIFY(store.entry(newer) == std::nullopt);
    QVERIFY(store.entry(newest).has_value());

    store.release(pinned);
    QVERIFY(store.entry(pinned).has_value());
    store.releasePinLease(pinned, kiriview::DisplayImagePinKind::Visible);
    QVERIFY(store.entry(pinned) == std::nullopt);
}

void TestDisplayImageStore::allPinKindsPreventEvictionAndReleaseDefersRemoval()
{
    for (const kiriview::DisplayImagePinKind kind : allPinKinds()) {
        kiriview::DisplayImageStore store(128);

        const QString pinned = store.insert(
            testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background));
        QVERIFY(store.acquirePinLease(pinned, kind));
        const QString newer = store.insert(
            testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background));
        const QString newest = store.insert(
            testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background));

        QCOMPARE(store.size(), qsizetype(2));
        QVERIFY(store.entry(pinned).has_value());
        QVERIFY(store.entry(newer) == std::nullopt);
        QVERIFY(store.entry(newest).has_value());

        store.release(pinned);
        QVERIFY(store.entry(pinned).has_value());
        store.releasePinLease(pinned, kind);
        QVERIFY(store.entry(pinned) == std::nullopt);
    }
}

void TestDisplayImageStore::reusableAcquisitionReturnsBufferedEntryForMatchingKey()
{
    kiriview::DisplayImageStore store(1024);
    const kiriview::DisplayImageReuseKey key = testReuseKey();

    const QString first = store.acquireReusable(testEntry(QSize(8, 4)), key);
    QVERIFY(!first.isEmpty());
    QVERIFY(store.acquirePinLease(first, kiriview::DisplayImagePinKind::BufferedDisplay));
    store.release(first);
    QVERIFY(store.entry(first).has_value());

    const QString second = store.acquireReusable(testEntry(QSize(8, 4)), key);

    QCOMPARE(second, first);
    QCOMPARE(store.size(), qsizetype(1));

    store.releasePinLease(first, kiriview::DisplayImagePinKind::BufferedDisplay);
    QVERIFY(store.entry(first).has_value());

    store.release(second);
    QVERIFY(!store.entry(first).has_value());
}

void TestDisplayImageStore::reusableAcquisitionClearsDeferredReleaseForAllPinKinds()
{
    for (const kiriview::DisplayImagePinKind kind : allPinKinds()) {
        kiriview::DisplayImageStore store(1024);
        const kiriview::DisplayImageReuseKey key
            = testReuseKey(QStringLiteral("pinned-page-%1").arg(static_cast<int>(kind)));

        const QString first = store.acquireReusable(testEntry(QSize(8, 4)), key);
        QVERIFY(!first.isEmpty());
        QVERIFY(store.acquirePinLease(first, kind));
        store.release(first);
        QVERIFY(store.entry(first).has_value());

        const QString second = store.acquireReusable(testEntry(QSize(8, 4)), key);

        QCOMPARE(second, first);
        QCOMPARE(store.size(), qsizetype(1));

        store.releasePinLease(first, kind);
        QVERIFY(store.entry(first).has_value());

        store.release(second);
        QVERIFY(!store.entry(first).has_value());
    }
}

void TestDisplayImageStore::reusableAcquisitionRequiresExactKeyMatch()
{
    kiriview::DisplayImageStore store(1024);
    const kiriview::DisplayImageReuseKey firstKey = testReuseKey(QStringLiteral("page-a"));
    kiriview::DisplayImageReuseKey secondKey = firstKey;
    secondKey.sourceIdentity = QStringLiteral("page-b");
    secondKey.locationIdentity = QStringLiteral("page-b");

    const QString first = store.acquireReusable(testEntry(QSize(8, 4)), firstKey);
    QVERIFY(!first.isEmpty());
    QVERIFY(store.acquirePinLease(first, kiriview::DisplayImagePinKind::BufferedDisplay));
    store.release(first);

    const QString second = store.acquireReusable(testEntry(QSize(8, 4)), secondKey);

    QVERIFY(!second.isEmpty());
    QVERIFY(first != second);
    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(store.entry(first).has_value());
    QVERIFY(store.entry(second).has_value());
}

void TestDisplayImageStore::indexedLookupStatsStayConstantForIdAndReusableLookups()
{
    kiriview::DisplayImageStore store(8192);
    std::vector<QString> ids;
    std::vector<kiriview::DisplayImageReuseKey> keys;

    for (int index = 0; index < 64; ++index) {
        kiriview::DisplayImageReuseKey key = testReuseKey(QStringLiteral("page-%1").arg(index));
        key.locationIdentity = QStringLiteral("scope-%1").arg(index);
        keys.push_back(key);
        ids.push_back(store.acquireReusable(testEntry(QSize(4, 4)), key));
    }

    assertStoreIndexesConsistent(store);
    kiriview::DisplayImageStoreDebugStats stats = store.debugStats();
    QCOMPARE(stats.entryCount, static_cast<qsizetype>(ids.size()));
    QCOMPARE(stats.reuseIndexEntryCount, stats.entryCount);
    QCOMPARE(stats.evictionIndexEntryCount, stats.entryCount);

    const QString targetId = ids.back();
    QVERIFY(store.entry(targetId).has_value());
    stats = store.debugStats();
    QCOMPARE(stats.lastIdLookupEntryScanCount, qsizetype(0));

    const QString reused = store.acquireReusable(testEntry(QSize(4, 4)), keys.back());
    QCOMPARE(reused, targetId);
    stats = store.debugStats();
    QCOMPARE(stats.lastReuseLookupEntryScanCount, qsizetype(0));
    QCOMPARE(stats.reuseIndexEntryCount, stats.entryCount);
    assertStoreIndexesConsistent(store);
}

void TestDisplayImageStore::indexesStayConsistentAcrossReuseEvictionReleaseAndClear()
{
    kiriview::DisplayImageStore store(192);
    const kiriview::DisplayImageReuseKey pinnedKey = testReuseKey(QStringLiteral("pinned"));
    const kiriview::DisplayImageReuseKey evictableKey = testReuseKey(QStringLiteral("evictable"));

    const QString pinned = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background), pinnedKey);
    const QString evictable = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background), evictableKey);
    const QString plain
        = store.insert(testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background));

    QVERIFY(!pinned.isEmpty());
    QVERIFY(!evictable.isEmpty());
    QVERIFY(!plain.isEmpty());
    assertStoreIndexesConsistent(store);
    QCOMPARE(store.debugStats().entryCount, qsizetype(3));
    QCOMPARE(store.debugStats().reuseIndexEntryCount, qsizetype(2));
    QCOMPARE(store.debugStats().evictionIndexEntryCount, qsizetype(3));

    QVERIFY(store.acquirePinLease(pinned, kiriview::DisplayImagePinKind::FrameRetention));
    assertStoreIndexesConsistent(store);
    QCOMPARE(store.debugStats().evictionIndexEntryCount, qsizetype(2));

    store.release(pinned);
    QVERIFY(store.entry(pinned).has_value());
    assertStoreIndexesConsistent(store);
    QCOMPARE(store.debugStats().entryCount, qsizetype(3));
    QCOMPARE(store.debugStats().reuseIndexEntryCount, qsizetype(2));
    QCOMPARE(store.debugStats().evictionIndexEntryCount, qsizetype(2));

    const QString reacquired = store.acquireReusable(
        testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Background), pinnedKey);
    QCOMPARE(reacquired, pinned);
    store.releasePinLease(pinned, kiriview::DisplayImagePinKind::FrameRetention);
    QVERIFY(store.entry(pinned).has_value());
    assertStoreIndexesConsistent(store);
    QCOMPARE(store.debugStats().evictionIndexEntryCount, qsizetype(3));

    const QString overflow
        = store.insert(testEntry(QSize(4, 4), kiriview::DisplayImageRetentionPriority::Visible));
    QVERIFY(!overflow.isEmpty());
    assertStoreIndexesConsistent(store);
    QCOMPARE(store.debugStats().entryCount, qsizetype(3));
    QVERIFY(store.entry(evictable) == std::nullopt);
    QVERIFY(store.entry(pinned).has_value());

    store.release(pinned);
    QVERIFY(store.entry(pinned) == std::nullopt);
    assertStoreIndexesConsistent(store);
    QCOMPARE(store.debugStats().entryCount, qsizetype(2));

    store.clear();
    assertStoreIndexesConsistent(store);
    QCOMPARE(store.debugStats().entryCount, qsizetype(0));
    QCOMPARE(store.debugStats().reuseIndexEntryCount, qsizetype(0));
    QCOMPARE(store.debugStats().evictionIndexEntryCount, qsizetype(0));
}

void TestDisplayImageStore::providerRequestsAreThreadSafeReads()
{
    auto store = std::make_shared<kiriview::DisplayImageStore>(4096);
    const QString id = store->insert(testEntry(QSize(16, 8)));
    kiriview::DisplayImageProvider provider(store);

    std::vector<std::future<QSize>> futures;
    for (int index = 0; index < 32; ++index) {
        futures.push_back(std::async(std::launch::async,
            [&provider, id]() { return provider.requestImage(id, nullptr, QSize(8, 8)).size(); }));
    }

    for (std::future<QSize>& future : futures) {
        QCOMPARE(future.get(), QSize(16, 8));
    }
}

QTEST_GUILESS_MAIN(TestDisplayImageStore)

#include "tst_displayimagestore.moc"
