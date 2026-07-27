// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionmediaopenwithruntime.h"

#include "async/imagecallback.h"

#include <QString>
#include <utility>

namespace kiriview {
DocumentSessionMediaOpenWithRuntime::DocumentSessionMediaOpenWithRuntime(
    MediaOpenWithProvider provider)
    : m_provider(mediaOpenWithProviderWithDefault(std::move(provider)))
{
}

DocumentSessionMediaOpenWithRuntime::~DocumentSessionMediaOpenWithRuntime()
{
    m_lifetime.reset();
    cancel();
}

void DocumentSessionMediaOpenWithRuntime::open(
    QObject* receiver, const MediaOpenWithPlan& plan, MediaOpenWithCallback callback)
{
    if (!plan.hasRequest()) {
        invokeIfSet(callback, MediaOpenWithResult::Failed,
            kioOperationValidationFailure(KioOperationKind::MediaOpenWith, QUrl(),
                QStringLiteral("No Open With target is available.")));
        return;
    }

    const std::weak_ptr<void> lifetime = m_lifetime;
    const std::shared_ptr<ImageAsyncOperationState> operationState = m_operation;
    const MediaOpenWithProvider provider = m_provider;
    const quint64 operationId = operationState->start();
    ImageIoJob previousJob = std::move(m_job);
    previousJob.cancel();
    if (lifetime.expired() || !operationState->accepts(operationId)) {
        return;
    }

    auto sharedCallback = std::make_shared<MediaOpenWithCallback>(std::move(callback));
    ImageIoJob startedJob = provider(receiver, *plan.request,
        [operationState, operationId, sharedCallback](
            MediaOpenWithResult result, const KioOperationFailure& failure) {
            if (!operationState->finish(operationId)) {
                return;
            }

            invokeIfSet(*sharedCallback, result, failure);
        });
    if (lifetime.expired() || !operationState->accepts(operationId)) {
        startedJob.cancel();
        return;
    }
    m_job = std::move(startedJob);
}

void DocumentSessionMediaOpenWithRuntime::cancel()
{
    const std::shared_ptr<ImageAsyncOperationState> operationState = m_operation;
    ImageIoJob job = std::move(m_job);
    operationState->cancel();
    job.cancel();
}

bool DocumentSessionMediaOpenWithRuntime::active() const { return m_operation->active(); }
}
