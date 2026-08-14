// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imageviewportintegrationruntime.h"

#include <ImageViewport/imageviewport.h>

#include <QQuickWindow>
#include <QTest>

#include <deque>
#include <functional>
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
        {},
        {},
        quality == kiriview::DisplayImageQuality::ThumbnailPreview
            ? kiriview::DisplayImagePreviewOrigin::XdgThumbnail
            : kiriview::DisplayImagePreviewOrigin::None,
        kiriview::StaticImageSourceDetailModel::FiniteRaster,
        kiriview::ImageSourceRevision::fromData(QByteArrayView("integration-test-image")),
        quality == kiriview::DisplayImageQuality::ThumbnailPreview
            ? kiriview::DisplayImageRasterKind::ProvisionalPreview
            : kiriview::DisplayImageRasterKind::AuthoritativeStill,
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
        kiriview::StaticDisplayImagePayload payload
            = displayPayload(std::move(sourceIdentity), quality);
        payload.rasterKind = kiriview::DisplayImageRasterKind::AuthoritativeStill;
        pending.completion(pending.identity,
            kiriview::ImageViewportProviderFrameResult::ready(std::move(payload),
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
    quint64 secondarySessionId = 0;
    kiriview::ImageViewportTargetTransitionIntent intent
        = kiriview::ImageViewportTargetTransitionIntent::SameNavigationScope;
    bool rightToLeft = false;
    bool anchorAtEnd = false;
    std::shared_ptr<PendingProviderSource> primarySource
        = std::make_shared<PendingProviderSource>();
    std::shared_ptr<PendingProviderSource> secondarySource;
    std::shared_ptr<kiriview::ImageViewportProviderResource> primaryResource;
    std::optional<kiriview::StaticDisplayImagePayload> primaryPredecodedImage;
    bool primaryUrlResolved = true;
    int primaryFactoryCalls = 0;
    int secondaryFactoryCalls = 0;
    std::function<void()> primaryFactoryCallback;
    std::function<void()> secondaryFactoryCallback;

    kiriview::ImageViewportIntegrationTarget target()
    {
        kiriview::ImageViewportIntegrationTarget result;
        result.sourceGeneration = generation;
        result.selectedSourceUrl = primaryUrl;
        result.resolvedPrimaryUrl = primaryUrlResolved ? primaryUrl : QUrl();
        result.secondaryUrl = secondaryUrl;
        result.secondarySessionId = secondaryUrl.isEmpty()
            ? 0
            : (secondarySessionId != 0 ? secondarySessionId : generation);
        result.transitionIntent = intent;
        result.rightToLeft = rightToLeft;
        result.anchorAtEnd = anchorAtEnd;
        result.primaryResource = [this]() {
            ++primaryFactoryCalls;
            if (primaryFactoryCallback) {
                primaryFactoryCallback();
            }
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
                if (secondaryFactoryCallback) {
                    secondaryFactoryCallback();
                }
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
    void reentrantResourceFactoryKeepsNewerTargetAuthoritative_data();
    void reentrantResourceFactoryKeepsNewerTargetAuthoritative();
    void reentrantProjectionDetachmentLeavesTargetPendingForReattachment();
    void projectionCallbackRetainsPublishedSnapshotAcrossReentry();
    void reentrantClearCannotEraseNewerTarget();
    void reentrantAttachmentReplacementKeepsLatestAttachment();
    void secondaryTargetRequiresSessionIdentity();
    void twoRoleTargetIsSubmittedAtomically();
    void readyTwoRoleTargetReattachesWithExactRoleSet();
    void gesturesAndScrollbarsUseMatchedComponentProjection();
    void targetAnchorAtEndAppliesThroughTransition();
    void failureReferenceResolvesOnlyForMatchingTarget();
    void targetTransitionsApplyIntentSpecificFallbackPolicy_data();
    void targetTransitionsApplyIntentSpecificFallbackPolicy();
    void displayedImageFollowsCommittedAndRetainedDisplay();
    void authoritativeCandidateWaitsForCommitOverProvisionalDisplay();
    void firstDisplayAutomaticallyRefinesWithoutInteraction();
    void firstDisplayRemainsAvailableDuringFailedForcedRefinement();
    void predecodedReplacementRetainsCommittedDisplayUntilRenderCommit();
    void failedShapeChangeKeepsRequestedTargetErrorWithoutPriorPixels();
    void deferredPrimaryUrlResolvesWithoutReplacingAcceptedTarget();
    void authoritativeDisplayExcludesProvisionalPixelsAndIncludesRetainedPixels();
    void completeAuthoritativeDisplayAvailabilityFollowsPresentationFallback();
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
    const std::weak_ptr<kiriview::ImageViewportProviderResource> firstResource
        = fixture.primaryResource;
    fixture.primaryResource.reset();
    QVERIFY(!firstResource.expired());

    runtime.attach(&replacement);
    QTRY_COMPARE(fixture.primaryFactoryCalls, 2);
    QTRY_VERIFY(firstResource.expired());
    QCOMPARE(first.state().request().status(), ImageViewportRequestStatus::NoRequest);
    QCOMPARE(replacement.state().request().acceptedRoleSet().primary(), true);

    const std::weak_ptr<kiriview::ImageViewportProviderResource> replacementResource
        = fixture.primaryResource;
    fixture.primaryResource.reset();
    QVERIFY(!replacementResource.expired());
    runtime.detach(&replacement);
    QTRY_VERIFY(replacementResource.expired());
    QCOMPARE(replacement.state().request().status(), ImageViewportRequestStatus::NoRequest);

    runtime.attach(&first);
    QTRY_COMPARE(fixture.primaryFactoryCalls, 3);
    QCOMPARE(first.state().request().acceptedRoleSet().primary(), true);
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

void TestImageViewportIntegrationRuntime::
    reentrantResourceFactoryKeepsNewerTargetAuthoritative_data()
{
    QTest::addColumn<bool>("reenterFromSecondaryFactory");

    QTest::newRow("primary factory") << false;
    QTest::newRow("secondary factory") << true;
}

void TestImageViewportIntegrationRuntime::reentrantResourceFactoryKeepsNewerTargetAuthoritative()
{
    QFETCH(bool, reenterFromSecondaryFactory);

    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture newer;
    newer.generation = 101;
    newer.primaryUrl = QUrl(QStringLiteral("file:///tmp/reentrant-shape.png"));
    if (!reenterFromSecondaryFactory) {
        newer.secondaryUrl = QUrl(QStringLiteral("file:///tmp/reentrant-newer-secondary.png"));
    }

    bool nestedAccepted = false;
    TargetFixture superseded;
    superseded.generation = 101;
    superseded.primaryUrl = newer.primaryUrl;
    if (reenterFromSecondaryFactory) {
        superseded.secondaryUrl
            = QUrl(QStringLiteral("file:///tmp/reentrant-superseded-secondary.png"));
        superseded.secondaryFactoryCallback = [&runtime, &newer, &nestedAccepted]() {
            nestedAccepted = runtime.submitTarget(newer.target());
        };
    } else {
        superseded.primaryFactoryCallback = [&runtime, &newer, &nestedAccepted]() {
            nestedAccepted = runtime.submitTarget(newer.target());
        };
    }

    QVERIFY(runtime.submitTarget(superseded.target()));
    QVERIFY(nestedAccepted);
    QTRY_COMPARE(newer.primarySource->pendingFrames.size(), std::size_t(1));
    newer.primarySource->completeNext(QStringLiteral("reentrant-newer"));
    if (newer.secondarySource != nullptr) {
        QTRY_COMPARE(newer.secondarySource->pendingFrames.size(), std::size_t(1));
        newer.secondarySource->completeNext(QStringLiteral("reentrant-newer-secondary"));
    }

    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(runtime.projection().sourceGeneration, newer.generation);
    QCOMPARE(runtime.projection().displayedUrl, newer.primaryUrl);
    QCOMPARE(runtime.projection().secondaryUrl, newer.secondaryUrl);
    QCOMPARE(
        viewport.state().request().acceptedRoleSet().secondary(), !newer.secondaryUrl.isEmpty());
}

void TestImageViewportIntegrationRuntime::
    reentrantProjectionDetachmentLeavesTargetPendingForReattachment()
{
    ImageViewport first;
    ImageViewport replacement;
    first.setSize(QSizeF(100, 80));
    replacement.setSize(QSizeF(100, 80));
    bool detached = false;
    kiriview::ImageViewportIntegrationRuntime* runtimePointer = nullptr;
    kiriview::ImageViewportIntegrationRuntime runtime(
        { [&first, &detached, &runtimePointer](
              const kiriview::ImageViewportIntegrationProjection& projection) {
            if (!detached && projection.correlated) {
                detached = true;
                runtimePointer->detach(&first);
            }
        } });
    runtimePointer = &runtime;
    runtime.attach(&first);

    TargetFixture fixture;
    fixture.generation = 103;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/reentrant-detach.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));
    QVERIFY(detached);
    QVERIFY(!runtime.projection().correlated);
    QCOMPARE(first.state().request().status(), ImageViewportRequestStatus::NoRequest);

    runtime.attach(&replacement);
    QCOMPARE(replacement.state().request().status(), ImageViewportRequestStatus::Loading);
    QVERIFY(runtime.projection().correlated);
    QCOMPARE(runtime.projection().sourceGeneration, fixture.generation);
}

void TestImageViewportIntegrationRuntime::projectionCallbackRetainsPublishedSnapshotAcrossReentry()
{
    ImageViewport viewport;
    viewport.setSize(QSizeF(100, 80));
    bool correlatedProjectionObserved = false;
    bool nestedProjectionObserved = false;
    bool publishedSnapshotRemainedStable = false;
    kiriview::ImageViewportIntegrationRuntime* runtimePointer = nullptr;
    kiriview::ImageViewportIntegrationRuntime runtime(
        { [&correlatedProjectionObserved, &nestedProjectionObserved,
              &publishedSnapshotRemainedStable,
              &runtimePointer](const kiriview::ImageViewportIntegrationProjection& projection) {
            if (!projection.correlated) {
                nestedProjectionObserved = correlatedProjectionObserved;
                return;
            }
            if (correlatedProjectionObserved) {
                return;
            }

            correlatedProjectionObserved = true;
            const quint64 publishedGeneration = projection.sourceGeneration;
            runtimePointer->clearTarget();
            publishedSnapshotRemainedStable
                = projection.correlated && projection.sourceGeneration == publishedGeneration;
        } });
    runtimePointer = &runtime;
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = 107;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/reentrant-projection-snapshot.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));

    QVERIFY(correlatedProjectionObserved);
    QVERIFY(nestedProjectionObserved);
    QVERIFY(publishedSnapshotRemainedStable);
    QVERIFY(!runtime.projection().correlated);
}

void TestImageViewportIntegrationRuntime::reentrantClearCannotEraseNewerTarget()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture initial;
    initial.generation = 104;
    initial.primaryUrl = QUrl(QStringLiteral("file:///tmp/reentrant-clear-initial.png"));
    QVERIFY(runtime.submitTarget(initial.target()));

    TargetFixture newer;
    newer.generation = 105;
    newer.primaryUrl = QUrl(QStringLiteral("file:///tmp/reentrant-clear-newer.png"));
    bool reentered = false;
    bool nestedAccepted = false;
    const QMetaObject::Connection connection
        = QObject::connect(&viewport, &ImageViewport::stateChanged, &viewport, [&]() {
              if (!reentered
                  && viewport.state().request().status() == ImageViewportRequestStatus::NoRequest) {
                  reentered = true;
                  nestedAccepted = runtime.submitTarget(newer.target());
              }
          });

    runtime.clearTarget();
    QObject::disconnect(connection);
    QVERIFY(reentered);
    QVERIFY(nestedAccepted);
    QTRY_COMPARE(newer.primarySource->pendingFrames.size(), std::size_t(1));
    newer.primarySource->completeNext(QStringLiteral("reentrant-clear-newer"));

    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(runtime.projection().sourceGeneration, newer.generation);
    QCOMPARE(runtime.projection().displayedUrl, newer.primaryUrl);
}

void TestImageViewportIntegrationRuntime::reentrantAttachmentReplacementKeepsLatestAttachment()
{
    ImageViewport first;
    ImageViewport supersededReplacement;
    ImageViewport latestReplacement;
    first.setSize(QSizeF(100, 80));
    supersededReplacement.setSize(QSizeF(100, 80));
    latestReplacement.setSize(QSizeF(100, 80));
    bool replaceAgain = false;
    kiriview::ImageViewportIntegrationRuntime* runtimePointer = nullptr;
    kiriview::ImageViewportIntegrationRuntime runtime(
        { [&latestReplacement, &replaceAgain, &runtimePointer](
              const kiriview::ImageViewportIntegrationProjection& projection) {
            if (replaceAgain && !projection.correlated) {
                replaceAgain = false;
                runtimePointer->attach(&latestReplacement);
            }
        } });
    runtimePointer = &runtime;
    runtime.attach(&first);

    TargetFixture fixture;
    fixture.generation = 106;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/reentrant-attachment.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));

    replaceAgain = true;
    runtime.attach(&supersededReplacement);

    QCOMPARE(
        supersededReplacement.state().request().status(), ImageViewportRequestStatus::NoRequest);
    QCOMPARE(latestReplacement.state().request().status(), ImageViewportRequestStatus::Loading);
    QVERIFY(runtime.projection().correlated);
    QCOMPARE(runtime.projection().sourceGeneration, fixture.generation);
}

void TestImageViewportIntegrationRuntime::secondaryTargetRequiresSessionIdentity()
{
    kiriview::ImageViewportIntegrationRuntime runtime;

    TargetFixture secondary;
    secondary.generation = 40;
    secondary.primaryUrl = QUrl(QStringLiteral("file:///tmp/primary.png"));
    secondary.secondaryUrl = QUrl(QStringLiteral("file:///tmp/secondary.png"));
    kiriview::ImageViewportIntegrationTarget missingIdentity = secondary.target();
    missingIdentity.secondarySessionId = 0;
    QVERIFY(!runtime.submitTarget(std::move(missingIdentity)));

    TargetFixture primaryOnly;
    primaryOnly.generation = 41;
    primaryOnly.primaryUrl = QUrl(QStringLiteral("file:///tmp/primary-only.png"));
    kiriview::ImageViewportIntegrationTarget orphanedIdentity = primaryOnly.target();
    orphanedIdentity.secondarySessionId = 17;
    QVERIFY(!runtime.submitTarget(std::move(orphanedIdentity)));

    QCOMPARE(secondary.primaryFactoryCalls, 0);
    QCOMPARE(secondary.secondaryFactoryCalls, 0);
    QCOMPARE(primaryOnly.primaryFactoryCalls, 0);
    QVERIFY(!runtime.projection().correlated);
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
    QCOMPARE(runtime.projection().secondarySessionId, fixture.generation);
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

void TestImageViewportIntegrationRuntime::readyTwoRoleTargetReattachesWithExactRoleSet()
{
    bool observeReattachment = false;
    bool observedAcceptedReattachment = false;
    bool acceptedRoleSetDegraded = false;
    bool displayedRoleSetDegraded = false;
    constexpr quint64 generation = 42;
    constexpr quint64 secondarySessionId = 420;
    const QUrl secondaryUrl(QStringLiteral("file:///tmp/reattached-secondary.png"));
    bool projectionShapeDegraded = false;
    kiriview::ImageViewportIntegrationRuntime runtime(
        { [&](const kiriview::ImageViewportIntegrationProjection& projection) {
            if (!observeReattachment || !projection.correlated
                || projection.sourceGeneration != generation) {
                return;
            }
            projectionShapeDegraded = projectionShapeDegraded
                || projection.secondaryUrl != secondaryUrl
                || projection.secondarySessionId != secondarySessionId;
        } });
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = generation;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/reattached-primary.png"));
    fixture.secondaryUrl = secondaryUrl;
    fixture.secondarySessionId = secondarySessionId;
    QVERIFY(runtime.submitTarget(fixture.target()));
    QTRY_COMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(1));
    QTRY_COMPARE(fixture.secondarySource->pendingFrames.size(), std::size_t(1));
    fixture.primarySource->completeNext(QStringLiteral("initial-primary"));
    fixture.secondarySource->completeNext(QStringLiteral("initial-secondary"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QVERIFY(runtime.projection().completeAuthoritativeDisplayAvailable);
    QVERIFY(runtime.projection().secondaryVisible);

    const QMetaObject::Connection stateConnection
        = QObject::connect(&viewport, &ImageViewport::stateChanged, &viewport, [&]() {
              if (!observeReattachment) {
                  return;
              }
              const ImageViewportStateSnapshot snapshot = viewport.state();
              if (!snapshot.request().acceptedPresentationTargetGeneration().isValid()) {
                  return;
              }
              observedAcceptedReattachment = true;
              const ImageViewportRoleSet acceptedRoles = snapshot.request().acceptedRoleSet();
              acceptedRoleSetDegraded = acceptedRoleSetDegraded || !acceptedRoles.primary()
                  || !acceptedRoles.secondary();
              if (!snapshot.display().displayedPresentationTargetGeneration().isValid()) {
                  return;
              }
              const ImageViewportRoleSet displayedRoles = snapshot.display().displayedRoleSet();
              displayedRoleSetDegraded = displayedRoleSetDegraded || !displayedRoles.primary()
                  || !displayedRoles.secondary();
          });

    fixture.primaryPredecodedImage = displayPayload(QStringLiteral("reattached-primary"));
    fixture.secondarySource->authoritativeSeed
        = displayPayload(QStringLiteral("reattached-secondary"));
    runtime.detach(&viewport);
    observeReattachment = true;
    runtime.attach(&viewport);

    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready
            && runtime.projection().secondaryVisible;
    }));
    QObject::disconnect(stateConnection);

    QVERIFY(observedAcceptedReattachment);
    QVERIFY(!acceptedRoleSetDegraded);
    QVERIFY(!displayedRoleSetDegraded);
    QVERIFY(!projectionShapeDegraded);
    const ImageViewportStateSnapshot snapshot = viewport.state();
    QCOMPARE(snapshot.request().status(), ImageViewportRequestStatus::Ready);
    QVERIFY(snapshot.request().acceptedRoleSet().primary());
    QVERIFY(snapshot.request().acceptedRoleSet().secondary());
    QVERIFY(snapshot.display().belongsToAcceptedPresentationTarget());
    QVERIFY(snapshot.display().displayedRoleSet().primary());
    QVERIFY(snapshot.display().displayedRoleSet().secondary());
    QVERIFY(runtime.projection().completeAuthoritativeDisplayAvailable);
    const std::optional<kiriview::StaticDisplayImagePayload> primary
        = runtime.displayedImage(ImageViewportPageRole::Primary);
    const std::optional<kiriview::StaticDisplayImagePayload> secondary
        = runtime.displayedImage(ImageViewportPageRole::Secondary);
    QVERIFY(primary.has_value());
    QVERIFY(secondary.has_value());
    QCOMPARE(primary->sourceIdentity, QStringLiteral("reattached-primary"));
    QCOMPARE(secondary->sourceIdentity, QStringLiteral("reattached-secondary"));
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

void TestImageViewportIntegrationRuntime::targetTransitionsApplyIntentSpecificFallbackPolicy_data()
{
    QTest::addColumn<int>("intent");
    QTest::newRow("same navigation scope")
        << static_cast<int>(kiriview::ImageViewportTargetTransitionIntent::SameNavigationScope);
    QTest::newRow("outside navigation scope")
        << static_cast<int>(kiriview::ImageViewportTargetTransitionIntent::OutsideNavigationScope);
    QTest::newRow("presentation shape change")
        << static_cast<int>(kiriview::ImageViewportTargetTransitionIntent::PresentationShapeChange);
}

void TestImageViewportIntegrationRuntime::targetTransitionsApplyIntentSpecificFallbackPolicy()
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

    const bool shapeChange = replacement.intent
        == kiriview::ImageViewportTargetTransitionIntent::PresentationShapeChange;
    QCOMPARE(viewport.state().display().status(),
        shapeChange ? ImageViewportDisplayStatus::Empty : ImageViewportDisplayStatus::Retained);
    QCOMPARE(viewport.state().display().retained(), !shapeChange);
    QVERIFY(!viewport.state().display().belongsToAcceptedPresentationTarget());
    QCOMPARE(viewport.state().display().displayedRoleSet().primary(), !shapeChange);
    QVERIFY(!viewport.state().display().displayedRoleSet().secondary());
    QCOMPARE(
        viewport.state().display().displayedPresentationTargetGeneration().isValid(), !shapeChange);
    QCOMPARE(runtime.projection().displayedUrl, QUrl());
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Loading);
    for (const kiriview::ImageViewportIntegrationProjection& projection : projections) {
        if (projection.sourceGeneration == replacement.generation) {
            QCOMPARE(projection.displayedUrl, QUrl());
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

void TestImageViewportIntegrationRuntime::firstDisplayAutomaticallyRefinesWithoutInteraction()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = 66;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/automatic-first-display-refinement.png"));
    QVERIFY(runtime.submitTarget(fixture.target()));
    QTRY_COMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(1));
    fixture.primarySource->completeNext(
        QStringLiteral("first-display"), kiriview::DisplayImageQuality::FirstDisplay);
    QVERIFY(driveRenderUntil(window, [&fixture, &viewport]() {
        return viewport.state().request().status() == ImageViewportRequestStatus::Ready
            && fixture.primarySource->pendingFrames.size() == 1;
    }));
    QCOMPARE(
        viewport.state().primary().display().quality(), ImageViewportPayloadQuality::FirstDisplay);
    QVERIFY(!viewport.state().primary().display().currentForDemand());
    QVERIFY(runtime.projection().completeAuthoritativeDisplayAvailable);

    fixture.primarySource->completeNext(
        QStringLiteral("refined"), kiriview::DisplayImageQuality::BoundedDetail);
    QVERIFY(driveRenderUntil(window, [&runtime, &viewport]() {
        const std::optional<kiriview::StaticDisplayImagePayload> displayed
            = runtime.displayedImage(ImageViewportPageRole::Primary);
        return viewport.state().primary().display().quality()
            == ImageViewportPayloadQuality::BoundedDetail
            && viewport.state().primary().display().currentForDemand() && displayed.has_value()
            && displayed->sourceIdentity == QStringLiteral("refined");
    }));

    QCOMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Ready);
    QVERIFY(runtime.projection().completeAuthoritativeDisplayAvailable);
    QCOMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(0));
    const std::optional<kiriview::StaticDisplayImagePayload> refined
        = runtime.displayedImage(ImageViewportPageRole::Primary);
    QVERIFY(refined.has_value());
    QCOMPARE(refined->quality, kiriview::DisplayImageQuality::BoundedDetail);
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
    QCOMPARE(runtime.projection().displayedUrl, QUrl());
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Loading);

    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(runtime.projection().displayedUrl, replacement.primaryUrl);
}

void TestImageViewportIntegrationRuntime::
    failedShapeChangeKeepsRequestedTargetErrorWithoutPriorPixels()
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
    QCOMPARE(viewport.state().display().status(), ImageViewportDisplayStatus::Empty);
    QVERIFY(!viewport.state().display().displayedRoleSet().primary());
    QVERIFY(!viewport.state().display().displayedRoleSet().secondary());
    QVERIFY(!viewport.state().display().displayedPresentationTargetGeneration().isValid());
    QCOMPARE(runtime.projection().displayedUrl, QUrl());
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
    QCOMPARE(runtime.projection().displayedUrl, QUrl());
    QCOMPARE(viewport.state().display().status(), ImageViewportDisplayStatus::Empty);
    QVERIFY(!viewport.state().display().displayedPresentationTargetGeneration().isValid());

    runtime.clearTarget();
    QCOMPARE(runtime.projection().displayedUrl, QUrl());
}

void TestImageViewportIntegrationRuntime::deferredPrimaryUrlResolvesWithoutReplacingAcceptedTarget()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture fixture;
    fixture.generation = 91;
    fixture.primaryUrl = QUrl(QStringLiteral("file:///tmp/book.cbz"));
    fixture.primaryUrlResolved = false;
    QVERIFY(runtime.submitTarget(fixture.target()));
    QTRY_COMPARE(fixture.primarySource->pendingFrames.size(), std::size_t(1));
    const ImageViewportPresentationTargetGenerationToken acceptedGeneration
        = viewport.state().request().acceptedPresentationTargetGeneration();
    QVERIFY(acceptedGeneration.isValid());

    const QUrl resolvedPageUrl(QStringLiteral("zip:///tmp/book.cbz!/01.jpg"));
    QVERIFY(runtime.resolvePrimaryTargetUrl(fixture.generation, resolvedPageUrl));
    QCOMPARE(viewport.state().request().acceptedPresentationTargetGeneration(), acceptedGeneration);
    QVERIFY(!runtime.resolvePrimaryTargetUrl(fixture.generation, resolvedPageUrl));
    QVERIFY(!runtime.resolvePrimaryTargetUrl(fixture.generation + 1, resolvedPageUrl));

    fixture.primarySource->completeNext(QStringLiteral("resolved-page"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(viewport.state().request().acceptedPresentationTargetGeneration(), acceptedGeneration);
    QCOMPARE(runtime.projection().displayedUrl, resolvedPageUrl);
}

void TestImageViewportIntegrationRuntime::
    authoritativeDisplayExcludesProvisionalPixelsAndIncludesRetainedPixels()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    TargetFixture initial;
    initial.generation = 92;
    initial.primaryUrl = QUrl(QStringLiteral("file:///tmp/initial.png"));
    QVERIFY(runtime.submitTarget(initial.target()));
    QTRY_COMPARE(initial.primarySource->pendingFrames.size(), std::size_t(1));
    QVERIFY(!runtime.hasAuthoritativeDisplay());

    initial.primarySource->emitNextProvisional(QStringLiteral("initial-preview"));
    QVERIFY(driveRenderUntil(window, [&viewport]() {
        return viewport.state().display().phase() == ImageViewportDisplayPhase::CommittedActive;
    }));
    QCOMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Loading);
    QVERIFY(!runtime.hasAuthoritativeDisplay());

    initial.primarySource->completeNext(QStringLiteral("initial-authoritative"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QVERIFY(runtime.hasAuthoritativeDisplay());

    TargetFixture replacement;
    replacement.generation = 93;
    replacement.primaryUrl = QUrl(QStringLiteral("file:///tmp/replacement.png"));
    QVERIFY(runtime.submitTarget(replacement.target()));
    QTRY_COMPARE(replacement.primarySource->pendingFrames.size(), std::size_t(1));
    QCOMPARE(viewport.state().display().phase(), ImageViewportDisplayPhase::PreviousActive);
    QVERIFY(runtime.hasAuthoritativeDisplay());
}

void TestImageViewportIntegrationRuntime::
    completeAuthoritativeDisplayAvailabilityFollowsPresentationFallback()
{
    kiriview::ImageViewportIntegrationRuntime runtime;
    QQuickWindow window;
    ImageViewport viewport;
    hostViewport(window, viewport);
    runtime.attach(&viewport);

    QVERIFY(!runtime.projection().completeAuthoritativeDisplayAvailable);

    TargetFixture initial;
    initial.generation = 94;
    initial.primaryUrl = QUrl(QStringLiteral("file:///tmp/initial-preview.png"));
    QVERIFY(runtime.submitTarget(initial.target()));
    QTRY_COMPARE(initial.primarySource->pendingFrames.size(), std::size_t(1));
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Loading);
    QVERIFY(!runtime.projection().completeAuthoritativeDisplayAvailable);

    initial.primarySource->emitNextProvisional(QStringLiteral("initial-preview"));
    QVERIFY(driveRenderUntil(window, [&viewport]() {
        return viewport.state().display().phase() == ImageViewportDisplayPhase::CommittedActive;
    }));
    QCOMPARE(viewport.state().request().status(), ImageViewportRequestStatus::Loading);
    QVERIFY(!runtime.projection().completeAuthoritativeDisplayAvailable);

    TargetFixture previewReplacement;
    previewReplacement.generation = 95;
    previewReplacement.primaryUrl = QUrl(QStringLiteral("file:///tmp/preview-replacement.png"));
    QVERIFY(runtime.submitTarget(previewReplacement.target()));
    QTRY_COMPARE(previewReplacement.primarySource->pendingFrames.size(), std::size_t(1));
    QVERIFY(!viewport.state().display().retained());
    QVERIFY(!runtime.projection().completeAuthoritativeDisplayAvailable);

    previewReplacement.primarySource->completeNext(
        QStringLiteral("initial-complete"), kiriview::DisplayImageQuality::ThumbnailPreview);
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QCOMPARE(viewport.state().primary().display().quality(), ImageViewportPayloadQuality::Preview);
    QVERIFY(runtime.projection().completeAuthoritativeDisplayAvailable);

    TargetFixture replacement;
    replacement.generation = 96;
    replacement.primaryUrl = QUrl(QStringLiteral("file:///tmp/replacement.png"));
    replacement.intent = kiriview::ImageViewportTargetTransitionIntent::SameNavigationScope;
    QVERIFY(runtime.submitTarget(replacement.target()));
    QTRY_COMPARE(replacement.primarySource->pendingFrames.size(), std::size_t(1));
    QCOMPARE(runtime.projection().displayedUrl, QUrl());
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Loading);
    QCOMPARE(viewport.state().display().phase(), ImageViewportDisplayPhase::PreviousActive);
    QVERIFY(runtime.projection().completeAuthoritativeDisplayAvailable);

    replacement.primarySource->completeNext(QStringLiteral("replacement-complete"));
    QVERIFY(driveRenderUntil(window, [&runtime]() {
        return runtime.projection().status == kiriview::ImageDocumentStatus::Ready;
    }));
    QVERIFY(runtime.projection().completeAuthoritativeDisplayAvailable);

    TargetFixture shapeChange;
    shapeChange.generation = 97;
    shapeChange.primaryUrl = replacement.primaryUrl;
    shapeChange.secondaryUrl = QUrl(QStringLiteral("file:///tmp/replacement-secondary.png"));
    shapeChange.intent = kiriview::ImageViewportTargetTransitionIntent::PresentationShapeChange;
    QVERIFY(runtime.submitTarget(shapeChange.target()));
    QCOMPARE(viewport.state().display().status(), ImageViewportDisplayStatus::Empty);
    QVERIFY(!runtime.projection().completeAuthoritativeDisplayAvailable);

    runtime.clearTarget();
    QCOMPARE(runtime.projection().displayedUrl, QUrl());
    QVERIFY(!runtime.projection().completeAuthoritativeDisplayAvailable);
}

QTEST_MAIN(TestImageViewportIntegrationRuntime)

#include "tst_imageviewportintegrationruntime.moc"
