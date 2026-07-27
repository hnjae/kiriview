// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionvideodocumentcommandruntime.h"

#include <QObject>
#include <QRectF>
#include <QTest>
#include <memory>

class TestDocumentSessionVideoDocumentCommandRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void forwardsRouteSourceThroughPort();
    void leaveModeStopsAndClearsSourceAfterClearingOutput();
    void leaveModeClearsWhenOutputIsStillAttachedWithoutSource();
    void leaveModeNoopsWhenAlreadyEmpty();
    void outputAttachmentPortForwardsSurfaceEffects();
    void leaveModeDetachCanDestroyRuntime();
    void leaveModeStopCanDestroyRuntime();
    void leaveModeDetachReentryPreservesNewSource();
    void leaveModeStopReentryPreservesNewSource();
    void leaveModeOutputQueryCanDestroyRuntime();
    void leaveModeClearHookCanDestroyRuntime();
    void leaveModeClearHookPreservesPreRetirementCleanupDecision();
    void retainedOutputAttachmentPortExpiresWithRuntime();
};

namespace {
struct VideoCommandProbe
{
    kiriview::DocumentSessionVideoDocumentCommandPort port()
    {
        return kiriview::DocumentSessionVideoDocumentCommandPort {
            { [this]() {
                 sourceUrl = QUrl();
                 events.push_back(QStringLiteral("clear-source"));
             },
                [this](const kiriview::ResolvedNavigationSource& source) {
                    sourceUrl = source.requestedUrl();
                    events.push_back(QStringLiteral("set-source"));
                },
                {} },
            { [this]() { events.push_back(QStringLiteral("stop")); } },
            { [this]() -> QObject* { return attachedVideoOutput; },
                [this](QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect) {
                    attachedVideoOutput = videoOutput;
                    lastContentRect = contentRect;
                    lastSourceRect = sourceRect;
                    events.push_back(videoOutput == nullptr ? QStringLiteral("detach-output")
                                                            : QStringLiteral("attach-output"));
                } },
        };
    }

    QUrl sourceUrl;
    QObject* attachedVideoOutput = nullptr;
    QRectF lastContentRect;
    QRectF lastSourceRect;
    QStringList events;
};
}

void TestDocumentSessionVideoDocumentCommandRuntime::forwardsRouteSourceThroughPort()
{
    VideoCommandProbe probe;
    kiriview::DocumentSessionVideoDocumentCommandRuntime runtime(probe.port());
    const QUrl videoUrl(QStringLiteral("file:///tmp/movie.mp4"));

    QVERIFY(runtime.setSource(kiriview::resolvedNavigationSource(videoUrl, {})));

    QCOMPARE(probe.sourceUrl, videoUrl);
    QCOMPARE(probe.events, QStringList({ QStringLiteral("set-source") }));
}

void TestDocumentSessionVideoDocumentCommandRuntime::
    leaveModeStopsAndClearsSourceAfterClearingOutput()
{
    VideoCommandProbe probe;
    QObject videoOutput;
    probe.attachedVideoOutput = &videoOutput;
    kiriview::DocumentSessionVideoDocumentCommandRuntime runtime(probe.port());

    QVERIFY(runtime.leaveMode(QUrl(QStringLiteral("file:///tmp/movie.mp4"))));

    QCOMPARE(probe.sourceUrl, QUrl());
    QCOMPARE(probe.attachedVideoOutput, nullptr);
    QCOMPARE(probe.events,
        QStringList({ QStringLiteral("detach-output"), QStringLiteral("stop"),
            QStringLiteral("clear-source") }));
}

void TestDocumentSessionVideoDocumentCommandRuntime::
    leaveModeClearsWhenOutputIsStillAttachedWithoutSource()
{
    VideoCommandProbe probe;
    QObject videoOutput;
    probe.attachedVideoOutput = &videoOutput;
    kiriview::DocumentSessionVideoDocumentCommandRuntime runtime(probe.port());

    QVERIFY(runtime.leaveMode(QUrl()));

    QCOMPARE(probe.attachedVideoOutput, nullptr);
    QCOMPARE(probe.events,
        QStringList({ QStringLiteral("detach-output"), QStringLiteral("stop"),
            QStringLiteral("clear-source") }));
}

void TestDocumentSessionVideoDocumentCommandRuntime::leaveModeNoopsWhenAlreadyEmpty()
{
    VideoCommandProbe probe;
    kiriview::DocumentSessionVideoDocumentCommandRuntime runtime(probe.port());

    QVERIFY(runtime.leaveMode(QUrl()));

    QVERIFY(probe.events.empty());
}

void TestDocumentSessionVideoDocumentCommandRuntime::outputAttachmentPortForwardsSurfaceEffects()
{
    VideoCommandProbe probe;
    kiriview::DocumentSessionVideoDocumentCommandRuntime runtime(probe.port());
    QObject videoOutput;
    const QRectF contentRect(1.0, 2.0, 320.0, 180.0);
    const QRectF sourceRect(3.0, 4.0, 640.0, 360.0);

    const kiriview::DocumentSessionVideoOutputAttachmentPort attachmentPort
        = runtime.outputAttachmentPort();
    attachmentPort.setVideoOutputAttachment(&videoOutput, contentRect, sourceRect);

    QCOMPARE(probe.attachedVideoOutput, &videoOutput);
    QCOMPARE(probe.lastContentRect, contentRect);
    QCOMPARE(probe.lastSourceRect, sourceRect);
    QCOMPARE(probe.events, QStringList({ QStringLiteral("attach-output") }));
}

void TestDocumentSessionVideoDocumentCommandRuntime::leaveModeDetachCanDestroyRuntime()
{
    QObject videoOutput;
    int stopCount = 0;
    int clearSourceCount = 0;
    std::unique_ptr<kiriview::DocumentSessionVideoDocumentCommandRuntime> runtime;
    kiriview::DocumentSessionVideoDocumentCommandPort commands;
    commands.source.clearSource = [&]() { ++clearSourceCount; };
    commands.playback.stop = [&]() { ++stopCount; };
    commands.output.videoOutput = [&]() -> QObject* { return &videoOutput; };
    commands.output.setVideoOutputAttachment
        = [&](QObject* nextVideoOutput, const QRectF&, const QRectF&) {
              if (nextVideoOutput == nullptr) {
                  runtime.reset();
              }
          };
    runtime = std::make_unique<kiriview::DocumentSessionVideoDocumentCommandRuntime>(
        std::move(commands));

    auto* runtimePointer = runtime.get();
    QVERIFY(!runtimePointer->leaveMode(QUrl(QStringLiteral("file:///tmp/movie.mp4"))));

    QVERIFY(runtime == nullptr);
    QCOMPARE(stopCount, 0);
    QCOMPARE(clearSourceCount, 0);
}

void TestDocumentSessionVideoDocumentCommandRuntime::leaveModeStopCanDestroyRuntime()
{
    QObject videoOutput;
    int clearSourceCount = 0;
    std::unique_ptr<kiriview::DocumentSessionVideoDocumentCommandRuntime> runtime;
    kiriview::DocumentSessionVideoDocumentCommandPort commands;
    commands.source.clearSource = [&]() { ++clearSourceCount; };
    commands.playback.stop = [&]() { runtime.reset(); };
    commands.output.videoOutput = [&]() -> QObject* { return &videoOutput; };
    commands.output.setVideoOutputAttachment = [](QObject*, const QRectF&, const QRectF&) { };
    runtime = std::make_unique<kiriview::DocumentSessionVideoDocumentCommandRuntime>(
        std::move(commands));

    auto* runtimePointer = runtime.get();
    QVERIFY(!runtimePointer->leaveMode(QUrl(QStringLiteral("file:///tmp/movie.mp4"))));

    QVERIFY(runtime == nullptr);
    QCOMPARE(clearSourceCount, 0);
}

void TestDocumentSessionVideoDocumentCommandRuntime::leaveModeDetachReentryPreservesNewSource()
{
    QObject videoOutput;
    const QUrl replacementUrl(QStringLiteral("file:///tmp/replacement.mp4"));
    QStringList events;
    kiriview::DocumentSessionVideoDocumentCommandRuntime* runtime = nullptr;
    kiriview::DocumentSessionVideoDocumentCommandPort commands;
    commands.source.clearSource = [&]() { events.push_back(QStringLiteral("clear-source")); };
    commands.source.setSource = [&](const kiriview::ResolvedNavigationSource& source) {
        QCOMPARE(source.requestedUrl(), replacementUrl);
        events.push_back(QStringLiteral("set-source"));
    };
    commands.playback.stop = [&]() { events.push_back(QStringLiteral("stop")); };
    commands.output.videoOutput = [&]() -> QObject* { return &videoOutput; };
    commands.output.setVideoOutputAttachment
        = [&](QObject* nextVideoOutput, const QRectF&, const QRectF&) {
              if (nextVideoOutput != nullptr) {
                  return;
              }
              events.push_back(QStringLiteral("detach-output"));
              QVERIFY(runtime->setSource(kiriview::resolvedNavigationSource(replacementUrl, {})));
          };
    kiriview::DocumentSessionVideoDocumentCommandRuntime ownedRuntime(std::move(commands));
    runtime = &ownedRuntime;

    QVERIFY(!runtime->leaveMode(QUrl(QStringLiteral("file:///tmp/old.mp4"))));

    QCOMPARE(
        events, QStringList({ QStringLiteral("detach-output"), QStringLiteral("set-source") }));
}

void TestDocumentSessionVideoDocumentCommandRuntime::leaveModeStopReentryPreservesNewSource()
{
    QObject videoOutput;
    const QUrl replacementUrl(QStringLiteral("file:///tmp/replacement.mp4"));
    QStringList events;
    kiriview::DocumentSessionVideoDocumentCommandRuntime* runtime = nullptr;
    kiriview::DocumentSessionVideoDocumentCommandPort commands;
    commands.source.clearSource = [&]() { events.push_back(QStringLiteral("clear-source")); };
    commands.source.setSource = [&](const kiriview::ResolvedNavigationSource& source) {
        QCOMPARE(source.requestedUrl(), replacementUrl);
        events.push_back(QStringLiteral("set-source"));
    };
    commands.playback.stop = [&]() {
        events.push_back(QStringLiteral("stop"));
        QVERIFY(runtime->setSource(kiriview::resolvedNavigationSource(replacementUrl, {})));
    };
    commands.output.videoOutput = [&]() -> QObject* { return &videoOutput; };
    commands.output.setVideoOutputAttachment
        = [&](QObject* nextVideoOutput, const QRectF&, const QRectF&) {
              if (nextVideoOutput == nullptr) {
                  events.push_back(QStringLiteral("detach-output"));
              }
          };
    kiriview::DocumentSessionVideoDocumentCommandRuntime ownedRuntime(std::move(commands));
    runtime = &ownedRuntime;

    QVERIFY(!runtime->leaveMode(QUrl(QStringLiteral("file:///tmp/old.mp4"))));

    QCOMPARE(events,
        QStringList({ QStringLiteral("detach-output"), QStringLiteral("stop"),
            QStringLiteral("set-source") }));
}

void TestDocumentSessionVideoDocumentCommandRuntime::leaveModeOutputQueryCanDestroyRuntime()
{
    QObject videoOutput;
    int detachCount = 0;
    int stopCount = 0;
    int clearSourceCount = 0;
    std::unique_ptr<kiriview::DocumentSessionVideoDocumentCommandRuntime> runtime;
    kiriview::DocumentSessionVideoDocumentCommandPort commands;
    commands.source.clearSource = [&]() { ++clearSourceCount; };
    commands.playback.stop = [&]() { ++stopCount; };
    commands.output.videoOutput = [&]() -> QObject* {
        runtime.reset();
        return &videoOutput;
    };
    commands.output.setVideoOutputAttachment
        = [&](QObject*, const QRectF&, const QRectF&) { ++detachCount; };
    runtime = std::make_unique<kiriview::DocumentSessionVideoDocumentCommandRuntime>(
        std::move(commands));

    auto* runtimePointer = runtime.get();
    QVERIFY(!runtimePointer->leaveMode(QUrl()));

    QVERIFY(runtime == nullptr);
    QCOMPARE(detachCount, 0);
    QCOMPARE(stopCount, 0);
    QCOMPARE(clearSourceCount, 0);
}

void TestDocumentSessionVideoDocumentCommandRuntime::leaveModeClearHookCanDestroyRuntime()
{
    QObject videoOutput;
    int stopCount = 0;
    int clearSourceCount = 0;
    std::unique_ptr<kiriview::DocumentSessionVideoDocumentCommandRuntime> runtime;
    kiriview::DocumentSessionVideoDocumentCommandPort commands;
    commands.source.clearSource = [&]() { ++clearSourceCount; };
    commands.playback.stop = [&]() { ++stopCount; };
    commands.output.videoOutput = [&]() -> QObject* { return &videoOutput; };
    runtime = std::make_unique<kiriview::DocumentSessionVideoDocumentCommandRuntime>(
        std::move(commands), [&]() { runtime.reset(); });

    auto* runtimePointer = runtime.get();
    QVERIFY(!runtimePointer->leaveMode(QUrl(QStringLiteral("file:///tmp/movie.mp4"))));

    QVERIFY(runtime == nullptr);
    QCOMPARE(stopCount, 0);
    QCOMPARE(clearSourceCount, 0);
}

void TestDocumentSessionVideoDocumentCommandRuntime::
    leaveModeClearHookPreservesPreRetirementCleanupDecision()
{
    QObject videoOutput;
    QObject* attachedVideoOutput = &videoOutput;
    int stopCount = 0;
    int clearSourceCount = 0;
    int clearHookCount = 0;
    kiriview::DocumentSessionVideoDocumentCommandPort commands;
    commands.source.clearSource = [&]() { ++clearSourceCount; };
    commands.playback.stop = [&]() { ++stopCount; };
    commands.output.videoOutput = [&]() { return attachedVideoOutput; };
    kiriview::DocumentSessionVideoDocumentCommandRuntime runtime(std::move(commands), [&]() {
        ++clearHookCount;
        attachedVideoOutput = nullptr;
    });

    QVERIFY(runtime.leaveMode(QUrl()));

    QCOMPARE(clearHookCount, 1);
    QCOMPARE(stopCount, 1);
    QCOMPARE(clearSourceCount, 1);
}

void TestDocumentSessionVideoDocumentCommandRuntime::
    retainedOutputAttachmentPortExpiresWithRuntime()
{
    int setAttachmentCount = 0;
    kiriview::DocumentSessionVideoOutputAttachmentPort attachmentPort;
    {
        kiriview::DocumentSessionVideoDocumentCommandPort commands;
        commands.output.setVideoOutputAttachment
            = [&](QObject*, const QRectF&, const QRectF&) { ++setAttachmentCount; };
        kiriview::DocumentSessionVideoDocumentCommandRuntime runtime(std::move(commands));
        attachmentPort = runtime.outputAttachmentPort();
    }
    QObject videoOutput;

    attachmentPort.setVideoOutputAttachment(&videoOutput, {}, {});

    QCOMPARE(setAttachmentCount, 0);
}

QTEST_GUILESS_MAIN(TestDocumentSessionVideoDocumentCommandRuntime)

#include "tst_documentsessionvideodocumentcommandruntime.moc"
