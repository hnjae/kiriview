// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "system/filedeletion.h"

#include "async/imagecallback.h"

#include <KIO/AskUserActionInterface>
#include <KIO/DeleteOrTrashJob>
#include <KJob>
#include <KLocalizedString>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMessageBox>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QStringList>
#include <QVariant>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace {
constexpr auto portalService = "org.freedesktop.portal.Desktop";
constexpr auto portalPath = "/org/freedesktop/portal/desktop";
constexpr auto trashPortalInterface = "org.freedesktop.portal.Trash";
constexpr auto trashPortalMethod = "TrashFile";

KIO::AskUserActionInterface::DeletionType kioDeletionType(kiriview::FileDeletionMode deletionMode)
{
    switch (deletionMode) {
    case kiriview::FileDeletionMode::MoveToTrash:
        return KIO::AskUserActionInterface::Trash;
    case kiriview::FileDeletionMode::DeletePermanently:
        return KIO::AskUserActionInterface::Delete;
    }

    return KIO::AskUserActionInterface::Trash;
}

void deleteQObjectLater(QObject* object)
{
    if (object != nullptr) {
        object->deleteLater();
    }
}

void cancelKJob(QObject* object)
{
    auto* job = qobject_cast<KJob*>(object);
    if (job != nullptr) {
        job->kill(KJob::Quietly);
    }
}

kiriview::KioOperationFailure successfulFileDeletion(const QUrl& targetUrl)
{
    return kiriview::KioOperationFailure {
        kiriview::KioOperationKind::FileDeletion,
        targetUrl,
        std::nullopt,
        false,
        QString(),
        QString(),
        false,
    };
}

kiriview::KioOperationFailure hostTrashBackendFailure(
    const QUrl& targetUrl, QString diagnosticDetail)
{
    return kiriview::KioOperationFailure {
        kiriview::KioOperationKind::FileDeletion,
        targetUrl,
        std::nullopt,
        false,
        QString(),
        std::move(diagnosticDetail),
        false,
        kiriview::KioOperationFailureCause::Backend,
    };
}

kiriview::KioOperationFailure canceledFileDeletion(const QUrl& targetUrl, QString diagnosticDetail)
{
    return kiriview::KioOperationFailure {
        kiriview::KioOperationKind::FileDeletion,
        targetUrl,
        std::nullopt,
        true,
        QString(),
        std::move(diagnosticDetail),
        false,
        kiriview::KioOperationFailureCause::Unknown,
    };
}

QString systemFailureDetail(const QString& operation, int errorNumber)
{
    return QStringLiteral("%1 failed with errno %2.").arg(operation).arg(errorNumber);
}

QString translatedKioText(const char* text) { return i18ndc("kio6", nullptr, text); }

QString translatedKioText(const char* context, const char* text)
{
    return i18ndc("kio6", context, text);
}

QString translatedKioMarkup(const char* context, const char* text, const QString& argument)
{
    return xi18ndc("kio6", context, text, argument);
}

bool isUnambiguousLocalFileUrl(const QUrl& targetUrl)
{
    return targetUrl.isValid() && targetUrl.scheme() == QLatin1String("file")
        && targetUrl.authority().isEmpty() && !targetUrl.hasQuery() && !targetUrl.hasFragment();
}

kiriview::ImageIoJob startKioFileDeletion(QObject* receiver,
    const kiriview::FileDeletionRequest& request, kiriview::FileDeletionCallback callback)
{
    if (request.targetUrl.isEmpty()) {
        kiriview::invokeIfSet(callback, kiriview::FileDeletionResult::Failed,
            kiriview::kioOperationValidationFailure(kiriview::KioOperationKind::FileDeletion,
                request.targetUrl, QStringLiteral("No file deletion target is available.")));
        return kiriview::ImageIoJob();
    }

    const QUrl targetUrl = request.targetUrl;
    auto* job = new KIO::DeleteOrTrashJob(QList<QUrl> { request.targetUrl },
        kioDeletionType(request.mode), KIO::AskUserActionInterface::ForceConfirmation, receiver);
    kiriview::ImageIoJob ioJob(job, cancelKJob);
    const kiriview::ImageIoJobCompletion completion = ioJob.completion();
    QObject* context = receiver == nullptr ? job : receiver;

    QObject::connect(job, &KJob::result, context,
        [completion, targetUrl, callback = std::move(callback)](KJob* finishedJob) mutable {
            completion.claimAndRun([&]() {
                if (finishedJob->error() == KJob::NoError) {
                    kiriview::invokeIfSet(callback, kiriview::FileDeletionResult::Succeeded,
                        successfulFileDeletion(targetUrl));
                    return;
                }

                const kiriview::KioOperationFailure failure = kiriview::kioOperationFailureFromKJob(
                    kiriview::KioOperationKind::FileDeletion, targetUrl, finishedJob->error(),
                    finishedJob->errorString());
                if (failure.canceled) {
                    kiriview::invokeIfSet(
                        callback, kiriview::FileDeletionResult::Canceled, failure);
                    return;
                }

                kiriview::invokeIfSet(callback, kiriview::FileDeletionResult::Failed, failure);
            });
        });
    job->start();
    return ioJob;
}

kiriview::ImageIoJob startTrashConfirmation(
    QObject* receiver, const QUrl& targetUrl, kiriview::FileDeletionConfirmationCallback callback)
{
    const QString displayedTarget = targetUrl.toDisplayString(QUrl::PreferLocalFile);
    auto* dialog = new QMessageBox(QMessageBox::Question, translatedKioText("Move to Trash"),
        translatedKioMarkup("@info",
            "Do you really want to move this item to the Trash?<nl/><filename>%1</filename>",
            displayedTarget),
        QMessageBox::Cancel, qApp->activeWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton* acceptButton = dialog->addButton(
        translatedKioText("@action:button", "Move to Trash"), QMessageBox::AcceptRole);
    dialog->setDefaultButton(acceptButton);
    dialog->setWindowModality(Qt::WindowModal);

    kiriview::ImageIoJob job(dialog, [](QObject* object) {
        auto* confirmationDialog = qobject_cast<QMessageBox*>(object);
        if (confirmationDialog != nullptr) {
            confirmationDialog->reject();
            confirmationDialog->deleteLater();
        }
    });
    const kiriview::ImageIoJobCompletion completion = job.completion();
    QObject* context = receiver == nullptr ? dialog : receiver;

    QObject::connect(dialog, &QDialog::finished, context,
        [completion, dialog, acceptButton, callback = std::move(callback)](int) mutable {
            const bool accepted = dialog->clickedButton() == acceptButton;
            completion.claimAndDelete([&]() {
                kiriview::invokeIfSet(callback,
                    accepted ? kiriview::FileDeletionConfirmationResult::Accepted
                             : kiriview::FileDeletionConfirmationResult::Rejected);
            });
        });
    dialog->show();
    return job;
}

kiriview::ImageIoJob invokeTrashPortal(
    QObject* receiver, int borrowedDescriptor, kiriview::HostTrashPortalCallback callback)
{
    if (!QDBusUnixFileDescriptor::isSupported()) {
        kiriview::invokeIfSet(callback, kiriview::HostTrashPortalResult::Failed,
            QStringLiteral("D-Bus file descriptor passing is unavailable."));
        return kiriview::ImageIoJob();
    }

    const QDBusUnixFileDescriptor descriptor(borrowedDescriptor);
    if (!descriptor.isValid()) {
        kiriview::invokeIfSet(callback, kiriview::HostTrashPortalResult::Failed,
            QStringLiteral("Could not duplicate the trash target descriptor."));
        return kiriview::ImageIoJob();
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        portalService, portalPath, trashPortalInterface, trashPortalMethod);
    message << QVariant::fromValue(descriptor);
    auto* watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message, std::numeric_limits<int>::max()),
        receiver);
    kiriview::ImageIoJob job(watcher, deleteQObjectLater);
    const kiriview::ImageIoJobCompletion completion = job.completion();
    QObject* context = receiver == nullptr ? watcher : receiver;

    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
        [completion, callback = std::move(callback)](
            QDBusPendingCallWatcher* finishedWatcher) mutable {
            completion.claimAndDelete([&]() {
                const QDBusMessage reply = finishedWatcher->reply();
                if (reply.type() == QDBusMessage::ReplyMessage
                    && kiriview::hostTrashPortalReplySucceeded(reply.arguments())) {
                    kiriview::invokeIfSet(
                        callback, kiriview::HostTrashPortalResult::Succeeded, QString());
                    return;
                }

                QString diagnostic = reply.errorMessage();
                if (!reply.errorName().isEmpty()) {
                    diagnostic = diagnostic.isEmpty()
                        ? reply.errorName()
                        : QStringLiteral("%1: %2").arg(reply.errorName(), diagnostic);
                }
                if (diagnostic.isEmpty()) {
                    diagnostic = reply.type() == QDBusMessage::ReplyMessage
                        ? QStringLiteral("The host trash portal rejected the target.")
                        : QStringLiteral("The host trash portal call failed.");
                }
                kiriview::invokeIfSet(
                    callback, kiriview::HostTrashPortalResult::Failed, std::move(diagnostic));
            });
        });
    return job;
}

class FlatpakTrashOperation final : public QObject
{
public:
    FlatpakTrashOperation(QObject* parent, QUrl targetUrl,
        kiriview::FileDeletionConfirmationProvider confirmationProvider,
        kiriview::HostTrashProvider hostTrashProvider, kiriview::FileDeletionCallback callback)
        : QObject(parent)
        , m_targetUrl(std::move(targetUrl))
        , m_confirmationProvider(std::move(confirmationProvider))
        , m_hostTrashProvider(std::move(hostTrashProvider))
        , m_callback(std::move(callback))
    {
    }

    ~FlatpakTrashOperation() override
    {
        m_stage = Stage::Finished;
        m_callback = {};
        m_stageJob.cancel();
    }

    void setCompletion(kiriview::ImageIoJobCompletion completion)
    {
        m_completion = std::move(completion);
    }

    void start()
    {
        if (!m_confirmationProvider || !m_hostTrashProvider) {
            finish(kiriview::FileDeletionResult::Failed,
                hostTrashBackendFailure(
                    m_targetUrl, QStringLiteral("Host trash support is unavailable.")));
            return;
        }

        m_stage = Stage::Confirming;
        const QPointer<FlatpakTrashOperation> self(this);
        kiriview::ImageIoJob job = m_confirmationProvider(
            this, m_targetUrl, [self](kiriview::FileDeletionConfirmationResult result) {
                if (self != nullptr) {
                    self->finishConfirmation(result);
                }
            });
        if (self != nullptr) {
            self->acceptStageJob(Stage::Confirming, std::move(job));
        } else {
            job.cancel();
        }
    }

    void cancel()
    {
        m_stage = Stage::Finished;
        m_callback = {};
        const QPointer<FlatpakTrashOperation> self(this);
        kiriview::ImageIoJob stageJob = std::move(m_stageJob);
        stageJob.cancel();
        if (self != nullptr) {
            self->deleteLater();
        }
    }

private:
    Q_DISABLE_COPY_MOVE(FlatpakTrashOperation)

    enum class Stage {
        Idle,
        Confirming,
        Trashing,
        Finished,
    };

    void finishConfirmation(kiriview::FileDeletionConfirmationResult result)
    {
        if (m_stage != Stage::Confirming || !m_completion.isActive()) {
            return;
        }
        if (result == kiriview::FileDeletionConfirmationResult::Rejected) {
            finish(kiriview::FileDeletionResult::Canceled,
                canceledFileDeletion(m_targetUrl,
                    QStringLiteral("File deletion confirmation "
                                   "was rejected.")));
            return;
        }

        const QPointer<FlatpakTrashOperation> self(this);
        kiriview::ImageIoJob confirmationJob = std::move(m_stageJob);
        confirmationJob.cancel();
        if (self == nullptr || self->m_stage != Stage::Confirming
            || !self->m_completion.isActive()) {
            return;
        }

        m_stage = Stage::Trashing;
        kiriview::ImageIoJob job = m_hostTrashProvider(this, m_targetUrl,
            [self](kiriview::FileDeletionResult deletionResult,
                const kiriview::KioOperationFailure& failure) {
                if (self != nullptr) {
                    self->finishHostTrash(deletionResult, failure);
                }
            });
        if (self != nullptr) {
            self->acceptStageJob(Stage::Trashing, std::move(job));
        } else {
            job.cancel();
        }
    }

    void finishHostTrash(
        kiriview::FileDeletionResult result, const kiriview::KioOperationFailure& failure)
    {
        if (m_stage != Stage::Trashing || !m_completion.isActive()) {
            return;
        }
        finish(result, failure);
    }

    void acceptStageJob(Stage expectedStage, kiriview::ImageIoJob job)
    {
        if (m_stage != expectedStage || !m_completion.isActive()) {
            job.cancel();
            return;
        }
        const QPointer<FlatpakTrashOperation> self(this);
        kiriview::ImageIoJob previousJob = std::move(m_stageJob);
        previousJob.cancel();
        if (self == nullptr || self->m_stage != expectedStage || !self->m_completion.isActive()) {
            job.cancel();
            return;
        }
        self->m_stageJob = std::move(job);
    }

    void finish(kiriview::FileDeletionResult result, kiriview::KioOperationFailure failure)
    {
        if (m_stage == Stage::Finished || !m_completion.isActive()) {
            return;
        }
        m_stage = Stage::Finished;
        const QPointer<FlatpakTrashOperation> self(this);
        kiriview::ImageIoJob stageJob = std::move(m_stageJob);
        stageJob.cancel();
        if (self == nullptr || !self->m_completion.isActive()) {
            return;
        }
        const kiriview::FileDeletionCallback callback = std::move(self->m_callback);
        self->m_completion.claimAndRun(
            [&]() { kiriview::invokeIfSet(callback, result, std::move(failure)); });
        if (self != nullptr) {
            self->deleteLater();
        }
    }

    QUrl m_targetUrl;
    kiriview::FileDeletionConfirmationProvider m_confirmationProvider;
    kiriview::HostTrashProvider m_hostTrashProvider;
    kiriview::FileDeletionCallback m_callback;
    kiriview::ImageIoJobCompletion m_completion;
    kiriview::ImageIoJob m_stageJob;
    Stage m_stage = Stage::Idle;
};

void cancelFlatpakTrashOperation(QObject* object)
{
    if (object != nullptr) {
        static_cast<FlatpakTrashOperation*>(object)->cancel();
    }
}
}

namespace kiriview {
FileDeletionCompletionAction fileDeletionCompletionAction(FileDeletionResult result)
{
    switch (result) {
    case FileDeletionResult::Succeeded:
        return FileDeletionCompletionAction::ClearDeletedTargetAndOpenFallback;
    case FileDeletionResult::Canceled:
        return FileDeletionCompletionAction::Ignore;
    case FileDeletionResult::Failed:
        return FileDeletionCompletionAction::ReportFailure;
    }

    return FileDeletionCompletionAction::ReportFailure;
}

bool hostTrashPortalReplySucceeded(const QVariantList& arguments)
{
    return arguments.size() == 1 && arguments.front().metaType().id() == QMetaType::UInt
        && arguments.front().toUInt() == 1;
}

HostTrashProvider hostTrashProviderForPortal(HostTrashPortalInvoker invoker)
{
    return [invoker = std::move(invoker)](
               QObject* receiver, const QUrl& targetUrl, FileDeletionCallback callback) mutable {
        if (!invoker || !isUnambiguousLocalFileUrl(targetUrl)) {
            invokeIfSet(callback, FileDeletionResult::Failed,
                kioOperationValidationFailure(KioOperationKind::FileDeletion, targetUrl,
                    QStringLiteral("No local host-trash target is available.")));
            return ImageIoJob();
        }

        const QString path = targetUrl.toLocalFile();
        const QByteArray encodedPath = QFile::encodeName(path);
        const QStringList pathComponents = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        const bool hasDotSegment = pathComponents.contains(QStringLiteral("."))
            || pathComponents.contains(QStringLiteral(".."));
        if (path.isEmpty() || !path.startsWith(QLatin1Char('/'))
            || path.startsWith(QLatin1String("//")) || path == QLatin1String("/") || hasDotSegment
            || encodedPath.contains('\0')) {
            invokeIfSet(callback, FileDeletionResult::Failed,
                kioOperationValidationFailure(KioOperationKind::FileDeletion, targetUrl,
                    QStringLiteral("The host-trash target path is invalid.")));
            return ImageIoJob();
        }

        QString inspectionPath = path;
        while (inspectionPath.size() > 1 && inspectionPath.endsWith(QLatin1Char('/'))) {
            inspectionPath.chop(1);
        }
        const QByteArray encodedInspectionPath = QFile::encodeName(inspectionPath);
        struct stat beforeStatus {};
        if (::lstat(encodedInspectionPath.constData(), &beforeStatus) != 0) {
            const int errorNumber = errno;
            invokeIfSet(callback, FileDeletionResult::Failed,
                hostTrashBackendFailure(
                    targetUrl, systemFailureDetail(QStringLiteral("lstat"), errorNumber)));
            return ImageIoJob();
        }
        const bool regularFile = S_ISREG(beforeStatus.st_mode);
        const bool directory = S_ISDIR(beforeStatus.st_mode);
        if (!regularFile && !directory) {
            invokeIfSet(callback, FileDeletionResult::Failed,
                hostTrashBackendFailure(
                    targetUrl, QStringLiteral("The host-trash target type is unsupported.")));
            return ImageIoJob();
        }

        int openFlags = O_CLOEXEC;
        if (regularFile) {
            openFlags |= O_RDWR | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY;
        } else {
#ifdef O_PATH
            openFlags |= O_PATH;
#else
            invokeIfSet(callback, FileDeletionResult::Failed,
                hostTrashBackendFailure(targetUrl,
                    QStringLiteral("Directory host-trash descriptors are unsupported.")));
            return ImageIoJob();
#endif
        }
        const int descriptor = ::open(encodedPath.constData(), openFlags);
        if (descriptor < 0) {
            const int errorNumber = errno;
            invokeIfSet(callback, FileDeletionResult::Failed,
                hostTrashBackendFailure(
                    targetUrl, systemFailureDetail(QStringLiteral("open"), errorNumber)));
            return ImageIoJob();
        }

        struct stat openedStatus {};
        const bool statusAvailable = ::fstat(descriptor, &openedStatus) == 0;
        const int statusErrorNumber = errno;
        const bool openedTargetMatches = statusAvailable
            && openedStatus.st_dev == beforeStatus.st_dev
            && openedStatus.st_ino == beforeStatus.st_ino
            && (openedStatus.st_mode & S_IFMT) == (beforeStatus.st_mode & S_IFMT);
        if (!openedTargetMatches) {
            ::close(descriptor);
            invokeIfSet(callback, FileDeletionResult::Failed,
                hostTrashBackendFailure(targetUrl,
                    statusAvailable
                        ? QStringLiteral("The host-trash target changed while it was opened.")
                        : systemFailureDetail(QStringLiteral("fstat"), statusErrorNumber)));
            return ImageIoJob();
        }

        ImageIoJob job = invoker(receiver, descriptor,
            [targetUrl, callback = std::move(callback)](
                HostTrashPortalResult result, QString diagnosticDetail) mutable {
                if (result == HostTrashPortalResult::Succeeded) {
                    invokeIfSet(
                        callback, FileDeletionResult::Succeeded, successfulFileDeletion(targetUrl));
                    return;
                }
                if (diagnosticDetail.isEmpty()) {
                    diagnosticDetail = QStringLiteral("The host trash portal rejected the target.");
                }
                invokeIfSet(callback, FileDeletionResult::Failed,
                    hostTrashBackendFailure(targetUrl, std::move(diagnosticDetail)));
            });
        ::close(descriptor);
        return job;
    };
}

FileDeletionProvider fileDeletionProviderForRuntime(FileDeletionRuntimeDependencies dependencies)
{
    return [dependencies = std::move(dependencies)](QObject* receiver, FileDeletionRequest request,
               FileDeletionCallback callback) mutable {
        const bool flatpakTrash
            = dependencies.flatpakSandboxed && request.mode == FileDeletionMode::MoveToTrash;
        const bool flatpakFileTrash
            = flatpakTrash && request.targetUrl.scheme() == QLatin1String("file");
        if (flatpakFileTrash && !isUnambiguousLocalFileUrl(request.targetUrl)) {
            invokeIfSet(callback, FileDeletionResult::Failed,
                kioOperationValidationFailure(KioOperationKind::FileDeletion, request.targetUrl,
                    QStringLiteral("The host-trash target URL is ambiguous.")));
            return ImageIoJob();
        }
        const bool useHostTrash = flatpakFileTrash && isUnambiguousLocalFileUrl(request.targetUrl);
        if (!useHostTrash) {
            if (dependencies.kioFileDeletionProvider) {
                return dependencies.kioFileDeletionProvider(
                    receiver, std::move(request), std::move(callback));
            }
            invokeIfSet(callback, FileDeletionResult::Failed,
                kioOperationValidationFailure(KioOperationKind::FileDeletion, request.targetUrl,
                    QStringLiteral("No file deletion backend is available.")));
            return ImageIoJob();
        }

        auto* operation = new FlatpakTrashOperation(receiver, std::move(request.targetUrl),
            dependencies.trashConfirmationProvider, dependencies.hostTrashProvider,
            std::move(callback));
        ImageIoJob job(operation, cancelFlatpakTrashOperation);
        operation->setCompletion(job.completion());
        operation->start();
        return job;
    };
}

FileDeletionProvider defaultFileDeletionProvider()
{
    FileDeletionProvider kioProvider
        = [](QObject* receiver, const FileDeletionRequest& request, FileDeletionCallback callback) {
              return startKioFileDeletion(receiver, request, std::move(callback));
          };
    FileDeletionConfirmationProvider confirmationProvider
        = [](QObject* receiver, const QUrl& targetUrl, FileDeletionConfirmationCallback callback) {
              return startTrashConfirmation(receiver, targetUrl, std::move(callback));
          };
    return fileDeletionProviderForRuntime(FileDeletionRuntimeDependencies {
        QFileInfo(QStringLiteral("/.flatpak-info")).isFile(),
        std::move(kioProvider),
        std::move(confirmationProvider),
        hostTrashProviderForPortal(invokeTrashPortal),
    });
}

FileDeletionProvider fileDeletionProviderWithDefault(FileDeletionProvider provider)
{
    if (provider) {
        return provider;
    }
    return defaultFileDeletionProvider();
}
}
