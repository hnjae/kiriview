#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QStringList>

namespace {

enum MetadataProjectionScenario {
    UnknownProviderFacts,
    PartialKnownFacts,
    CompleteStillFacts,
    CompleteTimedFacts,
    RuntimeStillMetadata,
    RuntimeTimedMetadata,
};

struct RoleMetadataProperties {
    const char* frameCount;
    const char* totalDuration;
    const char* frameSeekBounds;
    const char* positionSeekBounds;
    const char* timedPlaybackSupport;
    const char* frameSeekSupport;
    const char* positionSeekSupport;
};

RoleMetadataProperties metadataPropertiesForRole(bool secondaryRole)
{
    if (secondaryRole) {
        return {
            "secondaryFrameCount",
            "secondaryTotalDuration",
            "secondaryFrameSeekBounds",
            "secondaryPositionSeekBounds",
            "secondaryTimedPlaybackSupport",
            "secondaryFrameSeekSupport",
            "secondaryPositionSeekSupport",
        };
    }
    return {
        "primaryFrameCount",
        "primaryTotalDuration",
        "primaryFrameSeekBounds",
        "primaryPositionSeekBounds",
        "primaryTimedPlaybackSupport",
        "primaryFrameSeekSupport",
        "primaryPositionSeekSupport",
    };
}

void verifyImageViewportMetadataProjectionIsControllerForwarding()
{
    QFile sourceFile(QStringLiteral(IMAGEVIEWPORT_SOURCE_DIR "/src/imageviewport.cpp"));
    QVERIFY2(sourceFile.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(QStringLiteral("could not open %1").arg(sourceFile.fileName())));

    const QString source = QString::fromUtf8(sourceFile.readAll());
    const QStringList forbiddenTokens {
        QStringLiteral("m_hasCompleteProviderKnownMetadata"),
        QStringLiteral("m_providerKnown"),
        QStringLiteral("m_providerTimedPlaybackCapability"),
        QStringLiteral("m_providerFrameSeekCapability"),
        QStringLiteral("m_providerPositionSeekCapability"),
        QStringLiteral("providerCapabilityKnownTrue("),
        QStringLiteral("providerResolvedCapability("),
        QStringLiteral("capabilitySupportToTriState("),
    };
    for (const QString& token : forbiddenTokens) {
        QVERIFY2(!source.contains(token),
            qPrintable(QStringLiteral("ImageViewportPrivate metadata projection still uses %1")
                           .arg(token)));
    }
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

private slots:
    void roleScopedMetadataProjectionUsesOnePath_data();
    void roleScopedMetadataProjectionUsesOnePath();
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
    void secondaryProviderMetadataDoesNotReviveSupersededInitialTarget();
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

    const auto addRow = [](const char* name, bool secondaryRole,
                            MetadataProjectionScenario scenario, int expectedFrameCount,
                            int expectedTotalDuration, int expectedFrameMinimum,
                            int expectedFrameMaximum, int expectedPositionMinimum,
                            int expectedPositionMaximum, const char* expectedTimedPlaybackSupport,
                            const char* expectedFrameSeekSupport,
                            const char* expectedPositionSeekSupport) {
        QTest::newRow(name) << secondaryRole << static_cast<int>(scenario) << expectedFrameCount
                            << expectedTotalDuration << expectedFrameMinimum
                            << expectedFrameMaximum << expectedPositionMinimum
                            << expectedPositionMaximum << QByteArray(expectedTimedPlaybackSupport)
                            << QByteArray(expectedFrameSeekSupport)
                            << QByteArray(expectedPositionSeekSupport);
    };

    addRow("primary-unknown-provider-facts", false, UnknownProviderFacts, -1, -1, -1, -1, -1,
        -1, "Unavailable", "Unavailable", "Unavailable");
    addRow("secondary-unknown-provider-facts", true, UnknownProviderFacts, -1, -1, -1, -1, -1,
        -1, "Unavailable", "Unavailable", "Unavailable");
    addRow("primary-partial-known-facts", false, PartialKnownFacts, 3, -1, 0, 2, -1, -1,
        "Unavailable", "True", "Unavailable");
    addRow("secondary-partial-known-facts", true, PartialKnownFacts, 3, -1, 0, 2, -1, -1,
        "Unavailable", "True", "Unavailable");
    addRow("primary-complete-still-facts", false, CompleteStillFacts, 1, -1, 0, 0, -1, -1,
        "False", "True", "False");
    addRow("secondary-complete-still-facts", true, CompleteStillFacts, 1, -1, 0, 0, -1, -1,
        "False", "True", "False");
    addRow("primary-complete-timed-facts", false, CompleteTimedFacts, 2, 350, 0, 1, 0, 350,
        "True", "True", "True");
    addRow("secondary-complete-timed-facts", true, CompleteTimedFacts, 2, 350, 0, 1, 0, 350,
        "True", "True", "True");
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

    if (!secondaryRole && scenario == UnknownProviderFacts) {
        verifyImageViewportMetadataProjectionIsControllerForwarding();
    }

    ImageSequenceProviderKnownFacts knownFacts;
    ImageSequenceProviderAdapter::CapabilitySupport timedPlaybackSupport
        = ImageSequenceProviderAdapter::CapabilitySupport::Unavailable;
    ImageSequenceProviderAdapter::CapabilitySupport frameSeekSupport
        = ImageSequenceProviderAdapter::CapabilitySupport::Unavailable;
    ImageSequenceProviderAdapter::CapabilitySupport positionSeekSupport
        = ImageSequenceProviderAdapter::CapabilitySupport::Unavailable;

    switch (static_cast<MetadataProjectionScenario>(scenario)) {
    case UnknownProviderFacts:
    case RuntimeStillMetadata:
    case RuntimeTimedMetadata:
        break;
    case PartialKnownFacts:
        knownFacts = ImageSequenceProviderKnownFacts::timedFrameCount(QSizeF(16.0, 8.0), 3);
        frameSeekSupport = ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue;
        break;
    case CompleteStillFacts:
        knownFacts = ImageSequenceProviderKnownFacts::still(QSizeF(16.0, 8.0));
        break;
    case CompleteTimedFacts:
        knownFacts
            = ImageSequenceProviderKnownFacts::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
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
        QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                     QVariant::fromValue<QObject*>(providerResult->sequence())),
            ImageViewport::CommandOutcome::Accepted);
    } else {
        item.setSequence(providerResult->sequence());
    }

    switch (static_cast<MetadataProjectionScenario>(scenario)) {
    case RuntimeStillMetadata:
        QVERIFY(sessionFactory->lastSession());
        emit sessionFactory->lastSession()->metadataReady(
            sessionFactory->lastSession()->lastMetadataToken(),
            ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        drainQueuedProviderResults();
        break;
    case RuntimeTimedMetadata:
        QVERIFY(sessionFactory->lastSession());
        emit sessionFactory->lastSession()->metadataReady(
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

    const RoleMetadataProperties properties = metadataPropertiesForRole(secondaryRole);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(item.property(properties.frameCount).toInt(), expectedFrameCount);
    QCOMPARE(item.property(properties.totalDuration).toInt(), expectedTotalDuration);
    QCOMPARE(rangeProperty(item, properties.frameSeekBounds).minimum(), expectedFrameMinimum);
    QCOMPARE(rangeProperty(item, properties.frameSeekBounds).maximum(), expectedFrameMaximum);
    QCOMPARE(
        rangeProperty(item, properties.positionSeekBounds).minimum(), expectedPositionMinimum);
    QCOMPARE(
        rangeProperty(item, properties.positionSeekBounds).maximum(), expectedPositionMaximum);
    QCOMPARE(item.property(properties.timedPlaybackSupport).toInt(),
        enumValue(metaObject, "TriState", expectedTimedPlaybackSupport.constData()));
    QCOMPARE(item.property(properties.frameSeekSupport).toInt(),
        enumValue(metaObject, "TriState", expectedFrameSeekSupport.constData()));
    QCOMPARE(item.property(properties.positionSeekSupport).toInt(),
        enumValue(metaObject, "TriState", expectedPositionSeekSupport.constData()));

    if (!secondaryRole) {
        QCOMPARE(item.property("frameCount").toInt(), expectedFrameCount);
        QCOMPARE(item.property("totalDuration").toInt(), expectedTotalDuration);
        QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), expectedFrameMinimum);
        QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), expectedFrameMaximum);
        QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), expectedPositionMinimum);
        QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), expectedPositionMaximum);
        QCOMPARE(item.property("timedPlaybackSupport").toInt(),
            enumValue(metaObject, "TriState", expectedTimedPlaybackSupport.constData()));
        QCOMPARE(item.property("frameSeekSupport").toInt(),
            enumValue(metaObject, "TriState", expectedFrameSeekSupport.constData()));
        QCOMPARE(item.property("positionSeekSupport").toInt(),
            enumValue(metaObject, "TriState", expectedPositionSeekSupport.constData()));
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
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), 350);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
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
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 3);
    QCOMPARE(item.property("totalDuration").toInt(), 300);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 2);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), 300);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), 350);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    const RevisionToken initialRequestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RequestQueued"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 0);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), -1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), 350);

    QCOMPARE(item.seek(2), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.seekToPosition(351), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
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
        sessionFactory, ImageSequenceProviderKnownFacts::logicalSize(QSizeF(16.0, 8.0)));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
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
        ImageSequenceProviderKnownFacts::timedFrameCount(QSizeF(16.0, 8.0), 3),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 3);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 2);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), -1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), -1);
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
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
        sessionFactory, ImageSequenceProviderKnownFacts::logicalSize(QSizeF(16.0, 8.0)));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(8.0, 16.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("construction-time")));
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
        sessionFactory, ImageSequenceProviderKnownFacts::timedFrameCount(QSizeF(16.0, 8.0), 2));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250, 300 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("construction-time")));
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
        ImageSequenceProviderKnownFacts::logicalSize(QSizeF(16.0, 8.0)));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(8.0, 16.0)));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryCloseCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("secondaryFrameCount").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("construction-time")));
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
        ImageSequenceProviderMetadata(),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromProvider(&secondaryAdapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(secondarySessionFactory->lastSession());
    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryCloseCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("secondaryFrameCount").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("capabilities")));
}

void ImageViewportProviderMetadataTest::
    secondaryProviderMetadataDoesNotReviveSupersededInitialTarget()
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*secondaryMetadataRequestCount, 1);
    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), -1);

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), -1);

    QVERIFY(secondarySessionFactory->lastSession());
    emit secondarySessionFactory->lastSession()->metadataReady(
        secondarySessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*secondaryFrameRequestCount, 0);
    QCOMPARE(*secondaryCloseCount, 0);
    QCOMPARE(item.property("secondaryFrameCount").toInt(), 1);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), -1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString").toString().isEmpty());
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
        ImageSequenceProviderKnownFacts::fixedDurationFrames(QSizeF(16.0, 8.0), 2, 100));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 200);
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
        ImageSequenceProviderKnownFacts::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
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
    item.setSequence(result->sequence());
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
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);

    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(*playbackRequestCount, 0);
    QCOMPARE(*lastPlaybackFrame, -1);
    QCOMPARE(*lastPlaybackPosition, -1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(playbackSpy.count(), 0);
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
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
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
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
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
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
}

void ImageViewportProviderMetadataTest::providerDeclaredCapabilityContradictionRejectsMetadata()
{
    const auto verifyRejectedMetadata
        = [](const ImageSequenceProviderMetadata& metadata,
              ImageSequenceProviderAdapter::CapabilitySupport timedPlaybackSupport,
              ImageSequenceProviderAdapter::CapabilitySupport frameSeekSupport,
              ImageSequenceProviderAdapter::CapabilitySupport positionSeekSupport,
              const char* projectedProperty) {
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
              item.setSequence(result->sequence());
              const QMetaObject* metaObject = item.metaObject();

              QVERIFY(sessionFactory->lastSession());
              emit sessionFactory->lastSession()->metadataReady(
                  sessionFactory->lastSession()->lastMetadataToken(), metadata);
              drainQueuedProviderResults();

              QCOMPARE(*frameRequestCount, 0);
              QCOMPARE(*closeCount, 1);
              QCOMPARE(item.property("requestStatus").toInt(),
                  enumValue(metaObject, "RequestStatus", "Error"));
              QCOMPARE(item.property("requestReason").toInt(),
                  enumValue(metaObject, "RequestReason", "PayloadRejection"));
              QVERIFY(item.property("errorString")
                      .toString()
                      .contains(QStringLiteral("provider metadata")));
              QCOMPARE(item.property(projectedProperty).toInt(),
                  enumValue(metaObject, "TriState", "False"));
          };

    verifyRejectedMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable, "timedPlaybackSupport");
    verifyRejectedMetadata(ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable, "frameSeekSupport");
    verifyRejectedMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }),
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse, "positionSeekSupport");
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
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredTrue);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("provider metadata")));
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
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
        std::shared_ptr<quint64>(), positionRequestCount, lastPositionFrame, lastRequestedPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    auto metadata = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    metadata.setTimedPlaybackSupport(false);
    metadata.setPositionSeekSupport(false);
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(), metadata);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(*playbackRequestCount, 0);
    QCOMPARE(item.seekToPosition(100), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(*positionRequestCount, 0);
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
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
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
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
    CountingProviderAdapter adapter(sessionFactory, ImageSequenceProviderMetadata(),
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
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
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(2), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
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
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(250), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(std::numeric_limits<double>::infinity(), 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("provider metadata is invalid")));
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(1.0e20, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(
        item.property("errorString").toString().contains(QStringLiteral("maximumLogicalWidth")));
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVector<int> durations(ImageSequenceLimits::maximumTimedListFrameCount() + 1, 1);
    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), durations));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("maximumTimedListFrameCount")));
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
              item.setSequence(result->sequence());
              const QMetaObject* metaObject = item.metaObject();

              QVERIFY(sessionFactory->lastSession());
              emit sessionFactory->lastSession()->metadataReady(
                  sessionFactory->lastSession()->lastMetadataToken(),
                  ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), durations));
              drainQueuedProviderResults();

              QCOMPARE(*frameRequestCount, 0);
              QCOMPARE(*closeCount, 1);
              QCOMPARE(item.property("requestStatus").toInt(),
                  enumValue(metaObject, "RequestStatus", "Error"));
              QCOMPARE(item.property("requestReason").toInt(),
                  enumValue(metaObject, "RequestReason", "PayloadRejection"));
              QCOMPARE(item.property("displayStatus").toInt(),
                  enumValue(metaObject, "DisplayStatus", "Empty"));
              QCOMPARE(item.property("requestedFrame").toInt(), -1);
              QCOMPARE(item.property("displayedFrame").toInt(), -1);
              QVERIFY(item.property("errorString").toString().contains(expectedDiagnostic));
          };

    verifyRejectedDurations({}, QStringLiteral("provider metadata is invalid"));
    verifyRejectedDurations({ 0 }, QStringLiteral("positive"));
    verifyRejectedDurations({ 100, -1 }, QStringLiteral("positive"));
    verifyRejectedDurations({ ImageSequenceLimits::maximumFrameDuration() + 1 },
        QStringLiteral("maximumFrameDuration"));
    verifyRejectedDurations({ ImageSequenceLimits::maximumTotalSequenceDuration(), 1 },
        QStringLiteral("maximumTotalSequenceDuration"));
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    QCOMPARE(*frameRequestCount, 0);
    const RevisionToken metadataWaitingRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);

    const ImageSequenceProviderMetadata metadata
        = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(), metadata);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 0);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), -1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    verifyRevisionChanged(item, "requestRevision", metadataWaitingRequestRevision);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestRevisionSpy.count(), 1);
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const RevisionToken metadataWaitingRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);

    const ImageSequenceProviderMetadata metadata
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(), metadata);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), 350);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    verifyRevisionChanged(item, "requestRevision", metadataWaitingRequestRevision);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestRevisionSpy.count(), 1);
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 3);
    QCOMPARE(item.property("totalDuration").toInt(), 300);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 2);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), 300);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));

    QCOMPARE(item.seekToPosition(250), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RequestQueued"));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 2);
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 2);
    QCOMPARE(item.property("requestedPosition").toInt(), 250);
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);

    emit sessionFactory->lastSession()->providerProgress(metadataToken, 0.5);
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerWaiting(metadataToken);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(requestRevisionSpy.count(), 0);

    emit sessionFactory->lastSession()->metadataReady(metadataToken,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestRevisionSpy.count(), 1);

    emit sessionFactory->lastSession()->providerProgress(metadataToken, 1.0);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestRevisionSpy.count(), 1);
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);

    emit sessionFactory->lastSession()->providerProgress(metadataToken, -0.1);
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerProgress(metadataToken, 1.1);
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerProgress(
        metadataToken, std::numeric_limits<double>::quiet_NaN());
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerProgress(ImageSequenceProviderRequestToken(), 0.5);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(requestRevisionSpy.count(), 0);
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    emit sessionFactory->lastSession()->providerProgress(metadataToken, 0.5);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(*closeCount, 0);

    emit sessionFactory->lastSession()->providerFailed(
        metadataToken, QStringLiteral("metadata failed after progress"));
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QVERIFY(item.property("errorString")
            .toString()
            .contains(QStringLiteral("metadata failed after progress")));
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    const ImageSequenceProviderRequestToken frameToken
        = sessionFactory->lastSession()->lastFrameToken();
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->imageFrameReady(frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(*closeCount, 0);
    QVERIFY(item.hasPendingRenderCommitForTest());

    const RevisionToken renderWaitingRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    emit sessionFactory->lastSession()->providerProgress(frameToken, 0.75);
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerWaiting(frameToken);
    drainQueuedProviderResults();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), renderWaitingRequestRevision);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    const RevisionToken metadataWaitingRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), metadataWaitingRequestRevision);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(requestRevisionSpy.count(), 0);
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
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken
        = sessionFactory->lastSession()->lastMetadataToken();
    emit sessionFactory->lastSession()->metadataReady(metadataToken,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    emit sessionFactory->lastSession()->providerFailed(
        metadataToken, QStringLiteral("late metadata failure"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("errorString").toString(), QString());
}

QTEST_MAIN(ImageViewportProviderMetadataTest)

#include "tst_imageviewport_provider_metadata.moc"
