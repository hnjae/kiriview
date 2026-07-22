// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_provider_test_support.h"

#include <QtCore/QMetaProperty>

namespace {

enum MetadataProjectionScenario {
    UnknownProviderFacts,
    PartialKnownFacts,
    CompleteStillFacts,
    CompleteTimedFacts,
    RuntimeStillMetadata,
    RuntimeTimedMetadata,
};

QVariant metadataProperty(
    const ImageViewportRoleMetadataSnapshot& metadata, const char* propertyName)
{
    const QMetaObject& metaObject = ImageViewportRoleMetadataSnapshot::staticMetaObject;
    const int propertyIndex = metaObject.indexOfProperty(propertyName);
    return propertyIndex >= 0 ? metaObject.property(propertyIndex).readOnGadget(&metadata)
                              : QVariant {};
}

} // namespace

class ImageViewportProviderMetadataTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderMetadataTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:
    void roleScopedMetadataProjectionUsesOnePath_data();
    void roleScopedMetadataProjectionUsesOnePath();
    void providerPartialMetadataProjectsExplicitAvailability();
    void providerRuntimeAuthoredFactsRespectConstructionFacts();
    void sameTargetRefinementComparesAuthoredFactAvailability();
    void providerConstructionMetadataSelectsInitialFrameRequest();
    void providerFixedDurationConstructionMetadataSelectsInitialFrameRequest();
    void providerKnownConstructionMetadataSelectsInitialFrameWithoutDeclaredCapabilities();
    void providerKnownConstructionMetadataBindsAcceptedSeekImmediately();
    void providerKnownStillConstructionMetadataConstrainsCommands();
    void providerKnownConstructionMetadataRejectsSeeksPastKnownBounds();
    void providerLogicalSizeFactsWaitForRuntimeMetadata();
    void providerTimedFrameCountFactsProjectFrameBoundsOnly();
    void providerRuntimeMetadataContradictsKnownLogicalSizeFacts();
    void providerRuntimeMetadataContradictsKnownFrameCountFacts();
    void secondaryProviderRuntimeMetadataContradictsKnownLogicalSizeFacts();
    void secondaryProviderDeclaredCapabilityContradictionRejectsMetadata();
    void secondaryProviderMetadataDoesNotReviveStaleInitialTarget();
    void providerCompleteDurationFactsSelectInitialFrameWithoutMetadata();
    void providerCompleteKnownFactsSelectInitialFrameRequest();
    void providerMetadataLoadingPauseStopPreserveInitialRequest();
    void providerDeclaredCapabilityProjectsBeforeMetadata();
    void providerDeclaredTrueCapabilityProjectsBeforeMetadata();
    void providerKnownCapabilityProjectsBeforeMetadata();
    void providerDeclaredCapabilityContradictionRejectsMetadata();
    void providerDeclaredTrueCapabilityContradictionRejectsMetadata();
    void providerRuntimeMetadataCapabilitiesOverrideTimingInference();
    void providerDeclaredNoPlaybackRejectsPlayBeforeMetadata();
    void providerKnownNoPlaybackRejectsPlayBeforeMetadata();
    void providerDeclaredNoFrameSeekRejectsSeekBeforeMetadata();
    void providerDeclaredNoPositionSeekRejectsPositionSeekBeforeMetadata();
    void providerMetadataRejectsNonFiniteLogicalSize();
    void providerMetadataRejectsHugeFiniteLogicalSize();
    void providerMetadataRejectsPublishedFrameCountLimit();
    void providerMetadataRejectsPublishedDurationLimits();
    void providerStillMetadataSelectsInitialFrameRequest();
    void providerTimedMetadataSelectsInitialFrameRequest();
    void providerFixedDurationMetadataSelectsInitialFrameRequest();
    void providerProgressResultsAreAdvisory();
    void providerInvalidProgressResultsAreIgnored();
    void providerTerminalResultDominatesProgress();
    void providerFrameReadyDominatesLateProgress();
    void providerPositiveResizeWhileMetadataWaitingKeepsProviderWaiting();
    void providerMetadataReadySealsMetadataToken();
};

void ImageViewportProviderMetadataTest::providerPartialMetadataProjectsExplicitAvailability()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata::withSourceLogicalSize(QSizeF(16.0, 8.0)));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    const ImageViewportRoleMetadataSnapshot metadata = item.state().primary().metadata();
    QCOMPARE(metadata.available(), true);
    QCOMPARE(metadata.sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(metadata.frameCount(), -1);
    QCOMPARE(metadataProperty(metadata, "autoplay"),
        QVariant::fromValue(ImageViewportCapabilitySupport::Unavailable));
    QCOMPARE(metadata.loopMode(), ImageSequenceAuthoredAnimationLoopMode::Unavailable);
    QCOMPARE(metadata.loopCount(), -1);
}

void ImageViewportProviderMetadataTest::providerRuntimeAuthoredFactsRespectConstructionFacts()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata::withSourceLogicalSize(QSizeF(16.0, 8.0)));
    adapter.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::finiteLoop(3));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    useSynchronousProviderEventDeliveryForTest(item);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QVERIFY(sessionFactory->lastSession());

    ImageSequenceProviderMetadata runtime
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    runtime.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::infiniteLoop());
    emitProviderMetadataReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastMetadataToken(), runtime);

    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(item.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QVERIFY(!item.state().diagnostics().errorString().isEmpty());
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportProviderMetadataTest::sameTargetRefinementComparesAuthoredFactAvailability()
{
    ImageSequenceFactory factory;
    const auto makeFactory = [] {
        return std::make_shared<CountingProviderSessionFactory>(std::make_shared<int>(0),
            std::make_shared<int>(0), std::make_shared<int>(0), std::make_shared<int>(-1),
            std::make_shared<int>(0));
    };
    auto currentFactory = makeFactory();
    auto replacementFactory = makeFactory();
    const ImageSequenceProviderMetadata complete
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    CountingProviderAdapter currentAdapter(currentFactory, complete);
    CountingProviderAdapter replacementAdapter(replacementFactory, complete);
    replacementAdapter.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts {});
    QScopedPointer<ImageSequenceFactoryResult> current(factory.fromProvider(&currentAdapter));
    QScopedPointer<ImageSequenceFactoryResult> replacement(
        factory.fromProvider(&replacementAdapter));
    QVERIFY(current->sequence());
    QVERIFY(replacement->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(current->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    PresentationTargetTransitionPolicy refinementPolicy;
    refinementPolicy.setReplacementIntent(
        PresentationTargetTransitionPolicy::ReplacementIntent::SameTargetRefinement);

    const ImageViewportStateSnapshot before = item.state();
    QCOMPARE(item.setPresentationTarget(
                     ImageViewportPresentationTarget(replacement->sequence()), refinementPolicy)
                 .outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(item.state().request(), before.request());
    QCOMPARE(item.state().display(), before.display());
    QCOMPARE(item.state().presentation(), before.presentation());
}

void ImageViewportProviderMetadataTest::roleScopedMetadataProjectionUsesOnePath_data()
{
    QTest::addColumn<bool>("secondaryRole");
    QTest::addColumn<int>("scenario");
    QTest::addColumn<int>("expectedFrameCount");
    QTest::addColumn<int>("expectedTotalDuration");
    QTest::addColumn<int>("expectedFrameMinimum");
    QTest::addColumn<int>("expectedFrameMaximum");
    QTest::addColumn<int>("expectedPositionMinimum");
    QTest::addColumn<int>("expectedPositionMaximum");
    QTest::addColumn<QByteArray>("expectedTimedPlaybackSupport");
    QTest::addColumn<QByteArray>("expectedFrameSeekSupport");
    QTest::addColumn<QByteArray>("expectedPositionSeekSupport");

    const auto addRow
        = [](const char* name, bool secondaryRole, MetadataProjectionScenario scenario,
              int expectedFrameCount, int expectedTotalDuration, int expectedFrameMinimum,
              int expectedFrameMaximum, int expectedPositionMinimum, int expectedPositionMaximum,
              const char* expectedTimedPlaybackSupport, const char* expectedFrameSeekSupport,
              const char* expectedPositionSeekSupport) {
              QTest::newRow(name) << secondaryRole << static_cast<int>(scenario)
                                  << expectedFrameCount << expectedTotalDuration
                                  << expectedFrameMinimum << expectedFrameMaximum
                                  << expectedPositionMinimum << expectedPositionMaximum
                                  << QByteArray(expectedTimedPlaybackSupport)
                                  << QByteArray(expectedFrameSeekSupport)
                                  << QByteArray(expectedPositionSeekSupport);
          };

    addRow("primary-unknown-provider-facts", false, UnknownProviderFacts, -1, -1, -1, -1, -1, -1,
        "Unavailable", "Unavailable", "Unavailable");
    addRow("secondary-unknown-provider-facts", true, UnknownProviderFacts, -1, -1, -1, -1, -1, -1,
        "Unavailable", "Unavailable", "Unavailable");
    addRow("primary-partial-known-facts", false, PartialKnownFacts, 3, -1, 0, 2, -1, -1,
        "Unavailable", "True", "Unavailable");
    addRow("secondary-partial-known-facts", true, PartialKnownFacts, 3, -1, 0, 2, -1, -1,
        "Unavailable", "True", "Unavailable");
    addRow("primary-complete-still-facts", false, CompleteStillFacts, 1, -1, 0, 0, -1, -1, "False",
        "True", "False");
    addRow("secondary-complete-still-facts", true, CompleteStillFacts, 1, -1, 0, 0, -1, -1, "False",
        "True", "False");
    addRow("primary-complete-timed-facts", false, CompleteTimedFacts, 2, 350, 0, 1, 0, 350, "True",
        "True", "True");
    addRow("secondary-complete-timed-facts", true, CompleteTimedFacts, 2, 350, 0, 1, 0, 350, "True",
        "True", "True");
    addRow("primary-runtime-still-metadata", false, RuntimeStillMetadata, 1, -1, 0, 0, -1, -1,
        "False", "True", "False");
    addRow("secondary-runtime-still-metadata", true, RuntimeStillMetadata, 1, -1, 0, 0, -1, -1,
        "False", "True", "False");
    addRow("primary-runtime-timed-metadata", false, RuntimeTimedMetadata, 2, 350, 0, 1, 0, 350,
        "True", "True", "True");
    addRow("secondary-runtime-timed-metadata", true, RuntimeTimedMetadata, 2, 350, 0, 1, 0, 350,
        "True", "True", "True");
}

void ImageViewportProviderMetadataTest::roleScopedMetadataProjectionUsesOnePath()
{
    QFETCH(bool, secondaryRole);
    QFETCH(int, scenario);
    QFETCH(int, expectedFrameCount);
    QFETCH(int, expectedTotalDuration);
    QFETCH(int, expectedFrameMinimum);
    QFETCH(int, expectedFrameMaximum);
    QFETCH(int, expectedPositionMinimum);
    QFETCH(int, expectedPositionMaximum);
    QFETCH(QByteArray, expectedTimedPlaybackSupport);
    QFETCH(QByteArray, expectedFrameSeekSupport);
    QFETCH(QByteArray, expectedPositionSeekSupport);

    ImageSequenceProviderMetadata knownFacts;
    ImageViewportCapabilitySupport timedPlaybackSupport
        = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport frameSeekSupport = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport positionSeekSupport
        = ImageViewportCapabilitySupport::Unavailable;

    switch (static_cast<MetadataProjectionScenario>(scenario)) {
    case UnknownProviderFacts:
    case RuntimeStillMetadata:
    case RuntimeTimedMetadata:
        break;
    case PartialKnownFacts:
        knownFacts = ImageSequenceProviderMetadata::timedFrameCount(QSizeF(16.0, 8.0), 3);
        frameSeekSupport = ImageViewportCapabilitySupport::True;
        break;
    case CompleteStillFacts:
        knownFacts = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
        break;
    case CompleteTimedFacts:
        knownFacts = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
        break;
    }

    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter providerAdapter(
        sessionFactory, knownFacts, timedPlaybackSupport, frameSeekSupport, positionSeekSupport);
    QScopedPointer<ImageSequenceFactoryResult> providerResult(
        factory.fromProvider(&providerAdapter));
    QVERIFY(providerResult->sequence());

    QScopedPointer<ImageSequenceFactoryResult> primaryResult;
    ImageViewport item;
    if (secondaryRole) {
        QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
        primaryImage.fill(Qt::transparent);
        ImageFrame primaryFrame(primaryImage);
        primaryResult.reset(factory.fromFrame(&primaryFrame));
        QVERIFY(primaryResult->sequence());
        QCOMPARE(
            item.setPresentationTarget(ImageViewportPresentationTarget(
                                           primaryResult->sequence(), providerResult->sequence()),
                    PresentationTargetTransitionPolicy {})
                .outcome(),
            ImageViewportCommandOutcome::Accepted);
    } else {
        item.setPresentationTarget(ImageViewportPresentationTarget(providerResult->sequence()),
            PresentationTargetTransitionPolicy {});
    }

    switch (static_cast<MetadataProjectionScenario>(scenario)) {
    case RuntimeStillMetadata:
        QVERIFY(sessionFactory->lastSession());
        emitProviderMetadataReady(sessionFactory->lastSession(),
            sessionFactory->lastSession()->lastMetadataToken(),
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        drainQueuedProviderResults();
        break;
    case RuntimeTimedMetadata:
        QVERIFY(sessionFactory->lastSession());
        emitProviderMetadataReady(sessionFactory->lastSession(),
            sessionFactory->lastSession()->lastMetadataToken(),
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
        drainQueuedProviderResults();
        break;
    case UnknownProviderFacts:
    case PartialKnownFacts:
    case CompleteStillFacts:
    case CompleteTimedFacts:
        break;
    }

    const ImageViewportRoleMetadataSnapshot roleMetadata
        = secondaryRole ? item.state().secondary().metadata() : item.state().primary().metadata();
    const auto expectedTimedSupport = static_cast<ImageViewportCapabilitySupport>(
        enumValue(&ImageViewportEnums::staticMetaObject, "CapabilitySupport",
            expectedTimedPlaybackSupport.constData()));
    const auto expectedFrameSupport = static_cast<ImageViewportCapabilitySupport>(
        enumValue(&ImageViewportEnums::staticMetaObject, "CapabilitySupport",
            expectedFrameSeekSupport.constData()));
    const auto expectedPositionSupport = static_cast<ImageViewportCapabilitySupport>(
        enumValue(&ImageViewportEnums::staticMetaObject, "CapabilitySupport",
            expectedPositionSeekSupport.constData()));
    QCOMPARE(roleMetadata.frameCount(), expectedFrameCount);
    QCOMPARE(roleMetadata.totalDuration(), expectedTotalDuration);
    QCOMPARE(roleMetadata.frameSeekBounds().minimum(), expectedFrameMinimum);
    QCOMPARE(roleMetadata.frameSeekBounds().maximum(), expectedFrameMaximum);
    QCOMPARE(roleMetadata.positionSeekBounds().minimum(), expectedPositionMinimum);
    QCOMPARE(roleMetadata.positionSeekBounds().maximum(), expectedPositionMaximum);
    QCOMPARE(roleMetadata.timedPlaybackSupport(), expectedTimedSupport);
    QCOMPARE(roleMetadata.frameSeekSupport(), expectedFrameSupport);
    QCOMPARE(roleMetadata.positionSeekSupport(), expectedPositionSupport);

    if (!secondaryRole) {
        QCOMPARE(primaryFrameCount(item), expectedFrameCount);
        QCOMPARE(primaryTotalDuration(item), expectedTotalDuration);
        QCOMPARE(primaryFrameSeekBounds(item).minimum(), expectedFrameMinimum);
        QCOMPARE(primaryFrameSeekBounds(item).maximum(), expectedFrameMaximum);
        QCOMPARE(primaryPositionSeekBounds(item).minimum(), expectedPositionMinimum);
        QCOMPARE(primaryPositionSeekBounds(item).maximum(), expectedPositionMaximum);
        QCOMPARE(primaryTimedPlaybackSupport(item), expectedTimedSupport);
        QCOMPARE(primaryFrameSeekSupport(item), expectedFrameSupport);
        QCOMPARE(primaryPositionSeekSupport(item), expectedPositionSupport);
    }
}

void ImageViewportProviderMetadataTest::providerConstructionMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageViewportCapabilitySupport::True, ImageViewportCapabilitySupport::True,
        ImageViewportCapabilitySupport::True);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 1);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), 350);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
}

void ImageViewportProviderMetadataTest::
    providerFixedDurationConstructionMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100),
        ImageViewportCapabilitySupport::True, ImageViewportCapabilitySupport::True,
        ImageViewportCapabilitySupport::True);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryFrameCount(item), 3);
    QCOMPARE(primaryTotalDuration(item), 300);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 2);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), 300);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
}

void ImageViewportProviderMetadataTest::
    providerKnownConstructionMetadataSelectsInitialFrameWithoutDeclaredCapabilities()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 1);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), 350);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
}

void ImageViewportProviderMetadataTest::
    providerKnownConstructionMetadataBindsAcceptedSeekImmediately()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    const ImageViewportRevisionToken initialRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RequestQueued"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
    verifyRevisionChanged(item, "requestRevision", initialRequestRevision);
}

void ImageViewportProviderMetadataTest::providerKnownStillConstructionMetadataConstrainsCommands()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 0);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::False);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*frameRequestCount, 1);
}

void ImageViewportProviderMetadataTest::
    providerKnownConstructionMetadataRejectsSeeksPastKnownBounds()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 1);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), 350);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 2).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 351).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*closeCount, 0);
}

void ImageViewportProviderMetadataTest::providerLogicalSizeFactsWaitForRuntimeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata::withSourceLogicalSize(QSizeF(16.0, 8.0)));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), -1);
    QCOMPARE(primaryTotalDuration(item), -1);

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(primaryTotalDuration(item), -1);
}

void ImageViewportProviderMetadataTest::providerTimedFrameCountFactsProjectFrameBoundsOnly()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameCount(QSizeF(16.0, 8.0), 3),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::True,
        ImageViewportCapabilitySupport::Unavailable);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryFrameCount(item), 3);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 2);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);
}

void ImageViewportProviderMetadataTest::providerRuntimeMetadataContradictsKnownLogicalSizeFacts()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata::withSourceLogicalSize(QSizeF(16.0, 8.0)));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(8.0, 16.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("construction-time")));
}

void ImageViewportProviderMetadataTest::providerRuntimeMetadataContradictsKnownFrameCountFacts()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata::timedFrameCount(QSizeF(16.0, 8.0), 2));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250, 300 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("construction-time")));
}

void ImageViewportProviderMetadataTest::
    secondaryProviderRuntimeMetadataContradictsKnownLogicalSizeFacts()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory,
        ImageSequenceProviderMetadata::withSourceLogicalSize(QSizeF(16.0, 8.0)));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(8.0, 16.0)));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryCloseCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(secondaryFrameCount(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("construction-time")));
}

void ImageViewportProviderMetadataTest::
    secondaryProviderDeclaredCapabilityContradictionRejectsMetadata()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory,
        ImageSequenceProviderMetadata(), ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::False, ImageViewportCapabilitySupport::Unavailable);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryCloseCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(secondaryFrameCount(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("capabilities")));
}

void ImageViewportProviderMetadataTest::secondaryProviderMetadataDoesNotReviveStaleInitialTarget()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto secondarySessionCount = std::make_shared<int>(0);
    const auto secondaryMetadataRequestCount = std::make_shared<int>(0);
    const auto secondaryFrameRequestCount = std::make_shared<int>(0);
    const auto secondaryLastRequestedFrame = std::make_shared<int>(-1);
    const auto secondaryCloseCount = std::make_shared<int>(0);
    auto secondarySessionFactory = std::make_shared<CountingProviderSessionFactory>(
        secondarySessionCount, secondaryMetadataRequestCount, secondaryFrameRequestCount,
        secondaryLastRequestedFrame, secondaryCloseCount);
    CountingProviderAdapter secondaryAdapter(secondarySessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(*secondaryMetadataRequestCount, 1);
    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(secondaryRequestedFrame(item), -1);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), -1);

    QVERIFY(secondarySessionFactory->lastSession());
    emitProviderMetadataReady(secondarySessionFactory->lastSession(),
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryCloseCount, 0);
    QCOMPARE(secondaryFrameCount(item), 1);
    QCOMPARE(secondaryRequestedFrame(item), -1);
    QCOMPARE(secondaryRequestedPosition(item), -1);
    QVERIFY(viewportErrorString(item).isEmpty());
}

void ImageViewportProviderMetadataTest::
    providerCompleteDurationFactsSelectInitialFrameWithoutMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 2, 100));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 200);
}

void ImageViewportProviderMetadataTest::providerCompleteKnownFactsSelectInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);
}

void ImageViewportProviderMetadataTest::providerMetadataLoadingPauseStopPreserveInitialRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        playbackRequestCount, lastPlaybackFrame, lastPlaybackPosition, cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(*playbackRequestCount, 0);
    QCOMPARE(*lastPlaybackFrame, -1);
    QCOMPARE(*lastPlaybackPosition, -1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(
        item.stop(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(*playbackRequestCount, 0);
    QCOMPARE(*lastPlaybackFrame, -1);
    QCOMPARE(*lastPlaybackPosition, -1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
}

void ImageViewportProviderMetadataTest::providerDeclaredCapabilityProjectsBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata(), ImageViewportCapabilitySupport::False);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryFrameCount(item), -1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);
}

void ImageViewportProviderMetadataTest::providerDeclaredTrueCapabilityProjectsBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageViewportCapabilitySupport::True, ImageViewportCapabilitySupport::True,
        ImageViewportCapabilitySupport::True);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryFrameCount(item), -1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
}

void ImageViewportProviderMetadataTest::providerKnownCapabilityProjectsBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageViewportCapabilitySupport::True, ImageViewportCapabilitySupport::False,
        ImageViewportCapabilitySupport::True);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
}

void ImageViewportProviderMetadataTest::providerDeclaredCapabilityContradictionRejectsMetadata()
{
    const auto verifyRejectedMetadata
        = [](const ImageSequenceProviderMetadata& metadata,
              ImageViewportCapabilitySupport timedPlaybackSupport,
              ImageViewportCapabilitySupport frameSeekSupport,
              ImageViewportCapabilitySupport positionSeekSupport,
              ImageViewportCapabilitySupport (*projectedSupport)(const ImageViewport&)) {
              ImageSequenceFactory factory;
              const auto sessionCount = std::make_shared<int>(0);
              const auto metadataRequestCount = std::make_shared<int>(0);
              const auto frameRequestCount = std::make_shared<int>(0);
              const auto lastRequestedFrame = std::make_shared<int>(-1);
              const auto closeCount = std::make_shared<int>(0);
              auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
                  metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
              CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
                  timedPlaybackSupport, frameSeekSupport, positionSeekSupport);
              QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
              QVERIFY(result->sequence());

              ImageViewport item;
              item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                  PresentationTargetTransitionPolicy {});
              const QMetaObject* metaObject = item.metaObject();

              QVERIFY(sessionFactory->lastSession());
              emitProviderMetadataReady(sessionFactory->lastSession(),
                  sessionFactory->lastSession()->lastMetadataToken(), metadata);
              drainQueuedProviderResults();

              QCOMPARE(*frameRequestCount, 0);
              QCOMPARE(*closeCount, 1);
              QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
              QCOMPARE(requestReasonValue(item),
                  enumValue(metaObject, "RequestReason", "PayloadRejection"));
              QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider metadata")));
              QCOMPARE(projectedSupport(item), ImageViewportCapabilitySupport::False);
          };

    verifyRejectedMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageViewportCapabilitySupport::False, ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::Unavailable, primaryTimedPlaybackSupport);
    verifyRejectedMetadata(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::False,
        ImageViewportCapabilitySupport::Unavailable, primaryFrameSeekSupport);
    verifyRejectedMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::False, primaryPositionSeekSupport);
}

void ImageViewportProviderMetadataTest::providerDeclaredTrueCapabilityContradictionRejectsMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageViewportCapabilitySupport::True, ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::True);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider metadata")));
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
}

void ImageViewportProviderMetadataTest::providerRuntimeMetadataCapabilitiesOverrideTimingInference()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    const auto positionRequestCount = std::make_shared<int>(0);
    const auto lastPositionFrame = std::make_shared<int>(-1);
    const auto lastRequestedPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount,
        playbackRequestCount, lastPlaybackFrame, lastPlaybackPosition, std::shared_ptr<int>(),
        std::shared_ptr<ImageSequenceProviderRequestToken>(), positionRequestCount,
        lastPositionFrame, lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    auto metadata = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    metadata.setTimedPlaybackSupport(ImageViewportCapabilitySupport::False);
    metadata.setPositionSeekSupport(ImageViewportCapabilitySupport::False);
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(), metadata);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(*playbackRequestCount, 0);
    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 100).outcome(),
        ImageViewportCommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(*positionRequestCount, 0);
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
}

void ImageViewportProviderMetadataTest::providerDeclaredNoPlaybackRejectsPlayBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata(), ImageViewportCapabilitySupport::False);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Unsupported);

    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportProviderMetadataTest::providerKnownNoPlaybackRejectsPlayBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(
        sessionFactory, ImageSequenceProviderMetadata(), ImageViewportCapabilitySupport::False);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Unsupported);

    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportProviderMetadataTest::providerDeclaredNoFrameSeekRejectsSeekBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::False);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 2).outcome(),
        ImageViewportCommandOutcome::Unsupported);

    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportProviderMetadataTest::
    providerDeclaredNoPositionSeekRejectsPositionSeekBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport::False);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 250).outcome(),
        ImageViewportCommandOutcome::Unsupported);

    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportProviderMetadataTest::providerMetadataRejectsNonFiniteLogicalSize()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(std::numeric_limits<double>::infinity(), 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("provider metadata is invalid")));
}

void ImageViewportProviderMetadataTest::providerMetadataRejectsHugeFiniteLogicalSize()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(1.0e20, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("maximumSourceLogicalWidth")));
}

void ImageViewportProviderMetadataTest::providerMetadataRejectsPublishedFrameCountLimit()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVector<int> durations(ImageSequenceLimits::maximumFrameCount() + 1, 1);
    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), durations));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QVERIFY(viewportErrorString(item).contains(QStringLiteral("maximumFrameCount")));
}

void ImageViewportProviderMetadataTest::providerMetadataRejectsPublishedDurationLimits()
{
    auto verifyRejectedDurations
        = [](const QVector<int>& durations, const QString& expectedDiagnostic) {
              ImageSequenceFactory factory;
              const auto sessionCount = std::make_shared<int>(0);
              const auto metadataRequestCount = std::make_shared<int>(0);
              const auto frameRequestCount = std::make_shared<int>(0);
              const auto lastRequestedFrame = std::make_shared<int>(-1);
              const auto closeCount = std::make_shared<int>(0);
              auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
                  metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
              CountingProviderAdapter adapter(sessionFactory);
              QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
              QVERIFY(result->sequence());

              ImageViewport item;
              item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                  PresentationTargetTransitionPolicy {});
              const QMetaObject* metaObject = item.metaObject();

              QVERIFY(sessionFactory->lastSession());
              emitProviderMetadataReady(sessionFactory->lastSession(),
                  sessionFactory->lastSession()->lastMetadataToken(),
                  ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), durations));
              drainQueuedProviderResults();

              QCOMPARE(*frameRequestCount, 0);
              QCOMPARE(*closeCount, 1);
              QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
              QCOMPARE(requestReasonValue(item),
                  enumValue(metaObject, "RequestReason", "PayloadRejection"));
              QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
              QCOMPARE(primaryRequestedFrame(item), -1);
              QCOMPARE(primaryDisplayedFrame(item), -1);
              QVERIFY(viewportErrorString(item).contains(expectedDiagnostic));
          };

    verifyRejectedDurations({}, QStringLiteral("provider metadata is invalid"));
    verifyRejectedDurations({ 0 }, QStringLiteral("positive"));
    verifyRejectedDurations({ 100, -1 }, QStringLiteral("positive"));
    verifyRejectedDurations({ ImageSequenceLimits::maximumFrameDurationMilliseconds() + 1 },
        QStringLiteral("maximumFrameDurationMilliseconds"));
    verifyRejectedDurations({ ImageSequenceLimits::maximumTotalDurationMilliseconds(), 1 },
        QStringLiteral("maximumTotalDurationMilliseconds"));
}

void ImageViewportProviderMetadataTest::providerStillMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    QCOMPARE(*frameRequestCount, 0);
    const ImageViewportRevisionToken metadataWaitingRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken metadataWaitingDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken unresolvedTargetPresentationRevision
        = item.state().display().targetPresentationRevision();
    QVERIFY(unresolvedTargetPresentationRevision.isValid());
    QVERIFY(!item.state().display().displayedPresentationRevision().isValid());
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    const ImageSequenceProviderMetadata metadata
        = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(), metadata);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 0);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::False);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::False);
    verifyRevisionChanged(item, "requestRevision", metadataWaitingRequestRevision);
    verifyRevisionChanged(item, "displayRevision", metadataWaitingDisplayRevision);
    QVERIFY(item.state().display().targetPresentationRevision().isValid());
    QVERIFY(item.state().display().targetPresentationRevision()
        != unresolvedTargetPresentationRevision);
    QVERIFY(!item.state().display().displayedPresentationRevision().isValid());
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportProviderMetadataTest::providerTimedMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageViewportRevisionToken metadataWaitingRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    const ImageSequenceProviderMetadata metadata
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(), metadata);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 1);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), 350);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
    verifyRevisionChanged(item, "requestRevision", metadataWaitingRequestRevision);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportProviderMetadataTest::providerFixedDurationMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryFrameCount(item), 3);
    QCOMPARE(primaryTotalDuration(item), 300);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 2);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), 300);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 250).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RequestQueued"));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 2);
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryRequestedFrame(item), 2);
    QCOMPARE(primaryRequestedPosition(item), 250);
}

void ImageViewportProviderMetadataTest::providerProgressResultsAreAdvisory()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    for (int index = 0; index < 1024; ++index) {
        emitProviderProgress(sessionFactory->lastSession(), metadataToken, 0.5);
    }
    drainQueuedProviderResults();
    emitProviderWaiting(sessionFactory->lastSession(), metadataToken);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(stateSpy.count(), 0);

    emitProviderMetadataReady(sessionFactory->lastSession(), metadataToken,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(stateSpy.count(), 1);

    emitProviderProgress(sessionFactory->lastSession(), metadataToken, 1.0);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportProviderMetadataTest::providerInvalidProgressResultsAreIgnored()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    emitProviderProgress(sessionFactory->lastSession(), metadataToken, -0.1);
    drainQueuedProviderResults();
    emitProviderProgress(sessionFactory->lastSession(), metadataToken, 1.1);
    drainQueuedProviderResults();
    emitProviderProgress(
        sessionFactory->lastSession(), metadataToken, std::numeric_limits<double>::quiet_NaN());
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(stateSpy.count(), 0);

    emitProviderMetadataReady(sessionFactory->lastSession(), metadataToken,
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*closeCount, 0);
}

void ImageViewportProviderMetadataTest::providerTerminalResultDominatesProgress()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    for (int index = 0; index < 1024; ++index) {
        emitProviderProgress(sessionFactory->lastSession(), metadataToken, 0.5);
    }
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(*closeCount, 0);

    emitProviderFailed(sessionFactory->lastSession(), metadataToken,
        QStringLiteral("metadata failed after progress"));
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    verifyUntrustedProviderDiagnostic(item, QStringLiteral("metadata failed after progress"));
}

void ImageViewportProviderMetadataTest::providerFrameReadyDominatesLateProgress()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(*closeCount, 0);
    QVERIFY(hasPendingRenderCommitForTest(item));

    const ImageViewportRevisionToken renderWaitingRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    emitProviderProgress(sessionFactory->lastSession(), frameToken, 0.75);
    drainQueuedProviderResults();
    emitProviderWaiting(sessionFactory->lastSession(), frameToken);
    drainQueuedProviderResults();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), renderWaitingRequestRevision);
    QCOMPARE(viewportErrorString(item), QString());
}

void ImageViewportProviderMetadataTest::
    providerPositiveResizeWhileMetadataWaitingKeepsProviderWaiting()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(0.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    const ImageViewportRevisionToken metadataWaitingRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), metadataWaitingRequestRevision);
}

void ImageViewportProviderMetadataTest::providerMetadataReadySealsMetadataToken()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    emitProviderMetadataReady(sessionFactory->lastSession(), metadataToken,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    emitProviderFailed(
        sessionFactory->lastSession(), metadataToken, QStringLiteral("late metadata failure"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(viewportErrorString(item), QString());
}

QTEST_MAIN(ImageViewportProviderMetadataTest)

#include "tst_imageviewport_provider_metadata.moc"
