// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "system/kiooperationfailure.h"

#include <KIO/Global>
#include <KJob>

namespace kiriview {
namespace {
    bool isRetryableKioOperationError(KioOperationKind operationKind, int errorCode)
    {
        switch (operationKind) {
        case KioOperationKind::DirectoryListing:
        case KioOperationKind::FileDeletion:
        case KioOperationKind::MediaOpenWith:
            break;
        case KioOperationKind::Unknown:
            return false;
        }

        switch (errorCode) {
        case KIO::ERR_UNKNOWN_HOST:
        case KIO::ERR_CANNOT_CREATE_SOCKET:
        case KIO::ERR_CANNOT_CONNECT:
        case KIO::ERR_CONNECTION_BROKEN:
        case KIO::ERR_WORKER_DIED:
        case KIO::ERR_UNKNOWN_PROXY_HOST:
        case KIO::ERR_INTERNAL_SERVER:
        case KIO::ERR_SERVER_TIMEOUT:
        case KIO::ERR_SERVICE_NOT_AVAILABLE:
        case KIO::ERR_UNKNOWN_INTERRUPT:
        case KIO::ERR_CANNOT_CREATE_WORKER:
        case KIO::ERR_OWNER_DIED:
            return true;
        default:
            return false;
        }
    }
}

bool isKioOperationCanceledError(int errorCode)
{
    // Qt currently exposes these cancellation names with the same value.
    // NOLINTNEXTLINE(misc-redundant-expression)
    return errorCode == KJob::KilledJobError || errorCode == KIO::ERR_USER_CANCELED;
}

KioOperationFailure kioOperationFailureFromKJob(
    KioOperationKind operationKind, const QUrl& targetUrl, int errorCode, const QString& errorText)
{
    const bool canceled = isKioOperationCanceledError(errorCode);
    return KioOperationFailure {
        operationKind,
        targetUrl,
        errorCode,
        canceled,
        canceled ? QString() : errorText,
        errorText,
        !canceled && isRetryableKioOperationError(operationKind, errorCode),
        KioOperationFailureCause::Backend,
    };
}

KioOperationFailure kioOperationValidationFailure(
    KioOperationKind operationKind, const QUrl& targetUrl, const QString& diagnosticDetail)
{
    return KioOperationFailure {
        operationKind,
        targetUrl,
        std::nullopt,
        false,
        QString(),
        diagnosticDetail,
        false,
        KioOperationFailureCause::Validation,
    };
}

KioOperationFailure kioOperationResourceLimitFailure(
    KioOperationKind operationKind, const QUrl& targetUrl, const QString& diagnosticDetail)
{
    return KioOperationFailure {
        operationKind,
        targetUrl,
        std::nullopt,
        false,
        QString(),
        diagnosticDetail,
        false,
        KioOperationFailureCause::ResourceLimitExceeded,
    };
}
}
