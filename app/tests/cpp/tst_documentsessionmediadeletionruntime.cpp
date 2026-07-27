// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionmediadeletionruntime.h"

#include "image_async_test_support.h"
#include "navigation/directmedianavigationcandidateprovider.h"
#include "session/directmediacursor.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <utility>
#include <vector>

class TestDocumentSessionMediaDeletionRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyTargetDoesNotStartFileOperation();
    void startRunsFileOperationAndPublishesCompletionPlan();
    void directMediaStartLoadsCandidatesBeforeFileOperation();
    void directMediaSingleCandidateSuccessClearsSession();
    void directMediaMultiCandidateWithoutNextUsesPrevious();
    void directMediaStartKeepsActualTargetSeparateFromNavigationIdentity();
    void directMediaCandidateFailureAbortsBeforeFileOperation();
    void directMediaCandidatePhaseIsOwnedByRuntime();
    void directMediaCandidateLoadCancelRejectsLateCompletion();
    void cancelRejectsLateCompletion();
    void cancellationInvalidatesBeforeFileProviderCallback();
    void synchronousCompletionPreservesReplacementJob();
    void replacementStartRejectsStaleCompletion();
    void failedCompletionReportsFailureWithTypedFailure();
};

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(const QUrl& url)
{
    return kiriview::DirectMediaNavigationCandidate { url, url.fileName(QUrl::PrettyDecoded) };
}

struct ManualDirectMediaNavigationCandidateLoad
{
    QObject* object = nullptr;
    QUrl parentUrl;
    kiriview::DirectMediaNavigationCandidatesCallback callback;
    kiriview::ErrorCallback errorCallback;
    kiriview::ImageIoJobCompletion completion;
    bool canceled = false;
};

class ManualDirectMediaNavigationCandidateProvider
{
public:
    kiriview::DirectMediaNavigationCandidateProvider provider()
    {
        return kiriview::DirectMediaNavigationCandidateProvider {
            [this](QObject* receiver, QUrl parentUrl,
                kiriview::DirectMediaNavigationCandidatesCallback callback,
                kiriview::ErrorCallback errorCallback) {
                auto load = std::make_shared<ManualDirectMediaNavigationCandidateLoad>();
                load->parentUrl = std::move(parentUrl);
                load->callback = std::move(callback);
                load->errorCallback = std::move(errorCallback);

                kiriview::ImageIoJob job
                    = kiriview::TestSupport::Detail::startManualIoJob(receiver, load);
                m_loads.push_back(load);
                return job;
            },
        };
    }

    std::size_t loadCount() const { return m_loads.size(); }

    ManualDirectMediaNavigationCandidateLoad& loadAt(std::size_t index)
    {
        return *m_loads.at(index);
    }

    void deliverIgnoringCancellation(
        std::size_t index, std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
    {
        ManualDirectMediaNavigationCandidateLoad& load = loadAt(index);
        if (load.callback) {
            load.callback(std::move(candidates));
        }
    }

    void failIgnoringCancellation(std::size_t index, const QString& errorString)
    {
        ManualDirectMediaNavigationCandidateLoad& load = loadAt(index);
        if (load.errorCallback) {
            load.errorCallback(errorString);
        }
    }

private:
    std::vector<std::shared_ptr<ManualDirectMediaNavigationCandidateLoad>> m_loads;
};

kiriview::DirectMediaScope directMediaScope(const QUrl& currentUrl)
{
    return *kiriview::DirectMediaScope::fromSource(
        kiriview::ResolvedNavigationSource(currentUrl, {}, currentUrl), 7);
}

template <typename Operation>
const Operation* operationAt(
    const kiriview::DocumentSessionMediaDeletionCompletionPlan& plan, std::size_t index)
{
    if (index >= plan.routePlan.mutations.size()) {
        return nullptr;
    }

    return std::get_if<Operation>(&plan.routePlan.mutations.at(index));
}

struct RuntimeFixture
{
    QObject receiver;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    ManualDirectMediaNavigationCandidateProvider candidateProvider;
    kiriview::DocumentSessionMediaDeletionRuntime runtime {
        kiriview::TestSupport::fileDeletionProviderFor(fileDeletionProvider),
        candidateProvider.provider(),
    };
    int completionCount = 0;
    kiriview::DocumentSessionMediaDeletionCompletion completion;
    bool acceptScope = true;

    kiriview::DocumentSessionMediaDeletionStartPlan start(kiriview::FileDeletionMode mode,
        std::vector<kiriview::DirectMediaNavigationCandidate> candidates, const QUrl& currentUrl,
        kiriview::DocumentSessionKind kind = kiriview::DocumentSessionKind::Video)
    {
        return runtime.start(&receiver, mode, std::move(candidates), currentUrl, currentUrl, kind,
            [this](kiriview::DocumentSessionMediaDeletionCompletion deletionCompletion) {
                ++completionCount;
                completion = std::move(deletionCompletion);
            });
    }

    bool startDirectMedia(kiriview::FileDeletionMode mode, const kiriview::DirectMediaScope& scope,
        kiriview::DocumentSessionKind kind = kiriview::DocumentSessionKind::Video)
    {
        return runtime.startForDirectMedia(
            &receiver, mode, scope,
            [this](const kiriview::DirectMediaScope&) { return acceptScope; }, kind,
            [this](kiriview::DocumentSessionMediaDeletionCompletion deletionCompletion) {
                ++completionCount;
                completion = std::move(deletionCompletion);
            });
    }
};
}

void TestDocumentSessionMediaDeletionRuntime::emptyTargetDoesNotStartFileOperation()
{
    RuntimeFixture fixture;

    const kiriview::DocumentSessionMediaDeletionStartPlan plan = fixture.start(
        kiriview::FileDeletionMode::MoveToTrash, {}, QUrl(), kiriview::DocumentSessionKind::Video);

    QVERIFY(!plan.shouldStartDeletion);
    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(0));
    QVERIFY(!fixture.runtime.active());
    QCOMPARE(fixture.completionCount, 0);
}

void TestDocumentSessionMediaDeletionRuntime::startRunsFileOperationAndPublishesCompletionPlan()
{
    RuntimeFixture fixture;
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.jpg"));
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    const kiriview::DocumentSessionMediaDeletionStartPlan startPlan = fixture.start(
        kiriview::FileDeletionMode::DeletePermanently,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(currentUrl),
            directMediaNavigationCandidate(nextUrl) },
        currentUrl);

    QVERIFY(startPlan.shouldStartDeletion);
    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fixture.fileDeletionProvider.backOperation().request.targetUrl, currentUrl);
    QCOMPARE(fixture.fileDeletionProvider.backOperation().request.mode,
        kiriview::FileDeletionMode::DeletePermanently);
    QVERIFY(fixture.runtime.active());
    QCOMPARE(startPlan.fallbackPlan.preferredFallbackUrl.value(), nextUrl);

    fixture.fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(!fixture.runtime.active());
    QVERIFY(fixture.completion.plan.hasRoutePlan());
    QCOMPARE(
        fixture.completion.plan.routePlan.kind, kiriview::DocumentSessionRouteKind::DirectImage);
    QCOMPARE(fixture.completion.plan.routePlan.sourceUrl, nextUrl);
    QVERIFY(
        operationAt<kiriview::LeaveVideoModeRouteOperation>(fixture.completion.plan, 0) != nullptr);
    QCOMPARE(fixture.completion.failure.userMessage, QString());
}

void TestDocumentSessionMediaDeletionRuntime::directMediaStartLoadsCandidatesBeforeFileOperation()
{
    RuntimeFixture fixture;
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.jpg"));
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));

    const bool started = fixture.startDirectMedia(
        kiriview::FileDeletionMode::MoveToTrash, directMediaScope(currentUrl));

    QVERIFY(started);
    QCOMPARE(fixture.candidateProvider.loadCount(), std::size_t(1));
    QCOMPARE(fixture.candidateProvider.loadAt(0).parentUrl, localUrl(QStringLiteral("/media/")));
    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(0));

    fixture.candidateProvider.deliverIgnoringCancellation(0,
        { directMediaNavigationCandidate(previousUrl), directMediaNavigationCandidate(currentUrl),
            directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(1));
    QCOMPARE(fixture.fileDeletionProvider.backOperation().request.targetUrl, currentUrl);

    fixture.fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(fixture.completion.plan.hasRoutePlan());
    QCOMPARE(fixture.completion.plan.routePlan.sourceUrl, nextUrl);
}

void TestDocumentSessionMediaDeletionRuntime::directMediaSingleCandidateSuccessClearsSession()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));

    QVERIFY(fixture.startDirectMedia(
        kiriview::FileDeletionMode::MoveToTrash, directMediaScope(currentUrl)));
    fixture.candidateProvider.deliverIgnoringCancellation(
        0, { directMediaNavigationCandidate(currentUrl) });

    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(1));
    fixture.fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(fixture.completion.plan.hasRoutePlan());
    QVERIFY(!fixture.completion.plan.reportFailure);
    QCOMPARE(fixture.completion.plan.routePlan.kind, kiriview::DocumentSessionRouteKind::Empty);
    QVERIFY(fixture.completion.plan.routePlan.sourceUrl.isEmpty());
    QCOMPARE(fixture.completion.failure.userMessage, QString());
}

void TestDocumentSessionMediaDeletionRuntime::directMediaMultiCandidateWithoutNextUsesPrevious()
{
    RuntimeFixture fixture;
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.jpg"));
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));

    QVERIFY(fixture.startDirectMedia(
        kiriview::FileDeletionMode::MoveToTrash, directMediaScope(currentUrl)));
    fixture.candidateProvider.deliverIgnoringCancellation(0,
        { directMediaNavigationCandidate(previousUrl),
            directMediaNavigationCandidate(currentUrl) });

    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(1));
    fixture.fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(fixture.completion.plan.hasRoutePlan());
    QVERIFY(!fixture.completion.plan.reportFailure);
    QCOMPARE(
        fixture.completion.plan.routePlan.kind, kiriview::DocumentSessionRouteKind::DirectImage);
    QCOMPARE(fixture.completion.plan.routePlan.sourceUrl, previousUrl);
    QCOMPARE(fixture.completion.failure.userMessage, QString());
}

void TestDocumentSessionMediaDeletionRuntime::
    directMediaStartKeepsActualTargetSeparateFromNavigationIdentity()
{
    RuntimeFixture fixture;
    const QUrl actualTargetUrl = localUrl(QStringLiteral("/portal/02.mp4"));
    const QUrl navigationIdentityUrl = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl previousUrl = localUrl(QStringLiteral("/media/01.jpg"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/03.png"));
    const kiriview::DirectMediaScope scope = *kiriview::DirectMediaScope::fromSource(
        kiriview::ResolvedNavigationSource(actualTargetUrl, {}, navigationIdentityUrl), 7);

    QVERIFY(fixture.startDirectMedia(kiriview::FileDeletionMode::MoveToTrash, scope));
    QCOMPARE(fixture.candidateProvider.loadAt(0).parentUrl, localUrl(QStringLiteral("/media/")));
    fixture.candidateProvider.deliverIgnoringCancellation(0,
        { directMediaNavigationCandidate(previousUrl),
            directMediaNavigationCandidate(navigationIdentityUrl),
            directMediaNavigationCandidate(nextUrl) });

    QCOMPARE(fixture.fileDeletionProvider.backOperation().request.targetUrl, actualTargetUrl);
    fixture.fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);
    QCOMPARE(fixture.completion.plan.routePlan.sourceUrl, nextUrl);
}

void TestDocumentSessionMediaDeletionRuntime::directMediaCandidateFailureAbortsBeforeFileOperation()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));
    const QUrl parentUrl = localUrl(QStringLiteral("/media/"));

    QVERIFY(fixture.startDirectMedia(
        kiriview::FileDeletionMode::MoveToTrash, directMediaScope(currentUrl)));
    fixture.candidateProvider.failIgnoringCancellation(
        0, QStringLiteral("candidate listing failed"));

    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(0));
    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(!fixture.completion.plan.hasRoutePlan());
    QVERIFY(fixture.completion.plan.reportFailure);
    QCOMPARE(
        fixture.completion.failure.operationKind, kiriview::KioOperationKind::DirectoryListing);
    QCOMPARE(fixture.completion.failure.targetUrl, parentUrl);
    QCOMPARE(fixture.completion.failure.rawErrorCode, std::nullopt);
    QVERIFY(!fixture.completion.failure.canceled);
    QCOMPARE(fixture.completion.failure.userMessage, QStringLiteral("candidate listing failed"));
    QCOMPARE(
        fixture.completion.failure.diagnosticDetail, QStringLiteral("candidate listing failed"));
    QVERIFY(!fixture.completion.failure.retryable);
}

void TestDocumentSessionMediaDeletionRuntime::directMediaCandidatePhaseIsOwnedByRuntime()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));

    QVERIFY(fixture.startDirectMedia(
        kiriview::FileDeletionMode::MoveToTrash, directMediaScope(currentUrl)));

    QVERIFY(fixture.runtime.active());
    QCOMPARE(fixture.candidateProvider.loadCount(), std::size_t(1));
    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(0));
}

void TestDocumentSessionMediaDeletionRuntime::directMediaCandidateLoadCancelRejectsLateCompletion()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));

    QVERIFY(fixture.startDirectMedia(
        kiriview::FileDeletionMode::MoveToTrash, directMediaScope(currentUrl)));
    fixture.runtime.cancel();

    QVERIFY(fixture.candidateProvider.loadAt(0).canceled);
    fixture.candidateProvider.deliverIgnoringCancellation(
        0, { directMediaNavigationCandidate(currentUrl) });

    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(0));
    QCOMPARE(fixture.completionCount, 0);
}

void TestDocumentSessionMediaDeletionRuntime::cancelRejectsLateCompletion()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));

    fixture.start(kiriview::FileDeletionMode::MoveToTrash,
        { directMediaNavigationCandidate(currentUrl) }, currentUrl);
    fixture.runtime.cancel();
    QVERIFY(fixture.fileDeletionProvider.backOperation().canceled);
    QVERIFY(!fixture.runtime.active());

    fixture.fileDeletionProvider.deliverOperationAtIgnoringCancellation(
        0, kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(fixture.completionCount, 0);
}

void TestDocumentSessionMediaDeletionRuntime::cancellationInvalidatesBeforeFileProviderCallback()
{
    QObject receiver;
    bool providerCanceled = false;
    kiriview::FileDeletionProvider provider
        = [&](QObject* operationReceiver, kiriview::FileDeletionRequest request,
              kiriview::FileDeletionCallback callback) {
              QObject* token = new QObject(operationReceiver);
              const kiriview::KioOperationFailure failure
                  = kiriview::TestSupport::manualFileDeletionFailure(
                      request, kiriview::FileDeletionResult::Succeeded, {});
              return kiriview::ImageIoJob(
                  token, [&, callback = std::move(callback), failure](QObject* object) mutable {
                      providerCanceled = true;
                      callback(kiriview::FileDeletionResult::Succeeded, failure);
                      object->deleteLater();
                  });
          };
    kiriview::DocumentSessionMediaDeletionRuntime runtime(std::move(provider));
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));
    int completionCount = 0;
    runtime.start(&receiver, kiriview::FileDeletionMode::MoveToTrash,
        { directMediaNavigationCandidate(currentUrl) }, currentUrl, currentUrl,
        kiriview::DocumentSessionKind::Video,
        [&](kiriview::DocumentSessionMediaDeletionCompletion) { ++completionCount; });

    runtime.cancel();

    QVERIFY(providerCanceled);
    QCOMPARE(completionCount, 0);
    QVERIFY(!runtime.active());
}

void TestDocumentSessionMediaDeletionRuntime::synchronousCompletionPreservesReplacementJob()
{
    QObject receiver;
    kiriview::TestSupport::ManualFileDeletionProvider fileDeletionProvider;
    bool synchronouslyComplete = true;
    kiriview::FileDeletionProvider provider
        = [&](QObject* operationReceiver, kiriview::FileDeletionRequest request,
              kiriview::FileDeletionCallback callback) {
              kiriview::ImageIoJob job = fileDeletionProvider.start(
                  operationReceiver, std::move(request), std::move(callback));
              if (std::exchange(synchronouslyComplete, false)) {
                  fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);
              }
              return job;
          };
    kiriview::DocumentSessionMediaDeletionRuntime runtime(std::move(provider));
    const QUrl firstUrl = localUrl(QStringLiteral("/first/01.mp4"));
    const QUrl secondUrl = localUrl(QStringLiteral("/second/01.mp4"));
    int completionCount = 0;
    runtime.start(&receiver, kiriview::FileDeletionMode::MoveToTrash,
        { directMediaNavigationCandidate(firstUrl) }, firstUrl, firstUrl,
        kiriview::DocumentSessionKind::Video,
        [&](kiriview::DocumentSessionMediaDeletionCompletion) {
            ++completionCount;
            runtime.start(&receiver, kiriview::FileDeletionMode::MoveToTrash,
                { directMediaNavigationCandidate(secondUrl) }, secondUrl, secondUrl,
                kiriview::DocumentSessionKind::Video,
                [&](kiriview::DocumentSessionMediaDeletionCompletion) { ++completionCount; });
        });

    QCOMPARE(completionCount, 1);
    QCOMPARE(fileDeletionProvider.operationCount(), std::size_t(2));
    QVERIFY(!fileDeletionProvider.operationAt(1).canceled);
    QVERIFY(runtime.active());

    fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);
    QCOMPARE(completionCount, 2);
    QVERIFY(!runtime.active());
}

void TestDocumentSessionMediaDeletionRuntime::replacementStartRejectsStaleCompletion()
{
    RuntimeFixture fixture;
    const QUrl firstUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl firstFallbackUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/media/03.mp4"));
    const QUrl secondFallbackUrl = localUrl(QStringLiteral("/media/04.png"));

    fixture.start(kiriview::FileDeletionMode::MoveToTrash,
        { directMediaNavigationCandidate(firstUrl),
            directMediaNavigationCandidate(firstFallbackUrl) },
        firstUrl);
    fixture.start(kiriview::FileDeletionMode::MoveToTrash,
        { directMediaNavigationCandidate(secondUrl),
            directMediaNavigationCandidate(secondFallbackUrl) },
        secondUrl);

    QCOMPARE(fixture.fileDeletionProvider.operationCount(), std::size_t(2));
    QVERIFY(fixture.fileDeletionProvider.operationAt(0).canceled);

    fixture.fileDeletionProvider.deliverOperationAtIgnoringCancellation(
        0, kiriview::FileDeletionResult::Succeeded);
    QCOMPARE(fixture.completionCount, 0);

    fixture.fileDeletionProvider.finishBackOperation(kiriview::FileDeletionResult::Succeeded);
    QCOMPARE(fixture.completionCount, 1);
    QCOMPARE(fixture.completion.plan.routePlan.sourceUrl, secondFallbackUrl);
}

void TestDocumentSessionMediaDeletionRuntime::failedCompletionReportsFailureWithTypedFailure()
{
    RuntimeFixture fixture;
    const QUrl currentUrl = localUrl(QStringLiteral("/media/02.mp4"));

    fixture.start(kiriview::FileDeletionMode::MoveToTrash,
        { directMediaNavigationCandidate(currentUrl) }, currentUrl,
        kiriview::DocumentSessionKind::Image);
    fixture.fileDeletionProvider.finishBackOperation(
        kiriview::FileDeletionResult::Failed, QStringLiteral("delete failed"));

    QCOMPARE(fixture.completionCount, 1);
    QVERIFY(!fixture.completion.plan.hasRoutePlan());
    QVERIFY(fixture.completion.plan.reportFailure);
    QCOMPARE(fixture.completion.failure.operationKind, kiriview::KioOperationKind::FileDeletion);
    QCOMPARE(fixture.completion.failure.targetUrl, currentUrl);
    QCOMPARE(fixture.completion.failure.userMessage, QStringLiteral("delete failed"));
    QCOMPARE(fixture.completion.failure.diagnosticDetail, QStringLiteral("delete failed"));
    QVERIFY(fixture.completion.failure.retryable);
}

QTEST_GUILESS_MAIN(TestDocumentSessionMediaDeletionRuntime)

#include "tst_documentsessionmediadeletionruntime.moc"
