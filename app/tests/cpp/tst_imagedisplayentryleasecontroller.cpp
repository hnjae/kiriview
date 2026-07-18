// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagedisplayentryleasecontroller.h"

#include "image_test_support.h"
#include "location/imagedocumentlocation.h"

#include <QObject>
#include <QSize>
#include <QString>
#include <QTest>
#include <QUrl>
#include <memory>

class TestImageDisplayEntryLeaseController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void supersessionAndClearReleaseEntries();
    void reusableBufferKeepsOnlyCurrentAndPreviousSources();
    void reuseIdentitySeparatesDisplayScopeAndPageRole();
};

namespace {
constexpr qsizetype testByteBudget = 1024 * 1024;

kiriview::StaticDisplayImagePayload displayPayload(
    const QSize& size, const QString& sourceIdentity, const QString& displayScopeIdentity = {})
{
    kiriview::StaticDisplayImagePayload payload
        = kiriview::TestSupport::staticDisplayTestImagePayload(
            kiriview::TestSupport::testImage(size));
    payload.sourceIdentity = sourceIdentity;
    payload.displayScopeIdentity = displayScopeIdentity;
    return payload;
}
}

void TestImageDisplayEntryLeaseController::supersessionAndClearReleaseEntries()
{
    auto store = std::make_shared<kiriview::DisplayImageStore>(testByteBudget);
    kiriview::ImageDisplayEntryLeaseController leases(store, kiriview::DisplayedPageRole::Primary);

    const kiriview::DisplayEntryLease first
        = leases.acquireStaticDisplay(displayPayload(QSize(8, 4), QStringLiteral("page")), 1);
    QVERIFY(!first.entryId.isEmpty());
    QVERIFY(store->entry(first.entryId).has_value());

    const kiriview::DisplayEntryLease second
        = leases.acquireStaticDisplay(displayPayload(QSize(6, 4), QStringLiteral("page")), 2);
    QVERIFY(!second.entryId.isEmpty());
    QVERIFY(first.entryId != second.entryId);
    QVERIFY(!store->entry(first.entryId).has_value());
    QVERIFY(store->entry(second.entryId).has_value());

    leases.clearDisplay();
    leases.clearBufferedStaticDisplays();

    QVERIFY(!store->entry(second.entryId).has_value());
    QCOMPARE(store->size(), qsizetype(0));
}

void TestImageDisplayEntryLeaseController::reusableBufferKeepsOnlyCurrentAndPreviousSources()
{
    auto store = std::make_shared<kiriview::DisplayImageStore>(testByteBudget);
    kiriview::ImageDisplayEntryLeaseController leases(store, kiriview::DisplayedPageRole::Primary);

    const QString firstId
        = leases.acquireStaticDisplay(displayPayload(QSize(8, 4), QStringLiteral("page-a")), 1)
              .entryId;
    const QString secondId
        = leases.acquireStaticDisplay(displayPayload(QSize(8, 4), QStringLiteral("page-b")), 2)
              .entryId;
    const QString thirdId
        = leases.acquireStaticDisplay(displayPayload(QSize(8, 4), QStringLiteral("page-c")), 3)
              .entryId;

    QVERIFY(!store->entry(firstId).has_value());
    QVERIFY(store->entry(secondId).has_value());
    QVERIFY(store->entry(thirdId).has_value());
    QCOMPARE(store->size(), qsizetype(2));
}

void TestImageDisplayEntryLeaseController::reuseIdentitySeparatesDisplayScopeAndPageRole()
{
    auto store = std::make_shared<kiriview::DisplayImageStore>(testByteBudget);
    kiriview::ImageDisplayEntryLeaseController primary(store, kiriview::DisplayedPageRole::Primary);
    kiriview::ImageDisplayEntryLeaseController secondary(
        store, kiriview::DisplayedPageRole::Secondary);

    const QString firstScope = QStringLiteral("scope-a");
    const QString secondScope = QStringLiteral("scope-b");
    const kiriview::DisplayEntryLease first = primary.acquireStaticDisplay(
        displayPayload(QSize(8, 4), QStringLiteral("same-source"), firstScope), 1);
    const kiriview::DisplayEntryLease otherScope = primary.acquireStaticDisplay(
        displayPayload(QSize(8, 4), QStringLiteral("same-source"), secondScope), 2);
    const kiriview::DisplayEntryLease otherRole = secondary.acquireStaticDisplay(
        displayPayload(QSize(8, 4), QStringLiteral("same-source"), firstScope), 1);

    QVERIFY(first.entryId != otherScope.entryId);
    QVERIFY(first.entryId != otherRole.entryId);
    QVERIFY(otherScope.entryId != otherRole.entryId);

    const auto firstEntry = store->entry(first.entryId);
    const auto otherScopeEntry = store->entry(otherScope.entryId);
    const auto otherRoleEntry = store->entry(otherRole.entryId);
    QVERIFY(firstEntry.has_value());
    QVERIFY(otherScopeEntry.has_value());
    QVERIFY(otherRoleEntry.has_value());
    QCOMPARE(firstEntry->reuseKey->locationIdentity, firstScope);
    QCOMPARE(otherScopeEntry->reuseKey->locationIdentity, secondScope);
    QCOMPARE(firstEntry->reuseKey->pageRole, kiriview::DisplayedPageRole::Primary);
    QCOMPARE(otherRoleEntry->reuseKey->pageRole, kiriview::DisplayedPageRole::Secondary);
}

QTEST_GUILESS_MAIN(TestImageDisplayEntryLeaseController)

#include "tst_imagedisplayentryleasecontroller.moc"
