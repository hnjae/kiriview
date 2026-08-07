// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagespreadmodepolicy.h"

#include <QObject>
#include <QTest>

class TestImageSpreadModePolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void primaryPageSpreadEligibilityRequiresDisplayedComicArchiveImage();
};

void TestImageSpreadModePolicy::primaryPageSpreadEligibilityRequiresDisplayedComicArchiveImage()
{
    QVERIFY(!kiriview::imageSpreadPrimaryPageEligible(
        kiriview::ImageSpreadPrimaryPageEligibility { false, true, true }));
    QVERIFY(!kiriview::imageSpreadPrimaryPageEligible(
        kiriview::ImageSpreadPrimaryPageEligibility { true, false, true }));
    QVERIFY(!kiriview::imageSpreadPrimaryPageEligible(
        kiriview::ImageSpreadPrimaryPageEligibility { true, true, false }));
    QVERIFY(kiriview::imageSpreadPrimaryPageEligible(
        kiriview::ImageSpreadPrimaryPageEligibility { true, true, true }));
}

QTEST_GUILESS_MAIN(TestImageSpreadModePolicy)

#include "tst_imagespreadmodepolicy.moc"
