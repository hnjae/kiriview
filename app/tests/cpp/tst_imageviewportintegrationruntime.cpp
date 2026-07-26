// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imageviewportintegrationruntime.h"

#include <ImageViewport/imageviewport.h>

#include <QQuickWindow>
#include <QTest>

#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
kiriview::StaticDisplayImagePayload displayPayload(QString sourceIdentity,
    kiriview::DisplayImageQuality quality = kiriview::DisplayImageQuality::Exact)
{
    QImage image(40, 20, QImage::Format_RGBA8888);
    image.fill(QColor(20, 40, 60, 255));
    return kiriview::StaticDisplayImagePayload {
        std::move(sourceIdentity),
        {},
        QSize(40, 20),
        std::move(image),
        quality,
        {},
        {},
        quality == kiriview::DisplayImageQuality::ThumbnailPreview
            ? kiriview::DisplayImagePreviewOrigin::XdgThumbnail
            : kiriview::DisplayImagePreviewOrigin::None,
    };
}

kiriview::ImageLoadFailure loadFailure(const QUrl& url, quint64 sessionId)
{
    return kiriview::ImageLoadFailure {
        url,
        sessionId,
        kiriview::ImageLoadFailureKind::Decode,
        kiriview::DecodedImageFailureRoute::QtRaster,
        kiriview::DecodedImageFailureOperation::DecodeBlockingDisplayImage,
        QStringLiteral("Could not decode the image"),
        QStringLiteral("fake provider failure"),
        kiriview::ImageLoadFailureSeverity::Error,
        false,
    };
}

void hostViewport(QQuickWindow& window, ImageViewport& viewport)
{
    window.resize(100, 80);
    viewport.setParentItem(window.contentItem());
    viewport.setSize(QSizeF(100, 80));
    window.show();
}

void renderFrame(QQuickWindow& window)
{
    window.update();
    window.grabWindow();
    QCoreApplication::processEvents();
}

template <typename Predicate> bool driveRenderUntil(QQuickWindow& window, Predicate predicate)
{
    constexpr int maximumAttempts = 100;
    constexpr int eventIntervalMilliseconds = 5;
    for (int attempt = 0; attempt < maximumAttempts; ++attempt) {
        if (predicate()) {
            return true;
        }
        renderFrame(window);
        QTest::qWait(eventIntervalMilliseconds);
    }
    return predicate();
}

class PendingProviderSource final : public kiriview::ImageViewportProviderSource
{
public:
    struct PendingFrame
    {
        kiriview::ImageViewportProviderWorkIdentity identity;
        FrameCompletion completion;
    };

    ImageSequenceProviderMetadata constructionMetadata() const override
    {
        if (authoritativeSeed.has_value()) {
            return ImageSequenceProviderMetadata::still(authoritativeSeed->originalSize);
        }
        return ImageSequenceProviderMetadata::still(QSizeF(40, 20));
    }

    void requestMetadata(const kiriview::ImageViewportProviderWorkIdentity& identity,
        MetadataCompletion completion) override
    {
        completion(
            identity, kiriview::ImageViewportProviderMetadataResult::ready(constructionMetadata()));
    }

    void requestFrame(const kiriview::ImageViewportProviderWorkIdentity& identity,
        kiriview::ImageViewportProviderFrameRequest, FrameCompletion completion) override
    {
        if (authoritativeSeed.has_value()) {
            completion(identity,
                kiriview::ImageViewportProviderFrameResult::ready(*authoritativeSeed,
                    ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png")));
            return;
        }
        pendingFrames.push_back({ identity, std::move(completion) });
    }

    void cancel(const QVector<ImageSequenceProviderRequestToken>&) override { }
    void close() override { ++closeCount; }

    void completeNext(QString sourceIdentity,
        kiriview::DisplayImageQuality quality = kiriview::DisplayImageQuality::Exact)
    {
        QVERIFY(!pendingFrames.empty());
        PendingFrame pending = std::move(pendingFrames.front());
        pendingFrames.pop_front();
        pending.completion(pending.identity,
            kiriview::ImageViewportProviderFrameResult::ready(
                displayPayload(std::move(sourceIdentity), quality),
                ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png")));
    }

    void emitNextProvisional(QString sourceIdentity)
    {
        QVERIFY(!pendingFrames.empty());
        const PendingFrame& pending = pendingFrames.front();
        pending.completion(pending.identity,
            kiriview::ImageViewportProviderFrameResult::provisional(
                displayPayload(
                    std::move(sourceIdentity), kiriview::DisplayImageQuality::ThumbnailPreview),
                ImageSequenceProviderFrameEnvelope::stillFrame(), QStringLiteral("png")));
    }

    void failNext(kiriview::ImageLoadFailure failure)
    {
        QVERIFY(!pendingFrames.empty());
        PendingFrame pending = std::move(pendingFrames.front());
        pendingFrames.pop_front();
        pending.completion(pending.identity,
            kiriview::ImageViewportProviderFrameResult::failed(
                ImageSequenceProviderFailureCause::Decode, std::move(failure)));
    }

    std::deque<PendingFrame> pendingFrames;
    std::optional<kiriview::StaticDisplayImagePayload> authoritativeSeed;
    int closeCount = 0;
};

struct TargetFixture
{
    quint64 generation = 0;
    QUrl primaryUrl;
    QUrl secondaryUrl;
    kiriview::ImageViewportTargetTransitionIntent intent
        = kiriview::ImageViewportTargetTransitionIntent::SameNavigationScope;
    bool rightToLeft = false;
    bool anchorAtEnd = false;
    std::shared_ptr<PendingProviderSource> primarySource
        = std::make_shared<PendingProviderSource>();
    std::shared_ptr<PendingProviderSource> secondarySource;
    std::shared_ptr<kiriview::ImageViewportProviderResource> primaryResource;
    std::optional<kiriview::StaticDisplayImagePayload> primaryPredecodedImage;
    int primaryFactoryCalls = 0;
    int secondaryFactoryCalls = 0;

    kiriview::ImageViewportIntegrationTarget target()
    {
        kiriview::ImageViewportIntegrationTarget result;
        result.sourceGeneration = generation;
        result.primaryUrl = primaryUrl;
        result.secondaryUrl = secondaryUrl;
        result.transitionIntent = intent;
        result.rightToLeft = rightToLeft;
        result.anchorAtEnd = anchorAtEnd;
        result.primaryResource = [this]() {
            ++primaryFactoryCalls;
            primarySource->authoritativeSeed = primaryPredecodedImage;
            primaryResource = std::make_shared<kiriview::ImageViewportProviderResource>(generation,
                QStringLiteral("primary-%1").arg(generation), primarySource,
                std::make_shared<kiriview::DisplayImageStore>(1024 * 1024),
                std::make_shared<kiriview::ImageViewportFailureRegistry>());
            return primaryResource;
        };
        if (!secondaryUrl.isEmpty()) {
            if (secondarySource == nullptr) {
                secondarySource = std::make_shared<PendingProviderSource>();
            }
            result.secondaryResource = [this]() {
                ++secondaryFactoryCalls;
                return std::make_shared<kiriview::ImageViewportProviderResource>(generation,
                    QStringLiteral("secondary-%1").arg(generation), secondarySource,
                    std::make_shared<kiriview::DisplayImageStore>(1024 * 1024));
            };
        }
        return result;
    }
};
}

class TestImageViewportIntegrationRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void operationRecordCorrelatesReentrantState();
    void replacementAttachmentResubmitsCurrentTarget();
    void staleCompletionCannotPublishOverNewerTarget();
    void twoRoleTargetIsSubmittedAtomically();
    void gesturesAndScrollbarsUseMatchedComponentProjection();
    void targetAnchorAtEndAppliesThroughTransition();
    void failureReferenceResolvesOnlyForMatchingTarget();
    void targetTransitionsRetainPriorPresentationUntilReplacementCommit_data();
    void targetTransitionsRetainPriorPresentationUntilReplacementCommit();
    void displayedImageFollowsCommittedAndRetainedDisplay();
    void authoritativeCandidateWaitsForCommitOverProvisionalDisplay();
    void firstDisplayRemainsAvailableDuringFailedForcedRefinement();
    void predecodedReplacementRetainsCommittedDisplayUntilRenderCommit();
    void failedShapeChangeKeepsRequestedTargetErrorUntilClear();
};

void TestImageViewportIntegrationRuntime::operationRecordCorrelatesReentrantState()
{
    std::vector<kiriview::ImageViewportIntegrationProjection> projections;
    kiriview::ImageViewportIntegrationRuntime runtime(
        { [&projections](const kiriview::ImageViewportIntegrationProjection& projection) {
            projections.push_back(projection);
        } });
    ImageViewport viewport;
    viewport.setSize(QSizeF(100, 80));
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = 11;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/eleven.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));

    QTRY_COMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(1));
    QVERIFY(!projections.empty());
    for (const kiriview::ImageViewportIntegrationProjection& projection : projections) {
        if (projection.correlated) {
            QCOMPARE(projection.sourceGeneration, quint64(11));
        }
    }
    QVERIFY(runtime.projection().correlated);
    QCOMPARE(runtime.projection().sourceGeneration, quint64(11));
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Loading);
}

void TestImageViewportIntegrationRuntime::replacementAttachmentResubmitsCurrentTarget()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    ImageViewport first;
    ImageViewport replacement;
    first.setSize(QSizeF(100, 80));
    replacement.setSize(QSizeF(100, 80));
    runtime.attach(&first);

    TargetFixture fixture;
    fixture.generation = 21;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/twenty-one.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));
    QTRY_COMPARE(fixture.primaryFactoryCalls, 1);

    runtime.attach(&replacement);
    QTRY_COMPARE(fixture.primaryFactoryCalls, 2);
    QCOMPARE(first.state().request().status(), ImageViewportRequestStatus::NoRequest);
    QCOMPARE(replacement.state().request().acceptedRoleSet().primary(), true);

    runtime.detach(&first);
    QCOMPARE(replacement.state().request().acceptedRoleSet().primary(), true);
}

void TestImageViewportIntegrationRuntime::staleCompletionCannotPublishOverNewerTarget()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    ImageViewport viewport;
    viewport.setSize(QSizeF(100, 80));
    runtime.attach(&viewport);

    TargetFixture oldTarget;
    oldTarget.generation = 31;
    oldTarget.primaryUrl = QUrl(QStringLiteral("file:///tmp/old.png"));
    QVERIFY(runtime.submitTarget(oldTarget.target()));
    QTRY_COMPARE(oldTarget.primarySource->pendingFrames.size(), std::size_t(1));

    TargetFixture currentTarget;
    currentTarget.generation = 32;
    currentTarget.primaryUrl = QUrl(QStringLiteral("file:///tmp/current.png"));
    QVERIFY(runtime.submitTarget(currentTarget.target()));
    QTRY_COMPARE(currentTarget.primarySource->pendingFrames.size(), std::size_t(1));

    oldTarget.primarySource->completeNext(QStringLiteral("old"));
    QCoreApplication::processEvents();

    QCOMPARE(runtime.projection().sourceGeneration, quint64(32));
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Loading);
}

void TestImageViewportIntegrationRuntime::twoRoleTargetIsSubmittedAtomically()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = 41;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/primary.png"));
    fixture.secondaryUrl = QUrl(QStringLiteral("file:///tmp/secondary.png"));
    fixture.intent = kiriview::ImageViewportTargetTransitionIntent::SameNavigationScope;
    fixture.rightToLeft = true;
    QVERIFY(runtime.submitTarget(fixture.target()));

    QTRY_COMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(1));
    QTRY_COMPARE(fixture.secondarySource->pendingFrames.size(), std::size_t(1));
    QCOMPARE(viewport.state().request().acceptedRoleSet().primary(), true);
    QCOMPARE(viewport.state().request().acceptedRoleSet().secondary(), true);
    QCOMPARE(viewport.state().presentation().spreadDirection(),
        ImageViewportSpreadDirection::RightToLeft);
    QCOMPARE(runtime.projection().secondaryUrl, fixture.secondaryUrl);
    QCOMPARE(runtime.projection().secondaryVisible, false);

    fixture.primarySource->completeNext(QStringLiteral("primary"));
    renderFrame(window);
    QCOMPARE(runtime.projection().secondaryVisible, false);
    fixture.secondarySource->completeNext(QStringLiteral("secondary"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(runtime.projection().secondaryVisible, true);
}

void TestImageViewportIntegrationRuntime::gesturesAndScrollbarsUseMatchedComponentProjection()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    viewport.setSize(QSizeF(20, 10));
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = 45;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/gesture.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));
    QTRY_COMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(1));
    fixture.primarySource->completeNext(QStringLiteral("gesture"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));

    QVERIFY(runtime.setPreferredManualZoomPercent(100.0, QPointF(10, 5)));
    QTRY_VERIFY(runtime.projection().horizontallyPannable);
    QVERIFY(runtime.panBy(QPointF(10, 0)));
    QVERIFY(runtime.submitHorizontalScrollPosition(1.0));
    QTRY_COMPARE(runtime.projection().horizontalScrollPosition, qreal(1.0));
    QVERIFY(runtime.projection().horizontalScrollPageSize < 1.0);
}

void TestImageViewportIntegrationRuntime::targetAnchorAtEndAppliesThroughTransition()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    viewport.setSize(QSizeF(20, 10));
    runtime.attach(&viewport);

    TargetFixture initial;
    initial.generation = 46;
    initial.primaryUrl = QUrl(QStringLiteral("file:///tmp/anchor-initial.png"));
    QVERIFY(runtime.submitTarget(initial.target()));
    QTRY_COMPARE(initial.primarySource->pendingFrames.size(), std::size_t(1));
    initial.primarySource->completeNext(QStringLiteral("anchor-initial"));
    QTRY_COMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Ready);
    QVERIFY(runtime.setPreferredManualZoomPercent(100.0));
    QTRY_VERIFY(runtime.projection().horizontallyPannable);

    TargetFixture anchored;
    anchored.generation = 47;
    anchored.primaryUrl = QUrl(QStringLiteral("file:///tmp/anchor-next.png"));
    anchored.anchorAtEnd = true;
    QVERIFY(runtime.submitTarget(anchored.target()));
    QTRY_COMPARE(anchored.primarySource->pendingFrames.size(), std::size_t(1));
    anchored.primarySource->completeNext(QStringLiteral("anchor-next"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QVERIFY(runtime.projection().maximumContentPosition.x() > 0.0);
    QCOMPARE(runtime.projection().contentPosition, runtime.projection().maximumContentPosition);
}

void TestImageViewportIntegrationRuntime::failureReferenceResolvesOnlyForMatchingTarget()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    ImageViewport viewport;
    viewport.setSize(QSizeF(100, 80));
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = 51;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/failure.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));
    QTRY_COMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(1));
    fixture.primarySource->failNext(loadFailure(fixture.primaryUrl, 510));

    QTRY_COMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Error);
    QVERIFY(runtime.projection().failure.has_value());
    QCOMPARE(runtime.projection().failure->sessionId, quint64(510));
    QCOMPARE(runtime.projection().errorString, QStringLiteral("Could not decode the image"));

    TargetFixture replacement;
    replacement.generation = 52;
    replacement.primaryUrl = QUrl(QStringLiteral("file:///tmp/replacement.png"));
    QVERIFY(runtime.submitTarget(replacement.target()));
    QCOMPARE(runtime.projection().sourceGeneration, quint64(52));
    QVERIFY(!runtime.projection().failure.has_value());
}

void TestImageViewportIntegrationRuntime::
    targetTransitionsRetainPriorPresentationUntilReplacementCommit_data()
{
    QTest::addColumn<int>("intent");
    QTest::newRow("same navigation scope")
        << static_cast<int>(kiriview::ImageViewportTargetTransitionIntent::SameNavigationScope);
    QTest::newRow("outside navigation scope")
        << static_cast<int>(kiriview::ImageViewportTargetTransitionIntent::OutsideNavigationScope);
    QTest::newRow("presentation shape change")
        << static_cast<int>(kiriview::ImageViewportTargetTransitionIntent::PresentationShapeChange);
}

void TestImageViewportIntegrationRuntime::
    targetTransitionsRetainPriorPresentationUntilReplacementCommit()
{
    QFETCH(int, intent);
    std::vector<kiriview::ImageViewportIntegrationProjection> projections;
    kiriview::ImageViewportIntegrationRuntime runtime(
        { [&projections](const kiriview::ImageViewportIntegrationProjection& projection) {
            projections.push_back(projection);
        } });
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture initial;
    initial.generation = 61;
    initial.primaryUrl = QUrl(QStringLiteral("file:///tmp/initial.png"));
    QVERIFY(runtime.submitTarget(initial.target()));
    QTRY_COMPARE(initial.primarySource->pendingFrames.size(), std::size_t(1));
    initial.primarySource->completeNext(QStringLiteral("initial"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(runtime.projection().displayedUrl, initial.primaryUrl);
    QVERIFY(viewport.state().display().displayedRoleSet().primary());
    projections.clear();

    TargetFixture replacement;
    replacement.generation = 62;
    replacement.primaryUrl = intent
            == static_cast<int>(
                kiriview::ImageViewportTargetTransitionIntent::PresentationShapeChange)
        ? initial.primaryUrl
        : QUrl(QStringLiteral("file:///tmp/replacement.png"));
    replacement.intent = static_cast<kiriview::ImageViewportTargetTransitionIntent>(intent);
    if (replacement.intent
        == kiriview::ImageViewportTargetTransitionIntent::PresentationShapeChange) {
        replacement.secondaryUrl = QUrl(QStringLiteral("file:///tmp/secondary.png"));
    }
    QVERIFY(runtime.submitTarget(replacement.target()));
    QTRY_COMPARE(replacement.primarySource->pendingFrames.size(), std::size_t(1));

    QCOMPARE(viewport.state().display().status(), ImageViewportDisplayStatus::Retained);
    QVERIFY(viewport.state().display().retained());
    QVERIFY(!viewport.state().display().belongsToAcceptedPresentationTarget());
    QVERIFY(viewport.state().display().displayedRoleSet().primary());
    QVERIFY(!viewport.state().display().displayedRoleSet().secondary());
    QCOMPARE(runtime.projection().displayedUrl, initial.primaryUrl);
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Loading);
    for (const kiriview::ImageViewportIntegrationProjection& projection : projections) {
        if (projection.sourceGeneration == replacement.generation) {
            QCOMPARE(projection.displayedUrl, initial.primaryUrl);
        }
    }

    replacement.primarySource->completeNext(QStringLiteral("replacement"));
    if (replacement.secondarySource != nullptr) {
        QTRY_COMPARE(replacement.secondarySource->pendingFrames.size(), std::size_t(1));
        replacement.secondarySource->completeNext(QStringLiteral("secondary"));
    }
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(runtime.projection().displayedUrl, replacement.primaryUrl);
    QVERIFY(viewport.state().display().belongsToAcceptedPresentationTarget());
    QVERIFY(viewport.state().display().displayedRoleSet().primary());
    QCOMPARE(viewport.state().display().displayedRoleSet().secondary(),
        replacement.secondarySource != nullptr);
}

void TestImageViewportIntegrationRuntime::displayedImageFollowsCommittedAndRetainedDisplay()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture initial;
    initial.generation = 63;
    initial.primaryUrl = QUrl(QStringLiteral("file:///tmp/displayed-image-initial.png"));
    QVERIFY(runtime.submitTarget(initial.target()));
    QTRY_COMPARE(initial.primarySource->pendingFrames.size(), std::size_t(1));
    initial.primarySource->completeNext(QStringLiteral("committed"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    const std::optional<kiriview::StaticDisplayImagePayload> committed
        = runtime.displayedImage(ImageViewportPageRole::Primary);
    QVERIFY(committed.has_value());
    QCOMPARE(committed->sourceIdentity, QStringLiteral("committed"));

    TargetFixture replacement;
    replacement.generation = 64;
    replacement.primaryUrl = QUrl(QStringLiteral("file:///tmp/displayed-image-replacement.png"));
    QVERIFY(runtime.submitTarget(replacement.target()));
    QTRY_COMPARE(replacement.primarySource->pendingFrames.size(), std::size_t(1));
    QCOMPARE(viewport.state().display().status(), ImageViewportDisplayStatus::Retained);
    const std::optional<kiriview::StaticDisplayImagePayload> retained
        = runtime.displayedImage(ImageViewportPageRole::Primary);
    QVERIFY(retained.has_value());
    QCOMPARE(retained->sourceIdentity, QStringLiteral("committed"));
}

void TestImageViewportIntegrationRuntime::
    authoritativeCandidateWaitsForCommitOverProvisionalDisplay()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = 65;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/provisional-commit.png"));
    fixture.secondaryUrl = QUrl(QStringLiteral("file:///tmp/provisional-commit-secondary.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));
    QTRY_COMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(1));
    QTRY_COMPARE(fixture.secondarySource->pendingFrames.size(), std::size_t(1));
    fixture.primarySource->emitNextProvisional(QStringLiteral("preview"));
    fixture.secondarySource->emitNextProvisional(QStringLiteral("secondary-preview"));
    QVERIFY(driveRenderUntil(window, [&viewport]() {
        return viewport.state().display().status() == ImageViewportDisplayStatus::Ready
            && viewport.state().display().displayedRoleSet().secondary();
    }));
    QCOMPARE(viewport.state().primary().display().quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(
        viewport.state().secondary().display().quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Loading);
    QVERIFY(!runtime.displayedImage(ImageViewportPageRole::Primary).has_value());

    fixture.primarySource->completeNext(QStringLiteral("authoritative"));
    QVERIFY(driveRenderUntil(window, [&viewport]() {
        return viewport.state().primary().display().quality() == ImageViewportPayloadQuality::Exact;
    }));
    QCOMPARE(
        viewport.state().secondary().display().quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Loading);
    QVERIFY(!fixture.primaryResource
            ->currentStillDisplayImage(viewport.state().primary().display().demandRevision())
            .has_value());
    QVERIFY(!runtime.displayedImage(ImageViewportPageRole::Primary).has_value());

    fixture.secondarySource->completeNext(QStringLiteral("secondary-authoritative"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    const std::optional<kiriview::StaticDisplayImagePayload> committed
        = runtime.displayedImage(ImageViewportPageRole::Primary);
    QVERIFY(committed.has_value());
    QCOMPARE(committed->sourceIdentity, QStringLiteral("authoritative"));
}

void TestImageViewportIntegrationRuntime::firstDisplayRemainsAvailableDuringFailedForcedRefinement()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = 66;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/first-display.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));
    QTRY_COMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(1));
    fixture.primarySource->completeNext(
        QStringLiteral("first-display"), kiriview::DisplayImageQuality::FirstDisplay);
    QVERIFY(driveRenderUntil(window, [&fixture, &viewport]() {
        return viewport.state().request().status() == ImageViewportRequestStatus::Ready
            && fixture.primarySource->pendingFrames.size() == 1;
    }));
    QVERIFY(!viewport.state().primary().display().currentForDemand());

    const std::optional<kiriview::StaticDisplayImagePayload> committed
        = runtime.displayedImage(ImageViewportPageRole::Primary);
    QVERIFY(committed.has_value());
    QCOMPARE(committed->sourceIdentity, QStringLiteral("first-display"));
    QCOMPARE(committed->quality, kiriview::DisplayImageQuality::FirstDisplay);

    fixture.primarySource->failNext(loadFailure(fixture.primaryUrl, 660));
    QTRY_COMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Ready);
    const std::optional<kiriview::StaticDisplayImagePayload> preserved
        = runtime.displayedImage(ImageViewportPageRole::Primary);
    QVERIFY(preserved.has_value());
    QCOMPARE(preserved->sourceIdentity, QStringLiteral("first-display"));
}

void TestImageViewportIntegrationRuntime::
    predecodedReplacementRetainsCommittedDisplayUntilRenderCommit()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture initial;
    initial.generation = 63;
    initial.primaryUrl = QUrl(QStringLiteral("file:///tmp/predecoded-initial.png"));
    QVERIFY(runtime.submitTarget(initial.target()));
    QTRY_COMPARE(initial.primarySource->pendingFrames.size(), std::size_t(1));
    initial.primarySource->completeNext(QStringLiteral("predecoded-initial"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(runtime.projection().displayedUrl, initial.primaryUrl);

    TargetFixture replacement;
    replacement.generation = 64;
    replacement.primaryUrl = QUrl(QStringLiteral("file:///tmp/predecoded-replacement.png"));
    replacement.primaryPredecodedImage = displayPayload(QStringLiteral("predecoded-replacement"));
    QVERIFY(runtime.submitTarget(replacement.target()));

    QCOMPARE(viewport.state().display().status(), ImageViewportDisplayStatus::Retained);
    QVERIFY(viewport.state().display().displayedRoleSet().primary());
    QCOMPARE(runtime.projection().displayedUrl, initial.primaryUrl);
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Loading);

    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(runtime.projection().displayedUrl, replacement.primaryUrl);
}

void TestImageViewportIntegrationRuntime::failedShapeChangeKeepsRequestedTargetErrorUntilClear()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture initial;
    initial.generation = 71;
    initial.primaryUrl = QUrl(QStringLiteral("file:///tmp/initial.png"));
    QVERIFY(runtime.submitTarget(initial.target()));
    QTRY_COMPARE(initial.primarySource->pendingFrames.size(), std::size_t(1));
    initial.primarySource->completeNext(QStringLiteral("initial"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(runtime.projection().displayedUrl, initial.primaryUrl);

    TargetFixture shape;
    shape.generation = 72;
    shape.primaryUrl = initial.primaryUrl;
    shape.secondaryUrl = QUrl(QStringLiteral("file:///tmp/missing-secondary.png"));
    shape.intent = kiriview::ImageViewportTargetTransitionIntent::PresentationShapeChange;
    QVERIFY(runtime.submitTarget(shape.target()));
    QTRY_COMPARE(shape.secondarySource->pendingFrames.size(), std::size_t(1));
    shape.secondarySource->failNext(loadFailure(shape.secondaryUrl, 720));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Error;
    }));
    QCOMPARE(runtime.projection().sourceGeneration, quint64(72));
    QVERIFY(runtime.projection().failure.has_value());
    QCOMPARE(runtime.projection().failure->sessionId, quint64(720));

    QTRY_COMPARE(shape.primarySource->pendingFrames.size(), std::size_t(1));
    shape.primarySource->completeNext(QStringLiteral("primary"));
    renderFrame(window);
    QCOMPARE(runtime.projection().displayedUrl, initial.primaryUrl);

    runtime.clearTarget();
    QCOMPARE(runtime.projection().displayedUrl, QUrl());
}

QTEST_MAIN(TestImageViewportIntegrationRuntime)

#include "tst_imageviewportintegrationruntime.moc"
