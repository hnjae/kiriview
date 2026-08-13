// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "system/filedeletion.h"
#include "system/kiooperationfailure.h"

#include <KIO/Global>
#include <KJob>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariant>
#include <cstddef>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
template <typename Request, typename Callback> struct ManualOperation
{
    QObject* object = nullptr;
    Request request;
    Callback callback;
    kiriview::ImageIoJobCompletion completion;
    bool canceled = false;
};

template <typename Operation>
kiriview::ImageIoJob startManualOperation(
    QObject* receiver, const std::shared_ptr<Operation>& operation)
{
    operation->object = new QObject(receiver);
    std::weak_ptr<Operation> weakOperation = operation;
    kiriview::ImageIoJob job(operation->object, [weakOperation](QObject* object) {
        if (std::shared_ptr<Operation> activeOperation = weakOperation.lock()) {
            activeOperation->object = nullptr;
            activeOperation->canceled = true;
        }
        if (object != nullptr) {
            object->deleteLater();
        }
    });
    operation->completion = job.completion();
    return job;
}

template <typename Operation, typename Finish>
void finishManualOperation(const std::shared_ptr<Operation>& operation, Finish finish)
{
    QObject* object = operation->object;
    operation->completion.claimAndRun([&]() {
        operation->object = nullptr;
        finish(*operation);
        if (object != nullptr) {
            object->deleteLater();
        }
    });
}

struct ManualKioDeletionProvider
{
    using Operation
        = ManualOperation<kiriview::FileDeletionRequest, kiriview::FileDeletionCallback>;

    kiriview::FileDeletionProvider provider()
    {
        return [this](QObject* receiver, kiriview::FileDeletionRequest request,
                   kiriview::FileDeletionCallback callback) {
            auto operation = std::make_shared<Operation>();
            operation->request = std::move(request);
            operation->callback = std::move(callback);
            operations.push_back(operation);
            return startManualOperation(receiver, operation);
        };
    }

    void finish(std::size_t index, kiriview::FileDeletionResult result,
        kiriview::KioOperationFailure failure = {})
    {
        finishManualOperation(
            operations.at(index), [result, failure = std::move(failure)](Operation& operation) {
                operation.callback(result, failure);
            });
    }

    std::vector<std::shared_ptr<Operation>> operations;
};

struct ManualTrashConfirmationProvider
{
    using Operation = ManualOperation<QUrl, kiriview::FileDeletionConfirmationCallback>;

    kiriview::FileDeletionConfirmationProvider provider()
    {
        return [this](QObject* receiver, QUrl targetUrl,
                   kiriview::FileDeletionConfirmationCallback callback) {
            auto operation = std::make_shared<Operation>();
            operation->request = std::move(targetUrl);
            operation->callback = std::move(callback);
            operations.push_back(operation);
            return startManualOperation(receiver, operation);
        };
    }

    void finish(std::size_t index, kiriview::FileDeletionConfirmationResult result)
    {
        finishManualOperation(
            operations.at(index), [result](Operation& operation) { operation.callback(result); });
    }

    void deliverIgnoringCancellation(
        std::size_t index, kiriview::FileDeletionConfirmationResult result)
    {
        operations.at(index)->callback(result);
    }

    std::vector<std::shared_ptr<Operation>> operations;
};

struct ManualHostTrashProvider
{
    using Operation = ManualOperation<QUrl, kiriview::FileDeletionCallback>;

    kiriview::HostTrashProvider provider()
    {
        return [this](QObject* receiver, QUrl targetUrl, kiriview::FileDeletionCallback callback) {
            auto operation = std::make_shared<Operation>();
            operation->request = std::move(targetUrl);
            operation->callback = std::move(callback);
            operations.push_back(operation);
            return startManualOperation(receiver, operation);
        };
    }

    void finish(std::size_t index, kiriview::FileDeletionResult result,
        kiriview::KioOperationFailure failure = {})
    {
        finishManualOperation(
            operations.at(index), [result, failure = std::move(failure)](Operation& operation) {
                operation.callback(result, failure);
            });
    }

    void deliverIgnoringCancellation(std::size_t index, kiriview::FileDeletionResult result,
        const kiriview::KioOperationFailure& failure = {})
    {
        operations.at(index)->callback(result, failure);
    }

    std::vector<std::shared_ptr<Operation>> operations;
};

struct CompletionRecord
{
    kiriview::FileDeletionResult result = kiriview::FileDeletionResult::Failed;
    kiriview::KioOperationFailure failure;
};

struct FileDeletionRuntimeFixture
{
    explicit FileDeletionRuntimeFixture(bool flatpakSandboxed)
        : provider(
              kiriview::fileDeletionProviderForRuntime(kiriview::FileDeletionRuntimeDependencies {
                  flatpakSandboxed,
                  kio.provider(),
                  confirmation.provider(),
                  hostTrash.provider(),
              }))
    {
    }

    kiriview::ImageIoJob start(kiriview::FileDeletionRequest request)
    {
        return provider(&receiver, std::move(request),
            [this](
                kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure) {
                completions.push_back(CompletionRecord { result, failure });
            });
    }

    QObject receiver;
    ManualKioDeletionProvider kio;
    ManualTrashConfirmationProvider confirmation;
    ManualHostTrashProvider hostTrash;
    kiriview::FileDeletionProvider provider;
    std::vector<CompletionRecord> completions;
};
}

class TestFileDeletion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void kioOperationFailurePreservesKJobFields();
    void kioOperationFailureClassifiesRetryability_data();
    void kioOperationFailureClassifiesRetryability();
    void kioOperationFailureClassifiesCanceledErrors();
    void validationFailureHasDiagnosticWithoutRawCode();
    void completionActionRoutesDeletionResults();
    void flatpakLocalTrashConfirmsBeforeUsingHostTrash();
    void rejectedFlatpakTrashConfirmationCancelsWithoutSideEffect();
    void failedHostTrashPreservesFailureWithoutKioFallback();
    void nonPortalRoutesUseKio_data();
    void nonPortalRoutesUseKio();
    void ambiguousFlatpakFileTrashFailsWithoutBackend_data();
    void ambiguousFlatpakFileTrashFailsWithoutBackend();
    void cancelWhileConfirmingRejectsLateAcceptance();
    void cancelWhileUsingHostTrashRejectsLateCompletion();
    void synchronousConfirmationAndHostFailureCompleteOnce();
    void synchronousRejectedConfirmationCompletesOnce();
    void synchronousCompletionMayDestroyReceiver();
    void cancelingPreviousStageMayDestroyReceiver();
    void hostTrashPortalReplySucceeded_data();
    void hostTrashPortalReplySucceeded();
    void hostTrashAdapterPassesRegularFileDescriptor();
    void hostTrashAdapterPassesDirectoryFileDescriptor();
    void hostTrashAdapterAcceptsDirectoryUrlWithTrailingSlash();
    void hostTrashAdapterRejectsSymlinkSpecialAndMissingTargets();
    void hostTrashAdapterRejectsAmbiguousPathsAndUrls();
    void hostTrashAdapterMapsInvokerFailure();
};

void TestFileDeletion::kioOperationFailurePreservesKJobFields()
{
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/images/page.png"));
    const kiriview::KioOperationFailure failure
        = kiriview::kioOperationFailureFromKJob(kiriview::KioOperationKind::FileDeletion, targetUrl,
            KIO::ERR_ACCESS_DENIED, QStringLiteral("permission denied"));

    QCOMPARE(failure.operationKind, kiriview::KioOperationKind::FileDeletion);
    QCOMPARE(failure.targetUrl, targetUrl);
    QVERIFY(failure.rawErrorCode.has_value());
    QCOMPARE(*failure.rawErrorCode, KIO::ERR_ACCESS_DENIED);
    QVERIFY(!failure.canceled);
    QCOMPARE(failure.userMessage, QStringLiteral("permission denied"));
    QCOMPARE(failure.diagnosticDetail, QStringLiteral("permission denied"));
    QVERIFY(!failure.retryable);
}

void TestFileDeletion::kioOperationFailureClassifiesRetryability_data()
{
    QTest::addColumn<int>("operationKind");
    QTest::addColumn<int>("errorCode");
    QTest::addColumn<bool>("retryable");

    using Kind = kiriview::KioOperationKind;
    QTest::newRow("listing-connection-broken")
        << static_cast<int>(Kind::DirectoryListing) << static_cast<int>(KIO::ERR_CONNECTION_BROKEN)
        << true;
    QTest::newRow("delete-server-timeout") << static_cast<int>(Kind::FileDeletion)
                                           << static_cast<int>(KIO::ERR_SERVER_TIMEOUT) << true;
    QTest::newRow("open-with-service-unavailable")
        << static_cast<int>(Kind::MediaOpenWith) << static_cast<int>(KIO::ERR_SERVICE_NOT_AVAILABLE)
        << true;
    QTest::newRow("image-read-connection-broken")
        << static_cast<int>(Kind::ImageDataRead) << static_cast<int>(KIO::ERR_CONNECTION_BROKEN)
        << true;
    QTest::newRow("access-denied") << static_cast<int>(Kind::FileDeletion)
                                   << static_cast<int>(KIO::ERR_ACCESS_DENIED) << false;
    QTest::newRow("authentication-required")
        << static_cast<int>(Kind::ImageDataRead) << static_cast<int>(KIO::ERR_CANNOT_AUTHENTICATE)
        << false;
    QTest::newRow("malformed-url") << static_cast<int>(Kind::MediaOpenWith)
                                   << static_cast<int>(KIO::ERR_MALFORMED_URL) << false;
    QTest::newRow("unknown-kio-error")
        << static_cast<int>(Kind::DirectoryListing) << static_cast<int>(KIO::ERR_UNKNOWN) << false;
    QTest::newRow("unrecognized-error-code")
        << static_cast<int>(Kind::FileDeletion) << KJob::UserDefinedError + 4096 << false;
}

void TestFileDeletion::kioOperationFailureClassifiesRetryability()
{
    QFETCH(int, operationKind);
    QFETCH(int, errorCode);
    QFETCH(bool, retryable);

    const kiriview::KioOperationFailure failure = kiriview::kioOperationFailureFromKJob(
        static_cast<kiriview::KioOperationKind>(operationKind),
        QUrl::fromLocalFile(QStringLiteral("/images/page.png")), errorCode,
        QStringLiteral("backend failure"));

    QCOMPARE(failure.retryable, retryable);
}

void TestFileDeletion::kioOperationFailureClassifiesCanceledErrors()
{
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/images/page.png"));

    for (int errorCode :
        { static_cast<int>(KJob::KilledJobError), static_cast<int>(KIO::ERR_USER_CANCELED) }) {
        const kiriview::KioOperationFailure failure
            = kiriview::kioOperationFailureFromKJob(kiriview::KioOperationKind::MediaOpenWith,
                targetUrl, errorCode, QStringLiteral("operation canceled"));

        QCOMPARE(failure.operationKind, kiriview::KioOperationKind::MediaOpenWith);
        QCOMPARE(failure.targetUrl, targetUrl);
        QVERIFY(failure.rawErrorCode.has_value());
        QCOMPARE(*failure.rawErrorCode, errorCode);
        QVERIFY(failure.canceled);
        QCOMPARE(failure.userMessage, QString());
        QCOMPARE(failure.diagnosticDetail, QStringLiteral("operation canceled"));
        QVERIFY(!failure.retryable);
    }
}

void TestFileDeletion::validationFailureHasDiagnosticWithoutRawCode()
{
    const kiriview::KioOperationFailure failure
        = kiriview::kioOperationValidationFailure(kiriview::KioOperationKind::MediaOpenWith, QUrl(),
            QStringLiteral("No Open With target is available."));

    QCOMPARE(failure.operationKind, kiriview::KioOperationKind::MediaOpenWith);
    QVERIFY(failure.targetUrl.isEmpty());
    QVERIFY(!failure.rawErrorCode.has_value());
    QVERIFY(!failure.canceled);
    QCOMPARE(failure.userMessage, QString());
    QCOMPARE(failure.diagnosticDetail, QStringLiteral("No Open With target is available."));
    QVERIFY(!failure.retryable);
}

void TestFileDeletion::completionActionRoutesDeletionResults()
{
    QCOMPARE(static_cast<int>(
                 kiriview::fileDeletionCompletionAction(kiriview::FileDeletionResult::Succeeded)),
        static_cast<int>(
            kiriview::FileDeletionCompletionAction::ClearDeletedTargetAndOpenFallback));
    QCOMPARE(static_cast<int>(
                 kiriview::fileDeletionCompletionAction(kiriview::FileDeletionResult::Canceled)),
        static_cast<int>(kiriview::FileDeletionCompletionAction::Ignore));
    QCOMPARE(static_cast<int>(
                 kiriview::fileDeletionCompletionAction(kiriview::FileDeletionResult::Failed)),
        static_cast<int>(kiriview::FileDeletionCompletionAction::ReportFailure));
}

void TestFileDeletion::flatpakLocalTrashConfirmsBeforeUsingHostTrash()
{
    FileDeletionRuntimeFixture fixture(true);
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/images/book.cbz"));
    kiriview::ImageIoJob job
        = fixture.start({ targetUrl, kiriview::FileDeletionMode::MoveToTrash });

    QCOMPARE(fixture.confirmation.operations.size(), std::size_t(1));
    QCOMPARE(fixture.confirmation.operations.front()->request, targetUrl);
    QVERIFY(fixture.hostTrash.operations.empty());
    QVERIFY(fixture.kio.operations.empty());
    QVERIFY(fixture.completions.empty());

    fixture.confirmation.finish(0, kiriview::FileDeletionConfirmationResult::Accepted);

    QCOMPARE(fixture.hostTrash.operations.size(), std::size_t(1));
    QCOMPARE(fixture.hostTrash.operations.front()->request, targetUrl);
    QVERIFY(fixture.kio.operations.empty());
    QVERIFY(fixture.completions.empty());

    fixture.hostTrash.finish(0, kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(fixture.completions.size(), std::size_t(1));
    QCOMPARE(fixture.completions.front().result, kiriview::FileDeletionResult::Succeeded);
    QVERIFY(!job.isActive());
}

void TestFileDeletion::rejectedFlatpakTrashConfirmationCancelsWithoutSideEffect()
{
    FileDeletionRuntimeFixture fixture(true);
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/images/book.cbz"));
    kiriview::ImageIoJob job
        = fixture.start({ targetUrl, kiriview::FileDeletionMode::MoveToTrash });

    fixture.confirmation.finish(0, kiriview::FileDeletionConfirmationResult::Rejected);

    QCOMPARE(fixture.completions.size(), std::size_t(1));
    QCOMPARE(fixture.completions.front().result, kiriview::FileDeletionResult::Canceled);
    QVERIFY(fixture.completions.front().failure.canceled);
    QVERIFY(!fixture.completions.front().failure.rawErrorCode.has_value());
    QCOMPARE(fixture.completions.front().failure.userMessage, QString());
    QCOMPARE(
        fixture.completions.front().failure.cause, kiriview::KioOperationFailureCause::Unknown);
    QVERIFY(fixture.hostTrash.operations.empty());
    QVERIFY(fixture.kio.operations.empty());
    QVERIFY(!job.isActive());
}

void TestFileDeletion::failedHostTrashPreservesFailureWithoutKioFallback()
{
    FileDeletionRuntimeFixture fixture(true);
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/images/book.cbz"));
    kiriview::ImageIoJob job
        = fixture.start({ targetUrl, kiriview::FileDeletionMode::MoveToTrash });
    fixture.confirmation.finish(0, kiriview::FileDeletionConfirmationResult::Accepted);
    kiriview::KioOperationFailure failure;
    failure.operationKind = kiriview::KioOperationKind::FileDeletion;
    failure.targetUrl = targetUrl;
    failure.userMessage = QStringLiteral("Host trash is unavailable");
    failure.cause = kiriview::KioOperationFailureCause::Backend;

    fixture.hostTrash.finish(0, kiriview::FileDeletionResult::Failed, failure);

    QCOMPARE(fixture.completions.size(), std::size_t(1));
    QCOMPARE(fixture.completions.front().result, kiriview::FileDeletionResult::Failed);
    QCOMPARE(fixture.completions.front().failure.targetUrl, targetUrl);
    QCOMPARE(fixture.completions.front().failure.userMessage,
        QStringLiteral("Host trash is unavailable"));
    QVERIFY(!fixture.completions.front().failure.rawErrorCode.has_value());
    QVERIFY(!fixture.completions.front().failure.retryable);
    QCOMPARE(
        fixture.completions.front().failure.cause, kiriview::KioOperationFailureCause::Backend);
    QVERIFY(fixture.kio.operations.empty());
    QVERIFY(!job.isActive());
}

void TestFileDeletion::nonPortalRoutesUseKio_data()
{
    QTest::addColumn<bool>("flatpakSandboxed");
    QTest::addColumn<QUrl>("targetUrl");
    QTest::addColumn<int>("mode");

    QTest::newRow("native-local-trash")
        << false << QUrl::fromLocalFile(QStringLiteral("/images/book.cbz"))
        << static_cast<int>(kiriview::FileDeletionMode::MoveToTrash);
    QTest::newRow("flatpak-remote-trash")
        << true << QUrl(QStringLiteral("sftp://example.test/images/book.cbz"))
        << static_cast<int>(kiriview::FileDeletionMode::MoveToTrash);
    QTest::newRow("flatpak-local-permanent-delete")
        << true << QUrl::fromLocalFile(QStringLiteral("/images/book.cbz"))
        << static_cast<int>(kiriview::FileDeletionMode::DeletePermanently);
}

void TestFileDeletion::nonPortalRoutesUseKio()
{
    QFETCH(bool, flatpakSandboxed);
    QFETCH(QUrl, targetUrl);
    QFETCH(int, mode);
    const auto deletionMode = static_cast<kiriview::FileDeletionMode>(mode);
    FileDeletionRuntimeFixture fixture(flatpakSandboxed);
    kiriview::ImageIoJob job = fixture.start({ targetUrl, deletionMode });

    QCOMPARE(fixture.kio.operations.size(), std::size_t(1));
    QCOMPARE(fixture.kio.operations.front()->request.targetUrl, targetUrl);
    QCOMPARE(fixture.kio.operations.front()->request.mode, deletionMode);
    QVERIFY(fixture.confirmation.operations.empty());
    QVERIFY(fixture.hostTrash.operations.empty());

    fixture.kio.finish(0, kiriview::FileDeletionResult::Succeeded);

    QCOMPARE(fixture.completions.size(), std::size_t(1));
    QCOMPARE(fixture.completions.front().result, kiriview::FileDeletionResult::Succeeded);
    QVERIFY(!job.isActive());
}

void TestFileDeletion::ambiguousFlatpakFileTrashFailsWithoutBackend_data()
{
    QTest::addColumn<QUrl>("targetUrl");

    QTest::newRow("authority") << QUrl(QStringLiteral("file://server/images/book.cbz"));
    QTest::newRow("query") << QUrl(QStringLiteral("file:///images/book.cbz?view=1"));
    QTest::newRow("empty-query") << QUrl(QStringLiteral("file:///images/book.cbz?"));
    QTest::newRow("fragment") << QUrl(QStringLiteral("file:///images/book.cbz#page"));
    QTest::newRow("empty-fragment") << QUrl(QStringLiteral("file:///images/book.cbz#"));
}

void TestFileDeletion::ambiguousFlatpakFileTrashFailsWithoutBackend()
{
    QFETCH(QUrl, targetUrl);
    FileDeletionRuntimeFixture fixture(true);

    kiriview::ImageIoJob job
        = fixture.start({ targetUrl, kiriview::FileDeletionMode::MoveToTrash });

    QCOMPARE(fixture.completions.size(), std::size_t(1));
    QCOMPARE(fixture.completions.front().result, kiriview::FileDeletionResult::Failed);
    QCOMPARE(fixture.completions.front().failure.targetUrl, targetUrl);
    QCOMPARE(
        fixture.completions.front().failure.cause, kiriview::KioOperationFailureCause::Validation);
    QVERIFY(fixture.kio.operations.empty());
    QVERIFY(fixture.confirmation.operations.empty());
    QVERIFY(fixture.hostTrash.operations.empty());
    QVERIFY(!job.isActive());
}

void TestFileDeletion::cancelWhileConfirmingRejectsLateAcceptance()
{
    FileDeletionRuntimeFixture fixture(true);
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/images/book.cbz"));
    kiriview::ImageIoJob job
        = fixture.start({ targetUrl, kiriview::FileDeletionMode::MoveToTrash });

    job.cancel();

    QVERIFY(fixture.confirmation.operations.front()->canceled);
    QVERIFY(!job.isActive());
    fixture.confirmation.deliverIgnoringCancellation(
        0, kiriview::FileDeletionConfirmationResult::Accepted);
    QVERIFY(fixture.hostTrash.operations.empty());
    QVERIFY(fixture.completions.empty());
}

void TestFileDeletion::cancelWhileUsingHostTrashRejectsLateCompletion()
{
    FileDeletionRuntimeFixture fixture(true);
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/images/book.cbz"));
    kiriview::ImageIoJob job
        = fixture.start({ targetUrl, kiriview::FileDeletionMode::MoveToTrash });
    fixture.confirmation.finish(0, kiriview::FileDeletionConfirmationResult::Accepted);

    job.cancel();

    QVERIFY(fixture.hostTrash.operations.front()->canceled);
    QVERIFY(!job.isActive());
    fixture.hostTrash.deliverIgnoringCancellation(0, kiriview::FileDeletionResult::Succeeded);
    QVERIFY(fixture.completions.empty());
    QVERIFY(fixture.kio.operations.empty());
}

void TestFileDeletion::synchronousConfirmationAndHostFailureCompleteOnce()
{
    int kioInvocations = 0;
    int confirmationInvocations = 0;
    int hostTrashInvocations = 0;
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/images/book.cbz"));
    kiriview::FileDeletionProvider provider
        = kiriview::fileDeletionProviderForRuntime(kiriview::FileDeletionRuntimeDependencies {
            true,
            [&kioInvocations](
                QObject*, kiriview::FileDeletionRequest, kiriview::FileDeletionCallback) {
                ++kioInvocations;
                return kiriview::ImageIoJob();
            },
            [&confirmationInvocations](
                QObject*, QUrl, kiriview::FileDeletionConfirmationCallback callback) {
                ++confirmationInvocations;
                callback(kiriview::FileDeletionConfirmationResult::Accepted);
                return kiriview::ImageIoJob();
            },
            [&hostTrashInvocations](
                QObject*, QUrl target, kiriview::FileDeletionCallback callback) {
                ++hostTrashInvocations;
                kiriview::KioOperationFailure failure;
                failure.operationKind = kiriview::KioOperationKind::FileDeletion;
                failure.targetUrl = target;
                failure.cause = kiriview::KioOperationFailureCause::Backend;
                callback(kiriview::FileDeletionResult::Failed, failure);
                return kiriview::ImageIoJob();
            },
        });
    std::vector<CompletionRecord> completions;

    kiriview::ImageIoJob job = provider(this,
        kiriview::FileDeletionRequest {
            targetUrl,
            kiriview::FileDeletionMode::MoveToTrash,
        },
        [&completions](
            kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure) {
            completions.push_back(CompletionRecord { result, failure });
        });

    QCOMPARE(confirmationInvocations, 1);
    QCOMPARE(hostTrashInvocations, 1);
    QCOMPARE(kioInvocations, 0);
    QCOMPARE(completions.size(), std::size_t(1));
    QCOMPARE(completions.front().result, kiriview::FileDeletionResult::Failed);
    QCOMPARE(completions.front().failure.targetUrl, targetUrl);
    QVERIFY(!job.isActive());
}

void TestFileDeletion::synchronousRejectedConfirmationCompletesOnce()
{
    int kioInvocations = 0;
    int confirmationInvocations = 0;
    int hostTrashInvocations = 0;
    kiriview::FileDeletionProvider provider
        = kiriview::fileDeletionProviderForRuntime(kiriview::FileDeletionRuntimeDependencies {
            true,
            [&kioInvocations](
                QObject*, kiriview::FileDeletionRequest, kiriview::FileDeletionCallback) {
                ++kioInvocations;
                return kiriview::ImageIoJob();
            },
            [&confirmationInvocations](
                QObject*, QUrl, kiriview::FileDeletionConfirmationCallback callback) {
                ++confirmationInvocations;
                callback(kiriview::FileDeletionConfirmationResult::Rejected);
                return kiriview::ImageIoJob();
            },
            [&hostTrashInvocations](QObject*, QUrl, kiriview::FileDeletionCallback) {
                ++hostTrashInvocations;
                return kiriview::ImageIoJob();
            },
        });
    std::vector<CompletionRecord> completions;

    kiriview::ImageIoJob job = provider(this,
        kiriview::FileDeletionRequest {
            QUrl::fromLocalFile(QStringLiteral("/images/book.cbz")),
            kiriview::FileDeletionMode::MoveToTrash,
        },
        [&completions](
            kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure) {
            completions.push_back(CompletionRecord { result, failure });
        });

    QCOMPARE(confirmationInvocations, 1);
    QCOMPARE(hostTrashInvocations, 0);
    QCOMPARE(kioInvocations, 0);
    QCOMPARE(completions.size(), std::size_t(1));
    QCOMPARE(completions.front().result, kiriview::FileDeletionResult::Canceled);
    QVERIFY(!job.isActive());
}

void TestFileDeletion::synchronousCompletionMayDestroyReceiver()
{
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/images/book.cbz"));
    kiriview::FileDeletionProvider provider
        = kiriview::fileDeletionProviderForRuntime(kiriview::FileDeletionRuntimeDependencies {
            true,
            {},
            [](QObject*, QUrl, kiriview::FileDeletionConfirmationCallback callback) {
                callback(kiriview::FileDeletionConfirmationResult::Accepted);
                return kiriview::ImageIoJob();
            },
            [](QObject*, QUrl target, kiriview::FileDeletionCallback callback) {
                kiriview::KioOperationFailure failure;
                failure.operationKind = kiriview::KioOperationKind::FileDeletion;
                failure.targetUrl = target;
                failure.cause = kiriview::KioOperationFailureCause::Backend;
                callback(kiriview::FileDeletionResult::Failed, failure);
                return kiriview::ImageIoJob();
            },
        });
    auto* receiver = new QObject;
    const QPointer<QObject> guardedReceiver(receiver);
    int completions = 0;

    kiriview::ImageIoJob job = provider(receiver,
        kiriview::FileDeletionRequest {
            targetUrl,
            kiriview::FileDeletionMode::MoveToTrash,
        },
        [&completions, receiver](
            kiriview::FileDeletionResult, const kiriview::KioOperationFailure&) {
            ++completions;
            delete receiver;
        });

    QCOMPARE(completions, 1);
    QVERIFY(guardedReceiver.isNull());
    QVERIFY(!job.isActive());
}

void TestFileDeletion::cancelingPreviousStageMayDestroyReceiver()
{
    kiriview::FileDeletionConfirmationCallback confirmationCallback;
    int hostTrashInvocations = 0;
    int completions = 0;
    QObject* receiver = new QObject;
    const QPointer<QObject> guardedReceiver(receiver);
    kiriview::FileDeletionProvider provider
        = kiriview::fileDeletionProviderForRuntime(kiriview::FileDeletionRuntimeDependencies {
            true,
            {},
            [&confirmationCallback, &receiver](
                QObject* owner, QUrl, kiriview::FileDeletionConfirmationCallback callback) {
                confirmationCallback = std::move(callback);
                auto* token = new QObject(owner);
                return kiriview::ImageIoJob(
                    token, [&receiver](QObject*) { delete std::exchange(receiver, nullptr); });
            },
            [&hostTrashInvocations](QObject*, QUrl, kiriview::FileDeletionCallback) {
                ++hostTrashInvocations;
                return kiriview::ImageIoJob();
            },
        });

    kiriview::ImageIoJob job = provider(receiver,
        kiriview::FileDeletionRequest {
            QUrl::fromLocalFile(QStringLiteral("/images/book.cbz")),
            kiriview::FileDeletionMode::MoveToTrash,
        },
        [&completions](
            kiriview::FileDeletionResult, const kiriview::KioOperationFailure&) { ++completions; });
    QVERIFY(confirmationCallback);

    confirmationCallback(kiriview::FileDeletionConfirmationResult::Accepted);

    QVERIFY(guardedReceiver.isNull());
    QCOMPARE(hostTrashInvocations, 0);
    QCOMPARE(completions, 0);
    QVERIFY(!job.isActive());
}

void TestFileDeletion::hostTrashPortalReplySucceeded_data()
{
    QTest::addColumn<QVariantList>("arguments");
    QTest::addColumn<bool>("succeeded");

    QTest::newRow("success") << QVariantList { QVariant::fromValue(uint(1)) } << true;
    QTest::newRow("portal-failure") << QVariantList { QVariant::fromValue(uint(0)) } << false;
    QTest::newRow("unknown-unsigned-value")
        << QVariantList { QVariant::fromValue(uint(2)) } << false;
    QTest::newRow("missing-result") << QVariantList {} << false;
    QTest::newRow("signed-result") << QVariantList { QVariant::fromValue(1) } << false;
    QTest::newRow("string-result") << QVariantList { QStringLiteral("1") } << false;
    QTest::newRow("extra-result") << QVariantList { QVariant::fromValue(uint(1)),
        QVariant::fromValue(uint(0)) }
                                  << false;
}

void TestFileDeletion::hostTrashPortalReplySucceeded()
{
    QFETCH(QVariantList, arguments);
    QFETCH(bool, succeeded);

    QCOMPARE(kiriview::hostTrashPortalReplySucceeded(arguments), succeeded);
}

void TestFileDeletion::hostTrashAdapterPassesRegularFileDescriptor()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("book.cbz"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("archive"), qint64(7));
    file.close();

    struct stat expectedStatus {};
    const QByteArray encodedPath = QFile::encodeName(path);
    QCOMPARE(::stat(encodedPath.constData(), &expectedStatus), 0);

    int invocations = 0;
    int descriptorFlags = -1;
    struct stat observedStatus {};
    bool descriptorStatusAvailable = false;
    kiriview::HostTrashProvider provider = kiriview::hostTrashProviderForPortal(
        [&](QObject*, int borrowedDescriptor, kiriview::HostTrashPortalCallback callback) {
            ++invocations;
            descriptorStatusAvailable = ::fstat(borrowedDescriptor, &observedStatus) == 0;
            descriptorFlags = ::fcntl(borrowedDescriptor, F_GETFL);
            callback(kiriview::HostTrashPortalResult::Succeeded, {});
            return kiriview::ImageIoJob();
        });
    std::vector<CompletionRecord> completions;

    kiriview::ImageIoJob job = provider(this, QUrl::fromLocalFile(path),
        [&completions](
            kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure) {
            completions.push_back(CompletionRecord { result, failure });
        });

    QCOMPARE(invocations, 1);
    QVERIFY(descriptorStatusAvailable);
    QVERIFY(S_ISREG(observedStatus.st_mode));
    QVERIFY(observedStatus.st_dev == expectedStatus.st_dev);
    QVERIFY(observedStatus.st_ino == expectedStatus.st_ino);
    QVERIFY(descriptorFlags >= 0);
    QCOMPARE(descriptorFlags & O_ACCMODE, O_RDWR);
    QCOMPARE(completions.size(), std::size_t(1));
    QCOMPARE(completions.front().result, kiriview::FileDeletionResult::Succeeded);
    QVERIFY(!job.isActive());
}

void TestFileDeletion::hostTrashAdapterPassesDirectoryFileDescriptor()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("collection"));
    QVERIFY(QDir().mkpath(path));

    struct stat expectedStatus {};
    const QByteArray encodedPath = QFile::encodeName(path);
    QCOMPARE(::stat(encodedPath.constData(), &expectedStatus), 0);

    int invocations = 0;
    struct stat observedStatus {};
    bool descriptorStatusAvailable = false;
    kiriview::HostTrashProvider provider = kiriview::hostTrashProviderForPortal(
        [&](QObject*, int borrowedDescriptor, kiriview::HostTrashPortalCallback callback) {
            ++invocations;
            descriptorStatusAvailable = ::fstat(borrowedDescriptor, &observedStatus) == 0;
            callback(kiriview::HostTrashPortalResult::Succeeded, {});
            return kiriview::ImageIoJob();
        });
    std::vector<CompletionRecord> completions;

    kiriview::ImageIoJob job = provider(this, QUrl::fromLocalFile(path),
        [&completions](
            kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure) {
            completions.push_back(CompletionRecord { result, failure });
        });

    QCOMPARE(invocations, 1);
    QVERIFY(descriptorStatusAvailable);
    QVERIFY(S_ISDIR(observedStatus.st_mode));
    QVERIFY(observedStatus.st_dev == expectedStatus.st_dev);
    QVERIFY(observedStatus.st_ino == expectedStatus.st_ino);
    QCOMPARE(completions.size(), std::size_t(1));
    QCOMPARE(completions.front().result, kiriview::FileDeletionResult::Succeeded);
    QVERIFY(!job.isActive());
}

void TestFileDeletion::hostTrashAdapterAcceptsDirectoryUrlWithTrailingSlash()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("collection"));
    QVERIFY(QDir().mkpath(path));

    struct stat expectedStatus {};
    const QByteArray encodedPath = QFile::encodeName(path);
    QCOMPARE(::stat(encodedPath.constData(), &expectedStatus), 0);

    int invocations = 0;
    struct stat observedStatus {};
    bool descriptorStatusAvailable = false;
    kiriview::HostTrashProvider provider = kiriview::hostTrashProviderForPortal(
        [&](QObject*, int borrowedDescriptor, kiriview::HostTrashPortalCallback callback) {
            ++invocations;
            descriptorStatusAvailable = ::fstat(borrowedDescriptor, &observedStatus) == 0;
            callback(kiriview::HostTrashPortalResult::Succeeded, {});
            return kiriview::ImageIoJob();
        });
    std::vector<CompletionRecord> completions;

    kiriview::ImageIoJob job = provider(this, QUrl::fromLocalFile(path + QLatin1Char('/')),
        [&completions](
            kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure) {
            completions.push_back(CompletionRecord { result, failure });
        });

    QCOMPARE(invocations, 1);
    QVERIFY(descriptorStatusAvailable);
    QVERIFY(S_ISDIR(observedStatus.st_mode));
    QVERIFY(observedStatus.st_dev == expectedStatus.st_dev);
    QVERIFY(observedStatus.st_ino == expectedStatus.st_ino);
    QCOMPARE(completions.size(), std::size_t(1));
    QCOMPARE(completions.front().result, kiriview::FileDeletionResult::Succeeded);
    QVERIFY(!job.isActive());
}

void TestFileDeletion::hostTrashAdapterRejectsSymlinkSpecialAndMissingTargets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString referentPath = directory.filePath(QStringLiteral("referent.cbz"));
    QFile referent(referentPath);
    QVERIFY(referent.open(QIODevice::WriteOnly));
    referent.close();
    const QString symlinkPath = directory.filePath(QStringLiteral("link.cbz"));
    const QByteArray encodedReferentPath = QFile::encodeName(referentPath);
    const QByteArray encodedSymlinkPath = QFile::encodeName(symlinkPath);
    QCOMPARE(::symlink(encodedReferentPath.constData(), encodedSymlinkPath.constData()), 0);

    const QString fifoPath = directory.filePath(QStringLiteral("special"));
    const QByteArray encodedFifoPath = QFile::encodeName(fifoPath);
    QCOMPARE(::mkfifo(encodedFifoPath.constData(), 0600), 0);
    const QString missingPath = directory.filePath(QStringLiteral("missing.cbz"));

    const QString directoryReferentPath = directory.filePath(QStringLiteral("directory-referent"));
    QVERIFY(QDir().mkpath(directoryReferentPath));
    const QString directorySymlinkPath = directory.filePath(QStringLiteral("directory-link"));
    const QByteArray encodedDirectoryReferentPath = QFile::encodeName(directoryReferentPath);
    const QByteArray encodedDirectorySymlinkPath = QFile::encodeName(directorySymlinkPath);
    QCOMPARE(::symlink(
                 encodedDirectoryReferentPath.constData(), encodedDirectorySymlinkPath.constData()),
        0);

    const auto expectRejected = [this](const QString& path) {
        int invocations = 0;
        kiriview::HostTrashProvider provider = kiriview::hostTrashProviderForPortal(
            [&invocations](QObject*, int, kiriview::HostTrashPortalCallback) {
                ++invocations;
                return kiriview::ImageIoJob();
            });
        std::vector<CompletionRecord> completions;

        kiriview::ImageIoJob job = provider(this, QUrl::fromLocalFile(path),
            [&completions](
                kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure) {
                completions.push_back(CompletionRecord { result, failure });
            });

        QCOMPARE(invocations, 0);
        QCOMPARE(completions.size(), std::size_t(1));
        QCOMPARE(completions.front().result, kiriview::FileDeletionResult::Failed);
        QCOMPARE(completions.front().failure.targetUrl, QUrl::fromLocalFile(path));
        QVERIFY(!completions.front().failure.rawErrorCode.has_value());
        QVERIFY(!job.isActive());
    };

    expectRejected(symlinkPath);
    expectRejected(directorySymlinkPath + QLatin1Char('/'));
    expectRejected(fifoPath);
    expectRejected(missingPath);

    QVERIFY(QFileInfo(symlinkPath).isSymbolicLink());
    QVERIFY(QFileInfo::exists(referentPath));
    QVERIFY(QFileInfo(directorySymlinkPath).isSymbolicLink());
    QVERIFY(QFileInfo::exists(directoryReferentPath));
    QVERIFY(QFileInfo::exists(fifoPath));
}

void TestFileDeletion::hostTrashAdapterRejectsAmbiguousPathsAndUrls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString targetPath = directory.filePath(QStringLiteral("book.cbz"));
    QFile target(targetPath);
    QVERIFY(target.open(QIODevice::WriteOnly));
    target.close();

    const QString realDirectoryPath = directory.filePath(QStringLiteral("real"));
    const QString childDirectoryPath = realDirectoryPath + QStringLiteral("/child");
    QVERIFY(QDir().mkpath(childDirectoryPath));
    const QString alternatePath = realDirectoryPath + QStringLiteral("/alternate.cbz");
    QFile alternate(alternatePath);
    QVERIFY(alternate.open(QIODevice::WriteOnly));
    alternate.close();
    const QString symlinkPath = directory.filePath(QStringLiteral("link"));
    const QByteArray encodedChildDirectoryPath = QFile::encodeName(childDirectoryPath);
    const QByteArray encodedSymlinkPath = QFile::encodeName(symlinkPath);
    QCOMPARE(::symlink(encodedChildDirectoryPath.constData(), encodedSymlinkPath.constData()), 0);

    QUrl authorityUrl = QUrl::fromLocalFile(targetPath);
    authorityUrl.setHost(QStringLiteral("server"));
    QUrl queryUrl = QUrl::fromLocalFile(targetPath);
    queryUrl.setQuery(QStringLiteral("view=1"));
    QUrl fragmentUrl = QUrl::fromLocalFile(targetPath);
    fragmentUrl.setFragment(QStringLiteral("page"));
    const QUrl emptyQueryUrl(
        QUrl::fromLocalFile(targetPath).toString(QUrl::FullyEncoded) + QLatin1Char('?'));
    const QUrl emptyFragmentUrl(
        QUrl::fromLocalFile(targetPath).toString(QUrl::FullyEncoded) + QLatin1Char('#'));
    const QList<QUrl> rejectedTargets {
        QUrl::fromLocalFile(targetPath + QLatin1Char('/')),
        QUrl::fromLocalFile(QStringLiteral("/")),
        QUrl::fromLocalFile(QStringLiteral("/..")),
        QUrl(QStringLiteral("file:////tmp/book.cbz")),
        QUrl::fromLocalFile(symlinkPath + QStringLiteral("/../alternate.cbz")),
        QUrl::fromLocalFile(QStringLiteral(":/resource-target")),
        authorityUrl,
        queryUrl,
        emptyQueryUrl,
        fragmentUrl,
        emptyFragmentUrl,
    };

    for (const QUrl& targetUrl : rejectedTargets) {
        int invocations = 0;
        kiriview::HostTrashProvider provider = kiriview::hostTrashProviderForPortal(
            [&invocations](QObject*, int, kiriview::HostTrashPortalCallback) {
                ++invocations;
                return kiriview::ImageIoJob();
            });
        std::vector<CompletionRecord> completions;

        kiriview::ImageIoJob job = provider(this, targetUrl,
            [&completions](
                kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure) {
                completions.push_back(CompletionRecord { result, failure });
            });

        QCOMPARE(invocations, 0);
        QCOMPARE(completions.size(), std::size_t(1));
        QCOMPARE(completions.front().result, kiriview::FileDeletionResult::Failed);
        QCOMPARE(completions.front().failure.targetUrl, targetUrl);
        QVERIFY(!job.isActive());
    }

    QVERIFY(QFileInfo::exists(targetPath));
    QVERIFY(QFileInfo::exists(alternatePath));
    QVERIFY(QFileInfo(symlinkPath).isSymbolicLink());
}

void TestFileDeletion::hostTrashAdapterMapsInvokerFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("book.cbz"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    int invocations = 0;
    kiriview::HostTrashProvider provider = kiriview::hostTrashProviderForPortal(
        [&invocations](QObject*, int, kiriview::HostTrashPortalCallback callback) {
            ++invocations;
            callback(
                kiriview::HostTrashPortalResult::Failed, QStringLiteral("portal transport failed"));
            return kiriview::ImageIoJob();
        });
    std::vector<CompletionRecord> completions;
    const QUrl targetUrl = QUrl::fromLocalFile(path);

    kiriview::ImageIoJob job = provider(this, targetUrl,
        [&completions](
            kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure) {
            completions.push_back(CompletionRecord { result, failure });
        });

    QCOMPARE(invocations, 1);
    QCOMPARE(completions.size(), std::size_t(1));
    QCOMPARE(completions.front().result, kiriview::FileDeletionResult::Failed);
    QCOMPARE(completions.front().failure.operationKind, kiriview::KioOperationKind::FileDeletion);
    QCOMPARE(completions.front().failure.targetUrl, targetUrl);
    QVERIFY(!completions.front().failure.rawErrorCode.has_value());
    QVERIFY(!completions.front().failure.canceled);
    QVERIFY(!completions.front().failure.diagnosticDetail.isEmpty());
    QVERIFY(!completions.front().failure.retryable);
    QCOMPARE(completions.front().failure.cause, kiriview::KioOperationFailureCause::Backend);
    QVERIFY(!job.isActive());
}

QTEST_GUILESS_MAIN(TestFileDeletion)

#include "tst_filedeletion.moc"
