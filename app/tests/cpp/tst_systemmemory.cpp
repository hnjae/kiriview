// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "system/systemmemory.h"

#include <QObject>
#include <QTest>
#include <optional>
#include <utility>

class TestSystemMemory : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void snapshotUsesInjectedPhysicalMemoryReader();
    void unreadablePhysicalMemoryFallsBackToZero();
    void runtimeDefaultsFillMissingProbeAndPreserveOverrides();
    void acceptedSnapshotSuppressesFallbackProbe();
};

void TestSystemMemory::snapshotUsesInjectedPhysicalMemoryReader()
{
    int readCount = 0;
    kiriview::SystemMemoryRuntime runtime;
    runtime.readPhysicalSystemMemory = [&readCount]() -> std::optional<qsizetype> {
        ++readCount;
        return 4096;
    };

    const kiriview::SystemMemorySnapshot snapshot
        = kiriview::systemMemorySnapshot(std::move(runtime));

    QCOMPARE(readCount, 1);
    QCOMPARE(snapshot.physicalByteSize, qsizetype(4096));
    QVERIFY(snapshot.physicalByteSize > 0);
}

void TestSystemMemory::unreadablePhysicalMemoryFallsBackToZero()
{
    kiriview::SystemMemoryRuntime runtime;
    runtime.readPhysicalSystemMemory = []() -> std::optional<qsizetype> { return std::nullopt; };

    const kiriview::SystemMemorySnapshot snapshot
        = kiriview::systemMemorySnapshot(std::move(runtime));

    QCOMPARE(snapshot.physicalByteSize, qsizetype(0));
    QCOMPARE(snapshot.physicalByteSize, quint64(0));

    runtime.readPhysicalSystemMemory = []() -> std::optional<qsizetype> { return -128; };

    const kiriview::SystemMemorySnapshot negativeSnapshot
        = kiriview::systemMemorySnapshot(std::move(runtime));

    QCOMPARE(negativeSnapshot.physicalByteSize, qsizetype(0));
    QCOMPARE(negativeSnapshot.physicalByteSize, quint64(0));
}

void TestSystemMemory::runtimeDefaultsFillMissingProbeAndPreserveOverrides()
{
    int readCount = 0;
    kiriview::SystemMemoryRuntime runtime;
    runtime.readPhysicalSystemMemory = [&readCount]() -> std::optional<qsizetype> {
        ++readCount;
        return 128;
    };

    kiriview::SystemMemoryRuntime resolved
        = kiriview::systemMemoryRuntimeWithDefaults(std::move(runtime));
    QVERIFY(resolved.readPhysicalSystemMemory);
    QCOMPARE(resolved.readPhysicalSystemMemory().value_or(0), qsizetype(128));
    QCOMPARE(readCount, 1);

    resolved = kiriview::systemMemoryRuntimeWithDefaults({});
    QVERIFY(resolved.readPhysicalSystemMemory);
}

void TestSystemMemory::acceptedSnapshotSuppressesFallbackProbe()
{
    int readCount = 0;
    kiriview::SystemMemoryRuntime fallbackRuntime;
    fallbackRuntime.readPhysicalSystemMemory = [&readCount]() -> std::optional<qsizetype> {
        ++readCount;
        return 4096;
    };

    const kiriview::SystemMemorySnapshot accepted
        = kiriview::resolveSystemMemorySnapshot(kiriview::SystemMemorySnapshot {}, fallbackRuntime);

    QCOMPARE(accepted.physicalByteSize, qsizetype(0));
    QCOMPARE(readCount, 0);

    const kiriview::SystemMemorySnapshot fallback
        = kiriview::resolveSystemMemorySnapshot(std::nullopt, std::move(fallbackRuntime));

    QCOMPARE(fallback.physicalByteSize, qsizetype(4096));
    QCOMPARE(readCount, 1);
}

QTEST_GUILESS_MAIN(TestSystemMemory)

#include "tst_systemmemory.moc"
