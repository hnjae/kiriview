// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagesourcedata.h"

#include <QTest>

class TestImageSourceData : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void enforcesPerSourceAndAggregateLimits();
    void copiedLeaseRetainsOneReservationUntilLastRelease();
};

void TestImageSourceData::enforcesPerSourceAndAggregateLimits()
{
    kiriview::ImageSourceDataBudget budget(10, 6);
    kiriview::ImageSourceDataLease first = budget.startLease();
    kiriview::ImageSourceDataLease second = budget.startLease();

    QVERIFY(first.tryReserve(6));
    QVERIFY(!first.tryReserve(1));
    QVERIFY(second.tryReserve(4));
    QVERIFY(!second.tryReserve(1));
    QCOMPARE(first.reservedByteCount(), qsizetype(6));
    QCOMPARE(second.reservedByteCount(), qsizetype(4));
    QCOMPARE(budget.reservedByteCount(), qsizetype(10));

    first = {};
    QCOMPARE(budget.reservedByteCount(), qsizetype(4));
    QVERIFY(second.tryReserve(1));
    QCOMPARE(budget.reservedByteCount(), qsizetype(5));
}

void TestImageSourceData::copiedLeaseRetainsOneReservationUntilLastRelease()
{
    kiriview::ImageSourceDataBudget budget(8, 8);
    kiriview::ImageSourceDataLease original = budget.startLease();
    QVERIFY(original.tryReserve(7));

    kiriview::ImageSourceDataLease retained = original;
    original = {};
    QCOMPARE(retained.reservedByteCount(), qsizetype(7));
    QCOMPARE(budget.reservedByteCount(), qsizetype(7));

    retained = {};
    QCOMPARE(budget.reservedByteCount(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestImageSourceData)

#include "tst_imagesourcedata.moc"
