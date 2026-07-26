// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/imagedocumentpublicsignals.h"

#include <QObject>
#include <QStringList>
#include <QTest>
#include <vector>

class TestImageDocumentPublicSignals : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void viewportProjectionPlansOneCoherentPublicBatch();
    void statusPlansReadinessDependentPresentationSignals();
    void loadingTargetPlansDedicatedPublicSignal();
    void publicSignalBatchPlansDeduplicateDerivedSignalsInEmissionOrder();
    void emitterCommitsTheSessionSnapshotBeforeProjectionSignals();
};

namespace {
void comparePublicSignals(const std::vector<kiriview::ImageDocumentPublicSignal>& actual,
    const std::vector<kiriview::ImageDocumentPublicSignal>& expected)
{
    QCOMPARE(actual.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        QCOMPARE(actual.at(index), expected.at(index));
    }
}

kiriview::ImageDocumentPublicSignalOperations recordingOperations(QStringList& events)
{
    kiriview::ImageDocumentPublicSignalOperations operations;
    operations.sessionSnapshotChanged
        = [&events]() { events.append(QStringLiteral("sessionSnapshot")); };
    operations.statusChanged = [&events]() { events.append(QStringLiteral("status")); };
    operations.loadingChanged = [&events]() { events.append(QStringLiteral("loading")); };
    operations.loadingTargetChanged
        = [&events]() { events.append(QStringLiteral("loadingTarget")); };
    operations.errorStringChanged = [&events]() { events.append(QStringLiteral("errorString")); };
    operations.displayedUrlChanged = [&events]() { events.append(QStringLiteral("displayedUrl")); };
    operations.imageSizeChanged = [&events]() { events.append(QStringLiteral("imageSize")); };
    operations.viewportFrameChanged
        = [&events]() { events.append(QStringLiteral("viewportFrame")); };
    operations.zoomPercentKnownChanged
        = [&events]() { events.append(QStringLiteral("zoomPercentKnown")); };
    operations.zoomPercentChanged = [&events]() { events.append(QStringLiteral("zoomPercent")); };
    operations.zoomModeChanged = [&events]() { events.append(QStringLiteral("zoomMode")); };
    operations.maximumManualZoomPercentChanged
        = [&events]() { events.append(QStringLiteral("maximumManualZoomPercent")); };
    operations.twoPageModeChanged = [&events]() { events.append(QStringLiteral("twoPageMode")); };
    operations.rightToLeftReadingChanged
        = [&events]() { events.append(QStringLiteral("rightToLeftReading")); };
    operations.imageDocumentSourceScopeChanged
        = [&events]() { events.append(QStringLiteral("imageDocumentSourceScope")); };
    return operations;
}
}

void TestImageDocumentPublicSignals::viewportProjectionPlansOneCoherentPublicBatch()
{
    using Signal = kiriview::ImageDocumentPublicSignal;

    comparePublicSignals(
        kiriview::imageDocumentPublicSignals(kiriview::ImageDocumentChange::ViewportProjection),
        {
            Signal::Status,
            Signal::Loading,
            Signal::ErrorString,
            Signal::DisplayedUrl,
            Signal::ImageSize,
            Signal::ViewportFrame,
            Signal::ZoomPercentKnown,
            Signal::ZoomPercent,
            Signal::ZoomMode,
            Signal::MaximumManualZoomPercent,
            Signal::TwoPageMode,
            Signal::RightToLeftReading,
            Signal::ImageDocumentSourceScope,
        });
}

void TestImageDocumentPublicSignals::statusPlansReadinessDependentPresentationSignals()
{
    using Signal = kiriview::ImageDocumentPublicSignal;

    comparePublicSignals(
        kiriview::imageDocumentPublicSignals(kiriview::ImageDocumentChange::Status),
        {
            Signal::Status,
            Signal::ImageSize,
            Signal::ViewportFrame,
            Signal::ZoomPercentKnown,
            Signal::ZoomPercent,
            Signal::TwoPageMode,
        });
}

void TestImageDocumentPublicSignals::loadingTargetPlansDedicatedPublicSignal()
{
    using Signal = kiriview::ImageDocumentPublicSignal;

    comparePublicSignals(
        kiriview::imageDocumentPublicSignals(kiriview::ImageDocumentChange::LoadingTarget),
        { Signal::LoadingTarget });
}

void TestImageDocumentPublicSignals::
    publicSignalBatchPlansDeduplicateDerivedSignalsInEmissionOrder()
{
    using Change = kiriview::ImageDocumentChange;
    using Signal = kiriview::ImageDocumentPublicSignal;

    comparePublicSignals(kiriview::imageDocumentPublicSignalsForChanges(
                             { Change::TwoPageMode, Change::PageNavigation, Change::DisplayedUrl,
                                 Change::Status, Change::ViewportProjection, Change::TwoPageMode }),
        {
            Signal::TwoPageMode,
            Signal::PageNavigation,
            Signal::DisplayedUrl,
            Signal::Status,
            Signal::ImageSize,
            Signal::ViewportFrame,
            Signal::ZoomPercentKnown,
            Signal::ZoomPercent,
            Signal::Loading,
            Signal::ErrorString,
            Signal::ZoomMode,
            Signal::MaximumManualZoomPercent,
            Signal::RightToLeftReading,
            Signal::ImageDocumentSourceScope,
        });
}

void TestImageDocumentPublicSignals::emitterCommitsTheSessionSnapshotBeforeProjectionSignals()
{
    QStringList events;
    const kiriview::ImageDocumentPublicSignalEmitter emitter(recordingOperations(events));

    emitter.emitChanges({ kiriview::ImageDocumentChange::ViewportProjection });

    QCOMPARE(events,
        QStringList({
            QStringLiteral("sessionSnapshot"),
            QStringLiteral("status"),
            QStringLiteral("loading"),
            QStringLiteral("errorString"),
            QStringLiteral("displayedUrl"),
            QStringLiteral("imageSize"),
            QStringLiteral("viewportFrame"),
            QStringLiteral("zoomPercentKnown"),
            QStringLiteral("zoomPercent"),
            QStringLiteral("zoomMode"),
            QStringLiteral("maximumManualZoomPercent"),
            QStringLiteral("twoPageMode"),
            QStringLiteral("rightToLeftReading"),
            QStringLiteral("imageDocumentSourceScope"),
        }));
}

QTEST_GUILESS_MAIN(TestImageDocumentPublicSignals)

#include "tst_imagedocumentpublicsignals.moc"
