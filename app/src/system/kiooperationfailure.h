// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIOOPERATIONFAILURE_H
#define KIRIVIEW_KIOOPERATIONFAILURE_H

#include <QString>
#include <QUrl>
#include <functional>
#include <optional>

class QDebug;

namespace kiriview {
enum class KioOperationKind {
    Unknown,
    DirectoryListing,
    FileDeletion,
    MediaOpenWith,
    ImageDataRead,
};

enum class KioOperationFailureCause {
    Unknown,
    Backend,
    Validation,
    ResourceLimitExceeded,
};

struct KioOperationFailure
{
    KioOperationKind operationKind = KioOperationKind::Unknown;
    QUrl targetUrl;
    std::optional<int> rawErrorCode;
    bool canceled = false;
    QString userMessage;
    QString diagnosticDetail;
    bool retryable = false;
    KioOperationFailureCause cause = KioOperationFailureCause::Unknown;
};

QDebug operator<<(QDebug debug, const KioOperationFailure& failure);

using KioOperationFailureCallback = std::function<void(KioOperationFailure)>;

bool isKioOperationCanceledError(int errorCode);
KioOperationFailure kioOperationFailureFromKJob(
    KioOperationKind operationKind, const QUrl& targetUrl, int errorCode, const QString& errorText);
KioOperationFailure kioOperationValidationFailure(
    KioOperationKind operationKind, const QUrl& targetUrl, const QString& diagnosticDetail);
KioOperationFailure kioOperationResourceLimitFailure(
    KioOperationKind operationKind, const QUrl& targetUrl, const QString& diagnosticDetail);
}

#endif
