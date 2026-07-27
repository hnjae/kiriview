// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionvideooutputruntime.h"

#include <QObject>
#include <QRectF>
#include <QTest>
#include <memory>

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
    void reentrantReplacementKeepsReplacementGeometry();
    void activeOwnerDestructionDetachesOutput();
    void activeOutputDestructionDetachesOutput();
    void retiredEpochRejectsReentrantFreshClaim();
    void activationInvalidatesTokensIssuedWhileRetired();
    void destructionDetachesActiveClaimAfterRetiringEpoch();
    void destructionWithoutActiveClaimDoesNotTouchPort();
};

namespace {
constexpr kiriview::DocumentSessionVideoOutputClaimAdmission acceptedAdmission { 0, true };

struct AttachmentProbe
{
    kiriview::DocumentSessionVideoOutputAttachmentPort port()
    {
        return kiriview::DocumentSessionVideoOutputAttachmentPort {
            [this](QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect) {
                attachedVideoOutput = videoOutput;
                lastContentRect = contentRect;
                lastSourceRect = sourceRect;
                lastAttachmentWasDetach = videoOutput == nullptr;
                ++setAttachmentCount;
            },
        };
    }

    QObject* attachedVideoOutput = nullptr;
    QRectF lastContentRect;
    QRectF lastSourceRect;
    int setAttachmentCount = 0;
    bool lastAttachmentWasDetach = false;
};

bool reportSurfaceClaim(kiriview::DocumentSessionVideoOutputRuntime& runtime,
    const kiriview::DocumentSessionVideoOutputClaimReport& report)
{
    return runtime.reportSurfaceClaim(report, acceptedAdmission);
}
}

void TestDocumentSessionVideoOutputRuntime::appliesAcceptedAttachAndDetachThroughPort()
{
    AttachmentProbe probe;
    kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
    runtime.activateSurfaceClaimEpoch();
    QObject owner;
    QObject videoOutput;
    const QRectF contentRect(1.0, 2.0, 320.0, 180.0);
    const QRectF sourceRect(3.0, 4.0, 640.0, 360.0);

    QVERIFY(reportSurfaceClaim(runtime,
        { runtime.nextSurfaceClaimToken(), &owner, &videoOutput, true, contentRect, sourceRect }));
    QCOMPARE(probe.attachedVideoOutput, &videoOutput);
    QCOMPARE(probe.lastContentRect, contentRect);
    QCOMPARE(probe.lastSourceRect, sourceRect);
    QCOMPARE(probe.setAttachmentCount, 1);

    QVERIFY(reportSurfaceClaim(
        runtime, { runtime.nextSurfaceClaimToken(), &owner, nullptr, false, {}, {} }));
    QCOMPARE(probe.attachedVideoOutput, nullptr);
    QCOMPARE(probe.lastContentRect, QRectF());
    QCOMPARE(probe.lastSourceRect, QRectF());
    QCOMPARE(probe.setAttachmentCount, 2);
}

void TestDocumentSessionVideoOutputRuntime::rejectsStaleInvalidAndForeignClaimsWithoutTouchingPort()
{
    AttachmentProbe probe;
    kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
    runtime.activateSurfaceClaimEpoch();
    QObject owner;
    QObject otherOwner;
    QObject videoOutput;

    const QString staleToken = runtime.nextSurfaceClaimToken();
    const QString attachToken = runtime.nextSurfaceClaimToken();
    QVERIFY(reportSurfaceClaim(runtime, { attachToken, &owner, &videoOutput, true, {}, {} }));

    probe = AttachmentProbe {};
    QVERIFY(!reportSurfaceClaim(runtime, { staleToken, &owner, nullptr, false, {}, {} }));
    QVERIFY(!reportSurfaceClaim(
        runtime, { QStringLiteral("not-a-token"), &owner, &videoOutput, true, {}, {} }));
    QVERIFY(!reportSurfaceClaim(
        runtime, { runtime.nextSurfaceClaimToken(), nullptr, &videoOutput, true, {}, {} }));
    QVERIFY(!reportSurfaceClaim(
        runtime, { runtime.nextSurfaceClaimToken(), &otherOwner, nullptr, false, {}, {} }));
    QCOMPARE(probe.setAttachmentCount, 0);
    runtime.clearAttachment();
}

void TestDocumentSessionVideoOutputRuntime::rejectsGloballyStaleClaimsFromDifferentOwners()
{
    AttachmentProbe probe;
    kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
    runtime.activateSurfaceClaimEpoch();
    QObject staleOwner;
    QObject currentOwner;
    QObject staleVideoOutput;
    QObject currentVideoOutput;
    const QString staleToken = runtime.nextSurfaceClaimToken();
    const QString currentToken = runtime.nextSurfaceClaimToken();

    QVERIFY(reportSurfaceClaim(
        runtime, { currentToken, &currentOwner, &currentVideoOutput, true, {}, {} }));
    probe = AttachmentProbe {};

    QVERIFY(
        !reportSurfaceClaim(runtime, { staleToken, &staleOwner, &staleVideoOutput, true, {}, {} }));
    QCOMPARE(probe.setAttachmentCount, 0);
    runtime.clearAttachment();
}

void TestDocumentSessionVideoOutputRuntime::rejectsReplayedAndUnissuedClaimTokens()
{
    AttachmentProbe probe;
    kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
    runtime.activateSurfaceClaimEpoch();
    QObject owner;
    QObject videoOutput;
    const QString acceptedToken = runtime.nextSurfaceClaimToken();

    QVERIFY(reportSurfaceClaim(runtime, { acceptedToken, &owner, &videoOutput, true, {}, {} }));
    probe = AttachmentProbe {};

    QVERIFY(!reportSurfaceClaim(runtime, { acceptedToken, &owner, &videoOutput, true, {}, {} }));
    const quint64 unissuedRevision = acceptedToken.toULongLong() + 1;
    QVERIFY(!reportSurfaceClaim(
        runtime, { QString::number(unissuedRevision), &owner, &videoOutput, true, {}, {} }));

    const QString invalidPayloadToken = runtime.nextSurfaceClaimToken();
    QVERIFY(
        !reportSurfaceClaim(runtime, { invalidPayloadToken, nullptr, &videoOutput, true, {}, {} }));
    QVERIFY(
        !reportSurfaceClaim(runtime, { invalidPayloadToken, &owner, &videoOutput, true, {}, {} }));
    QCOMPARE(probe.setAttachmentCount, 0);
    runtime.clearAttachment();
}

void TestDocumentSessionVideoOutputRuntime::clearInvalidatesEveryPreviouslyIssuedClaim()
{
    AttachmentProbe probe;
    kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
    runtime.activateSurfaceClaimEpoch();
    QObject owner;
    QObject videoOutput;
    const QString issuedBeforeClear = runtime.nextSurfaceClaimToken();

    runtime.clearAttachment();

    QVERIFY(
        !reportSurfaceClaim(runtime, { issuedBeforeClear, &owner, &videoOutput, true, {}, {} }));
    QCOMPARE(probe.setAttachmentCount, 1);
    QVERIFY(probe.lastAttachmentWasDetach);
}

void TestDocumentSessionVideoOutputRuntime::clearForgetsActiveClaim()
{
    AttachmentProbe probe;
    kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
    runtime.activateSurfaceClaimEpoch();
    QObject owner;
    QObject videoOutput;

    QVERIFY(reportSurfaceClaim(
        runtime, { runtime.nextSurfaceClaimToken(), &owner, &videoOutput, true, {}, {} }));

    runtime.clearAttachment();

    probe = AttachmentProbe {};
    QVERIFY(!reportSurfaceClaim(
        runtime, { runtime.nextSurfaceClaimToken(), &owner, nullptr, false, {}, {} }));
    QCOMPARE(probe.setAttachmentCount, 0);
}

void TestDocumentSessionVideoOutputRuntime::reentrantReplacementKeepsReplacementGeometry()
{
    auto firstOwner = std::make_unique<QObject>();
    QObject replacementOwner;
    auto firstVideoOutput = std::make_unique<QObject>();
    QObject replacementVideoOutput;
    AttachmentProbe probe;
    const QRectF firstContentRect(1.0, 2.0, 320.0, 180.0);
    const QRectF firstSourceRect(3.0, 4.0, 640.0, 360.0);
    const QRectF replacementContentRect(5.0, 6.0, 800.0, 450.0);
    const QRectF replacementSourceRect(7.0, 8.0, 1600.0, 900.0);
    bool reentered = false;
    bool replacementAccepted = false;
    std::unique_ptr<kiriview::DocumentSessionVideoOutputRuntime> runtime;
    kiriview::DocumentSessionVideoOutputAttachmentPort port;
    port.setVideoOutputAttachment
        = [&](QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect) {
              probe.attachedVideoOutput = videoOutput;
              probe.lastContentRect = contentRect;
              probe.lastSourceRect = sourceRect;
              probe.lastAttachmentWasDetach = videoOutput == nullptr;
              ++probe.setAttachmentCount;
              if (videoOutput != firstVideoOutput.get() || reentered) {
                  return;
              }
              reentered = true;
              replacementAccepted = reportSurfaceClaim(*runtime,
                  { runtime->nextSurfaceClaimToken(), &replacementOwner, &replacementVideoOutput,
                      true, replacementContentRect, replacementSourceRect });
          };
    runtime = std::make_unique<kiriview::DocumentSessionVideoOutputRuntime>(port);
    runtime->activateSurfaceClaimEpoch();

    QVERIFY(reportSurfaceClaim(*runtime,
        { runtime->nextSurfaceClaimToken(), firstOwner.get(), firstVideoOutput.get(), true,
            firstContentRect, firstSourceRect }));

    QVERIFY(reentered);
    QVERIFY(replacementAccepted);
    QCOMPARE(probe.attachedVideoOutput, &replacementVideoOutput);
    QCOMPARE(probe.lastContentRect, replacementContentRect);
    QCOMPARE(probe.lastSourceRect, replacementSourceRect);

    firstOwner.reset();
    firstVideoOutput.reset();

    QCOMPARE(probe.attachedVideoOutput, &replacementVideoOutput);
    QCOMPARE(probe.lastContentRect, replacementContentRect);
    QCOMPARE(probe.lastSourceRect, replacementSourceRect);
}

void TestDocumentSessionVideoOutputRuntime::activeOwnerDestructionDetachesOutput()
{
    AttachmentProbe probe;
    kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
    runtime.activateSurfaceClaimEpoch();
    auto owner = std::make_unique<QObject>();
    QObject videoOutput;

    QVERIFY(reportSurfaceClaim(
        runtime, { runtime.nextSurfaceClaimToken(), owner.get(), &videoOutput, true, {}, {} }));
    QCOMPARE(probe.attachedVideoOutput, &videoOutput);
    const QString pendingClaimToken = runtime.nextSurfaceClaimToken();

    owner.reset();

    QCOMPARE(probe.attachedVideoOutput, nullptr);
    QVERIFY(probe.lastAttachmentWasDetach);

    QObject staleOwner;
    QObject staleVideoOutput;
    QVERIFY(!reportSurfaceClaim(
        runtime, { pendingClaimToken, &staleOwner, &staleVideoOutput, true, {}, {} }));
}

void TestDocumentSessionVideoOutputRuntime::activeOutputDestructionDetachesOutput()
{
    AttachmentProbe probe;
    kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
    runtime.activateSurfaceClaimEpoch();
    QObject owner;
    auto videoOutput = std::make_unique<QObject>();

    QVERIFY(reportSurfaceClaim(
        runtime, { runtime.nextSurfaceClaimToken(), &owner, videoOutput.get(), true, {}, {} }));
    QCOMPARE(probe.attachedVideoOutput, videoOutput.get());

    videoOutput.reset();

    QCOMPARE(probe.setAttachmentCount, 2);
    QVERIFY(probe.lastAttachmentWasDetach);
}

void TestDocumentSessionVideoOutputRuntime::retiredEpochRejectsReentrantFreshClaim()
{
    QObject owner;
    QObject videoOutput;
    QObject* attachedVideoOutput = nullptr;
    bool reattachAttempted = false;
    bool reattachAccepted = true;
    std::unique_ptr<kiriview::DocumentSessionVideoOutputRuntime> runtime;
    kiriview::DocumentSessionVideoOutputAttachmentPort port;
    port.setVideoOutputAttachment = [&](QObject* output, const QRectF&, const QRectF&) {
        attachedVideoOutput = output;
        if (output != nullptr || reattachAttempted) {
            return;
        }
        reattachAttempted = true;
        reattachAccepted = reportSurfaceClaim(
            *runtime, { runtime->nextSurfaceClaimToken(), &owner, &videoOutput, true, {}, {} });
    };
    runtime = std::make_unique<kiriview::DocumentSessionVideoOutputRuntime>(std::move(port));
    runtime->activateSurfaceClaimEpoch();
    QVERIFY(reportSurfaceClaim(
        *runtime, { runtime->nextSurfaceClaimToken(), &owner, &videoOutput, true, {}, {} }));

    runtime->retireSurfaceClaimEpoch();

    QVERIFY(reattachAttempted);
    QVERIFY(!reattachAccepted);
    QCOMPARE(attachedVideoOutput, nullptr);
}

void TestDocumentSessionVideoOutputRuntime::activationInvalidatesTokensIssuedWhileRetired()
{
    AttachmentProbe probe;
    kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
    QObject owner;
    QObject videoOutput;
    const QString retiredToken = runtime.nextSurfaceClaimToken();

    runtime.activateSurfaceClaimEpoch();

    QVERIFY(!reportSurfaceClaim(runtime, { retiredToken, &owner, &videoOutput, true, {}, {} }));
    QVERIFY(reportSurfaceClaim(
        runtime, { runtime.nextSurfaceClaimToken(), &owner, &videoOutput, true, {}, {} }));
    QCOMPARE(probe.attachedVideoOutput, &videoOutput);
}

void TestDocumentSessionVideoOutputRuntime::destructionDetachesActiveClaimAfterRetiringEpoch()
{
    QObject owner;
    QObject videoOutput;
    QObject* attachedVideoOutput = nullptr;
    bool reattachAttempted = false;
    bool reattachAccepted = true;
    kiriview::DocumentSessionVideoOutputRuntime* runtimeDuringDestruction = nullptr;
    kiriview::DocumentSessionVideoOutputAttachmentPort port;
    port.setVideoOutputAttachment = [&](QObject* output, const QRectF&, const QRectF&) {
        attachedVideoOutput = output;
        if (output != nullptr || reattachAttempted) {
            return;
        }
        reattachAttempted = true;
        runtimeDuringDestruction->activateSurfaceClaimEpoch();
        reattachAccepted = reportSurfaceClaim(*runtimeDuringDestruction,
            { runtimeDuringDestruction->nextSurfaceClaimToken(), &owner, &videoOutput, true, {},
                {} });
    };
    auto runtime = std::make_unique<kiriview::DocumentSessionVideoOutputRuntime>(std::move(port));
    runtimeDuringDestruction = runtime.get();
    runtime->activateSurfaceClaimEpoch();
    QVERIFY(reportSurfaceClaim(
        *runtime, { runtime->nextSurfaceClaimToken(), &owner, &videoOutput, true, {}, {} }));

    runtime.reset();

    QVERIFY(reattachAttempted);
    QVERIFY(!reattachAccepted);
    QCOMPARE(attachedVideoOutput, nullptr);
}

void TestDocumentSessionVideoOutputRuntime::destructionWithoutActiveClaimDoesNotTouchPort()
{
    AttachmentProbe probe;

    {
        kiriview::DocumentSessionVideoOutputRuntime runtime(probe.port());
        runtime.activateSurfaceClaimEpoch();
    }

    QCOMPARE(probe.setAttachmentCount, 0);
}

QTEST_GUILESS_MAIN(TestDocumentSessionVideoOutputRuntime)

#include "tst_documentsessionvideooutputruntime.moc"
