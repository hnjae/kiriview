// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imagedocumentnotifications.h"

#include <QObject>
#include <QTest>
#include <vector>

class TestImageDocumentNotifications : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void applicationOwnedNotificationPlansRemainFocused();
};

namespace {
void compareChanges(const std::vector<kiriview::ImageDocumentChange>& actual,
    const std::vector<kiriview::ImageDocumentChange>& expected)
{
    QCOMPARE(actual.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        QCOMPARE(actual.at(index), expected.at(index));
    }
}
}

void TestImageDocumentNotifications::applicationOwnedNotificationPlansRemainFocused()
{
    compareChanges(kiriview::imageDocumentDisplayedLocationNotifications(true, true),
        { kiriview::ImageDocumentChange::DisplayedUrl,
            kiriview::ImageDocumentChange::WindowTitleFileName });
    compareChanges(kiriview::imageDocumentDisplayedLocationNotifications(false, false), {});
    compareChanges(kiriview::imageDocumentTwoPageModeNotifications(),
        { kiriview::ImageDocumentChange::TwoPageMode });
    compareChanges(kiriview::imageDocumentRightToLeftReadingNotifications(false),
        { kiriview::ImageDocumentChange::RightToLeftReading });
    compareChanges(kiriview::imageDocumentRightToLeftReadingNotifications(true),
        { kiriview::ImageDocumentChange::RightToLeftReading,
            kiriview::ImageDocumentChange::TwoPageMode });
}

QTEST_GUILESS_MAIN(TestImageDocumentNotifications)

#include "tst_imagedocumentnotifications.moc"
