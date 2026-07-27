// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionvideooutputruntime.h"

#include <QObject>
#include <QRectF>
#include <QTest>

class TestDocumentSessionVideoOutputRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesAcceptedAttachAndDetachThroughPort();
    void rejectsStaleInvalidAndForeignClaimsWithoutTouchingPort();
    void rejectsGloballyStaleClaimsFromDifferentOwners();
    void rejectsReplayedAndUnissuedClaimTokens();
    void clearInvalidatesEveryPreviouslyIssuedClaim();
    void clearForgetsActiveClaim();
};

namespace {
constexpr kiriview::DocumentSessionVideoOutputClaimAdmission acceptedAdmission { 0, true };

struct AttachmentProbe
{
    kiriview::DocumentSessionVideoOutputAttachmentPort port()
    {
        return kiriview::DocumentSessionVideoOutputAttachmentPort {
            [this](QObject* videoOutput) {
                attachedVideoOutput = videoOutput;
                ++setVideoOutputCount;
            },
            [this](const QRectF& contentRect, const QRectF& sourceRect) {
                lastContentRect = contentRect;
                lastSourceRect = sourceRect;
                ++setGeometryCount;
            },
        };
    }

    QObject* attachedVideoOutput = nullptr;
    QRectF lastContentRect;
    QRectF lastSourceRect;
    int setVideoOutputCount = 0;
    int setGeometryCount = 0;
};

bool reportSurfaceClaim(kiriview::DocumentSessionVideoOutputRuntime& runtime,
    const kiriview::DocumentSessionVideoOutputClaimReport& report,
    const kiriview::DocumentSessionVideoOutputAttachmentPort& port)
{
    return runtime.reportSurfaceClaim(report, acceptedAdmission, port);
}
}

void TestDocumentSessionVideoOutputRuntime::appliesAcceptedAttachAndDetachThroughPort()
{
    kiriview::DocumentSessionVideoOutputRuntime runtime;
    QObject owner;
    QObject videoOutput;
    AttachmentProbe probe;
    const QRectF contentRect(1.0, 2.0, 320.0, 180.0);
    const QRectF sourceRect(3.0, 4.0, 640.0, 360.0);

    QVERIFY(reportSurfaceClaim(runtime,
        { runtime.nextSurfaceClaimToken(), &owner, &videoOutput, true, contentRect, sourceRect },
        probe.port()));
    QCOMPARE(probe.attachedVideoOutput, &videoOutput);
    QCOMPARE(probe.lastContentRect, contentRect);
    QCOMPARE(probe.lastSourceRect, sourceRect);
    QCOMPARE(probe.setVideoOutputCount, 1);
    QCOMPARE(probe.setGeometryCount, 1);

    QVERIFY(reportSurfaceClaim(runtime,
        { runtime.nextSurfaceClaimToken(), &owner, nullptr, false, {}, {} }, probe.port()));
    QCOMPARE(probe.attachedVideoOutput, nullptr);
    QCOMPARE(probe.setVideoOutputCount, 2);
    QCOMPARE(probe.setGeometryCount, 1);
}

void TestDocumentSessionVideoOutputRuntime::rejectsStaleInvalidAndForeignClaimsWithoutTouchingPort()
{
    kiriview::DocumentSessionVideoOutputRuntime runtime;
    QObject owner;
    QObject otherOwner;
    QObject videoOutput;
    AttachmentProbe probe;

    const QString staleToken = runtime.nextSurfaceClaimToken();
    const QString attachToken = runtime.nextSurfaceClaimToken();
    QVERIFY(reportSurfaceClaim(
        runtime, { attachToken, &owner, &videoOutput, true, {}, {} }, probe.port()));

    probe = AttachmentProbe {};
    QVERIFY(
        !reportSurfaceClaim(runtime, { staleToken, &owner, nullptr, false, {}, {} }, probe.port()));
    QVERIFY(!reportSurfaceClaim(runtime,
        { QStringLiteral("not-a-token"), &owner, &videoOutput, true, {}, {} }, probe.port()));
    QVERIFY(!reportSurfaceClaim(runtime,
        { runtime.nextSurfaceClaimToken(), nullptr, &videoOutput, true, {}, {} }, probe.port()));
    QVERIFY(!reportSurfaceClaim(runtime,
        { runtime.nextSurfaceClaimToken(), &otherOwner, nullptr, false, {}, {} }, probe.port()));
    QCOMPARE(probe.setVideoOutputCount, 0);
    QCOMPARE(probe.setGeometryCount, 0);
}

void TestDocumentSessionVideoOutputRuntime::rejectsGloballyStaleClaimsFromDifferentOwners()
{
    kiriview::DocumentSessionVideoOutputRuntime runtime;
    QObject staleOwner;
    QObject currentOwner;
    QObject staleVideoOutput;
    QObject currentVideoOutput;
    AttachmentProbe probe;
    const QString staleToken = runtime.nextSurfaceClaimToken();
    const QString currentToken = runtime.nextSurfaceClaimToken();

    QVERIFY(reportSurfaceClaim(
        runtime, { currentToken, &currentOwner, &currentVideoOutput, true, {}, {} }, probe.port()));
    probe = AttachmentProbe {};

    QVERIFY(!reportSurfaceClaim(
        runtime, { staleToken, &staleOwner, &staleVideoOutput, true, {}, {} }, probe.port()));
    QCOMPARE(probe.setVideoOutputCount, 0);
    QCOMPARE(probe.setGeometryCount, 0);
}

void TestDocumentSessionVideoOutputRuntime::rejectsReplayedAndUnissuedClaimTokens()
{
    kiriview::DocumentSessionVideoOutputRuntime runtime;
    QObject owner;
    QObject videoOutput;
    AttachmentProbe probe;
    const QString acceptedToken = runtime.nextSurfaceClaimToken();

    QVERIFY(reportSurfaceClaim(
        runtime, { acceptedToken, &owner, &videoOutput, true, {}, {} }, probe.port()));
    probe = AttachmentProbe {};

    QVERIFY(!reportSurfaceClaim(
        runtime, { acceptedToken, &owner, &videoOutput, true, {}, {} }, probe.port()));
    const quint64 unissuedRevision = acceptedToken.toULongLong() + 1;
    QVERIFY(!reportSurfaceClaim(runtime,
        { QString::number(unissuedRevision), &owner, &videoOutput, true, {}, {} }, probe.port()));

    const QString invalidPayloadToken = runtime.nextSurfaceClaimToken();
    QVERIFY(!reportSurfaceClaim(
        runtime, { invalidPayloadToken, nullptr, &videoOutput, true, {}, {} }, probe.port()));
    QVERIFY(!reportSurfaceClaim(
        runtime, { invalidPayloadToken, &owner, &videoOutput, true, {}, {} }, probe.port()));
    QCOMPARE(probe.setVideoOutputCount, 0);
    QCOMPARE(probe.setGeometryCount, 0);
}

void TestDocumentSessionVideoOutputRuntime::clearInvalidatesEveryPreviouslyIssuedClaim()
{
    kiriview::DocumentSessionVideoOutputRuntime runtime;
    QObject owner;
    QObject videoOutput;
    AttachmentProbe probe;
    const QString issuedBeforeClear = runtime.nextSurfaceClaimToken();

    runtime.clear();

    QVERIFY(!reportSurfaceClaim(
        runtime, { issuedBeforeClear, &owner, &videoOutput, true, {}, {} }, probe.port()));
    QCOMPARE(probe.setVideoOutputCount, 0);
    QCOMPARE(probe.setGeometryCount, 0);
}

void TestDocumentSessionVideoOutputRuntime::clearForgetsActiveClaim()
{
    kiriview::DocumentSessionVideoOutputRuntime runtime;
    QObject owner;
    QObject videoOutput;
    AttachmentProbe probe;

    QVERIFY(reportSurfaceClaim(runtime,
        { runtime.nextSurfaceClaimToken(), &owner, &videoOutput, true, {}, {} }, probe.port()));

    runtime.clear();

    probe = AttachmentProbe {};
    QVERIFY(!reportSurfaceClaim(runtime,
        { runtime.nextSurfaceClaimToken(), &owner, nullptr, false, {}, {} }, probe.port()));
    QCOMPARE(probe.setVideoOutputCount, 0);
    QCOMPARE(probe.setGeometryCount, 0);
}

QTEST_GUILESS_MAIN(TestDocumentSessionVideoOutputRuntime)

#include "tst_documentsessionvideooutputruntime.moc"
