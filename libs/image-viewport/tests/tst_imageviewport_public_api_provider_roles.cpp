// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

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

private Q_SLOTS:
    void secondaryProviderPresentationTargetOpensRoleLocalSessionAndWaits();
    void secondaryProviderMetadataUpdatesRoleObservations();
};

void ImageViewportPublicApiProviderRolesTest::
    secondaryProviderPresentationTargetOpensRoleLocalSessionAndWaits()
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
    const auto outcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(item.metaObject(), "RequestStatus", "Loading"));
    QCOMPARE(
        requestReasonValue(item), enumValue(item.metaObject(), "RequestReason", "ProviderWaiting"));
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QVERIFY(sessionFactory->lastSession());

    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(10.0, 20.0)));
    drainQueuedProviderResults();

    QCOMPARE(secondaryFrameCount(item), 1);
    QCOMPARE(secondaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(secondaryFrameSeekBounds(item).maximum(), 0);
    QCOMPARE(secondaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(secondaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::False);
}
QTEST_MAIN(ImageViewportPublicApiProviderRolesTest)

#include "tst_imageviewport_public_api_provider_roles.moc"
