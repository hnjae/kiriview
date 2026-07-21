// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagespreadmodepolicy.h"

#include <QObject>
#include <QTest>

class TestImageSpreadModePolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readingControlsRequireDisplayedComicArchiveImage();
};

void TestImageSpreadModePolicy::readingControlsRequireDisplayedComicArchiveImage()
{
    QVERIFY(!kiriview::imageSpreadReadingControlsAvailable(
        kiriview::ImageSpreadReadingAvailability { false, true, true }));
    QVERIFY(!kiriview::imageSpreadReadingControlsAvailable(
        kiriview::ImageSpreadReadingAvailability { true, false, true }));
    QVERIFY(!kiriview::imageSpreadReadingControlsAvailable(
        kiriview::ImageSpreadReadingAvailability { true, true, false }));
    QVERIFY(kiriview::imageSpreadReadingControlsAvailable(
        kiriview::ImageSpreadReadingAvailability { true, true, true }));
}

QTEST_GUILESS_MAIN(TestImageSpreadModePolicy)

#include "tst_imagespreadmodepolicy.moc"
