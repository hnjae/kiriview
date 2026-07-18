// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/ImageViewport>

#include <QtTest/QTest>

#include <memory>

class SourceIngressContractSession final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderSession
{
public:
    using ImageSequenceProviderSession::ImageSequenceProviderSession;

    void request(const ImageSequenceProviderRequest&) override { }
};

class SourceIngressContractAdapter final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderAdapter
{
public:
    explicit SourceIngressContractAdapter(
        std::shared_ptr<int> invocationCount, QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_invocationCount(std::move(invocationCount))
    {
    }

    ImageSequenceProviderDescriptor descriptor() const override
    {
        const auto invocationCount = m_invocationCount;
        return ImageSequenceProviderDescriptor(ImageSequenceProviderMetadata::still(QSizeF(4, 2)),
            ImageSequenceProviderThreadingContract::AffinityBound, [invocationCount]() {
                ++*invocationCount;
                return ImageSequenceProviderSessionFactoryResult::created(
                    new SourceIngressContractSession);
            });
    }

private:
    std::shared_ptr<int> m_invocationCount;
};

class ReusingSessionAdapter final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderAdapter
{
public:
    explicit ReusingSessionAdapter(QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
    {
    }

    ImageSequenceProviderDescriptor descriptor() const override
    {
        const auto session = m_session;
        return ImageSequenceProviderDescriptor(ImageSequenceProviderMetadata::still(QSizeF(4, 2)),
            ImageSequenceProviderThreadingContract::AffinityBound,
            [session]() { return ImageSequenceProviderSessionFactoryResult::created(session); });
    }

private:
    SourceIngressContractSession* m_session = new SourceIngressContractSession;
};

class FailingSessionAdapter final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderAdapter
{
public:
    explicit FailingSessionAdapter(QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
    {
    }

    ImageSequenceProviderDescriptor descriptor() const override
    {
        return ImageSequenceProviderDescriptor(ImageSequenceProviderMetadata::still(QSizeF(4, 2)),
            ImageSequenceProviderThreadingContract::AffinityBound,
            []() { return ImageSequenceProviderSessionFactoryResult::failed(); });
    }
};

class SourceIngressContractTest : public QObject
{
    Q_OBJECT

public:
    explicit SourceIngressContractTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void factoryUsesCanonicalOutcomeAndReason();
    void frameOwnsReusablePayloadFacts();
    void timedFramesCarryContiguousIntervals();
    void providerFactoryIsDeferredUntilGenerationAcceptance();
    void providerSessionCannotBelongToTwoLiveGenerations();
    void providerSessionFactoryFailureIsGenerationScoped();
};

void SourceIngressContractTest::factoryUsesCanonicalOutcomeAndReason()
{
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(nullptr));

    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QCOMPARE(result->reason(), ImageSequenceFactoryReason::InvalidFrame);
    QCOMPARE(result->sequence(), nullptr);
    QVERIFY(!result->errorString().isEmpty());
    QVERIFY(result->metaObject()->indexOfProperty("warningString") < 0);
}

void SourceIngressContractTest::frameOwnsReusablePayloadFacts()
{
    QImage image(2, 1, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    ImageFrame frame(image, QSizeF(4, 2), QSizeF(2, 1), QSizeF(0.5, 0.5), image.sizeInBytes(),
        ImageViewportPayloadQuality::Preview, ImageViewportPayloadExactness::NotExact, true,
        ImageFrame::OrientationPolicy::Identity, QStringLiteral("preview/argb32"));

    QCOMPARE(frame.sourceLogicalSize(), QSizeF(4, 2));
    QCOMPARE(frame.payloadRasterSize(), QSizeF(2, 1));
    QCOMPARE(frame.sourceToPayloadScale(), QSizeF(0.5, 0.5));
    QCOMPARE(frame.quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(frame.exactness(), ImageViewportPayloadExactness::NotExact);
    QCOMPARE(frame.formatIdentifier(), QStringLiteral("preview/argb32"));

    ImageFrame inaccurateAlpha(image, QSizeF(4, 2), QSizeF(2, 1), QSizeF(0.5, 0.5),
        image.sizeInBytes(), ImageViewportPayloadQuality::Preview,
        ImageViewportPayloadExactness::NotExact, false, ImageFrame::OrientationPolicy::Identity,
        {});
    QVERIFY(!inaccurateAlpha.isValid());

    ImageFrame unboundedFormat(image, QSizeF(4, 2), QSizeF(2, 1), QSizeF(0.5, 0.5),
        image.sizeInBytes(), ImageViewportPayloadQuality::Preview,
        ImageViewportPayloadExactness::NotExact, true, ImageFrame::OrientationPolicy::Identity,
        QString(ImageSequenceLimits::maximumFormatIdentifierCharacters() + 1, QLatin1Char('x')));
    QVERIFY(unboundedFormat.isValid());
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> formatResult(factory.fromFrame(&unboundedFormat));
    QCOMPARE(formatResult->reason(), ImageSequenceFactoryReason::LimitExceeded);

    QImage oversizedRaster(ImageSequenceLimits::maximumPayloadRasterWidth() + 1, 1,
        QImage::Format_ARGB32_Premultiplied);
    oversizedRaster.fill(Qt::transparent);
    ImageFrame rasterFrame(oversizedRaster, QSizeF(1, 1), QSizeF(oversizedRaster.size()),
        QSizeF(oversizedRaster.width(), 1), oversizedRaster.sizeInBytes(),
        ImageViewportPayloadQuality::Preview, ImageViewportPayloadExactness::NotExact, true,
        ImageFrame::OrientationPolicy::Identity, {});
    QScopedPointer<ImageSequenceFactoryResult> rasterResult(factory.fromFrame(&rasterFrame));
    QCOMPARE(rasterResult->reason(), ImageSequenceFactoryReason::LimitExceeded);

    ImageSequenceProviderFrameEnvelope envelope;
    envelope.setFrame(0);
    envelope.setFrameStartPosition(-1);
    envelope.setFrameDuration(-1);
    QVERIFY(
        ImageSequenceProviderFrameEnvelope::staticMetaObject.indexOfProperty("sourceLogicalSize")
        < 0);
    QVERIFY(
        ImageSequenceProviderFrameEnvelope::staticMetaObject.indexOfProperty("payloadRasterSize")
        < 0);
}

void SourceIngressContractTest::timedFramesCarryContiguousIntervals()
{
    QImage image(2, 1, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame first(image);
    ImageFrame second(image);
    TimedImageFrameList frames;

    QVERIFY(frames.appendFrame(TimedImageFrame(&first, 0, 100)));
    QVERIFY(frames.appendFrame(TimedImageFrame(&second, 100, 250)));
    QCOMPARE(frames.frames().size(), 2);
    QCOMPARE(frames.frames().at(1).startPosition(), 100);
    QCOMPARE(frames.frames().at(1).duration(), 250);
    QVERIFY(!frames.appendFrame(TimedImageFrame(&second, 200, 50)));

    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&frames));
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Created);
    QVERIFY(
        ImageSequenceEnums::staticMetaObject.indexOfEnumerator("AuthoredAnimationLoopMode") >= 0);
}

void SourceIngressContractTest::providerFactoryIsDeferredUntilGenerationAcceptance()
{
    auto invocationCount = std::make_shared<int>(0);
    SourceIngressContractAdapter adapter(invocationCount);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Created);
    QCOMPARE(result->reason(), ImageSequenceFactoryReason::NoError);
    QCOMPARE(*invocationCount, 0);

    ImageViewport viewport;
    ImageViewportPresentationTarget target(result->sequence());
    viewport.setPresentationTarget(target, PresentationTargetTransitionPolicy {});

    QCOMPARE(*invocationCount, 1);
}

void SourceIngressContractTest::providerSessionCannotBelongToTwoLiveGenerations()
{
    ReusingSessionAdapter adapter;
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport first;
    ImageViewport second;
    const ImageViewportPresentationTarget target(result->sequence());
    first.setPresentationTarget(target, PresentationTargetTransitionPolicy {});
    second.setPresentationTarget(target, PresentationTargetTransitionPolicy {});

    QCOMPARE(second.state().request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(second.state().request().reason(), ImageViewportRequestReason::ProviderFailure);
}

void SourceIngressContractTest::providerSessionFactoryFailureIsGenerationScoped()
{
    FailingSessionAdapter adapter;
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Created);

    ImageViewport viewport;
    viewport.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(viewport.state().request().reason(), ImageViewportRequestReason::ProviderFailure);
    QVERIFY(!viewport.state().diagnostics().errorString().isEmpty());
    QVERIFY(!viewport.state().diagnostics().errorString().contains(
        QStringLiteral("redacted open failure")));
}

QTEST_MAIN(SourceIngressContractTest)

#include "tst_source_ingress_contract.moc"
