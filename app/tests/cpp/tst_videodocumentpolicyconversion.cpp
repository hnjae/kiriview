// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "bridge/videodocumentpolicyconversion.h"
#include "kiriview/src/policy/videodocumentpolicy.cxx.h"

#include <QObject>
#include <QTest>
#include <variant>

class TestVideoDocumentPolicyConversion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void statusSnapshotAndPlanRoundTripThroughRustPolicy();
    void playbackSnapshotMapsPlainFields();
    void playbackPlanConversionMapsPlainFieldsAndBackendVariants();
};

namespace {
template <typename Operation>
const Operation* operationAt(const kiriview::VideoPlaybackControlPlan& plan, std::size_t index)
{
    return std::get_if<Operation>(&plan.backendOperations.at(index));
}
}

void TestVideoDocumentPolicyConversion::statusSnapshotAndPlanRoundTripThroughRustPolicy()
{
    const kiriview::RustVideoDocumentStatusSnapshot snapshot
        = kiriview::Bridge::rustVideoDocumentStatusSnapshot(kiriview::VideoDocumentStatusSnapshot {
            false,
            false,
            true,
            kiriview::VideoMediaStatus::EndOfMedia,
        });

    const kiriview::VideoDocumentStatusPlan plan
        = kiriview::Bridge::videoDocumentStatusPlanFromRust(
            kiriview::rustVideoDocumentStatusPlan(snapshot));

    QCOMPARE(plan.status, kiriview::VideoDocumentStatus::Ready);
    QVERIFY(plan.mediaEnded);
    QVERIFY(plan.clearPlaying);
}

void TestVideoDocumentPolicyConversion::playbackSnapshotMapsPlainFields()
{
    const kiriview::RustVideoPlaybackControlSnapshot snapshot
        = kiriview::Bridge::rustVideoPlaybackControlSnapshot(
            kiriview::VideoPlaybackControlSnapshot {
                false,
                true,
                false,
                true,
                true,
                10000,
                10000,
            });

    QVERIFY(!snapshot.source_url_empty);
    QVERIFY(snapshot.media_backend_available);
    QVERIFY(!snapshot.playing);
    QVERIFY(snapshot.media_ended);
    QVERIFY(snapshot.seekable);
    QCOMPARE(snapshot.position, qint64(10000));
    QCOMPARE(snapshot.duration, qint64(10000));
}

void TestVideoDocumentPolicyConversion::playbackPlanConversionMapsPlainFieldsAndBackendVariants()
{
    kiriview::RustVideoPlaybackControlPlan rustPlan {};
    rustPlan.state_delta.media_ended_changed = true;
    rustPlan.state_delta.media_ended = false;
    rustPlan.state_delta.playing_changed = true;
    rustPlan.state_delta.playing = true;
    rustPlan.state_delta.position_changed = true;
    rustPlan.state_delta.position = 1234;
    rustPlan.backend_operations.push_back(kiriview::RustVideoPlaybackBackendOperation {
        kiriview::RustVideoPlaybackBackendOperationKind::EnsureBackend,
        0,
    });
    rustPlan.backend_operations.push_back(kiriview::RustVideoPlaybackBackendOperation {
        kiriview::RustVideoPlaybackBackendOperationKind::Play,
        0,
    });
    rustPlan.backend_operations.push_back(kiriview::RustVideoPlaybackBackendOperation {
        kiriview::RustVideoPlaybackBackendOperationKind::Pause,
        0,
    });
    rustPlan.backend_operations.push_back(kiriview::RustVideoPlaybackBackendOperation {
        kiriview::RustVideoPlaybackBackendOperationKind::Stop,
        0,
    });
    rustPlan.backend_operations.push_back(kiriview::RustVideoPlaybackBackendOperation {
        kiriview::RustVideoPlaybackBackendOperationKind::SetPosition,
        1234,
    });

    const kiriview::VideoPlaybackControlPlan plan
        = kiriview::Bridge::videoPlaybackControlPlanFromRust(rustPlan);

    QVERIFY(plan.stateDelta.mediaEnded.has_value());
    QCOMPARE(plan.stateDelta.mediaEnded.value(), false);
    QVERIFY(plan.stateDelta.playing.has_value());
    QCOMPARE(plan.stateDelta.playing.value(), true);
    QVERIFY(plan.stateDelta.position.has_value());
    QCOMPARE(plan.stateDelta.position.value(), qint64(1234));
    QCOMPARE(plan.backendOperations.size(), std::size_t(5));
    QVERIFY(operationAt<kiriview::EnsureVideoPlaybackBackendOperation>(plan, 0) != nullptr);
    QVERIFY(operationAt<kiriview::PlayVideoPlaybackOperation>(plan, 1) != nullptr);
    QVERIFY(operationAt<kiriview::PauseVideoPlaybackOperation>(plan, 2) != nullptr);
    QVERIFY(operationAt<kiriview::StopVideoPlaybackOperation>(plan, 3) != nullptr);
    const auto* setPosition = operationAt<kiriview::SetVideoPlaybackPositionOperation>(plan, 4);
    QVERIFY(setPosition != nullptr);
    QCOMPARE(setPosition->position, qint64(1234));
}

QTEST_GUILESS_MAIN(TestVideoDocumentPolicyConversion)

#include "tst_videodocumentpolicyconversion.moc"
