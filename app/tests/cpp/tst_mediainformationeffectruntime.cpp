// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/mediainformationeffectruntime.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTest>
#include <QUrl>
#include <memory>
#include <vector>

namespace {
kiriview::MediaInformationProjectionSnapshot availableSnapshot(const QUrl& targetUrl)
{
    kiriview::MediaInformationProjectionSnapshot snapshot;
    snapshot.available = true;
    snapshot.targetUrl = targetUrl;
    snapshot.canCopyFilePath = true;
    snapshot.canOpenContainingFolder = true;
    return snapshot;
}
}

class TestMediaInformationEffectRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void unavailableProjectionRejectsEffects();
    void commandsResolveTheLatestCommittedTarget();
    void admittedJobIsOwnedUntilRuntimeTeardown();
    void commandPortAndReturnedJobSurviveSynchronousRuntimeTeardown();
};

void TestMediaInformationEffectRuntime::unavailableProjectionRejectsEffects()
{
    kiriview::MediaInformationProjectionSnapshot snapshot;
    int copiedCount = 0;
    int openedCount = 0;
    kiriview::MediaInformationEffectRuntime runtime([&snapshot]() { return snapshot; },
        {
            [&copiedCount](const QString&) { ++copiedCount; },
            [&openedCount](const QUrl&) -> QObject* {
                ++openedCount;
                return new QObject();
            },
        });

    const kiriview::MediaInformationEffectCommandPort commands = runtime.commandPort();
    commands.copyFilePath();
    commands.openContainingFolder();

    QCOMPARE(copiedCount, 0);
    QCOMPARE(openedCount, 0);
}

void TestMediaInformationEffectRuntime::commandsResolveTheLatestCommittedTarget()
{
    kiriview::MediaInformationProjectionSnapshot snapshot
        = availableSnapshot(QUrl::fromLocalFile(QStringLiteral("/media/first image.png")));
    std::vector<QString> copiedTexts;
    std::vector<QUrl> openedTargets;
    std::vector<QPointer<QObject>> jobs;
    kiriview::MediaInformationEffectRuntime runtime([&snapshot]() { return snapshot; },
        {
            [&copiedTexts](const QString& text) { copiedTexts.push_back(text); },
            [&openedTargets, &jobs](const QUrl& targetUrl) -> QObject* {
                openedTargets.push_back(targetUrl);
                auto* job = new QObject();
                jobs.push_back(job);
                return job;
            },
        });
    const kiriview::MediaInformationEffectCommandPort commands = runtime.commandPort();

    commands.copyFilePath();
    commands.openContainingFolder();
    snapshot = availableSnapshot(QUrl::fromLocalFile(QStringLiteral("/replacement/second.png")));
    commands.copyFilePath();
    commands.openContainingFolder();

    QCOMPARE(copiedTexts.size(), std::size_t(2));
    QCOMPARE(copiedTexts.at(0), QStringLiteral("/media/first image.png"));
    QCOMPARE(copiedTexts.at(1), QStringLiteral("/replacement/second.png"));
    QCOMPARE(openedTargets.size(), std::size_t(2));
    QCOMPARE(openedTargets.at(0), QUrl::fromLocalFile(QStringLiteral("/media/first image.png")));
    QCOMPARE(openedTargets.at(1), QUrl::fromLocalFile(QStringLiteral("/replacement/second.png")));
    QCOMPARE(jobs.size(), std::size_t(2));
    for (const QPointer<QObject>& job : jobs) {
        QVERIFY(!job.isNull());
        QCOMPARE(job->parent(), &runtime);
    }
}

void TestMediaInformationEffectRuntime::admittedJobIsOwnedUntilRuntimeTeardown()
{
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/media/current.png"));
    const kiriview::MediaInformationProjectionSnapshot snapshot = availableSnapshot(targetUrl);
    QPointer<QObject> job;
    auto runtime = std::make_unique<kiriview::MediaInformationEffectRuntime>(
        [snapshot]() { return snapshot; },
        kiriview::MediaInformationEffects {
            {},
            [&job](const QUrl&) -> QObject* {
                auto* createdJob = new QObject();
                job = createdJob;
                return createdJob;
            },
        });

    runtime->commandPort().openContainingFolder();

    QVERIFY(!job.isNull());
    QCOMPARE(job->parent(), runtime.get());
    runtime.reset();
    QVERIFY(job.isNull());
}

void TestMediaInformationEffectRuntime::commandPortAndReturnedJobSurviveSynchronousRuntimeTeardown()
{
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/media/current.png"));
    const kiriview::MediaInformationProjectionSnapshot snapshot = availableSnapshot(targetUrl);
    QPointer<QObject> returnedJob;
    std::unique_ptr<kiriview::MediaInformationEffectRuntime> runtime;
    kiriview::MediaInformationEffects effects;
    effects.openContainingFolder = [&runtime, &returnedJob](const QUrl&) -> QObject* {
        auto* job = new QObject();
        returnedJob = job;
        runtime.reset();
        return job;
    };
    runtime = std::make_unique<kiriview::MediaInformationEffectRuntime>(
        [snapshot]() { return snapshot; }, std::move(effects));
    const kiriview::MediaInformationEffectCommandPort commands = runtime->commandPort();

    commands.openContainingFolder();

    QVERIFY(runtime == nullptr);
    QVERIFY(returnedJob.isNull());
    commands.openContainingFolder();
}

QTEST_GUILESS_MAIN(TestMediaInformationEffectRuntime)

#include "tst_mediainformationeffectruntime.moc"
