// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_FILEDELETION_H
#define KIRIVIEW_FILEDELETION_H

#include "async/imageiojob.h"
#include "system/kiooperationfailure.h"

#include <QString>
#include <QUrl>
#include <QVariantList>
#include <functional>

class QObject;

namespace kiriview {
enum class FileDeletionMode {
    MoveToTrash,
    DeletePermanently,
};

enum class FileDeletionResult {
    Succeeded,
    Canceled,
    Failed,
};

enum class FileDeletionCompletionAction {
    ClearDeletedTargetAndOpenFallback,
    Ignore,
    ReportFailure,
};

struct FileDeletionRequest
{
    QUrl targetUrl;
    FileDeletionMode mode = FileDeletionMode::MoveToTrash;
};

using FileDeletionCallback = std::function<void(FileDeletionResult, const KioOperationFailure&)>;
using FileDeletionProvider
    = std::function<ImageIoJob(QObject*, FileDeletionRequest, FileDeletionCallback)>;

enum class FileDeletionConfirmationResult {
    Accepted,
    Rejected,
};

using FileDeletionConfirmationCallback = std::function<void(FileDeletionConfirmationResult)>;
using FileDeletionConfirmationProvider
    = std::function<ImageIoJob(QObject*, QUrl, FileDeletionConfirmationCallback)>;
using HostTrashProvider = std::function<ImageIoJob(QObject*, QUrl, FileDeletionCallback)>;

enum class HostTrashPortalResult {
    Succeeded,
    Failed,
};

using HostTrashPortalCallback = std::function<void(HostTrashPortalResult, QString)>;
// The descriptor is borrowed for the duration of the invocation. An asynchronous invoker must
// duplicate it before returning.
using HostTrashPortalInvoker = std::function<ImageIoJob(QObject*, int, HostTrashPortalCallback)>;

struct FileDeletionRuntimeDependencies
{
    bool flatpakSandboxed = false;
    FileDeletionProvider kioFileDeletionProvider;
    FileDeletionConfirmationProvider trashConfirmationProvider;
    HostTrashProvider hostTrashProvider;
};

FileDeletionCompletionAction fileDeletionCompletionAction(FileDeletionResult result);
FileDeletionProvider defaultFileDeletionProvider();
FileDeletionProvider fileDeletionProviderWithDefault(FileDeletionProvider provider);
HostTrashProvider hostTrashProviderForPortal(HostTrashPortalInvoker invoker);
bool hostTrashPortalReplySucceeded(const QVariantList& arguments);
FileDeletionProvider fileDeletionProviderForRuntime(FileDeletionRuntimeDependencies dependencies);
}

#endif
