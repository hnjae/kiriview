// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailjobexecutor.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <memory>
#include <utility>
#include <vector>

namespace {
using Bucket = kiriview::ActiveNavigationThumbnailDemandBucket;

QImage image(QColor color)
{
    QImage result(QSize(2, 1), QImage::Format_RGBA8888);
    result.fill(color);
    return result;
}

kiriview::ActiveNavigationThumbnailWorkRequest request(
    quint64 id, kiriview::ThumbnailSourceAdapterPlanKind planKind)
{
    const QByteArray path("/media/one.png");
    kiriview::ThumbnailSourceAdapterPlan plan;
    plan.kind = planKind;
    plan.localPathBytes = path;
    plan.originalIdentity = kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(path);
    auto sourceKey
        = kiriview::thumbnailSourceRevisionKey(1, QUrl::fromLocalFile(QString::fromUtf8(path)),
            QStringLiteral("one.png"), QStringLiteral("image"),
            kiriview::activeNavigationThumbnailSourceKindIdentity(
                kiriview::ActiveNavigationThumbnailSourceKind::DirectImage),
            7);
    return {
        { id },
        std::move(sourceKey),
        Bucket::Large,
        kiriview::ActiveNavigationThumbnailWorkKind::Foreground,
        std::move(plan),
    };
}

struct ManualProviders
{
    kiriview::ThumbnailCacheLookupProvider lookup()
    {
        return [this](QObject*, kiriview::ThumbnailCacheLookupRequest request,
                   kiriview::ThumbnailCacheLookupCallback callback) {
            lookupRequests.push_back(std::move(request));
            lookupCallbacks.push_back(std::move(callback));
            return kiriview::ImageIoJob {};
        };
    }

    kiriview::ThumbnailGenerationProvider generation()
    {
        return [this](QObject*, kiriview::ThumbnailGenerationRequest request,
                   kiriview::ThumbnailGenerationCallback callback) {
            generationRequests.push_back(std::move(request));
            generationCallbacks.push_back(std::move(callback));
            return kiriview::ImageIoJob {};
        };
    }

    std::vector<kiriview::ThumbnailCacheLookupRequest> lookupRequests;
    std::vector<kiriview::ThumbnailCacheLookupCallback> lookupCallbacks;
    std::vector<kiriview::ThumbnailGenerationRequest> generationRequests;
    std::vector<kiriview::ThumbnailGenerationCallback> generationCallbacks;
};
}

class TestActiveNavigationThumbnailJobExecutor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cacheHitCompletesWithRequestIdentity();
    void cacheMissKeepsWorkIdentityAcrossGeneration();
    void readyGenerationPreservesCacheInstallDiagnostic();
    void inMemoryWorkSkipsLookupAndReportsTypedFailures();
    void resourceLimitFailurePreservesTypedKind();
    void cancellationAndPhaseChangesRejectLateCallbacks();
    void synchronousCompletionAndDestructionAreCallbackSafe();
};

void TestActiveNavigationThumbnailJobExecutor::cacheHitCompletesWithRequestIdentity()
{
    ManualProviders providers;
    std::vector<kiriview::ActiveNavigationThumbnailWorkCompletion> completions;
    kiriview::ActiveNavigationThumbnailJobExecutor executor(this, providers.lookup(),
        providers.generation(),
        [&](auto completion) { completions.push_back(std::move(completion)); });

    QVERIFY(
        executor.start(request(41, kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile)));
    QCOMPARE(providers.lookupCallbacks.size(), std::size_t(1));
    providers.lookupCallbacks.front()({ kiriview::ThumbnailCacheLookupStatus::Ready,
        image(Qt::green), Bucket::Large, Bucket::Large, {}, {} });

    QCOMPARE(completions.size(), std::size_t(1));
    QCOMPARE(completions.front().workId.value, quint64(41));
    QCOMPARE(completions.front().bucket, Bucket::Large);
    QVERIFY(std::holds_alternative<kiriview::ActiveNavigationThumbnailReadyWorkResult>(
        completions.front().result));
}

void TestActiveNavigationThumbnailJobExecutor::cacheMissKeepsWorkIdentityAcrossGeneration()
{
    ManualProviders providers;
    std::vector<kiriview::ActiveNavigationThumbnailWorkCompletion> completions;
    kiriview::ActiveNavigationThumbnailJobExecutor executor(this, providers.lookup(),
        providers.generation(),
        [&](auto completion) { completions.push_back(std::move(completion)); });

    executor.start(request(9, kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile));
    providers.lookupCallbacks.front()(
        { kiriview::ThumbnailCacheLookupStatus::Missing, {}, Bucket::Large });
    QCOMPARE(providers.generationCallbacks.size(), std::size_t(1));
    providers.generationCallbacks.front()(
        { kiriview::ThumbnailGenerationStatus::Ready, {}, image(Qt::blue), Bucket::Large });

    QCOMPARE(completions.size(), std::size_t(1));
    QCOMPARE(completions.front().workId.value, quint64(9));
    const auto& ready
        = std::get<kiriview::ActiveNavigationThumbnailReadyWorkResult>(completions.front().result);
    QCOMPARE(ready.image.pixelColor(0, 0), QColor(Qt::blue));
    QVERIFY(!ready.diagnostic.has_value());
}

void TestActiveNavigationThumbnailJobExecutor::readyGenerationPreservesCacheInstallDiagnostic()
{
    ManualProviders providers;
    std::vector<kiriview::ActiveNavigationThumbnailWorkCompletion> completions;
    kiriview::ActiveNavigationThumbnailJobExecutor executor(this, providers.lookup(),
        providers.generation(),
        [&](auto completion) { completions.push_back(std::move(completion)); });

    executor.start(request(10, kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile));
    providers.lookupCallbacks.front()(
        { kiriview::ThumbnailCacheLookupStatus::Missing, {}, Bucket::Large });
    providers.generationCallbacks.front()({ kiriview::ThumbnailGenerationStatus::Ready, {},
        image(Qt::blue), Bucket::Large, {}, QStringLiteral("cache install failed"),
        kiriview::ThumbnailGenerationDiagnosticKind::CacheInstallFailed });

    QCOMPARE(completions.size(), std::size_t(1));
    const auto& ready
        = std::get<kiriview::ActiveNavigationThumbnailReadyWorkResult>(completions.front().result);
    QCOMPARE(ready.image.pixelColor(0, 0), QColor(Qt::blue));
    QVERIFY(ready.diagnostic.has_value());
    QCOMPARE(ready.diagnostic->kind,
        kiriview::ActiveNavigationThumbnailDiagnosticKind::CacheInstallFailed);
    QCOMPARE(ready.diagnostic->errorString, QStringLiteral("cache install failed"));
}

void TestActiveNavigationThumbnailJobExecutor::inMemoryWorkSkipsLookupAndReportsTypedFailures()
{
    ManualProviders providers;
    std::vector<kiriview::ActiveNavigationThumbnailWorkCompletion> completions;
    kiriview::ActiveNavigationThumbnailJobExecutor executor(this, providers.lookup(),
        providers.generation(),
        [&](auto completion) { completions.push_back(std::move(completion)); });

    executor.start(request(3, kiriview::ThumbnailSourceAdapterPlanKind::InMemoryOnly));
    QVERIFY(providers.lookupRequests.empty());
    QVERIFY(!providers.generationRequests.front().cacheInstallEnabled);
    providers.generationCallbacks.front()({ kiriview::ThumbnailGenerationStatus::Failed, {}, {},
        Bucket::Large, {}, QStringLiteral("decode failed") });
    const auto& generationFailure
        = std::get<kiriview::ActiveNavigationThumbnailFailedWorkResult>(completions.front().result);
    QCOMPARE(generationFailure.failureKind,
        kiriview::ActiveNavigationThumbnailFailureKind::GenerationFailed);
    QCOMPARE(generationFailure.errorString, QStringLiteral("decode failed"));

    completions.clear();
    kiriview::ActiveNavigationThumbnailJobExecutor unavailable(
        this, {}, {}, [&](auto completion) { completions.push_back(std::move(completion)); });
    unavailable.start(request(4, kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile));
    const auto& unavailableFailure
        = std::get<kiriview::ActiveNavigationThumbnailFailedWorkResult>(completions.front().result);
    QCOMPARE(unavailableFailure.failureKind,
        kiriview::ActiveNavigationThumbnailFailureKind::CacheLookupProviderUnavailable);
}

void TestActiveNavigationThumbnailJobExecutor::resourceLimitFailurePreservesTypedKind()
{
    ManualProviders providers;
    std::vector<kiriview::ActiveNavigationThumbnailWorkCompletion> completions;
    kiriview::ActiveNavigationThumbnailJobExecutor executor(this, providers.lookup(),
        providers.generation(),
        [&](auto completion) { completions.push_back(std::move(completion)); });

    QVERIFY(executor.start(request(17, kiriview::ThumbnailSourceAdapterPlanKind::InMemoryOnly)));
    QCOMPARE(providers.generationCallbacks.size(), std::size_t(1));
    providers.generationCallbacks.front()(
        { kiriview::ThumbnailGenerationStatus::ResourceLimitExceeded, {}, {}, Bucket::Large, {},
            QStringLiteral("workspace limit exceeded") });

    QCOMPARE(completions.size(), std::size_t(1));
    const auto& failure
        = std::get<kiriview::ActiveNavigationThumbnailFailedWorkResult>(completions.front().result);
    QCOMPARE(
        failure.failureKind, kiriview::ActiveNavigationThumbnailFailureKind::ResourceLimitExceeded);
    QCOMPARE(failure.errorString, QStringLiteral("workspace limit exceeded"));
}

void TestActiveNavigationThumbnailJobExecutor::cancellationAndPhaseChangesRejectLateCallbacks()
{
    ManualProviders providers;
    std::vector<kiriview::ActiveNavigationThumbnailWorkCompletion> completions;
    kiriview::ActiveNavigationThumbnailJobExecutor executor(this, providers.lookup(),
        providers.generation(),
        [&](auto completion) { completions.push_back(std::move(completion)); });

    executor.start(request(5, kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile));
    const auto lookupCallback = providers.lookupCallbacks.front();
    lookupCallback({ kiriview::ThumbnailCacheLookupStatus::Missing, {}, Bucket::Large });
    lookupCallback({ kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::red), Bucket::Large });
    QVERIFY(completions.empty());
    QVERIFY(executor.cancel({ 5 }));
    providers.generationCallbacks.front()(
        { kiriview::ThumbnailGenerationStatus::Ready, {}, image(Qt::green), Bucket::Large });
    QVERIFY(completions.empty());
}

void TestActiveNavigationThumbnailJobExecutor::synchronousCompletionAndDestructionAreCallbackSafe()
{
    int canceledReturnedJobs = 0;
    std::vector<kiriview::ActiveNavigationThumbnailWorkCompletion> completions;
    kiriview::ThumbnailGenerationProvider synchronous
        = [&](QObject*, kiriview::ThumbnailGenerationRequest request,
              kiriview::ThumbnailGenerationCallback callback) {
              callback({ kiriview::ThumbnailGenerationStatus::Ready, {}, image(Qt::green),
                  request.requestedBucket });
              return kiriview::ImageIoJob(new QObject, [&](QObject* object) {
                  ++canceledReturnedJobs;
                  delete object;
              });
          };
    {
        kiriview::ActiveNavigationThumbnailJobExecutor executor(this, {}, synchronous,
            [&](auto completion) { completions.push_back(std::move(completion)); });
        executor.start(request(11, kiriview::ThumbnailSourceAdapterPlanKind::InMemoryOnly));
    }
    QCOMPARE(completions.size(), std::size_t(1));
    QCOMPARE(canceledReturnedJobs, 1);

    ManualProviders providers;
    auto executor = std::make_unique<kiriview::ActiveNavigationThumbnailJobExecutor>(this,
        providers.lookup(), providers.generation(),
        [&](auto completion) { completions.push_back(std::move(completion)); });
    executor->start(request(12, kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile));
    const auto forcedCallback = providers.lookupCallbacks.front();
    executor.reset();
    forcedCallback({ kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::red), Bucket::Large });
    QCOMPARE(completions.size(), std::size_t(1));
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailJobExecutor)

#include "tst_activenavigationthumbnailjobexecutor.moc"
