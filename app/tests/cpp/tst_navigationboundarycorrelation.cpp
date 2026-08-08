// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/navigationboundarycorrelation.h"

#include <QTest>

class TestNavigationBoundaryCorrelation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void firstAndLastBoundariesRemainCurrentOnlyForTheirRequestedEdge();
    void correlationRejectsChangedSelectionScopeOrKnowledge();
    void unavailableOrUnconfirmedBoundaryCannotBeCorrelated();
};

void TestNavigationBoundaryCorrelation::
    firstAndLastBoundariesRemainCurrentOnlyForTheirRequestedEdge()
{
    kiriview::NavigationBoundaryFacts facts {
        true,
        true,
        true,
        true,
        1,
        QUrl::fromLocalFile(QStringLiteral("/media/only.png")),
    };
    const std::optional<kiriview::NavigationBoundaryCorrelation> first
        = kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::First, facts);
    const std::optional<kiriview::NavigationBoundaryCorrelation> last
        = kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::Last, facts);
    QVERIFY(first.has_value());
    QVERIFY(last.has_value());

    facts.atKnownLast = false;
    QVERIFY(kiriview::navigationBoundaryCorrelationIsCurrent(*first, facts));
    QVERIFY(!kiriview::navigationBoundaryCorrelationIsCurrent(*last, facts));

    facts.atKnownFirst = false;
    facts.atKnownLast = true;
    QVERIFY(!kiriview::navigationBoundaryCorrelationIsCurrent(*first, facts));
    QVERIFY(kiriview::navigationBoundaryCorrelationIsCurrent(*last, facts));
}

void TestNavigationBoundaryCorrelation::correlationRejectsChangedSelectionScopeOrKnowledge()
{
    const kiriview::NavigationBoundaryFacts initial {
        true,
        true,
        true,
        false,
        2,
        QUrl::fromLocalFile(QStringLiteral("/books/book.cbz/01.png")),
    };
    const std::optional<kiriview::NavigationBoundaryCorrelation> correlation
        = kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::First, initial);
    QVERIFY(correlation.has_value());
    QVERIFY(kiriview::navigationBoundaryCorrelationIsCurrent(*correlation, initial));

    kiriview::NavigationBoundaryFacts normalizedEquivalent = initial;
    normalizedEquivalent.selectionUrl
        = QUrl::fromLocalFile(QStringLiteral("/books/./book.cbz/chapter/../01.png"));
    QVERIFY(kiriview::navigationBoundaryCorrelationIsCurrent(*correlation, normalizedEquivalent));

    kiriview::NavigationBoundaryFacts changed = initial;
    changed.selectionUrl = QUrl::fromLocalFile(QStringLiteral("/books/book.cbz/02.png"));
    QVERIFY(!kiriview::navigationBoundaryCorrelationIsCurrent(*correlation, changed));

    changed = initial;
    changed.scope = 1;
    QVERIFY(!kiriview::navigationBoundaryCorrelationIsCurrent(*correlation, changed));

    changed = initial;
    changed.known = false;
    QVERIFY(!kiriview::navigationBoundaryCorrelationIsCurrent(*correlation, changed));

    changed = initial;
    changed.available = false;
    QVERIFY(!kiriview::navigationBoundaryCorrelationIsCurrent(*correlation, changed));
}

void TestNavigationBoundaryCorrelation::unavailableOrUnconfirmedBoundaryCannotBeCorrelated()
{
    kiriview::NavigationBoundaryFacts facts {
        true,
        true,
        false,
        false,
        1,
        QUrl::fromLocalFile(QStringLiteral("/media/current.png")),
    };
    QVERIFY(!kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::First, facts)
            .has_value());
    QVERIFY(!kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::Last, facts)
            .has_value());

    facts.atKnownFirst = true;
    facts.known = false;
    QVERIFY(!kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::First, facts)
            .has_value());

    facts.known = true;
    facts.available = false;
    QVERIFY(!kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::First, facts)
            .has_value());

    facts.available = true;
    facts.selectionUrl = {};
    QVERIFY(!kiriview::correlateNavigationBoundary(kiriview::NavigationBoundaryEdge::First, facts)
            .has_value());
}

QTEST_GUILESS_MAIN(TestNavigationBoundaryCorrelation)

#include "tst_navigationboundarycorrelation.moc"
