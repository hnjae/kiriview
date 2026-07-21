// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "cache/imagecachepolicy.h"

#include <QObject>
#include <QTest>
#include <QtGlobal>

class TestImageCachePolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void displayImageCacheByteBudgetUsesPreferredLimitAndSystemMemoryCap();
    void displayImageCachePreferredByteBudgetIsNamedPolicyDefault();
    void predecodeCacheByteBudgetUsesPreferredLimitAndSystemMemoryCap();
    void thumbnailCacheByteBudgetUsesPreferredLimitAndSystemMemoryCap();
    void resolvedCacheBudgetsUseSnapshotAndPreserveOverrides();
    void resolvedCacheBudgetsUseDisplayImagePreferredDefault();
    void resolvedCacheBudgetsUseExplicitDisplayImageOverride();
};

void TestImageCachePolicy::displayImageCacheByteBudgetUsesPreferredLimitAndSystemMemoryCap()
{
    const qsizetype preferredByteBudget = kiriview::displayImageCachePreferredByteBudget();

    QCOMPARE(kiriview::displayImageCacheByteBudgetForSystemMemory(0, preferredByteBudget),
        preferredByteBudget);
    QCOMPARE(kiriview::displayImageCacheByteBudgetForSystemMemory(
                 preferredByteBudget, preferredByteBudget),
        preferredByteBudget / 16);
    QCOMPARE(kiriview::displayImageCacheByteBudgetForSystemMemory(
                 preferredByteBudget * 32, preferredByteBudget),
        preferredByteBudget);
    QCOMPARE(kiriview::displayImageCacheByteBudgetForSystemMemory(preferredByteBudget, -1),
        qsizetype(0));
}

void TestImageCachePolicy::displayImageCachePreferredByteBudgetIsNamedPolicyDefault()
{
    constexpr qsizetype preferredByteBudget = 512 * 1024 * 1024;

    QCOMPARE(kiriview::displayImageCachePreferredByteBudget(), preferredByteBudget);
}

void TestImageCachePolicy::predecodeCacheByteBudgetUsesPreferredLimitAndSystemMemoryCap()
{
    constexpr qsizetype preferredByteBudget = 1024 * 1024 * 1024;

    QCOMPARE(kiriview::predecodeCacheByteBudgetForSystemMemory(0), preferredByteBudget);
    QCOMPARE(kiriview::predecodeCacheByteBudgetForSystemMemory(0), preferredByteBudget);
    QCOMPARE(kiriview::predecodeCacheByteBudgetForSystemMemory(preferredByteBudget),
        preferredByteBudget / 8);
    QCOMPARE(kiriview::predecodeCacheByteBudgetForSystemMemory(preferredByteBudget * 16),
        preferredByteBudget);
}

void TestImageCachePolicy::thumbnailCacheByteBudgetUsesPreferredLimitAndSystemMemoryCap()
{
    constexpr qsizetype preferredByteBudget = 64 * 1024 * 1024;

    QCOMPARE(kiriview::thumbnailCachePreferredByteBudget(), preferredByteBudget);
    QCOMPARE(kiriview::thumbnailCacheByteBudgetForSystemMemory(0), preferredByteBudget);
    QCOMPARE(kiriview::thumbnailCacheByteBudgetForSystemMemory(preferredByteBudget),
        preferredByteBudget / 64);
    QCOMPARE(kiriview::thumbnailCacheByteBudgetForSystemMemory(preferredByteBudget * 128),
        preferredByteBudget);
}

void TestImageCachePolicy::resolvedCacheBudgetsUseSnapshotAndPreserveOverrides()
{
    constexpr qsizetype displayImagePreferredByteBudget = 512 * 1024 * 1024;
    constexpr qsizetype predecodePreferredByteBudget = 1024 * 1024 * 1024;
    constexpr qsizetype thumbnailPreferredByteBudget = 64 * 1024 * 1024;

    const kiriview::ImageCacheBudgets defaultBudgets = kiriview::resolvedImageCacheBudgets(
        kiriview::ImageCacheBudgetRequest {
            0,
            0,
            displayImagePreferredByteBudget,
            0,
        },
        kiriview::SystemMemorySnapshot { predecodePreferredByteBudget });
    QCOMPARE(defaultBudgets.predecodeCacheByteBudget, predecodePreferredByteBudget / 8);
    QCOMPARE(defaultBudgets.displayImageCacheByteBudget, displayImagePreferredByteBudget / 8);
    QCOMPARE(defaultBudgets.thumbnailCacheByteBudget, predecodePreferredByteBudget / 64);

    const kiriview::ImageCacheBudgets explicitBudgets = kiriview::resolvedImageCacheBudgets(
        kiriview::ImageCacheBudgetRequest {
            4096,
            32768,
            displayImagePreferredByteBudget,
            16384,
        },
        kiriview::SystemMemorySnapshot { predecodePreferredByteBudget });
    QCOMPARE(explicitBudgets.predecodeCacheByteBudget, qsizetype(4096));
    QCOMPARE(explicitBudgets.displayImageCacheByteBudget, qsizetype(32768));
    QCOMPARE(explicitBudgets.thumbnailCacheByteBudget, qsizetype(16384));
}

void TestImageCachePolicy::resolvedCacheBudgetsUseDisplayImagePreferredDefault()
{
    const qsizetype preferredByteBudget = kiriview::displayImageCachePreferredByteBudget();

    const kiriview::ImageCacheBudgets budgets
        = kiriview::resolvedImageCacheBudgets(kiriview::ImageCacheBudgetRequest {},
            kiriview::SystemMemorySnapshot {
                preferredByteBudget * 32,
            });

    QCOMPARE(budgets.displayImageCacheByteBudget, preferredByteBudget);
}

void TestImageCachePolicy::resolvedCacheBudgetsUseExplicitDisplayImageOverride()
{
    const kiriview::ImageCacheBudgets budgets = kiriview::resolvedImageCacheBudgets(
        kiriview::ImageCacheBudgetRequest {
            0,
            65536,
            0,
            0,
        },
        kiriview::SystemMemorySnapshot {});

    QCOMPARE(budgets.displayImageCacheByteBudget, qsizetype(65536));
}

QTEST_GUILESS_MAIN(TestImageCachePolicy)

#include "tst_imagecachepolicy.moc"
