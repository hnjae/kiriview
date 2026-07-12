// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/directmediacursor.h"

#include <QObject>
#include <QTest>
#include <QUrl>

namespace {
kiriview::ResolvedNavigationSource resolved(const QUrl& url) { return { url, {}, url }; }
}

class TestDirectMediaCursor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void generationChangesOnlyWithEffectiveIdentity();
    void generationUsesNormalizedEffectiveIdentity();
    void scopeUsesEffectiveUrlParentAndGeneration();
    void scopeEqualityUsesSourceKeysAndGeneration();
    void scopeEqualityIncludesParentSourceKey();
    void scopeMatchingAcceptsNormalizedConfirmation();
    void mismatchedConfirmationPreservesPendingSnapshot();
};

void TestDirectMediaCursor::generationChangesOnlyWithEffectiveIdentity()
{
    kiriview::DirectMediaCursor cursor;
    const QUrl firstImage = QUrl::fromLocalFile(QStringLiteral("/media/01.png"));
    const QUrl secondImage = QUrl::fromLocalFile(QStringLiteral("/media/02.png"));
    const QUrl video = QUrl::fromLocalFile(QStringLiteral("/media/03.mp4"));

    const quint64 initialGeneration = cursor.generation;

    kiriview::requestDirectImageCursor(cursor, resolved(firstImage));
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), firstImage);
    QCOMPARE(cursor.pendingSource.requestedUrl(), firstImage);
    QVERIFY(cursor.stableSource.isEmpty());
    const quint64 requestedGeneration = cursor.generation;
    QVERIFY(requestedGeneration > initialGeneration);

    kiriview::requestDirectImageCursor(cursor, resolved(firstImage));
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), firstImage);
    QCOMPARE(cursor.generation, requestedGeneration);

    kiriview::confirmDirectImageCursor(cursor, firstImage);
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), firstImage);
    QVERIFY(cursor.pendingSource.isEmpty());
    QCOMPARE(cursor.stableSource.requestedUrl(), firstImage);
    const quint64 confirmedGeneration = cursor.generation;
    QCOMPARE(confirmedGeneration, requestedGeneration);

    kiriview::confirmDirectImageCursor(cursor, firstImage);
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), firstImage);
    QCOMPARE(cursor.generation, confirmedGeneration);

    kiriview::requestDirectImageCursor(cursor, resolved(secondImage));
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), secondImage);
    QCOMPARE(cursor.stableSource.requestedUrl(), firstImage);
    QCOMPARE(cursor.pendingSource.requestedUrl(), secondImage);
    const quint64 replacementGeneration = cursor.generation;
    QVERIFY(replacementGeneration > confirmedGeneration);

    kiriview::requestDirectImageCursor(cursor, resolved(secondImage));
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), secondImage);
    QCOMPARE(cursor.generation, replacementGeneration);

    kiriview::restoreDirectImageCursorAfterFailure(cursor);
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), firstImage);
    QVERIFY(cursor.pendingSource.isEmpty());
    QCOMPARE(cursor.stableSource.requestedUrl(), firstImage);
    const quint64 restoredGeneration = cursor.generation;
    QVERIFY(restoredGeneration > replacementGeneration);

    kiriview::restoreDirectImageCursorAfterFailure(cursor);
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), firstImage);
    QCOMPARE(cursor.generation, restoredGeneration);

    kiriview::setDirectVideoCursor(cursor, resolved(video));
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), video);
    QCOMPARE(cursor.stableSource.requestedUrl(), video);
    const quint64 videoGeneration = cursor.generation;
    QVERIFY(videoGeneration > restoredGeneration);

    kiriview::setDirectVideoCursor(cursor, resolved(video));
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), video);
    QCOMPARE(cursor.generation, videoGeneration);

    kiriview::clearDirectMediaCursor(cursor);
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), QUrl());
    QVERIFY(cursor.stableSource.isEmpty());
    const quint64 clearedGeneration = cursor.generation;
    QVERIFY(clearedGeneration > videoGeneration);

    kiriview::clearDirectMediaCursor(cursor);
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), QUrl());
    QCOMPARE(cursor.generation, clearedGeneration);
}

void TestDirectMediaCursor::mismatchedConfirmationPreservesPendingSnapshot()
{
    kiriview::DirectMediaCursor cursor;
    const QUrl requestedImage(QStringLiteral("file:///media/01.png"));
    const QUrl staleConfirmation(QStringLiteral("file:///media/02.png"));
    const kiriview::ResolvedNavigationSource source(requestedImage,
        kiriview::NavigationSourceFacts { QStringLiteral("/host/01.png"), QStringLiteral("/run") },
        QUrl::fromLocalFile(QStringLiteral("/host/01.png")));

    kiriview::requestDirectImageCursor(cursor, source);
    const kiriview::DirectMediaCursor pending = cursor;

    QVERIFY(!kiriview::confirmDirectImageCursor(cursor, staleConfirmation));
    QCOMPARE(cursor.pendingSource.requestedUrl(), pending.pendingSource.requestedUrl());
    QCOMPARE(cursor.pendingSource.navigationUrl(), pending.pendingSource.navigationUrl());
    QCOMPARE(cursor.pendingSource.requestedUrl(), pending.pendingSource.requestedUrl());
    QCOMPARE(cursor.stableSource.requestedUrl(), pending.stableSource.requestedUrl());
    QCOMPARE(cursor.generation, pending.generation);
}

void TestDirectMediaCursor::generationUsesNormalizedEffectiveIdentity()
{
    kiriview::DirectMediaCursor cursor;
    const QUrl requestedImage(QStringLiteral("file:///media/chapter/../01.png"));
    const QUrl displayedImage(QStringLiteral("file:///media/01.png"));
    const QUrl replacementImage(QStringLiteral("file:///media/02.png"));

    kiriview::requestDirectImageCursor(cursor, resolved(requestedImage));
    const quint64 requestedGeneration = cursor.generation;

    kiriview::confirmDirectImageCursor(cursor, displayedImage);
    QCOMPARE(kiriview::effectiveDirectMediaCursorUrl(cursor), requestedImage);
    QCOMPARE(cursor.generation, requestedGeneration);

    kiriview::requestDirectImageCursor(cursor, resolved(replacementImage));
    QVERIFY(cursor.generation > requestedGeneration);
}

void TestDirectMediaCursor::scopeUsesEffectiveUrlParentAndGeneration()
{
    kiriview::DirectMediaCursor cursor;
    const QUrl requestedImage(QStringLiteral("file:///media/a/../b/01.png"));

    kiriview::requestDirectImageCursor(cursor, resolved(requestedImage));

    const kiriview::DirectMediaScope scope = kiriview::directMediaScopeForCursor(cursor);
    QCOMPARE(scope.currentUrl, requestedImage);
    QCOMPARE(scope.parentUrl, QUrl(QStringLiteral("file:///media/b/")));
    QCOMPARE(scope.generation, cursor.generation);
}

void TestDirectMediaCursor::scopeEqualityUsesSourceKeysAndGeneration()
{
    const kiriview::DirectMediaScope requestedScope {
        QUrl(QStringLiteral("file:///media/chapter/../01.png")),
        QUrl(QStringLiteral("file:///media/chapter/..")),
        7,
        kiriview::sourceKeyForUrl(QUrl(QStringLiteral("file:///media/01.png"))),
        kiriview::sourceKeyForUrl(QUrl(QStringLiteral("file:///media/"))),
        QUrl(QStringLiteral("file:///media/01.png")),
    };
    kiriview::DirectMediaScope equivalentScope {
        QUrl(QStringLiteral("file:///media/01.png")),
        QUrl(QStringLiteral("file:///media/")),
        7,
        kiriview::sourceKeyForUrl(QUrl(QStringLiteral("file:///media/01.png"))),
        kiriview::sourceKeyForUrl(QUrl(QStringLiteral("file:///media/"))),
        QUrl(QStringLiteral("file:///media/01.png")),
    };
    QVERIFY(requestedScope == equivalentScope);

    equivalentScope.generation = 8;
    QVERIFY(!(requestedScope == equivalentScope));

    equivalentScope = requestedScope;
    equivalentScope.currentUrl = QUrl(QStringLiteral("file:///media/01.PNG"));
    QVERIFY(requestedScope == equivalentScope);
}

void TestDirectMediaCursor::scopeEqualityIncludesParentSourceKey()
{
    const kiriview::DirectMediaScope firstScope {
        QUrl(QStringLiteral("file:///media/01.png")),
        QUrl(QStringLiteral("file:///media/")),
        7,
        kiriview::sourceKeyForUrl(QUrl(QStringLiteral("file:///media/01.png"))),
        kiriview::sourceKeyForUrl(QUrl(QStringLiteral("file:///media/"))),
        QUrl(QStringLiteral("file:///media/01.png")),
    };
    const kiriview::DirectMediaScope otherParentScope {
        QUrl(QStringLiteral("file:///media/01.png")),
        QUrl(QStringLiteral("file:///other/")),
        7,
        kiriview::sourceKeyForUrl(QUrl(QStringLiteral("file:///media/01.png"))),
        kiriview::sourceKeyForUrl(QUrl(QStringLiteral("file:///other/"))),
        QUrl(QStringLiteral("file:///media/01.png")),
    };

    QVERIFY(!(firstScope == otherParentScope));
}

void TestDirectMediaCursor::scopeMatchingAcceptsNormalizedConfirmation()
{
    kiriview::DirectMediaCursor cursor;
    const QUrl requestedImage(QStringLiteral("file:///media/chapter/../01.png"));
    const QUrl confirmedImage(QStringLiteral("file:///media/01.png"));

    kiriview::requestDirectImageCursor(cursor, resolved(requestedImage));
    const kiriview::DirectMediaScope requestedScope = kiriview::directMediaScopeForCursor(cursor);
    QVERIFY(kiriview::directMediaScopeMatchesCursor(cursor, requestedScope));

    kiriview::confirmDirectImageCursor(cursor, confirmedImage);
    QVERIFY(kiriview::directMediaScopeMatchesCursor(cursor, requestedScope));

    kiriview::requestDirectImageCursor(
        cursor, resolved(QUrl(QStringLiteral("file:///media/02.png"))));
    QVERIFY(!kiriview::directMediaScopeMatchesCursor(cursor, requestedScope));
}

QTEST_GUILESS_MAIN(TestDirectMediaCursor)

#include "test_directmediacursor.moc"
