// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rendering/imagerendering.h"

#include <QObject>
#include <QSize>
#include <QSizeF>
#include <QTest>

class TestImageRendering : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void scaledImageSizeToFitKeepsAspectRatioWithoutUpscaling();
    void scaledImageSizeToFitRejectsInvalidInput();
    void firstDisplayScaledImageSizeOnlyReturnsDownscaleTarget();
};

void TestImageRendering::scaledImageSizeToFitKeepsAspectRatioWithoutUpscaling()
{
    QCOMPARE(kiriview::scaledImageSizeToFit(QSizeF(4000.0, 2000.0), QSize(1000, 1000)),
        QSize(1000, 500));
    QCOMPARE(kiriview::scaledImageSizeToFit(QSizeF(333.0, 100.0), QSize(200, 200)), QSize(200, 61));
    QCOMPARE(
        kiriview::scaledImageSizeToFit(QSizeF(200.0, 100.0), QSize(1000, 1000)), QSize(200, 100));
    QCOMPARE(kiriview::scaledImageSizeToFit(QSizeF(0.5, 0.5), QSize(100, 100)), QSize(1, 1));
}

void TestImageRendering::scaledImageSizeToFitRejectsInvalidInput()
{
    const qreal nan = std::numeric_limits<qreal>::quiet_NaN();
    QCOMPARE(kiriview::scaledImageSizeToFit(QSizeF(), QSize(100, 100)), QSize());
    QCOMPARE(kiriview::scaledImageSizeToFit(QSizeF(100.0, 100.0), QSize()), QSize());
    QCOMPARE(kiriview::scaledImageSizeToFit(QSizeF(nan, 100.0), QSize(100, 100)), QSize());
}

void TestImageRendering::firstDisplayScaledImageSizeOnlyReturnsDownscaleTarget()
{
    QCOMPARE(
        kiriview::firstDisplayScaledImageSize(QSize(1600, 1200), QSize(400, 300)), QSize(400, 300));
    QCOMPARE(kiriview::firstDisplayScaledImageSize(QSize(200, 100), QSize(400, 300)), QSize());
    QCOMPARE(kiriview::firstDisplayScaledImageSize(QSize(1600, 1200), QSize()), QSize());
}

QTEST_GUILESS_MAIN(TestImageRendering)

#include "tst_imagerendering.moc"
