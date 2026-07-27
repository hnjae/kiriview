// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/videodocumentpublicsignals.h"

#include <QObject>
#include <QStringList>
#include <QTest>
#include <vector>

class TestVideoDocumentPublicSignals : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publicSignalPlansReturnSignalsInEmissionOrder();
    void publicSignalBatchPlansDeduplicateSignalsInEmissionOrder();
    void publicationPlansSessionSnapshotBeforeDependentSignals();
    void emitterDispatchesChangeSignalsInProjectionOrder();
    void emitterSkipsSessionSnapshotForUnrelatedChanges();
};

namespace {
void comparePublicSignals(const std::vector<kiriview::VideoDocumentPublicSignal>& actual,
    const std::vector<kiriview::VideoDocumentPublicSignal>& expected)
{
    QCOMPARE(actual.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        QCOMPARE(actual.at(index), expected.at(index));
    }
}

kiriview::VideoDocumentPublicSignalOperations recordingOperations(QStringList& events)
{
    kiriview::VideoDocumentPublicSignalOperations operations;
    operations.sessionSnapshotChanged
        = [&events]() { events.append(QStringLiteral("sessionSnapshot")); };
    operations.sourceUrlChanged = [&events]() { events.append(QStringLiteral("sourceUrl")); };
    operations.statusChanged = [&events]() { events.append(QStringLiteral("status")); };
    operations.errorStringChanged = [&events]() { events.append(QStringLiteral("errorString")); };
    operations.windowTitleFileNameChanged
        = [&events]() { events.append(QStringLiteral("windowTitleFileName")); };
    operations.hasVideoChanged = [&events]() { events.append(QStringLiteral("hasVideo")); };
    operations.hasAudioChanged = [&events]() { events.append(QStringLiteral("hasAudio")); };
    operations.videoSizeChanged = [&events]() { events.append(QStringLiteral("videoSize")); };
    operations.zoomPercentKnownChanged
        = [&events]() { events.append(QStringLiteral("zoomPercentKnown")); };
    operations.zoomPercentChanged = [&events]() { events.append(QStringLiteral("zoomPercent")); };
    operations.videoOutputChanged = [&events]() { events.append(QStringLiteral("videoOutput")); };
    operations.embeddedMetadataChanged
        = [&events]() { events.append(QStringLiteral("embeddedMetadata")); };
    operations.playbackControlProjectionChanged
        = [&events]() { events.append(QStringLiteral("playbackControlProjection")); };
    return operations;
}
}

void TestVideoDocumentPublicSignals::publicSignalPlansReturnSignalsInEmissionOrder()
{
    using Change = kiriview::VideoDocumentChange;
    using Signal = kiriview::VideoDocumentPublicSignal;

    comparePublicSignals(
        kiriview::videoDocumentPublicSignals(Change::SourceUrl), { Signal::SourceUrl });
    comparePublicSignals(kiriview::videoDocumentPublicSignals(Change::Status), { Signal::Status });
    comparePublicSignals(
        kiriview::videoDocumentPublicSignals(Change::ErrorString), { Signal::ErrorString });
    comparePublicSignals(kiriview::videoDocumentPublicSignals(Change::WindowTitleFileName),
        { Signal::WindowTitleFileName });
    comparePublicSignals(
        kiriview::videoDocumentPublicSignals(Change::HasVideo), { Signal::HasVideo });
    comparePublicSignals(
        kiriview::videoDocumentPublicSignals(Change::HasAudio), { Signal::HasAudio });
    comparePublicSignals(
        kiriview::videoDocumentPublicSignals(Change::VideoSize), { Signal::VideoSize });
    comparePublicSignals(kiriview::videoDocumentPublicSignals(Change::ZoomPercentKnown),
        { Signal::ZoomPercentKnown });
    comparePublicSignals(
        kiriview::videoDocumentPublicSignals(Change::ZoomPercent), { Signal::ZoomPercent });
    comparePublicSignals(
        kiriview::videoDocumentPublicSignals(Change::VideoOutput), { Signal::VideoOutput });
}

void TestVideoDocumentPublicSignals::publicSignalBatchPlansDeduplicateSignalsInEmissionOrder()
{
    using Change = kiriview::VideoDocumentChange;
    using Signal = kiriview::VideoDocumentPublicSignal;

    comparePublicSignals(kiriview::videoDocumentPublicSignalsForChanges({ Change::Status,
                             Change::HasAudio, Change::Status, Change::VideoSize }),
        { Signal::Status, Signal::HasAudio, Signal::VideoSize });
}

void TestVideoDocumentPublicSignals::publicationPlansSessionSnapshotBeforeDependentSignals()
{
    using Change = kiriview::VideoDocumentChange;
    using Signal = kiriview::VideoDocumentPublicSignal;

    comparePublicSignals(kiriview::videoDocumentPublicationSignalsForChanges({ Change::HasAudio,
                             Change::Status, Change::VideoOutput, Change::VideoSize }),
        { Signal::SessionSnapshot, Signal::HasAudio, Signal::Status, Signal::VideoOutput,
            Signal::VideoSize });
}

void TestVideoDocumentPublicSignals::emitterDispatchesChangeSignalsInProjectionOrder()
{
    QStringList events;
    const kiriview::VideoDocumentPublicSignalEmitter emitter(recordingOperations(events));

    for (const kiriview::VideoDocumentPublicSignal signal :
        kiriview::videoDocumentPublicationSignalsForChanges(
            { kiriview::VideoDocumentChange::HasAudio, kiriview::VideoDocumentChange::HasVideo,
                kiriview::VideoDocumentChange::ZoomPercent })) {
        emitter.emitSignal(signal);
    }
    emitter.emitSignal(kiriview::VideoDocumentPublicSignal::VideoOutput);
    emitter.emitSignal(kiriview::VideoDocumentPublicSignal::PlaybackControlProjection);

    QCOMPARE(events,
        QStringList({
            QStringLiteral("sessionSnapshot"),
            QStringLiteral("hasAudio"),
            QStringLiteral("hasVideo"),
            QStringLiteral("zoomPercent"),
            QStringLiteral("videoOutput"),
            QStringLiteral("playbackControlProjection"),
        }));
}

void TestVideoDocumentPublicSignals::emitterSkipsSessionSnapshotForUnrelatedChanges()
{
    QStringList events;
    const kiriview::VideoDocumentPublicSignalEmitter emitter(recordingOperations(events));

    for (const kiriview::VideoDocumentPublicSignal signal :
        kiriview::videoDocumentPublicationSignalsForChanges(
            { kiriview::VideoDocumentChange::HasAudio,
                kiriview::VideoDocumentChange::VideoOutput })) {
        emitter.emitSignal(signal);
    }

    QCOMPARE(events,
        QStringList({
            QStringLiteral("hasAudio"),
            QStringLiteral("videoOutput"),
        }));
}

QTEST_GUILESS_MAIN(TestVideoDocumentPublicSignals)

#include "tst_videodocumentpublicsignals.moc"
