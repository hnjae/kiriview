// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnaildemand.h"

#include <QObject>
#include <QTest>

class TestActiveNavigationThumbnailDemand : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bucketPolicyMapsPhysicalEdge();
};

void TestActiveNavigationThumbnailDemand::bucketPolicyMapsPhysicalEdge()
{
    using Bucket = kiriview::ActiveNavigationThumbnailDemandBucket;
    QCOMPARE(kiriview::activeNavigationThumbnailDemandBucketForPhysicalMaxEdge(0), Bucket::None);
    QCOMPARE(
        kiriview::activeNavigationThumbnailDemandBucketForPhysicalMaxEdge(128), Bucket::Normal);
    QCOMPARE(kiriview::activeNavigationThumbnailDemandBucketForPhysicalMaxEdge(129), Bucket::Large);
    QCOMPARE(kiriview::activeNavigationThumbnailDemandBucketForPhysicalMaxEdge(256), Bucket::Large);
    QCOMPARE(
        kiriview::activeNavigationThumbnailDemandBucketForPhysicalMaxEdge(512), Bucket::XLarge);
    QCOMPARE(
        kiriview::activeNavigationThumbnailDemandBucketForPhysicalMaxEdge(513), Bucket::XXLarge);
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailDemand)

#include "tst_activenavigationthumbnaildemand.moc"
