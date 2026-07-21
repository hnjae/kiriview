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
kiriview::StaticDisplayImagePayload displayPayload(QString sourceIdentity)
{
    QImage image(40, 20, QImage::Format_RGBA8888);
    image.fill(QColor(20, 40, 60, 255));
    return kiriview::StaticDisplayImagePayload {
        std::move(sourceIdentity),
        {},
        QSize(40, 20),
        std::move(image),
        kiriview::DisplayImageQuality::Exact,
        {},
        {},
        kiriview::DisplayImagePreviewOrigin::None,
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
        pendingFrames.push_back({ identity, std::move(completion) });
    }

    void cancel(const QVector<ImageSequenceProviderRequestToken>&) override { }
    void close() override { ++closeCount; }

    void completeNext(QString sourceIdentity)
    {
        QVERIFY(!pendingFrames.empty());
        PendingFrame pending = std::move(pendingFrames.front());
        pendingFrames.pop_front();
        pending.completion(pending.identity,
            kiriview::ImageViewportProviderFrameResult::ready(
                displayPayload(std::move(sourceIdentity)),
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
    std::optional<bool> priorTwoPageModeEnabled;
    std::shared_ptr<PendingProviderSource> primarySource
        = std::make_shared<PendingProviderSource>();
    std::shared_ptr<PendingProviderSource> secondarySource;
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
        result.priorTwoPageModeEnabled = priorTwoPageModeEnabled;
        result.primaryResource = [this]() {
            ++primaryFactoryCalls;
            return std::make_shared<kiriview::ImageViewportProviderResource>(generation,
                QStringLiteral("primary-%1").arg(generation), primarySource,
                std::make_shared<kiriview::DisplayImageStore>(1024 * 1024));
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
    void failedShapeChangeRestoresPriorApplicationPolicy();
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
    for (int attempt = 0;
        attempt < 100 && runtime.projection().status != kiriview::ImageDocumentStatus::Ready;
        ++attempt) {
        renderFrame(window);
        QTest::qWait(5);
    }
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Ready);
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
    for (int attempt = 0;
        attempt < 100 && runtime.projection().status != kiriview::ImageDocumentStatus::Ready;
        ++attempt) {
        renderFrame(window);
        QTest::qWait(5);
    }
    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Ready);

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
    for (int attempt = 0;
        attempt < 100 && runtime.projection().status != kiriview::ImageDocumentStatus::Ready;
        ++attempt) {
        renderFrame(window);
        QTest::qWait(5);
    }

    QCOMPARE(runtime.projection().status, kiriview::ImageDocumentStatus::Ready);
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

void TestImageViewportIntegrationRuntime::failedShapeChangeRestoresPriorApplicationPolicy()
{
    std::vector<bool> restoredPolicies;
    kiriview::ImageViewportIntegrationRuntime runtime(
        { {}, [&restoredPolicies](bool enabled) { restoredPolicies.push_back(enabled); } });
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
    for (int attempt = 0;
        attempt < 100 && runtime.projection().status != kiriview::ImageDocumentStatus::Ready;
        ++attempt) {
        renderFrame(window);
        QTest::qWait(5);
    }
    QCOMPARE(runtime.projection().displayedUrl, initial.primaryUrl);

    TargetFixture shape;
    shape.generation = 62;
    shape.primaryUrl = initial.primaryUrl;
    shape.secondaryUrl = QUrl(QStringLiteral("file:///tmp/missing-secondary.png"));
    shape.intent = kiriview::ImageViewportTargetTransitionIntent::PresentationShapeChange;
    shape.priorTwoPageModeEnabled = false;
    QVERIFY(runtime.submitTarget(shape.target()));
    QTRY_COMPARE(shape.secondarySource->pendingFrames.size(), std::size_t(1));
    shape.secondarySource->failNext(loadFailure(shape.secondaryUrl, 620));
    for (int attempt = 0; attempt < 100 && !runtime.projection().restoredTransition; ++attempt) {
        renderFrame(window);
        QTest::qWait(5);
    }

    QVERIFY(runtime.projection().restoredTransition);
    QCOMPARE(runtime.projection().sourceGeneration, quint64(62));
    QCOMPARE(runtime.projection().displayedUrl, initial.primaryUrl);
    QCOMPARE(restoredPolicies.size(), std::size_t(1));
    QCOMPARE(restoredPolicies.front(), false);
}

QTEST_MAIN(TestImageViewportIntegrationRuntime)

#include "tst_imageviewportintegrationruntime.moc"
