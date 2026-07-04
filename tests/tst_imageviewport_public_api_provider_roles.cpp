#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

class ImageViewportPublicApiProviderRolesTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportPublicApiProviderRolesTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void secondaryProviderPageSetOpensRoleLocalSessionAndWaits();
    void secondaryProviderMetadataUpdatesRoleObservations();
};

void ImageViewportPublicApiProviderRolesTest::
    secondaryProviderPageSetOpensRoleLocalSessionAndWaits()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromProvider(&adapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    const auto outcome = item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
        QVariant::fromValue<QObject*>(secondaryResult->sequence()));

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("primarySequence").value<ImageSequence*>(), primaryResult->sequence());
    QCOMPARE(
        item.property("secondarySequence").value<ImageSequence*>(), secondaryResult->sequence());
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(item.metaObject(), "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(item.metaObject(), "RequestReason", "ProviderWaiting"));
}

void ImageViewportPublicApiProviderRolesTest::secondaryProviderMetadataUpdatesRoleObservations()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromProvider(&adapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    QVERIFY(sessionFactory->lastSession());

    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(10.0, 20.0)));
    drainQueuedProviderResults();

    QCOMPARE(item.property("secondaryFrameCount").toInt(), 1);
    QCOMPARE(rangeProperty(item, "secondaryFrameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "secondaryFrameSeekBounds").maximum(), 0);
    QCOMPARE(item.property("secondaryFrameSeekSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "True"));
    QCOMPARE(item.property("secondaryTimedPlaybackSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "False"));
}
QTEST_MAIN(ImageViewportPublicApiProviderRolesTest)

#include "tst_imageviewport_public_api_provider_roles.moc"
