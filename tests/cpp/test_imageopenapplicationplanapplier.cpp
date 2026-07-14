// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imagedocumentstate.h"
#include "document/imageopenapplicationplanapplier.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <utility>
#include <variant>
#include <vector>

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

template <typename Operation> bool hasOperation(const kiriview::ImageDocumentRuntimePlan& plan)
{
    for (const kiriview::ImageDocumentRuntimeOperation& operation : plan) {
        if (std::holds_alternative<Operation>(operation)) {
            return true;
        }
    }
    return false;
}
}

class TestImageOpenApplicationPlanApplier : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void validPlanAppliesStateAndReturnsEffects();
    void invalidReadyWithErrorPlanDoesNotMutateStateOrRunEffects();
    void invalidReadyWithEmptySourcePlanDoesNotMutateStateOrRunEffects();
    void invalidReadyVideoSourceWithoutUnsupportedFlagDoesNotMutateStateOrRunEffects();
    void invalidReadyWithUnrelatedContainerNavigationPlanDoesNotMutateStateOrRunEffects();
    void invalidNullWithContainerNavigationPlanDoesNotMutateStateOrRunEffects();
};

void TestImageOpenApplicationPlanApplier::validPlanAppliesStateAndReturnsEffects()
{
    kiriview::ImageDocumentState state;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/page.png"));
    state.setLoading(true);
    state.setStatus(kiriview::ImageDocumentStatus::Loading);

    kiriview::ImageOpenApplicationPlan applicationPlan;
    applicationPlan.stateDelta.sourceUrl = imageUrl;
    applicationPlan.stateDelta.displayedLocation
        = kiriview::DisplayedImageLocation::fromUrl(imageUrl);
    applicationPlan.stateDelta.containerNavigationUrl = QUrl();
    applicationPlan.stateDelta.loading = false;
    applicationPlan.stateDelta.status = kiriview::ImageDocumentStatus::Ready;
    applicationPlan.stateDelta.errorString = QString();
    applicationPlan.runtimePlan.push_back(kiriview::UpdatePageNavigationOperation {});

    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::applyImageOpenApplicationPlan(state, std::move(applicationPlan));

    QCOMPARE(state.sourceUrl(), imageUrl);
    QCOMPARE(state.displayedUrl(), imageUrl);
    QVERIFY(!state.loading());
    QCOMPARE(state.status(), kiriview::ImageDocumentStatus::Ready);
    QVERIFY(hasOperation<kiriview::UpdatePageNavigationOperation>(plan));
}

void TestImageOpenApplicationPlanApplier::invalidReadyWithErrorPlanDoesNotMutateStateOrRunEffects()
{
    std::vector<kiriview::ImageDocumentChange> changes;
    kiriview::ImageDocumentState state(
        [&changes](kiriview::ImageDocumentChange change) { changes.push_back(change); });
    const QUrl sourceUrl = localUrl(QStringLiteral("/images/current.png"));
    state.setSourceUrl(sourceUrl);
    state.setLoading(true);
    state.setStatus(kiriview::ImageDocumentStatus::Loading);
    changes.clear();

    kiriview::ImageOpenApplicationPlan applicationPlan;
    applicationPlan.stateDelta.loading = false;
    applicationPlan.stateDelta.status = kiriview::ImageDocumentStatus::Ready;
    applicationPlan.stateDelta.errorString = QStringLiteral("late error");
    applicationPlan.runtimePlan.push_back(kiriview::UpdatePageNavigationOperation {});

    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::applyImageOpenApplicationPlan(state, std::move(applicationPlan));

    QCOMPARE(state.sourceUrl(), sourceUrl);
    QVERIFY(state.loading());
    QCOMPARE(state.status(), kiriview::ImageDocumentStatus::Loading);
    QVERIFY(state.errorString().isEmpty());
    QVERIFY(plan.empty());
    QVERIFY(changes.empty());
}

void TestImageOpenApplicationPlanApplier::
    invalidReadyWithEmptySourcePlanDoesNotMutateStateOrRunEffects()
{
    std::vector<kiriview::ImageDocumentChange> changes;
    kiriview::ImageDocumentState state(
        [&changes](kiriview::ImageDocumentChange change) { changes.push_back(change); });
    const QUrl sourceUrl = localUrl(QStringLiteral("/images/current.png"));
    const QUrl displayedUrl = localUrl(QStringLiteral("/images/displayed.png"));
    state.setSourceUrl(sourceUrl);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(displayedUrl));
    state.setStatus(kiriview::ImageDocumentStatus::Loading);
    state.setLoading(true);
    changes.clear();

    kiriview::ImageOpenApplicationPlan applicationPlan;
    applicationPlan.stateDelta.sourceUrl = QUrl();
    applicationPlan.stateDelta.loading = false;
    applicationPlan.stateDelta.status = kiriview::ImageDocumentStatus::Ready;
    applicationPlan.stateDelta.errorString = QString();
    applicationPlan.runtimePlan.push_back(kiriview::UpdatePageNavigationOperation {});

    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::applyImageOpenApplicationPlan(state, std::move(applicationPlan));

    QCOMPARE(state.sourceUrl(), sourceUrl);
    QCOMPARE(state.displayedUrl(), displayedUrl);
    QVERIFY(state.loading());
    QCOMPARE(state.status(), kiriview::ImageDocumentStatus::Loading);
    QVERIFY(plan.empty());
    QVERIFY(changes.empty());
}

void TestImageOpenApplicationPlanApplier::
    invalidReadyVideoSourceWithoutUnsupportedFlagDoesNotMutateStateOrRunEffects()
{
    std::vector<kiriview::ImageDocumentChange> changes;
    kiriview::ImageDocumentState state(
        [&changes](kiriview::ImageDocumentChange change) { changes.push_back(change); });
    const QUrl sourceUrl = localUrl(QStringLiteral("/images/current.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/archive/video.mp4"));
    state.setSourceUrl(sourceUrl);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(sourceUrl));
    state.setStatus(kiriview::ImageDocumentStatus::Loading);
    state.setLoading(true);
    changes.clear();

    kiriview::ImageOpenApplicationPlan applicationPlan;
    applicationPlan.stateDelta.sourceUrl = videoUrl;
    applicationPlan.stateDelta.sourceKind = kiriview::ImageDocumentPageKind::Video;
    applicationPlan.stateDelta.displayedLocation
        = kiriview::DisplayedImageLocation::fromUrl(videoUrl);
    applicationPlan.stateDelta.loading = false;
    applicationPlan.stateDelta.status = kiriview::ImageDocumentStatus::Ready;
    applicationPlan.stateDelta.errorString = QString();
    applicationPlan.runtimePlan.push_back(kiriview::UpdatePageNavigationOperation {});

    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::applyImageOpenApplicationPlan(state, std::move(applicationPlan));

    QCOMPARE(state.sourceUrl(), sourceUrl);
    QCOMPARE(state.displayedUrl(), sourceUrl);
    QCOMPARE(state.sourceKind(), kiriview::ImageDocumentPageKind::Image);
    QVERIFY(!state.unsupportedOpenedCollectionVideo());
    QVERIFY(state.loading());
    QCOMPARE(state.status(), kiriview::ImageDocumentStatus::Loading);
    QVERIFY(plan.empty());
    QVERIFY(changes.empty());
}

void TestImageOpenApplicationPlanApplier::
    invalidReadyWithUnrelatedContainerNavigationPlanDoesNotMutateStateOrRunEffects()
{
    std::vector<kiriview::ImageDocumentChange> changes;
    kiriview::ImageDocumentState state(
        [&changes](kiriview::ImageDocumentChange change) { changes.push_back(change); });
    const QUrl sourceUrl = localUrl(QStringLiteral("/images/current.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/images/next.png"));
    const QUrl unrelatedContainerUrl = localUrl(QStringLiteral("/books/book.cbz"));
    state.setSourceUrl(sourceUrl);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(sourceUrl));
    state.setStatus(kiriview::ImageDocumentStatus::Loading);
    state.setLoading(true);
    changes.clear();

    kiriview::ImageOpenApplicationPlan applicationPlan;
    applicationPlan.stateDelta.sourceUrl = nextUrl;
    applicationPlan.stateDelta.displayedLocation
        = kiriview::DisplayedImageLocation::fromUrl(nextUrl);
    applicationPlan.stateDelta.containerNavigationUrl = unrelatedContainerUrl;
    applicationPlan.stateDelta.loading = false;
    applicationPlan.stateDelta.status = kiriview::ImageDocumentStatus::Ready;
    applicationPlan.stateDelta.errorString = QString();
    applicationPlan.runtimePlan.push_back(kiriview::UpdatePageNavigationOperation {});

    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::applyImageOpenApplicationPlan(state, std::move(applicationPlan));

    QCOMPARE(state.sourceUrl(), sourceUrl);
    QCOMPARE(state.displayedUrl(), sourceUrl);
    QVERIFY(state.containerNavigationUrl().isEmpty());
    QVERIFY(state.loading());
    QCOMPARE(state.status(), kiriview::ImageDocumentStatus::Loading);
    QVERIFY(plan.empty());
    QVERIFY(changes.empty());
}

void TestImageOpenApplicationPlanApplier::
    invalidNullWithContainerNavigationPlanDoesNotMutateStateOrRunEffects()
{
    std::vector<kiriview::ImageDocumentChange> changes;
    kiriview::ImageDocumentState state(
        [&changes](kiriview::ImageDocumentChange change) { changes.push_back(change); });
    const QUrl sourceUrl = localUrl(QStringLiteral("/images/current.png"));
    const QUrl containerUrl = localUrl(QStringLiteral("/images/"));
    state.setSourceUrl(sourceUrl);
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(sourceUrl));
    state.setStatus(kiriview::ImageDocumentStatus::Loading);
    state.setLoading(true);
    changes.clear();

    kiriview::ImageOpenApplicationPlan applicationPlan;
    applicationPlan.stateDelta.containerNavigationUrl = containerUrl;
    applicationPlan.stateDelta.loading = false;
    applicationPlan.stateDelta.status = kiriview::ImageDocumentStatus::Null;
    applicationPlan.stateDelta.errorString = QString();
    applicationPlan.runtimePlan.push_back(kiriview::ClearPresentationImageOperation {});

    const kiriview::ImageDocumentRuntimePlan plan
        = kiriview::applyImageOpenApplicationPlan(state, std::move(applicationPlan));

    QCOMPARE(state.sourceUrl(), sourceUrl);
    QCOMPARE(state.displayedUrl(), sourceUrl);
    QVERIFY(state.containerNavigationUrl().isEmpty());
    QVERIFY(state.loading());
    QCOMPARE(state.status(), kiriview::ImageDocumentStatus::Loading);
    QVERIFY(plan.empty());
    QVERIFY(changes.empty());
}

QTEST_GUILESS_MAIN(TestImageOpenApplicationPlanApplier)

#include "test_imageopenapplicationplanapplier.moc"
