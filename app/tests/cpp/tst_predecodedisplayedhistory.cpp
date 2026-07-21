// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "image_test_support.h"
#include "location/imageurl.h"
#include "predecode/predecodedisplayedhistory.h"

#include <QObject>
#include <QTest>
#include <QUrl>

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

    history.setDisplayedUrls({ firstUrl, QUrl(), firstUrl, secondUrl });

    QVERIFY(history.currentContains(firstUrl));
    QVERIFY(history.currentContains(secondUrl));
    QCOMPARE(history.currentPriority(firstUrl), std::size_t(0));
    QCOMPARE(history.currentPriority(secondUrl), std::size_t(1));
    QVERIFY(!history.recentContains(firstUrl));
}

void TestPredecodeDisplayedHistory::previousCurrentUrlsMoveToRecentHistory()
{
    kiriview::PredecodeDisplayedHistory history;
    const QUrl firstUrl = kiriview::TestSupport::indexedImageUrl(1);
    const QUrl secondUrl = kiriview::TestSupport::indexedImageUrl(2);
    const QUrl thirdUrl = kiriview::TestSupport::indexedImageUrl(3);

    history.setDisplayedUrls({ firstUrl, secondUrl });
    history.setDisplayedUrls({ thirdUrl });

    QVERIFY(history.currentContains(kiriview::normalizedImageUrl(thirdUrl)));
    QVERIFY(history.recentContains(kiriview::normalizedImageUrl(firstUrl)));
    QCOMPARE(history.currentPriority(kiriview::normalizedImageUrl(thirdUrl)), std::size_t(0));
    QCOMPARE(history.recentPriority(kiriview::normalizedImageUrl(firstUrl)), std::size_t(1));
}

void TestPredecodeDisplayedHistory::redisplayedUrlsAreRemovedFromRecentHistory()
{
    kiriview::PredecodeDisplayedHistory history;
    const QUrl firstUrl = kiriview::TestSupport::indexedImageUrl(1);
    const QUrl secondUrl = kiriview::TestSupport::indexedImageUrl(2);

    history.setDisplayedUrls({ firstUrl });
    history.setDisplayedUrls({ secondUrl });
    history.setDisplayedUrls({ firstUrl });

    QVERIFY(history.currentContains(firstUrl));
    QVERIFY(!history.recentContains(firstUrl));
    QVERIFY(history.recentContains(secondUrl));
    QCOMPARE(history.recentPriority(secondUrl), std::size_t(0));
}

void TestPredecodeDisplayedHistory::recentHistoryKeepsOnlyMostRecentFourUrls()
{
    kiriview::PredecodeDisplayedHistory history;
    for (int index = 0; index < 6; ++index) {
        history.setDisplayedUrls({ kiriview::TestSupport::indexedImageUrl(index) });
    }

    QVERIFY(history.currentContains(kiriview::TestSupport::indexedImageUrl(5)));
    for (int index = 1; index <= 4; ++index) {
        const QUrl url = kiriview::TestSupport::indexedImageUrl(5 - index);
        QVERIFY(history.recentContains(url));
        QCOMPARE(history.recentPriority(url), static_cast<std::size_t>(index - 1));
    }
    QVERIFY(!history.currentContains(kiriview::TestSupport::indexedImageUrl(0)));
    QVERIFY(!history.recentContains(kiriview::TestSupport::indexedImageUrl(0)));
}

void TestPredecodeDisplayedHistory::clearRemovesCurrentAndRecentHistory()
{
    kiriview::PredecodeDisplayedHistory history;
    history.setDisplayedUrls({ kiriview::TestSupport::indexedImageUrl(1) });
    history.setDisplayedUrls({ kiriview::TestSupport::indexedImageUrl(2) });

    history.clear();

    QVERIFY(!history.currentContains(kiriview::TestSupport::indexedImageUrl(2)));
    QVERIFY(!history.recentContains(kiriview::TestSupport::indexedImageUrl(1)));
}

QTEST_GUILESS_MAIN(TestPredecodeDisplayedHistory)

#include "tst_predecodedisplayedhistory.moc"
