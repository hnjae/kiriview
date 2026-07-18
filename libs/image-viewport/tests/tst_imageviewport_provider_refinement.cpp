// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_provider_test_support.h"

namespace {
class RefinementProviderSession final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderSession
{
public:
    explicit RefinementProviderSession(QSizeF logicalSize, QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_logicalSize(logicalSize)
    {
    }

    void request(const ImageSequenceProviderRequest& request) override
    {
        requests.append(request);
        if (request.kind() == ImageSequenceProviderRequestKind::Metadata) {
            emitProviderMetadataReady(
                this, request.token(), ImageSequenceProviderMetadata::still(m_logicalSize));
        }
    }

    void emitReady(const ImageSequenceProviderRequest& request, QColor color,
        bool echoDemandRevision = true, QSize payloadRasterSize = QSize(16, 8))
    {
        QImage image(payloadRasterSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(color);
        const bool exact = QSizeF(payloadRasterSize) == m_logicalSize;
        auto frame = std::make_unique<ImageFrame>(image, m_logicalSize, QSizeF(payloadRasterSize),
            QSizeF(payloadRasterSize.width() / m_logicalSize.width(),
                payloadRasterSize.height() / m_logicalSize.height()),
            image.sizeInBytes(),
            exact ? ImageViewportPayloadQuality::Exact : ImageViewportPayloadQuality::Preview,
            exact ? ImageViewportPayloadExactness::ExactForSource
                  : ImageViewportPayloadExactness::NotExact,
            true, ImageFrame::OrientationPolicy::Identity, QString {});
        auto envelope = ImageSequenceProviderFrameEnvelope::stillFrame();
        if (echoDemandRevision)
            envelope.setDemandRevision(request.demand().demandRevision());
        emit providerEvent(ImageSequenceProviderEvent::frameReady(request.token(),
            new ImageSequenceProviderFrameHandle(std::move(frame), this), envelope));
    }

    QVector<ImageSequenceProviderRequest> frameRequests() const
    {
        QVector<ImageSequenceProviderRequest> result;
        for (const auto& request : requests) {
            if (request.kind() == ImageSequenceProviderRequestKind::Frame)
                result.append(request);
        }
        return result;
    }

    QVector<ImageSequenceProviderRequest> requests;

private:
    QSizeF m_logicalSize;
};

class RefinementProviderAdapter final // clazy:exclude=missing-qobject-macro
    : public ImageSequenceProviderAdapter
{
public:
    explicit RefinementProviderAdapter(
        QSizeF logicalSize = QSizeF(16, 8), QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_logicalSize(logicalSize)
    {
    }

    ImageSequenceProviderDescriptor descriptor() const override
    {
        return ImageSequenceProviderDescriptor(
            {}, ImageSequenceProviderThreadingContract::ThreadSafe, [this]() {
                session = new RefinementProviderSession(m_logicalSize);
                return ImageSequenceProviderSessionFactoryResult::created(session);
            });
    }

    mutable QPointer<RefinementProviderSession> session;

private:
    QSizeF m_logicalSize;
};

struct ReadyProviderViewport
{
    RefinementProviderAdapter adapter;
    QScopedPointer<ImageSequenceFactoryResult> sequence;
    ImageViewport viewport;

    explicit ReadyProviderViewport(
        QSizeF logicalSize = QSizeF(16, 8), QSize payloadRasterSize = QSize(16, 8))
        : adapter(logicalSize)
    {
        ImageSequenceFactory factory;
        sequence.reset(factory.fromProvider(&adapter));
        Q_ASSERT(sequence && sequence->sequence());
        viewport.setSize(QSizeF(100.0, 50.0));
        useSynchronousProviderEventDeliveryForTest(viewport);
        viewport.setPresentationTarget(ImageViewportPresentationTarget(sequence->sequence()),
            PresentationTargetTransitionPolicy {});
        Q_ASSERT(adapter.session);
        const auto frames = adapter.session->frameRequests();
        Q_ASSERT(frames.size() == 1);
        adapter.session->emitReady(frames.constFirst(), Qt::red, true, payloadRasterSize);
        acknowledgePendingRenderCommitForTest(viewport);
        Q_ASSERT(requestStatus(viewport) == ImageViewportRequestStatus::Ready);
    }
};

void setQualityPreference(ImageViewport& viewport, ImageViewportQualityPreference preference)
{
    ImageViewportPresentationCommand command;
    command.setQualityPreference(preference);
    QCOMPARE(viewport.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
}

void setExactnessPreference(ImageViewport& viewport, ImageViewportExactnessPreference preference)
{
    ImageViewportPresentationCommand command;
    command.setExactnessPreference(preference);
    QCOMPARE(viewport.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
}
}

class ImageViewportProviderRefinementTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportProviderRefinementTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void largeLogicalSourceAcceptsBoundedPreviewAndRefinement();
    void committedProviderPayloadRefinesWithoutLeavingReady();
    void newerDemandCancelsOlderRefinementAndStaleResultCannotCommit();
    void refinementFailureIsIsolatedFromTheDisplayRequest();
    void refinementMissingDemandRevisionIsIsolatedFromTheDisplayRequest();
    void refinementKindMismatchIsGenerationTerminal();
    void refinementNeverIssuedTokenIsGenerationTerminal();
    void refinementCommandDeliveryFailureIsGenerationTerminal();
    void refinementRenderFailureIsIsolatedFromTheDisplayRequest();
    void spreadRefinementsCommitAsOneCompleteCandidateSet();
};

void ImageViewportProviderRefinementTest::largeLogicalSourceAcceptsBoundedPreviewAndRefinement()
{
    const QSizeF logicalSize(200000, 100000);
    ReadyProviderViewport fixture(logicalSize, QSize(200, 100));

    QCOMPARE(fixture.viewport.state().primary().display().sourceLogicalSize(), logicalSize);
    QCOMPARE(fixture.viewport.state().primary().display().payloadRasterSize(), QSizeF(200, 100));
    QCOMPARE(fixture.adapter.session->frameRequests().constFirst().demand().sourceLogicalSize(),
        logicalSize);

    setQualityPreference(fixture.viewport, ImageViewportQualityPreference::BalancedDetail);
    const auto refinement = fixture.adapter.session->frameRequests().constLast();
    QCOMPARE(refinement.demand().sourceLogicalSize(), logicalSize);
    QVERIFY(
        refinement.demand().maximumPayloadBytes() <= ImageSequenceLimits::maximumPayloadBytes());

    fixture.adapter.session->emitReady(refinement, Qt::blue, true, QSize(400, 200));
    acknowledgePendingRenderCommitForTest(fixture.viewport);

    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Ready);
    QCOMPARE(fixture.viewport.state().primary().display().sourceLogicalSize(), logicalSize);
    QCOMPARE(fixture.viewport.state().primary().display().payloadRasterSize(), QSizeF(400, 200));
    QCOMPARE(fixture.viewport.state().primary().display().currentForDemand(), true);
}

void ImageViewportProviderRefinementTest::committedProviderPayloadRefinesWithoutLeavingReady()
{
    ReadyProviderViewport fixture;
    const auto before = fixture.viewport.state();
    QCOMPARE(before.primary().display().currentForDemand(), true);

    setQualityPreference(fixture.viewport, ImageViewportQualityPreference::BalancedDetail);

    const auto frames = fixture.adapter.session->frameRequests();
    QCOMPARE(frames.size(), 2);
    QCOMPARE(frames.constLast().kind(), ImageSequenceProviderRequestKind::Frame);
    QCOMPARE(frames.constLast().demand().qualityPreference(),
        ImageViewportQualityPreference::BalancedDetail);
    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Ready);
    QCOMPARE(displayStatus(fixture.viewport), ImageViewportDisplayStatus::Ready);
    QCOMPARE(fixture.viewport.state().primary().display().currentForDemand(), false);
    QCOMPARE(fixture.viewport.state().primary().display().demandRevision(),
        before.primary().display().demandRevision());

    fixture.adapter.session->emitReady(frames.constLast(), Qt::blue);

    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Ready);
    QCOMPARE(displayStatus(fixture.viewport), ImageViewportDisplayStatus::Ready);
    QCOMPARE(fixture.viewport.state().primary().display().demandRevision(),
        before.primary().display().demandRevision());
    QVERIFY(hasPendingRenderCommitForTest(fixture.viewport));

    acknowledgePendingRenderCommitForTest(fixture.viewport);

    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Ready);
    QCOMPARE(displayStatus(fixture.viewport), ImageViewportDisplayStatus::Ready);
    QCOMPARE(fixture.viewport.state().primary().display().currentForDemand(), true);
    QCOMPARE(fixture.viewport.state().primary().display().demandRevision(),
        frames.constLast().demand().demandRevision());
    QCOMPARE(fixture.adapter.session->frameRequests().size(), 2);
}

void ImageViewportProviderRefinementTest::
    newerDemandCancelsOlderRefinementAndStaleResultCannotCommit()
{
    ReadyProviderViewport fixture;
    setQualityPreference(fixture.viewport, ImageViewportQualityPreference::BalancedDetail);
    const auto older = fixture.adapter.session->frameRequests().constLast();

    setExactnessPreference(fixture.viewport, ImageViewportExactnessPreference::PreferExact);

    const auto frames = fixture.adapter.session->frameRequests();
    QCOMPARE(frames.size(), 3);
    const auto& newer = frames.constLast();
    bool cancelledOlder = false;
    for (const auto& request : std::as_const(fixture.adapter.session->requests)) {
        cancelledOlder = cancelledOlder
            || (request.kind() == ImageSequenceProviderRequestKind::Cancel
                && request.tokens().contains(older.token()));
    }
    QVERIFY(cancelledOlder);

    fixture.adapter.session->emitReady(older, Qt::green);
    QVERIFY(!hasPendingRenderCommitForTest(fixture.viewport));
    QCOMPARE(fixture.viewport.state().primary().display().currentForDemand(), false);

    fixture.adapter.session->emitReady(newer, Qt::blue);
    QVERIFY(hasPendingRenderCommitForTest(fixture.viewport));
    acknowledgePendingRenderCommitForTest(fixture.viewport);
    QCOMPARE(fixture.viewport.state().primary().display().currentForDemand(), true);
    QCOMPARE(fixture.viewport.state().primary().display().demandRevision(),
        newer.demand().demandRevision());
}

void ImageViewportProviderRefinementTest::refinementFailureIsIsolatedFromTheDisplayRequest()
{
    ReadyProviderViewport fixture;
    setQualityPreference(fixture.viewport, ImageViewportQualityPreference::ExactDetail);
    const auto refinement = fixture.adapter.session->frameRequests().constLast();

    emitProviderFailed(
        fixture.adapter.session, refinement.token(), QStringLiteral("detail unavailable"));

    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Ready);
    QCOMPARE(requestReason(fixture.viewport), ImageViewportRequestReason::Ready);
    QCOMPARE(displayStatus(fixture.viewport), ImageViewportDisplayStatus::Ready);
    QCOMPARE(fixture.viewport.state().diagnostics().errorString(), QString());
    QCOMPARE(fixture.viewport.state().primary().display().currentForDemand(), false);
}

void ImageViewportProviderRefinementTest::
    refinementMissingDemandRevisionIsIsolatedFromTheDisplayRequest()
{
    ReadyProviderViewport fixture;
    const auto committedDemandRevision
        = fixture.viewport.state().primary().display().demandRevision();
    setQualityPreference(fixture.viewport, ImageViewportQualityPreference::ExactDetail);
    const auto refinement = fixture.adapter.session->frameRequests().constLast();

    fixture.adapter.session->emitReady(refinement, Qt::blue, false);

    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Ready);
    QCOMPARE(requestReason(fixture.viewport), ImageViewportRequestReason::Ready);
    QCOMPARE(displayStatus(fixture.viewport), ImageViewportDisplayStatus::Ready);
    QCOMPARE(
        fixture.viewport.state().primary().display().demandRevision(), committedDemandRevision);
    QCOMPARE(fixture.viewport.state().primary().display().currentForDemand(), false);
    QVERIFY(!hasPendingRenderCommitForTest(fixture.viewport));
}

void ImageViewportProviderRefinementTest::refinementKindMismatchIsGenerationTerminal()
{
    ReadyProviderViewport fixture;
    setQualityPreference(fixture.viewport, ImageViewportQualityPreference::ExactDetail);
    const auto refinement = fixture.adapter.session->frameRequests().constLast();

    emitProviderMetadataReady(fixture.adapter.session, refinement.token(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));

    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Error);
    QCOMPARE(requestReason(fixture.viewport), ImageViewportRequestReason::PayloadRejection);
    QVERIFY(fixture.viewport.state().diagnostics().errorString().contains(
        QStringLiteral("provider protocol violation")));
}

void ImageViewportProviderRefinementTest::refinementNeverIssuedTokenIsGenerationTerminal()
{
    ReadyProviderViewport fixture;
    setQualityPreference(fixture.viewport, ImageViewportQualityPreference::ExactDetail);
    const auto refinement = fixture.adapter.session->frameRequests().constLast();
    const auto neverIssuedToken
        = providerRequestTokenForTest(providerRequestTokenValueForTest(refinement.token()) + 1);

    emitProviderFailed(
        fixture.adapter.session, neverIssuedToken, QStringLiteral("wrong refinement token"));

    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Error);
    QCOMPARE(requestReason(fixture.viewport), ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(displayStatus(fixture.viewport), ImageViewportDisplayStatus::Ready);
    QVERIFY(fixture.viewport.state().diagnostics().errorString().contains(
        QStringLiteral("provider protocol violation")));
    QCOMPARE(fixture.viewport.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Unsupported);
}

void ImageViewportProviderRefinementTest::refinementCommandDeliveryFailureIsGenerationTerminal()
{
    ReadyProviderViewport fixture;
    failNextProviderCommandDeliveryForTest(fixture.viewport, ImageViewportPageRole::Primary);

    setQualityPreference(fixture.viewport, ImageViewportQualityPreference::ExactDetail);

    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Error);
    QCOMPARE(requestReason(fixture.viewport), ImageViewportRequestReason::ProviderFailure);
    QCOMPARE(displayStatus(fixture.viewport), ImageViewportDisplayStatus::Ready);
    QVERIFY(fixture.viewport.state().diagnostics().errorString().contains(
        QStringLiteral("provider command delivery failed")));
    QCOMPARE(fixture.viewport.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Unsupported);
}

void ImageViewportProviderRefinementTest::refinementRenderFailureIsIsolatedFromTheDisplayRequest()
{
    ReadyProviderViewport fixture;
    setQualityPreference(fixture.viewport, ImageViewportQualityPreference::ExactDetail);
    const auto refinement = fixture.adapter.session->frameRequests().constLast();
    fixture.adapter.session->emitReady(refinement, Qt::blue);
    QVERIFY(hasPendingRenderCommitForTest(fixture.viewport));

    acknowledgePendingPrimaryRenderFailureForTest(fixture.viewport);

    QCOMPARE(requestStatus(fixture.viewport), ImageViewportRequestStatus::Ready);
    QCOMPARE(requestReason(fixture.viewport), ImageViewportRequestReason::Ready);
    QCOMPARE(displayStatus(fixture.viewport), ImageViewportDisplayStatus::Ready);
    QCOMPARE(fixture.viewport.state().diagnostics().errorString(), QString());
    QCOMPARE(fixture.viewport.state().primary().display().currentForDemand(), false);
    QVERIFY(!hasPendingRenderCommitForTest(fixture.viewport));
}

void ImageViewportProviderRefinementTest::spreadRefinementsCommitAsOneCompleteCandidateSet()
{
    ImageSequenceFactory factory;
    RefinementProviderAdapter primaryAdapter;
    RefinementProviderAdapter secondaryAdapter;
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromProvider(&primaryAdapter));
    QScopedPointer<ImageSequenceFactoryResult> secondary(factory.fromProvider(&secondaryAdapter));
    QVERIFY(primary->sequence());
    QVERIFY(secondary->sequence());

    ImageViewport viewport;
    viewport.setSize(QSizeF(100.0, 50.0));
    useSynchronousProviderEventDeliveryForTest(viewport);
    QCOMPARE(viewport
                 .setPresentationTarget(
                     ImageViewportPresentationTarget(primary->sequence(), secondary->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QVERIFY(primaryAdapter.session);
    QVERIFY(secondaryAdapter.session);
    primaryAdapter.session->emitReady(
        primaryAdapter.session->frameRequests().constFirst(), Qt::red);
    secondaryAdapter.session->emitReady(
        secondaryAdapter.session->frameRequests().constFirst(), Qt::green);
    acknowledgePendingRenderCommitForTest(viewport);
    const auto committed = viewport.state();
    QCOMPARE(committed.primary().display().currentForDemand(), true);
    QCOMPARE(committed.secondary().display().currentForDemand(), true);

    setQualityPreference(viewport, ImageViewportQualityPreference::BalancedDetail);
    QCOMPARE(primaryAdapter.session->frameRequests().size(), 2);
    QCOMPARE(secondaryAdapter.session->frameRequests().size(), 2);
    const auto primaryRefinement = primaryAdapter.session->frameRequests().constLast();
    const auto secondaryRefinement = secondaryAdapter.session->frameRequests().constLast();
    primaryAdapter.session->emitReady(primaryRefinement, Qt::blue);
    secondaryAdapter.session->emitReady(secondaryRefinement, Qt::yellow);

    QCOMPARE(requestStatus(viewport), ImageViewportRequestStatus::Ready);
    QCOMPARE(viewport.state().primary().display().demandRevision(),
        committed.primary().display().demandRevision());
    QCOMPARE(viewport.state().secondary().display().demandRevision(),
        committed.secondary().display().demandRevision());
    acknowledgePendingRenderCommitForTest(viewport);

    QCOMPARE(requestStatus(viewport), ImageViewportRequestStatus::Ready);
    QCOMPARE(displayStatus(viewport), ImageViewportDisplayStatus::Ready);
    QCOMPARE(viewport.state().primary().display().demandRevision(),
        primaryRefinement.demand().demandRevision());
    QCOMPARE(viewport.state().secondary().display().demandRevision(),
        secondaryRefinement.demand().demandRevision());
    QCOMPARE(viewport.state().primary().display().currentForDemand(), true);
    QCOMPARE(viewport.state().secondary().display().currentForDemand(), true);
}

QTEST_MAIN(ImageViewportProviderRefinementTest)

#include "tst_imageviewport_provider_refinement.moc"
