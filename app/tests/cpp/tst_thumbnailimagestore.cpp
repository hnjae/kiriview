// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/thumbnailimagestore.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QtGlobal>
#include <vector>

class TestThumbnailImageStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void evictsLeastRecentlyUsedImagesByByteBudget();
    void imageAccessRefreshesLruOrder();
    void visiblePriorityOutlivesNearbyWhenBudgetIsTight();
    void evictsBackgroundBeforeNearbyBeforeVisible();
    void explicitReleaseRemovesImageAndByteCost();
    void priorityUpdateRefreshesLruOrder();
    void budgetTrimPreservesSurvivingLookupsAndAccounting();
    void idsAreStableAndNotReusedAfterEviction();
    void newlyInsertedEntryCanBeImmediatelyEvictedByPriority();
    void clearRemovesAllLookupsAndAccounting();
    void mutationSubscriptionDistinguishesPressureFromAdmissionOpportunity();
    void priorityDemotionReportsAdmissionOpportunity();
    void smallerReplacementReportsRecoveredFreeSpace();
    void mutationSubscriptionEndsWithReceiverLifetime();
    void providerReturnsStoredImageForRequestedSize();
};

namespace {
QImage testImage(QSize size, QColor color)
{
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(color);
    return image;
}

QImage testImage(QColor color) { return testImage(QSize(4, 4), color); }
}

void TestThumbnailImageStore::evictsLeastRecentlyUsedImagesByByteBudget()
{
    kiriview::ThumbnailImageStore store(128);

    const QString first = store.insert(testImage(Qt::red));
    const QString second = store.insert(testImage(Qt::green));
    const QString third = store.insert(testImage(Qt::blue));

    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY(!third.isEmpty());
    QCOMPARE(store.size(), qsizetype(2));
    QCOMPARE(store.byteCost(), qsizetype(128));
    QVERIFY(store.image(first).isNull());
    QVERIFY(!store.image(second).isNull());
    QVERIFY(!store.image(third).isNull());
}

void TestThumbnailImageStore::imageAccessRefreshesLruOrder()
{
    kiriview::ThumbnailImageStore store(128);

    const QString first = store.insert(testImage(Qt::red));
    const QString second = store.insert(testImage(Qt::green));
    QVERIFY(!store.image(first).isNull());
    const QString third = store.insert(testImage(Qt::blue));

    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(!store.image(first).isNull());
    QVERIFY(store.image(second).isNull());
    QVERIFY(!store.image(third).isNull());
}

void TestThumbnailImageStore::visiblePriorityOutlivesNearbyWhenBudgetIsTight()
{
    kiriview::ThumbnailImageStore store(128);

    const QString visible
        = store.insert(testImage(Qt::red), kiriview::ThumbnailImageRetentionPriority::Visible);
    const QString nearby
        = store.insert(testImage(Qt::green), kiriview::ThumbnailImageRetentionPriority::Nearby);
    const QString newerNearby
        = store.insert(testImage(Qt::blue), kiriview::ThumbnailImageRetentionPriority::Nearby);

    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(!store.image(visible).isNull());
    QVERIFY(store.image(nearby).isNull());
    QVERIFY(!store.image(newerNearby).isNull());
}

void TestThumbnailImageStore::evictsBackgroundBeforeNearbyBeforeVisible()
{
    kiriview::ThumbnailImageStore store(128);

    const QString background
        = store.insert(testImage(Qt::red), kiriview::ThumbnailImageRetentionPriority::Background);
    const QString nearby
        = store.insert(testImage(Qt::green), kiriview::ThumbnailImageRetentionPriority::Nearby);
    const QString visible
        = store.insert(testImage(Qt::blue), kiriview::ThumbnailImageRetentionPriority::Visible);

    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(store.image(background).isNull());
    QVERIFY(!store.image(nearby).isNull());
    QVERIFY(!store.image(visible).isNull());

    const QString secondBackground
        = store.insert(testImage(Qt::cyan), kiriview::ThumbnailImageRetentionPriority::Background);
    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(store.image(secondBackground).isNull());
    QVERIFY(!store.image(nearby).isNull());
    QVERIFY(!store.image(visible).isNull());

    const QString newerNearby
        = store.insert(testImage(Qt::magenta), kiriview::ThumbnailImageRetentionPriority::Nearby);
    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(store.image(nearby).isNull());
    QVERIFY(!store.image(newerNearby).isNull());
    QVERIFY(!store.image(visible).isNull());
}

void TestThumbnailImageStore::explicitReleaseRemovesImageAndByteCost()
{
    kiriview::ThumbnailImageStore store(128);

    const QString id = store.insert(testImage(Qt::red));
    QVERIFY(!id.isEmpty());
    QCOMPARE(store.size(), qsizetype(1));
    QCOMPARE(store.byteCost(), qsizetype(64));

    store.release(id);
    QCOMPARE(store.size(), qsizetype(0));
    QCOMPARE(store.byteCost(), qsizetype(0));
    QVERIFY(store.image(id).isNull());
}

void TestThumbnailImageStore::priorityUpdateRefreshesLruOrder()
{
    kiriview::ThumbnailImageStore store(128);

    const QString first = store.insert(testImage(Qt::red));
    const QString second = store.insert(testImage(Qt::green));
    store.updatePriority(first, kiriview::ThumbnailImageRetentionPriority::Nearby);
    const QString third = store.insert(testImage(Qt::blue));

    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(!store.image(first).isNull());
    QVERIFY(store.image(second).isNull());
    QVERIFY(!store.image(third).isNull());
}

void TestThumbnailImageStore::budgetTrimPreservesSurvivingLookupsAndAccounting()
{
    kiriview::ThumbnailImageStore store(192);

    const QString first = store.insert(testImage(Qt::red));
    const QString second = store.insert(testImage(Qt::green));
    const QString third = store.insert(testImage(Qt::blue));
    store.setByteBudget(128);

    QCOMPARE(store.byteBudget(), qsizetype(128));
    QCOMPARE(store.byteCost(), qsizetype(128));
    QCOMPARE(store.size(), qsizetype(2));
    QVERIFY(store.image(first).isNull());
    QVERIFY(!store.image(second).isNull());
    QVERIFY(!store.image(third).isNull());

    store.release(second);
    QCOMPARE(store.byteCost(), qsizetype(64));
    QVERIFY(store.image(second).isNull());
    QVERIFY(!store.image(third).isNull());
}

void TestThumbnailImageStore::idsAreStableAndNotReusedAfterEviction()
{
    kiriview::ThumbnailImageStore store(128);

    const QString first = store.insert(testImage(Qt::red));
    const QString second = store.insert(testImage(Qt::green));
    const QString third = store.insert(testImage(Qt::blue));
    const QString fourth = store.insert(testImage(Qt::cyan));

    QVERIFY(first != second);
    QVERIFY(first != third);
    QVERIFY(first != fourth);
    QVERIFY(second != third);
    QVERIFY(second != fourth);
    QVERIFY(third != fourth);
    QVERIFY(store.image(first).isNull());
    QVERIFY(store.image(second).isNull());
    QVERIFY(!store.image(third).isNull());
    QVERIFY(!store.image(fourth).isNull());
}

void TestThumbnailImageStore::newlyInsertedEntryCanBeImmediatelyEvictedByPriority()
{
    kiriview::ThumbnailImageStore store(128);

    const QString visible
        = store.insert(testImage(Qt::red), kiriview::ThumbnailImageRetentionPriority::Visible);
    const QString nearby
        = store.insert(testImage(Qt::green), kiriview::ThumbnailImageRetentionPriority::Nearby);
    const QString background
        = store.insert(testImage(Qt::blue), kiriview::ThumbnailImageRetentionPriority::Background);

    QVERIFY(background.isEmpty());
    QCOMPARE(store.size(), qsizetype(2));
    QCOMPARE(store.byteCost(), qsizetype(128));
    QVERIFY(!store.image(visible).isNull());
    QVERIFY(!store.image(nearby).isNull());
    QVERIFY(store.image(background).isNull());
}

void TestThumbnailImageStore::clearRemovesAllLookupsAndAccounting()
{
    kiriview::ThumbnailImageStore store(128);

    const QString first = store.insert(testImage(Qt::red));
    const QString second = store.insert(testImage(Qt::green));
    store.clear();

    QCOMPARE(store.size(), qsizetype(0));
    QCOMPARE(store.byteCost(), qsizetype(0));
    QVERIFY(store.image(first).isNull());
    QVERIFY(store.image(second).isNull());
}

void TestThumbnailImageStore::mutationSubscriptionDistinguishesPressureFromAdmissionOpportunity()
{
    kiriview::ThumbnailImageStore store(128);
    QObject receiver;
    std::vector<kiriview::ThumbnailImageStoreMutation> notifications;
    const QMetaObject::Connection connection = store.subscribeToMutations(
        &receiver, [&store, &notifications](kiriview::ThumbnailImageStoreMutation mutation) {
            static_cast<void>(store.byteCost());
            notifications.push_back(std::move(mutation));
        });
    QVERIFY(connection);

    const QString first = store.insert(testImage(Qt::red));
    const QString second = store.insert(testImage(Qt::green));
    const QString third = store.insert(testImage(Qt::blue));
    QCOMPARE(notifications.size(), std::size_t(1));
    QCOMPARE(notifications.front().removedIds, QStringList { first });
    QVERIFY(notifications.front().pressureConstrained);
    QVERIFY(!notifications.front().admissionOpportunity);

    store.release(second);
    QCOMPARE(notifications.size(), std::size_t(2));
    QCOMPARE(notifications.back().removedIds, QStringList { second });
    QVERIFY(!notifications.back().pressureConstrained);
    QVERIFY(notifications.back().admissionOpportunity);

    store.setByteBudget(32);
    QCOMPARE(notifications.size(), std::size_t(3));
    QCOMPARE(notifications.back().removedIds, QStringList { third });
    QVERIFY(notifications.back().pressureConstrained);
    QVERIFY(!notifications.back().admissionOpportunity);

    store.setByteBudget(128);
    QCOMPARE(notifications.size(), std::size_t(4));
    QVERIFY(notifications.back().removedIds.isEmpty());
    QVERIFY(!notifications.back().pressureConstrained);
    QVERIFY(notifications.back().admissionOpportunity);
    const QString fourth = store.insert(testImage(Qt::cyan));
    const QString fifth = store.insert(testImage(Qt::magenta));
    store.clear();
    QCOMPARE(notifications.size(), std::size_t(5));
    QVERIFY(!notifications.back().pressureConstrained);
    QVERIFY(notifications.back().admissionOpportunity);
    QSet<QString> clearedIds;
    for (const QString& id : notifications.back().removedIds) {
        clearedIds.insert(id);
    }
    QCOMPARE(clearedIds, QSet<QString>({ fourth, fifth }));
}

void TestThumbnailImageStore::mutationSubscriptionEndsWithReceiverLifetime()
{
    kiriview::ThumbnailImageStore store(64);
    int notificationCount = 0;
    {
        QObject receiver;
        const QMetaObject::Connection connection = store.subscribeToMutations(&receiver,
            [&notificationCount](kiriview::ThumbnailImageStoreMutation) { ++notificationCount; });
        QVERIFY(connection);
        store.insert(testImage(Qt::red));
        store.insert(testImage(Qt::green));
        QCOMPARE(notificationCount, 1);
    }

    store.insert(testImage(Qt::blue));
    QCOMPARE(notificationCount, 1);
}

void TestThumbnailImageStore::priorityDemotionReportsAdmissionOpportunity()
{
    kiriview::ThumbnailImageStore store(64);
    const QString id
        = store.insert(testImage(Qt::red), kiriview::ThumbnailImageRetentionPriority::Visible);
    QObject receiver;
    std::vector<kiriview::ThumbnailImageStoreMutation> notifications;
    const QMetaObject::Connection connection = store.subscribeToMutations(
        &receiver, [&notifications](kiriview::ThumbnailImageStoreMutation mutation) {
            notifications.push_back(std::move(mutation));
        });
    QVERIFY(connection);

    store.updatePriority(id, kiriview::ThumbnailImageRetentionPriority::Nearby);

    QCOMPARE(notifications.size(), std::size_t(1));
    QVERIFY(notifications.front().removedIds.isEmpty());
    QVERIFY(!notifications.front().pressureConstrained);
    QVERIFY(notifications.front().admissionOpportunity);

    store.updatePriority(id, kiriview::ThumbnailImageRetentionPriority::Nearby);
    store.updatePriority(id, kiriview::ThumbnailImageRetentionPriority::Visible);
    QCOMPARE(notifications.size(), std::size_t(1));
}

void TestThumbnailImageStore::smallerReplacementReportsRecoveredFreeSpace()
{
    kiriview::ThumbnailImageStore store(64);
    QObject receiver;
    std::vector<kiriview::ThumbnailImageStoreMutation> notifications;
    const QMetaObject::Connection connection = store.subscribeToMutations(
        &receiver, [&notifications](kiriview::ThumbnailImageStoreMutation mutation) {
            notifications.push_back(std::move(mutation));
        });
    QVERIFY(connection);

    const QString large
        = store.insert(testImage(Qt::red), kiriview::ThumbnailImageRetentionPriority::Nearby);
    const QString small = store.insert(
        testImage(QSize(2, 4), Qt::blue), kiriview::ThumbnailImageRetentionPriority::Visible);

    QVERIFY(!small.isEmpty());
    QCOMPARE(notifications.size(), std::size_t(1));
    QCOMPARE(notifications.front().removedIds, QStringList { large });
    QVERIFY(notifications.front().pressureConstrained);
    QVERIFY(notifications.front().admissionOpportunity);
    QCOMPARE(store.byteCost(), qsizetype(32));
}

void TestThumbnailImageStore::providerReturnsStoredImageForRequestedSize()
{
    auto store = std::make_shared<kiriview::ThumbnailImageStore>(12800);
    const QString id = store->insert(testImage(QSize(80, 40), Qt::red));
    QVERIFY(!id.isEmpty());

    kiriview::ThumbnailImageProvider provider(store);
    QSize reportedSize;
    const QImage image = provider.requestImage(id, &reportedSize, QSize(20, 20));

    QCOMPARE(reportedSize, QSize(80, 40));
    QCOMPARE(image.size(), QSize(80, 40));
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
}

QTEST_GUILESS_MAIN(TestThumbnailImageStore)

#include "tst_thumbnailimagestore.moc"
