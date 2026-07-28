// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "image_test_support.h"
#include "location/imageurl.h"
#include "predecode/predecodedisplayedhistory.h"

#include <QObject>
#include <QTest>
#include <QUrl>

namespace {
kiriview::DisplayedImageLocation displayedLocation(const QUrl& url)
{
    return kiriview::DisplayedImageLocation::fromUrl(url);
}
}

class TestPredecodeDisplayedHistory : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void currentUrlsAreNormalizedAndDeduplicated();
    void previousCurrentUrlsMoveToRecentHistory();
    void redisplayedUrlsAreRemovedFromRecentHistory();
    void recentHistoryKeepsOnlyMostRecentFourUrls();
    void clearRemovesCurrentAndRecentHistory();
};

void TestPredecodeDisplayedHistory::currentUrlsAreNormalizedAndDeduplicated()
{
    kiriview::PredecodeDisplayedHistory history;
    const QUrl firstUrl = kiriview::TestSupport::indexedImageUrl(1);
    const QUrl secondUrl = kiriview::TestSupport::indexedImageUrl(2);

    history.setDisplayedLocations({ displayedLocation(firstUrl), {}, displayedLocation(firstUrl),
        displayedLocation(secondUrl) });

    QVERIFY(history.currentContains(displayedLocation(firstUrl)));
    QVERIFY(history.currentContains(displayedLocation(secondUrl)));
    QCOMPARE(history.currentPriority(displayedLocation(firstUrl)), std::size_t(0));
    QCOMPARE(history.currentPriority(displayedLocation(secondUrl)), std::size_t(1));
    QVERIFY(!history.recentContains(displayedLocation(firstUrl)));
}

void TestPredecodeDisplayedHistory::previousCurrentUrlsMoveToRecentHistory()
{
    kiriview::PredecodeDisplayedHistory history;
    const QUrl firstUrl = kiriview::TestSupport::indexedImageUrl(1);
    const QUrl secondUrl = kiriview::TestSupport::indexedImageUrl(2);
    const QUrl thirdUrl = kiriview::TestSupport::indexedImageUrl(3);

    history.setDisplayedLocations({ displayedLocation(firstUrl), displayedLocation(secondUrl) });
    history.setDisplayedLocations({ displayedLocation(thirdUrl) });

    QVERIFY(history.currentContains(displayedLocation(kiriview::normalizedImageUrl(thirdUrl))));
    QVERIFY(history.recentContains(displayedLocation(kiriview::normalizedImageUrl(firstUrl))));
    QCOMPARE(history.currentPriority(displayedLocation(kiriview::normalizedImageUrl(thirdUrl))),
        std::size_t(0));
    QCOMPARE(history.recentPriority(displayedLocation(kiriview::normalizedImageUrl(firstUrl))),
        std::size_t(1));
}

void TestPredecodeDisplayedHistory::redisplayedUrlsAreRemovedFromRecentHistory()
{
    kiriview::PredecodeDisplayedHistory history;
    const QUrl firstUrl = kiriview::TestSupport::indexedImageUrl(1);
    const QUrl secondUrl = kiriview::TestSupport::indexedImageUrl(2);

    history.setDisplayedLocations({ displayedLocation(firstUrl) });
    history.setDisplayedLocations({ displayedLocation(secondUrl) });
    history.setDisplayedLocations({ displayedLocation(firstUrl) });

    QVERIFY(history.currentContains(displayedLocation(firstUrl)));
    QVERIFY(!history.recentContains(displayedLocation(firstUrl)));
    QVERIFY(history.recentContains(displayedLocation(secondUrl)));
    QCOMPARE(history.recentPriority(displayedLocation(secondUrl)), std::size_t(0));
}

void TestPredecodeDisplayedHistory::recentHistoryKeepsOnlyMostRecentFourUrls()
{
    kiriview::PredecodeDisplayedHistory history;
    for (int index = 0; index < 6; ++index) {
        history.setDisplayedLocations(
            { displayedLocation(kiriview::TestSupport::indexedImageUrl(index)) });
    }

    QVERIFY(history.currentContains(displayedLocation(kiriview::TestSupport::indexedImageUrl(5))));
    for (int index = 1; index <= 4; ++index) {
        const QUrl url = kiriview::TestSupport::indexedImageUrl(5 - index);
        QVERIFY(history.recentContains(displayedLocation(url)));
        QCOMPARE(
            history.recentPriority(displayedLocation(url)), static_cast<std::size_t>(index - 1));
    }
    QVERIFY(!history.currentContains(displayedLocation(kiriview::TestSupport::indexedImageUrl(0))));
    QVERIFY(!history.recentContains(displayedLocation(kiriview::TestSupport::indexedImageUrl(0))));
}

void TestPredecodeDisplayedHistory::clearRemovesCurrentAndRecentHistory()
{
    kiriview::PredecodeDisplayedHistory history;
    history.setDisplayedLocations({ displayedLocation(kiriview::TestSupport::indexedImageUrl(1)) });
    history.setDisplayedLocations({ displayedLocation(kiriview::TestSupport::indexedImageUrl(2)) });

    history.clear();

    QVERIFY(!history.currentContains(displayedLocation(kiriview::TestSupport::indexedImageUrl(2))));
    QVERIFY(!history.recentContains(displayedLocation(kiriview::TestSupport::indexedImageUrl(1))));
}

QTEST_GUILESS_MAIN(TestPredecodeDisplayedHistory)

#include "tst_predecodedisplayedhistory.moc"
