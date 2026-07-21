// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/heifcontainer.h"

#include "image_test_support.h"

#include <QByteArray>
#include <QObject>
#include <QTest>

namespace {
using kiriview::TestSupport::heifFtypBox;
}

class TestHeifContainer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void detectsMajorAndCompatibleBrands();
};

void TestHeifContainer::detectsMajorAndCompatibleBrands()
{
    const QByteArray avifStill = heifFtypBox("avif", {});
    const kiriview::HeifContainerInfo avifStillInfo = kiriview::heifContainerInfo(avifStill);
    QVERIFY(avifStillInfo.isHeif());
    QVERIFY(avifStillInfo.stillImage);
    QVERIFY(!avifStillInfo.imageSequence);
    QVERIFY(kiriview::isLikelyHeifContainer(avifStill));
    QVERIFY(kiriview::isLikelyHeifStillImageContainer(avifStill));

    const QByteArray avifSequence = heifFtypBox("avis", {});
    const kiriview::HeifContainerInfo avifSequenceInfo = kiriview::heifContainerInfo(avifSequence);
    QVERIFY(avifSequenceInfo.isHeif());
    QVERIFY(!avifSequenceInfo.stillImage);
    QVERIFY(avifSequenceInfo.imageSequence);
    QVERIFY(kiriview::isLikelyHeifContainer(avifSequence));
    QVERIFY(!kiriview::isLikelyHeifStillImageContainer(avifSequence));

    const QByteArray compatibleStill = heifFtypBox("zzzz", { "mif1" });
    QVERIFY(kiriview::isLikelyHeifContainer(compatibleStill));
    QVERIFY(kiriview::isLikelyHeifStillImageContainer(compatibleStill));

    const QByteArray compatibleSequence = heifFtypBox("zzzz", { "avcs" });
    QVERIFY(kiriview::isLikelyHeifContainer(compatibleSequence));
    QVERIFY(!kiriview::isLikelyHeifStillImageContainer(compatibleSequence));

    const QByteArray nonHeif = heifFtypBox("png ", {});
    const kiriview::HeifContainerInfo nonHeifInfo = kiriview::heifContainerInfo(nonHeif);
    QVERIFY(!nonHeifInfo.isHeif());
    QVERIFY(!nonHeifInfo.stillImage);
    QVERIFY(!nonHeifInfo.imageSequence);
    QVERIFY(!kiriview::isLikelyHeifContainer(nonHeif));
    QVERIFY(!kiriview::isLikelyHeifStillImageContainer(nonHeif));
}

QTEST_GUILESS_MAIN(TestHeifContainer)

#include "tst_heifcontainer.moc"
