// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionvideodocumentcommandruntime.h"

#include <QObject>
#include <QRectF>
#include <QTest>

class TestDocumentSessionVideoDocumentCommandRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void forwardsRouteSourceThroughPort();
    void leaveModeStopsAndClearsSourceAfterClearingOutput();
    void leaveModeClearsWhenOutputIsStillAttachedWithoutSource();
    void leaveModeNoopsWhenAlreadyEmpty();
    void outputAttachmentPortForwardsSurfaceEffects();
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
                [this](QObject* videoOutput) {
                    attachedVideoOutput = videoOutput;
                    events.push_back(videoOutput == nullptr ? QStringLiteral("detach-output")
                                                            : QStringLiteral("attach-output"));
                },
                [this](const QRectF& contentRect, const QRectF& sourceRect) {
                    lastContentRect = contentRect;
                    lastSourceRect = sourceRect;
                    events.push_back(QStringLiteral("set-geometry"));
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

    runtime.setSource(kiriview::resolvedNavigationSource(videoUrl, {}));

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

    runtime.leaveMode(QUrl(QStringLiteral("file:///tmp/movie.mp4")));

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

    runtime.leaveMode(QUrl());

    QCOMPARE(probe.attachedVideoOutput, nullptr);
    QCOMPARE(probe.events,
        QStringList({ QStringLiteral("detach-output"), QStringLiteral("stop"),
            QStringLiteral("clear-source") }));
}

void TestDocumentSessionVideoDocumentCommandRuntime::leaveModeNoopsWhenAlreadyEmpty()
{
    VideoCommandProbe probe;
    kiriview::DocumentSessionVideoDocumentCommandRuntime runtime(probe.port());

    runtime.leaveMode(QUrl());

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
    attachmentPort.setVideoOutput(&videoOutput);
    attachmentPort.setVideoOutputGeometry(contentRect, sourceRect);

    QCOMPARE(probe.attachedVideoOutput, &videoOutput);
    QCOMPARE(probe.lastContentRect, contentRect);
    QCOMPARE(probe.lastSourceRect, sourceRect);
    QCOMPARE(probe.events,
        QStringList({ QStringLiteral("attach-output"), QStringLiteral("set-geometry") }));
}

QTEST_GUILESS_MAIN(TestDocumentSessionVideoDocumentCommandRuntime)

#include "tst_documentsessionvideodocumentcommandruntime.moc"
