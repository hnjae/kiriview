// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportproviderbridge_p.h"

#include "imageviewporttoken_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

ViewportProviderExecutorOutcome ViewportProviderExecutor::releaseFailureHandle(
    const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
    ImageSequenceProviderFailureHandle* failureHandle)
{
    delete failureHandle;
    if (sessionControl) {
        sessionControl->completeHandleReleaseOnSessionAffinity();
    }
    return ViewportProviderExecutorOutcome::Completed;
}

namespace {
QMutex& providerSessionOwnershipMutex()
{
    static QMutex mutex;
    return mutex;
}

QSet<const ImageSequenceProviderSession*>& ownedProviderSessions()
{
    static QSet<const ImageSequenceProviderSession*> sessions;
    return sessions;
}

quint64 allocateProviderLeaseId()
{
    static QMutex mutex;
    static quint64 nextId = 0;
    QMutexLocker locker(&mutex);
    if (nextId == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport provider frame lease identity exhausted");
    }
    return ++nextId;
}

bool claimProviderSession(ImageSequenceProviderSession* session)
{
    QMutexLocker locker(&providerSessionOwnershipMutex());
    if (ownedProviderSessions().contains(session)) {
        return false;
    }
    ownedProviderSessions().insert(session);
    return true;
}

void releaseProviderSession(const ImageSequenceProviderSession* session)
{
    QMutexLocker locker(&providerSessionOwnershipMutex());
    ownedProviderSessions().remove(session);
}

QVector<ImageSequenceProviderRequestToken> cleanupTokens(
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken)
{
    QVector<ImageSequenceProviderRequestToken> tokens;
    if (metadataToken.isValid()) {
        tokens.append(metadataToken);
    }
    if (frameToken.isValid() && frameToken != metadataToken) {
        tokens.append(frameToken);
    }
    return tokens;
}

void requestProviderCleanup(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken)
{
    QVector<ImageSequenceProviderRequestToken> tokens = cleanupTokens(metadataToken, frameToken);
    if (!tokens.isEmpty()) {
        session->request(ImageSequenceProviderRequest::cancel(std::move(tokens)));
    }
    session->request(ImageSequenceProviderRequest::close());
}

class QtViewportProviderExecutor final : public ViewportProviderExecutor
{
public:
    ViewportProviderExecutorOutcome invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract,
        std::function<void()> command) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        if (threadingContract == ImageSequenceProviderThreadingContract::ThreadSafe) {
            command();
            return ViewportProviderExecutorOutcome::Completed;
        }
        if (session->thread() == QThread::currentThread()) {
            command();
            return ViewportProviderExecutorOutcome::Completed;
        }
        return QMetaObject::invokeMethod(session, std::move(command), Qt::BlockingQueuedConnection)
            ? ViewportProviderExecutorOutcome::Completed
            : ViewportProviderExecutorOutcome::RetryableFailure;
    }

    ViewportProviderExecutorOutcome queueSessionClose(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        ImageSequenceProviderSession* session
            = sessionControl ? sessionControl->session() : nullptr;
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        return QMetaObject::invokeMethod(
                   session,
                   [sessionControl, session, metadataToken, frameToken]() {
                       requestProviderCleanup(session, metadataToken, frameToken);
                       sessionControl->completeCloseOnSessionAffinity();
                   },
                   Qt::QueuedConnection)
            ? ViewportProviderExecutorOutcome::Scheduled
            : ViewportProviderExecutorOutcome::RetryableFailure;
    }

    ViewportProviderExecutorOutcome releaseFrameHandle(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFrameHandle* frameHandle) override
    {
        ImageSequenceProviderSession* session
            = sessionControl ? sessionControl->session() : nullptr;
        const auto threadingContract = sessionControl
            ? sessionControl->threadingContract()
            : ImageSequenceProviderThreadingContract::AffinityBound;
        if (!frameHandle) {
            if (!sessionControl || !session) {
                return ViewportProviderExecutorOutcome::RetryableFailure;
            }
            if (session->thread() == QThread::currentThread()) {
                sessionControl->completeHandleReleaseOnSessionAffinity();
                return ViewportProviderExecutorOutcome::Completed;
            }
            return QMetaObject::invokeMethod(
                       session,
                       [sessionControl]() {
                           sessionControl->completeHandleReleaseOnSessionAffinity();
                       },
                       Qt::QueuedConnection)
                ? ViewportProviderExecutorOutcome::Scheduled
                : ViewportProviderExecutorOutcome::RetryableFailure;
        }
        if (threadingContract == ImageSequenceProviderThreadingContract::ThreadSafe) {
            frameHandle->release();
            if (!session || session->thread() == QThread::currentThread()) {
                delete frameHandle;
                if (sessionControl) {
                    sessionControl->completeHandleReleaseOnSessionAffinity();
                }
                return ViewportProviderExecutorOutcome::Completed;
            }
            return QMetaObject::invokeMethod(
                       session,
                       [sessionControl, frameHandle]() {
                           delete frameHandle;
                           sessionControl->completeHandleReleaseOnSessionAffinity();
                       },
                       Qt::QueuedConnection)
                ? ViewportProviderExecutorOutcome::Scheduled
                : ViewportProviderExecutorOutcome::RetryableFailure;
        }
        if (!session)
            return ViewportProviderExecutorOutcome::RetryableFailure;
        if (session->thread() == QThread::currentThread()) {
            frameHandle->release();
            delete frameHandle;
            sessionControl->completeHandleReleaseOnSessionAffinity();
            return ViewportProviderExecutorOutcome::Completed;
        }
        return QMetaObject::invokeMethod(
                   session,
                   [sessionControl, frameHandle]() {
                       frameHandle->release();
                       delete frameHandle;
                       sessionControl->completeHandleReleaseOnSessionAffinity();
                   },
                   Qt::QueuedConnection)
            ? ViewportProviderExecutorOutcome::Scheduled
            : ViewportProviderExecutorOutcome::RetryableFailure;
    }

    ViewportProviderExecutorOutcome releaseFailureHandle(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFailureHandle* failureHandle) override
    {
        ImageSequenceProviderSession* session
            = sessionControl ? sessionControl->session() : nullptr;
        if (!sessionControl) {
            delete failureHandle;
            return ViewportProviderExecutorOutcome::Completed;
        }
        const auto threadingContract = sessionControl->threadingContract();
        if (threadingContract == ImageSequenceProviderThreadingContract::ThreadSafe) {
            if (failureHandle) {
                failureHandle->release();
            }
            if (!session || session->thread() == QThread::currentThread()) {
                delete failureHandle;
                sessionControl->completeHandleReleaseOnSessionAffinity();
                return ViewportProviderExecutorOutcome::Completed;
            }
            return QMetaObject::invokeMethod(
                       session,
                       [sessionControl, failureHandle]() {
                           delete failureHandle;
                           sessionControl->completeHandleReleaseOnSessionAffinity();
                       },
                       Qt::QueuedConnection)
                ? ViewportProviderExecutorOutcome::Scheduled
                : ViewportProviderExecutorOutcome::RetryableFailure;
        }
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        if (session->thread() == QThread::currentThread()) {
            delete failureHandle;
            sessionControl->completeHandleReleaseOnSessionAffinity();
            return ViewportProviderExecutorOutcome::Completed;
        }
        return QMetaObject::invokeMethod(
                   session,
                   [sessionControl, failureHandle]() {
                       delete failureHandle;
                       sessionControl->completeHandleReleaseOnSessionAffinity();
                   },
                   Qt::QueuedConnection)
            ? ViewportProviderExecutorOutcome::Scheduled
            : ViewportProviderExecutorOutcome::RetryableFailure;
    }
};

ViewportProviderExecutor& qtViewportProviderExecutor()
{
    static QtViewportProviderExecutor executor;
    return executor;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
class SynchronousViewportProviderExecutor final : public ViewportProviderExecutor
{
public:
    ViewportProviderExecutorOutcome invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract, std::function<void()> command) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        command();
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome queueSessionClose(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        ImageSequenceProviderSession* session
            = sessionControl ? sessionControl->session() : nullptr;
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        requestProviderCleanup(session, metadataToken, frameToken);
        sessionControl->completeCloseOnSessionAffinity();
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome releaseFrameHandle(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFrameHandle* frameHandle) override
    {
        delete frameHandle;
        if (sessionControl) {
            sessionControl->completeHandleReleaseOnSessionAffinity();
        }
        return ViewportProviderExecutorOutcome::Completed;
    }
};
#endif

ViewportProviderExecutor* defaultProviderExecutor() { return &qtViewportProviderExecutor(); }

bool executorAccepted(ViewportProviderExecutorOutcome outcome)
{
    return outcome != ViewportProviderExecutorOutcome::RetryableFailure;
}

ViewportProviderEvent providerEvent(ImageViewportPageRole role, quint64 sessionSerial,
    quint64 generation, ImageSequenceProviderEventKind kind,
    ImageSequenceProviderRequestToken token)
{
    ViewportProviderEvent event;
    event.kind = kind;
    event.role = role;
    event.sessionSerial = sessionSerial;
    event.generation = generation;
    event.token = token;
    return event;
}

ViewportProviderEvent viewportProviderEventFromTyped(ImageViewportPageRole role,
    quint64 sessionSerial, quint64 generation, const ImageSequenceProviderEvent& typedEvent)
{
    ViewportProviderEvent event
        = providerEvent(role, sessionSerial, generation, typedEvent.kind(), typedEvent.token());
    event.metadata = typedEvent.metadata();
    event.frameHandle = typedEvent.frameHandle();
    event.frameEnvelope = typedEvent.frameEnvelope();
    event.progress = typedEvent.progress();
    event.unsupportedCause = typedEvent.unsupportedCause();
    const ImageSequenceProviderFailure failure = typedEvent.failure();
    event.providerFailureAvailable = failure.isValid();
    event.providerCause = event.providerFailureAvailable
        ? failure.cause()
        : ImageSequenceProviderFailureCause::Unavailable;
    event.failureHandle = failure.applicationFailureHandle();
    if (event.providerFailureAvailable && event.failureHandle) {
        event.providerReference = event.failureHandle->reference();
    }
    return event;
}

ImageViewportInternal::ProviderTransportDiagnostic providerTransportDiagnostic(
    ImageViewportPageRole role, ImageViewportInternal::ProviderTransportOperation operation,
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken,
    bool queued, bool pendingCleanup, quint64 generation = 0, quint64 sessionSerial = 0,
    quint64 providerLeaseId = 0)
{
    return {
        true,
        role,
        operation,
        metadataToken.isValid(),
        metadataToken.isValid()
            ? ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(metadataToken)
            : 0,
        frameToken.isValid(),
        frameToken.isValid()
            ? ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(frameToken)
            : 0,
        queued,
        pendingCleanup,
        generation,
        sessionSerial,
        providerLeaseId,
    };
}

ImageViewportInternal::ProviderTransportDiagnostic providerTransportDiagnostic(
    ImageViewportPageRole role, const ImageSequenceProviderRequest& request, quint64 generation,
    quint64 sessionSerial)
{
    if (request.kind() != ImageSequenceProviderRequestKind::Cancel) {
        return {};
    }
    const QVector<ImageSequenceProviderRequestToken> tokens = request.tokens();
    return providerTransportDiagnostic(role,
        ImageViewportInternal::ProviderTransportOperation::Cancel, {},
        tokens.isEmpty() ? ImageSequenceProviderRequestToken {} : tokens.first(), false, false,
        generation, sessionSerial);
}
}

class ViewportProviderEventEndpoint
{
public:
    explicit ViewportProviderEventEndpoint(std::function<void(const ViewportProviderEvent&)> sink)
        : eventSink(std::move(sink))
    {
    }

    bool deliver(const ViewportProviderEvent& event)
    {
        std::function<void(const ViewportProviderEvent&)> sink;
        {
            QMutexLocker locker(&mutex);
            sink = eventSink;
        }
        if (!sink) {
            return false;
        }
        sink(event);
        return true;
    }

    void observeRequest(const ImageSequenceProviderRequest& request)
    {
        QMutexLocker locker(&mutex);
        switch (request.kind()) {
        case ImageSequenceProviderRequestKind::Metadata:
        case ImageSequenceProviderRequestKind::Frame:
        case ImageSequenceProviderRequestKind::Position:
        case ImageSequenceProviderRequestKind::Playback:
            highestIssuedToken = qMax(highestIssuedToken,
                ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(request.token()));
            if (!activeTokens.contains(request.token())) {
                activeTokens.append(request.token());
            }
            Q_ASSERT(activeTokens.size() <= 2);
            break;
        case ImageSequenceProviderRequestKind::Cancel: {
            const auto tokens = request.tokens();
            for (ImageSequenceProviderRequestToken token : std::as_const(tokens)) {
                retireTokenLocked(token);
            }
            break;
        }
        case ImageSequenceProviderRequestKind::Close:
            retireAllTokensLocked();
            break;
        }
    }

    void observeRequestDeliveryFailure(const ImageSequenceProviderRequest& request)
    {
        switch (request.kind()) {
        case ImageSequenceProviderRequestKind::Metadata:
        case ImageSequenceProviderRequestKind::Frame:
        case ImageSequenceProviderRequestKind::Position:
        case ImageSequenceProviderRequestKind::Playback: {
            QMutexLocker locker(&mutex);
            retireTokenLocked(request.token());
            break;
        }
        case ImageSequenceProviderRequestKind::Cancel:
        case ImageSequenceProviderRequestKind::Close:
            break;
        }
    }

    void observeTerminal(ImageSequenceProviderRequestToken token)
    {
        QMutexLocker locker(&mutex);
        retireTokenLocked(token);
    }

    void observeClose()
    {
        QMutexLocker locker(&mutex);
        retireAllTokensLocked();
    }

    bool enqueueAdvisory(ViewportProviderEvent event)
    {
        QMutexLocker locker(&mutex);
        const AdvisoryCategory category = classifyAdvisoryLocked(event.token);
        qsizetype pendingIndex = -1;
        for (qsizetype index = 0; index < pendingAdvisories.size(); ++index) {
            const auto& pending = pendingAdvisories.at(index);
            if (pending.category == category
                && (category != AdvisoryCategory::Active || pending.event.token == event.token)) {
                pendingIndex = index;
                break;
            }
        }
        if (pendingIndex < 0) {
            pendingAdvisories.append({ category, std::move(event) });
            Q_ASSERT(pendingAdvisories.size() <= 4);
        } else if (category == AdvisoryCategory::Active && usefulActiveAdvisory(event)
            && !usefulActiveAdvisory(pendingAdvisories.at(pendingIndex).event)) {
            pendingAdvisories[pendingIndex].event // clazy:exclude=detaching-member
                = std::move(event);
        }
        if (advisoryDeliveryScheduled) {
            return false;
        }
        advisoryDeliveryScheduled = true;
        return true;
    }

    QVector<ViewportProviderEvent> takePendingAdvisories()
    {
        QMutexLocker locker(&mutex);
        QVector<ViewportProviderEvent> result;
        result.reserve(pendingAdvisories.size());
        for (auto& pending : pendingAdvisories) {
            result.append(std::move(pending.event));
        }
        pendingAdvisories.clear();
        advisoryDeliveryScheduled = false;
        return result;
    }

    void advisoryDeliverySchedulingFailed()
    {
        QMutexLocker locker(&mutex);
        pendingAdvisories.clear();
        advisoryDeliveryScheduled = false;
    }

    void revoke()
    {
        QMutexLocker locker(&mutex);
        eventSink = {};
        activeTokens.clear();
        pendingAdvisories.clear();
        advisoryDeliveryScheduled = false;
    }

private:
    enum class AdvisoryCategory {
        Active,
        Stale,
        Mismatch,
    };

    struct PendingAdvisory
    {
        AdvisoryCategory category = AdvisoryCategory::Stale;
        ViewportProviderEvent event;
    };

    static bool usefulActiveAdvisory(const ViewportProviderEvent& event)
    {
        return event.kind == ImageSequenceProviderEventKind::Waiting
            || (event.kind == ImageSequenceProviderEventKind::Progress
                && std::isfinite(event.progress) && event.progress >= 0.0 && event.progress <= 1.0);
    }

    AdvisoryCategory classifyAdvisoryLocked(ImageSequenceProviderRequestToken token) const
    {
        if (activeTokens.contains(token)) {
            return AdvisoryCategory::Active;
        }
        const quint64 tokenValue
            = ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(token);
        if (activeTokens.isEmpty() || (token.isValid() && tokenValue <= highestIssuedToken)) {
            return AdvisoryCategory::Stale;
        }
        return AdvisoryCategory::Mismatch;
    }

    void retireTokenLocked(ImageSequenceProviderRequestToken token)
    {
        activeTokens.removeAll(token);
        bool staleRepresentativeExists = std::any_of(pendingAdvisories.cbegin(),
            pendingAdvisories.cend(), [](const PendingAdvisory& candidate) {
                return candidate.category == AdvisoryCategory::Stale;
            });
        for (qsizetype index = pendingAdvisories.size(); index-- > 0;) {
            auto& pending = pendingAdvisories[index]; // clazy:exclude=detaching-member
            if (pending.category != AdvisoryCategory::Active || pending.event.token != token) {
                continue;
            }
            if (staleRepresentativeExists) {
                pendingAdvisories.removeAt(index);
            } else {
                pending.category = AdvisoryCategory::Stale;
                staleRepresentativeExists = true;
            }
        }
    }

    void retireAllTokensLocked()
    {
        const QVector<ImageSequenceProviderRequestToken> tokens = activeTokens;
        for (ImageSequenceProviderRequestToken token : tokens) {
            retireTokenLocked(token);
        }
    }

    QMutex mutex;
    std::function<void(const ViewportProviderEvent&)> eventSink;
    QVector<ImageSequenceProviderRequestToken> activeTokens;
    QVector<PendingAdvisory> pendingAdvisories;
    quint64 highestIssuedToken = 0;
    bool advisoryDeliveryScheduled = false;
};

class ViewportProviderLeaseRegistry
    : public std::enable_shared_from_this<ViewportProviderLeaseRegistry>
{
public:
    struct LeaseSnapshot
    {
        std::shared_ptr<ViewportProviderSessionControl> sessionControl;
        QPointer<ImageSequenceProviderFrameHandle> frameHandle;
        QPointer<ImageSequenceProviderFailureHandle> failureHandle;
        bool failureLease = false;
        quint64 generation = 0;
        quint64 sessionSerial = 0;
    };

    explicit ViewportProviderLeaseRegistry(ViewportProviderExecutor* executor)
        : providerExecutor(executor)
        , cleanupDispatchTarget(QCoreApplication::instance())
    {
    }

    void setExecutor(ViewportProviderExecutor& executor)
    {
        QMutexLocker locker(&mutex);
        providerExecutor = &executor;
    }

    quint64 claim(const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFrameHandle* frameHandle)
    {
        if (!sessionControl || !frameHandle) {
            return 0;
        }
        sessionControl->claimHandleLease();
        const quint64 leaseId = allocateProviderLeaseId();
        bool releaseAutomatically = false;
        {
            QMutexLocker locker(&mutex);
            leases.insert(leaseId,
                { sessionControl, frameHandle, nullptr, false, sessionControl->generation(),
                    sessionControl->sessionSerial(), true, false });
            if (automaticCleanup) {
                retiredLeases.insert(leaseId);
                releaseAutomatically = true;
            }
        }
        if (releaseAutomatically) {
            scheduleAutomaticRelease(leaseId);
        }
        return leaseId;
    }

    quint64 claim(const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFailureHandle* failureHandle, quint64 generation = 0,
        quint64 sessionSerial = 0)
    {
        if (!failureHandle) {
            return 0;
        }
        if (sessionControl) {
            sessionControl->claimHandleLease();
            generation = sessionControl->generation();
            sessionSerial = sessionControl->sessionSerial();
        }
        const quint64 leaseId = allocateProviderLeaseId();
        bool releaseAutomatically = false;
        {
            QMutexLocker locker(&mutex);
            leases.insert(leaseId,
                { sessionControl, nullptr, failureHandle, true, generation, sessionSerial, true,
                    false });
            if (automaticCleanup) {
                retiredLeases.insert(leaseId);
                releaseAutomatically = true;
            }
        }
        if (releaseAutomatically) {
            scheduleAutomaticRelease(leaseId);
        }
        return leaseId;
    }

    void completeEventDelivery(quint64 leaseId)
    {
        QMutexLocker locker(&mutex);
        leases.detach();
        auto it = leases.find(leaseId); // clazy:exclude=detaching-member
        if (it != leases.end()) {
            it->pendingEngineDelivery = false;
        }
    }

    void reconcile(const QSet<quint64>& liveLeaseIds)
    {
        QMutexLocker locker(&mutex);
        for (auto it = leases.cbegin(); it != leases.cend(); ++it) {
            if (!it->pendingEngineDelivery && !liveLeaseIds.contains(it.key())) {
                retiredLeases.insert(it.key());
            }
        }
    }

    void retire(quint64 leaseId)
    {
        if (leaseId == 0) {
            return;
        }
        bool releaseAutomatically = false;
        {
            QMutexLocker locker(&mutex);
            if (leases.contains(leaseId)) {
                retiredLeases.insert(leaseId);
                releaseAutomatically = automaticCleanup;
            }
        }
        if (releaseAutomatically) {
            scheduleAutomaticRelease(leaseId);
        }
    }

    void retireAll()
    {
        QVector<quint64> leaseIds;
        {
            QMutexLocker locker(&mutex);
            automaticCleanup = true;
            leaseIds.reserve(leases.size());
            for (auto it = leases.cbegin(); it != leases.cend(); ++it) {
                retiredLeases.insert(it.key());
                leaseIds.append(it.key());
            }
        }
        for (quint64 leaseId : leaseIds) {
            scheduleAutomaticRelease(leaseId);
        }
    }

    QVector<quint64> retiredLeaseIds() const
    {
        QMutexLocker locker(&mutex);
        QVector<quint64> leaseIds;
        leaseIds.reserve(retiredLeases.size());
        for (quint64 leaseId : retiredLeases) {
            const auto it = leases.constFind(leaseId);
            if (it != leases.cend() && !it->releaseScheduling) {
                leaseIds.append(leaseId);
            }
        }
        return leaseIds;
    }

    std::optional<LeaseSnapshot> takeLeaseForRelease(quint64 leaseId)
    {
        QMutexLocker locker(&mutex);
        leases.detach();
        auto it = leases.find(leaseId); // clazy:exclude=detaching-member
        if (it == leases.end() || it->releaseScheduling) {
            return std::nullopt;
        }
        it->releaseScheduling = true;
        return LeaseSnapshot { it->sessionControl, it->frameHandle, it->failureHandle,
            it->failureLease, it->generation, it->sessionSerial };
    }

    void releaseSchedulingFailed(quint64 leaseId)
    {
        QMutexLocker locker(&mutex);
        leases.detach();
        auto it = leases.find(leaseId); // clazy:exclude=detaching-member
        if (it != leases.end()) {
            it->releaseScheduling = false;
        }
    }

    void erase(quint64 leaseId)
    {
        QMutexLocker locker(&mutex);
        leases.remove(leaseId);
        retiredLeases.remove(leaseId);
    }

    bool hasPendingCleanup() const
    {
        QMutexLocker locker(&mutex);
        return !retiredLeases.isEmpty();
    }

private:
    struct LeaseRecord
    {
        std::shared_ptr<ViewportProviderSessionControl> sessionControl;
        QPointer<ImageSequenceProviderFrameHandle> frameHandle;
        QPointer<ImageSequenceProviderFailureHandle> failureHandle;
        bool failureLease = false;
        quint64 generation = 0;
        quint64 sessionSerial = 0;
        bool pendingEngineDelivery = true;
        bool releaseScheduling = false;
    };

    void scheduleAutomaticRelease(quint64 leaseId)
    {
        const auto self = weak_from_this().lock();
        if (!self) {
            return;
        }
        if (cleanupDispatchTarget) {
            QMetaObject::invokeMethod(
                cleanupDispatchTarget, [self, leaseId]() { self->releaseAutomatically(leaseId); },
                Qt::QueuedConnection);
        }
    }

    void releaseAutomatically(quint64 leaseId)
    {
        const auto lease = takeLeaseForRelease(leaseId);
        if (!lease) {
            return;
        }
        ViewportProviderExecutor* executor = nullptr;
        {
            QMutexLocker locker(&mutex);
            executor = providerExecutor;
        }
        const auto outcome = !executor ? ViewportProviderExecutorOutcome::RetryableFailure
            : lease->failureLease
            ? executor->releaseFailureHandle(lease->sessionControl, lease->failureHandle)
            : executor->releaseFrameHandle(lease->sessionControl, lease->frameHandle);
        if (executorAccepted(outcome)) {
            erase(leaseId);
            return;
        }
        releaseSchedulingFailed(leaseId);
        const auto self = weak_from_this().lock();
        if (!self) {
            return;
        }
        if (cleanupDispatchTarget) {
            QTimer::singleShot(10, cleanupDispatchTarget,
                [self, leaseId]() { self->releaseAutomatically(leaseId); });
        }
    }

    mutable QMutex mutex;
    QHash<quint64, LeaseRecord> leases;
    QSet<quint64> retiredLeases;
    ViewportProviderExecutor* providerExecutor = nullptr;
    QPointer<QObject> cleanupDispatchTarget;
    bool automaticCleanup = false;
};

class ViewportProviderSessionCleanupRegistry
    : public std::enable_shared_from_this<ViewportProviderSessionCleanupRegistry>
{
public:
    struct SessionSnapshot
    {
        QPointer<ImageSequenceProviderSession> session;
        std::shared_ptr<ViewportProviderSessionControl> control;
        ImageSequenceProviderRequestToken metadataToken;
        ImageSequenceProviderRequestToken frameToken;
        bool closeScheduled = false;
    };

    ViewportProviderSessionCleanupRegistry()
        : cleanupDispatchTarget(QCoreApplication::instance())
    {
    }

    void adopt(SessionSnapshot session)
    {
        if (!session.session || !session.control) {
            return;
        }
        const bool needsClose = !session.closeScheduled;
        {
            QMutexLocker locker(&mutex);
            sessions.insert(session.session.data(), std::move(session));
        }
        if (needsClose) {
            scheduleRetry();
        }
    }

    void sessionDestroyed(ImageSequenceProviderSession* session)
    {
        QMutexLocker locker(&mutex);
        sessions.remove(session);
        if (sessions.isEmpty()) {
            retryDelayIndex = 0;
        }
    }

    bool takeForcedCloseFailure()
    {
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
        QMutexLocker locker(&mutex);
        if (forcedCloseFailuresRemaining > 0) {
            --forcedCloseFailuresRemaining;
            return true;
        }
#endif
        return false;
    }

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void failNextCloseDeliveries(qsizetype count)
    {
        QMutexLocker locker(&mutex);
        forcedCloseFailuresRemaining = std::max<qsizetype>(count, 0);
    }
#endif

private:
    static constexpr std::array<int, 5> retryDelays { 0, 10, 50, 250, 1000 };

    bool hasUnscheduledCloseLocked() const
    {
        return std::any_of(sessions.cbegin(), sessions.cend(),
            [](const SessionSnapshot& session) { return !session.closeScheduled; });
    }

    void scheduleRetry()
    {
        std::shared_ptr<ViewportProviderSessionCleanupRegistry> self;
        QPointer<QObject> target;
        int delay = 0;
        {
            QMutexLocker locker(&mutex);
            if (retryScheduled || !hasUnscheduledCloseLocked() || !cleanupDispatchTarget) {
                return;
            }
            retryScheduled = true;
            delay = retryDelays[size_t(retryDelayIndex)];
            target = cleanupDispatchTarget;
            self = shared_from_this();
        }
        QTimer::singleShot(delay, target, [self]() { self->retryPending(); });
    }

    void retryPending()
    {
        QVector<ImageSequenceProviderSession*> sessionKeys;
        {
            QMutexLocker locker(&mutex);
            retryScheduled = false;
            sessionKeys.reserve(sessions.size());
            for (auto it = sessions.cbegin(); it != sessions.cend(); ++it) {
                if (!it->closeScheduled) {
                    sessionKeys.append(it.key());
                }
            }
        }

        bool progress = false;
        bool failed = false;
        for (ImageSequenceProviderSession* session : std::as_const(sessionKeys)) {
            SessionSnapshot snapshot;
            {
                QMutexLocker locker(&mutex);
                const auto it = sessions.constFind(session);
                if (it == sessions.cend() || it->closeScheduled) {
                    continue;
                }
                snapshot = it.value();
            }
            const auto outcome = takeForcedCloseFailure()
                ? ViewportProviderExecutorOutcome::RetryableFailure
                : qtViewportProviderExecutor().queueSessionClose(
                      snapshot.control, snapshot.metadataToken, snapshot.frameToken);
            QMutexLocker locker(&mutex);
            auto it = sessions.find(session); // clazy:exclude=detaching-member
            if (it == sessions.end() || it->closeScheduled) {
                continue;
            }
            if (executorAccepted(outcome)) {
                it->closeScheduled = true;
                progress = true;
            } else {
                failed = true;
            }
        }

        {
            QMutexLocker locker(&mutex);
            if (progress) {
                retryDelayIndex = 0;
            } else if (failed && retryDelayIndex < int(retryDelays.size()) - 1) {
                ++retryDelayIndex;
            }
        }
        if (failed) {
            scheduleRetry();
        }
    }

    mutable QMutex mutex;
    QHash<ImageSequenceProviderSession*, SessionSnapshot> sessions;
    QPointer<QObject> cleanupDispatchTarget;
    int retryDelayIndex = 0;
    bool retryScheduled = false;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    qsizetype forcedCloseFailuresRemaining = 0;
#endif
};

ViewportProviderSessionControl::ViewportProviderSessionControl(
    ImageSequenceProviderSession* session, ImageSequenceProviderThreadingContract threadingContract,
    quint64 generation, quint64 sessionSerial)
    : providerSession(session)
    , contract(threadingContract)
    , generationIdentity(generation)
    , sessionIdentity(sessionSerial)
{
}

ImageSequenceProviderSession* ViewportProviderSessionControl::session() const
{
    QMutexLocker locker(&mutex);
    return providerSession;
}

ImageSequenceProviderThreadingContract ViewportProviderSessionControl::threadingContract() const
{
    return contract;
}

quint64 ViewportProviderSessionControl::generation() const { return generationIdentity; }

quint64 ViewportProviderSessionControl::sessionSerial() const { return sessionIdentity; }

bool ViewportProviderSessionControl::beginEventIngress()
{
    QMutexLocker locker(&mutex);
    if (!providerSession || destructionStarted) {
        return false;
    }
    ++activeIngressCount;
    return true;
}

void ViewportProviderSessionControl::claimHandleLease()
{
    QMutexLocker locker(&mutex);
    ++handleLeaseCount;
}

void ViewportProviderSessionControl::endEventIngress()
{
    bool scheduleCheck = false;
    {
        QMutexLocker locker(&mutex);
        Q_ASSERT(activeIngressCount > 0);
        --activeIngressCount;
        scheduleCheck = closeCompleted && activeIngressCount == 0 && handleLeaseCount == 0;
    }
    if (scheduleCheck) {
        scheduleDestructionCheck();
    }
}

void ViewportProviderSessionControl::completeCloseOnSessionAffinity()
{
    {
        QMutexLocker locker(&mutex);
        closeCompleted = true;
    }
    destroySessionIfReadyOnSessionAffinity();
}

void ViewportProviderSessionControl::completeHandleReleaseOnSessionAffinity()
{
    {
        QMutexLocker locker(&mutex);
        Q_ASSERT(handleLeaseCount > 0);
        --handleLeaseCount;
    }
    destroySessionIfReadyOnSessionAffinity();
}

void ViewportProviderSessionControl::markSessionDestroyed()
{
    QMutexLocker locker(&mutex);
    providerSession = nullptr;
    destructionStarted = true;
}

void ViewportProviderSessionControl::destroySessionIfReadyOnSessionAffinity()
{
    ImageSequenceProviderSession* sessionToDestroy = nullptr;
    {
        QMutexLocker locker(&mutex);
        if (!providerSession || destructionStarted || !closeCompleted || activeIngressCount != 0
            || handleLeaseCount != 0) {
            return;
        }
        destructionStarted = true;
        sessionToDestroy = providerSession;
        providerSession = nullptr;
    }
    Q_ASSERT(sessionToDestroy->thread() == QThread::currentThread());
    sessionToDestroy->setParent(nullptr);
    delete sessionToDestroy;
}

void ViewportProviderSessionControl::scheduleDestructionCheck()
{
    ImageSequenceProviderSession* currentSession = session();
    if (!currentSession) {
        return;
    }
    if (currentSession->thread() == QThread::currentThread()) {
        destroySessionIfReadyOnSessionAffinity();
        return;
    }
    const auto self = shared_from_this();
    QMetaObject::invokeMethod(
        currentSession, [self]() { self->destroySessionIfReadyOnSessionAffinity(); },
        Qt::QueuedConnection);
}

ViewportProviderBridge::ViewportProviderBridge(ImageViewportPageRole role)
    : role(role)
    , providerExecutor(defaultProviderExecutor())
    , leaseRegistry(std::make_shared<ViewportProviderLeaseRegistry>(providerExecutor))
    , sessionCleanupRegistry(std::make_shared<ViewportProviderSessionCleanupRegistry>())
{
}

ViewportProviderBridge::~ViewportProviderBridge()
{
    leaseRegistry->setExecutor(qtViewportProviderExecutor());
    for (const auto& endpoint : std::as_const(eventEndpoints)) {
        if (const auto value = endpoint.lock()) {
            value->revoke();
        }
    }
    leaseRegistry->retireAll();
    for (const SessionRecord& record : std::as_const(sessions)) {
        sessionCleanupRegistry->adopt({ record.session, record.control, record.metadataToken,
            record.frameToken, record.lifecycle == SessionLifecycle::Closing });
    }
}

ViewportProviderTransportResult ViewportProviderBridge::closeSession(
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken)
{
    return closeSession(0, 0, metadataToken, frameToken);
}

ViewportProviderTransportResult ViewportProviderBridge::closeSession(quint64 generation,
    quint64 sessionSerial, ImageSequenceProviderRequestToken metadataToken,
    ImageSequenceProviderRequestToken frameToken)
{
    ViewportProviderTransportResult result;
    ImageSequenceProviderSession* session = nullptr;
    if (generation == 0 && sessionSerial == 0) {
        session = activeSession;
    } else {
        for (auto it = sessions.cbegin(); it != sessions.cend(); ++it) {
            if (it->generation == generation && it->sessionSerial == sessionSerial) {
                session = it.key();
                break;
            }
        }
    }
    if (!session) {
        return result;
    }
    sessions.detach();
    auto recordIt = sessions.find(session); // clazy:exclude=detaching-member
    if (recordIt == sessions.end()) {
        activeSession.clear();
        return result;
    }
    SessionRecord& record = recordIt.value();
    record.metadataToken = metadataToken;
    record.frameToken = frameToken;
    record.eventEndpoint->observeClose();
    if (activeSession == session) {
        activeSession.clear();
    }

    if (takeForcedDeliveryFailureForTest()) {
        result.diagnostic = providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Close, metadataToken, frameToken,
            false, true, record.generation, record.sessionSerial);
        record.lifecycle = SessionLifecycle::CleanupPending;
        return result;
    }

    result.delivered
        = executorAccepted(queueSessionClose(record.control, metadataToken, frameToken));
    if (!result.delivered) {
        result.diagnostic = providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Close, metadataToken, frameToken,
            false, true, record.generation, record.sessionSerial);
        record.lifecycle = SessionLifecycle::CleanupPending;
        return result;
    }
    record.lifecycle = SessionLifecycle::Closing;
    return result;
}

ViewportProviderTransportResult ViewportProviderBridge::activateSession(
    quint64 generation, quint64 sessionSerial)
{
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it) {
        if (it->generation == generation && it->sessionSerial == sessionSerial
            && it->lifecycle == SessionLifecycle::Active && it->session) {
            activeSession = it->session;
            return { true };
        }
    }
    return {};
}

ViewportProviderSessionOpenTransportResult ViewportProviderBridge::openSession(
    const ViewportProviderSessionOpenInput& input)
{
    if (!input.factory || !input.callbackTarget || !input.eventSink || input.generation == 0
        || input.sessionSerial == 0) {
        return {};
    }

    const ImageSequenceProviderSessionFactoryResult factoryResult = (*input.factory)();
    ImageSequenceProviderSession* session = factoryResult.session();
    const bool validCreated
        = factoryResult.outcome() == ImageSequenceProviderSessionFactoryOutcome::Created && session
        && !factoryResult.failure().isValid();
    if (!validCreated) {
        const ImageSequenceProviderFailure failure = factoryResult.failure();
        ImageSequenceProviderFailureHandle* handle = failure.applicationFailureHandle();
        const quint64 leaseId
            = leaseRegistry->claim({}, handle, input.generation, input.sessionSerial);
        const bool admitted
            = factoryResult.outcome() == ImageSequenceProviderSessionFactoryOutcome::Failed
            && !session && failure.isValid();
        return { false, admitted,
            admitted ? failure.cause() : ImageSequenceProviderFailureCause::Unavailable,
            admitted && handle ? handle->reference() : ImageSequenceProviderFailureReference {},
            leaseId };
    }
    if (!claimProviderSession(session)) {
        return {};
    }
    if (session->parent() && session->thread() == QThread::currentThread()) {
        session->setParent(nullptr);
    }
    const auto sessionControl = std::make_shared<ViewportProviderSessionControl>(
        session, input.threadingContract, input.generation, input.sessionSerial);
    const auto eventEndpoint = std::make_shared<ViewportProviderEventEndpoint>(input.eventSink);
    QObject::connect(session, &QObject::destroyed, session,
        [session, sessionControl, cleanupRegistry = sessionCleanupRegistry]() {
            sessionControl->markSessionDestroyed();
            cleanupRegistry->sessionDestroyed(session);
            releaseProviderSession(session);
        });
    activeSession = session;
    sessions.insert(session,
        { session, input.threadingContract, input.generation, input.sessionSerial,
            SessionLifecycle::Active, {}, {}, sessionControl, eventEndpoint });
    eventEndpoints.append(eventEndpoint);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    const bool deliverSynchronously = synchronousEventDelivery;
#else
    constexpr bool deliverSynchronously = false;
#endif

    QObject::connect(
        session, &ImageSequenceProviderSession::providerEvent, input.callbackTarget,
        [sessionControl, eventEndpoint, leaseRegistry = leaseRegistry,
            callbackTarget = input.callbackTarget, eventRole = role,
            sessionSerial = input.sessionSerial, generation = input.generation,
            deliverSynchronously](const ImageSequenceProviderEvent& typedEvent) {
            if (!sessionControl->beginEventIngress()) {
                return;
            }
            ViewportProviderEvent event
                = viewportProviderEventFromTyped(eventRole, sessionSerial, generation, typedEvent);
            if (event.frameHandle) {
                event.frameLeaseId = leaseRegistry->claim(sessionControl, event.frameHandle);
            }
            if (event.failureHandle) {
                event.failureLeaseId = leaseRegistry->claim(sessionControl, event.failureHandle);
            }
            const bool advisory = event.kind == ImageSequenceProviderEventKind::Waiting
                || event.kind == ImageSequenceProviderEventKind::Progress;
            if (!deliverSynchronously && advisory) {
                if (eventEndpoint->enqueueAdvisory(std::move(event))) {
                    auto deliverAdvisories = [eventEndpoint]() {
                        const auto events = eventEndpoint->takePendingAdvisories();
                        for (const auto& pendingEvent : events) {
                            eventEndpoint->deliver(pendingEvent);
                        }
                    };
                    if (!QMetaObject::invokeMethod(
                            callbackTarget, std::move(deliverAdvisories), Qt::QueuedConnection)) {
                        eventEndpoint->advisoryDeliverySchedulingFailed();
                    }
                }
                sessionControl->endEventIngress();
                return;
            }
            if (!advisory) {
                eventEndpoint->observeTerminal(event.token);
            }
            auto deliver = [eventEndpoint, leaseRegistry, event]() {
                if (!eventEndpoint->deliver(event)) {
                    leaseRegistry->retire(event.frameLeaseId);
                    leaseRegistry->retire(event.failureLeaseId);
                }
            };
            if (deliverSynchronously) {
                deliver();
            } else if (!QMetaObject::invokeMethod(
                           callbackTarget, std::move(deliver), Qt::QueuedConnection)
                && (event.frameLeaseId != 0 || event.failureLeaseId != 0)) {
                leaseRegistry->retire(event.frameLeaseId);
                leaseRegistry->retire(event.failureLeaseId);
            }
            sessionControl->endEventIngress();
        },
        Qt::DirectConnection);

    return { true };
}

ViewportProviderTransportResult ViewportProviderBridge::deliverRequest(
    const ImageSequenceProviderRequest& request)
{
    ViewportProviderTransportResult result;
    if (!request.isValid()) {
        return result;
    }
    ImageSequenceProviderSession* session = activeSession;
    auto recordIt = sessions.constFind(session);
    const quint64 generation = recordIt == sessions.cend() ? 0 : recordIt->generation;
    const quint64 sessionSerial = recordIt == sessions.cend() ? 0 : recordIt->sessionSerial;
    const auto eventEndpoint = recordIt == sessions.cend()
        ? std::shared_ptr<ViewportProviderEventEndpoint> {}
        : recordIt->eventEndpoint;
    if (eventEndpoint) {
        eventEndpoint->observeRequest(request);
    }
    if (takeForcedDeliveryFailureForTest()) {
        if (eventEndpoint) {
            eventEndpoint->observeRequestDeliveryFailure(request);
        }
        result.diagnostic = providerTransportDiagnostic(role, request, generation, sessionSerial);
        return result;
    }
    const auto threadingContract = recordIt == sessions.cend()
        ? ImageSequenceProviderThreadingContract::AffinityBound
        : recordIt->threadingContract;
    result.delivered = executorAccepted(executor().invokeSessionCommand(
        session, threadingContract, [session, request]() { session->request(request); }));
    if (!result.delivered) {
        if (eventEndpoint) {
            eventEndpoint->observeRequestDeliveryFailure(request);
        }
        result.diagnostic = providerTransportDiagnostic(role, request, generation, sessionSerial);
    }
    return result;
}

void ViewportProviderBridge::setExecutor(ViewportProviderExecutor& executor)
{
    providerExecutor = &executor;
    leaseRegistry->setExecutor(executor);
}

ViewportProviderExecutor& ViewportProviderBridge::executor() const
{
    return providerExecutor ? *providerExecutor : qtViewportProviderExecutor();
}

ViewportProviderExecutorOutcome ViewportProviderBridge::queueSessionClose(
    const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken)
{
    if (sessionCleanupRegistry->takeForcedCloseFailure()) {
        return ViewportProviderExecutorOutcome::RetryableFailure;
    }
    return executor().queueSessionClose(sessionControl, metadataToken, frameToken);
}

void ViewportProviderBridge::pruneExpiredEventEndpoints()
{
    eventEndpoints.removeIf([](const std::weak_ptr<ViewportProviderEventEndpoint>& endpoint) {
        return endpoint.expired();
    });
}

void ViewportProviderBridge::completeFrameEventDelivery(quint64 leaseId)
{
    leaseRegistry->completeEventDelivery(leaseId);
}

void ViewportProviderBridge::completeFailureEventDelivery(quint64 leaseId)
{
    leaseRegistry->completeEventDelivery(leaseId);
}

void ViewportProviderBridge::reconcileLeases(const QSet<quint64>& liveLeaseIds)
{
    leaseRegistry->reconcile(liveLeaseIds);
}

ViewportProviderCleanupResult ViewportProviderBridge::drainCleanup(bool retryPendingSessions)
{
    ViewportProviderCleanupResult result;
    const auto retired = leaseRegistry->retiredLeaseIds();
    for (quint64 leaseId : retired) {
        ViewportProviderCleanupResult released = releaseLease(leaseId);
        result.diagnostics.append(released.diagnostics);
        result.progress = result.progress || released.progress;
    }
    retrySessionCleanup(result, retryPendingSessions);
    result.pending = hasPendingCleanup();
    return result;
}

bool ViewportProviderBridge::hasPendingCleanup() const
{
    if (leaseRegistry->hasPendingCleanup()) {
        return true;
    }
    for (const SessionRecord& record : sessions) {
        if (record.lifecycle != SessionLifecycle::Active) {
            return true;
        }
    }
    return false;
}

ViewportProviderCleanupResult ViewportProviderBridge::releaseAllProviderLeases()
{
    leaseRegistry->retireAll();
    return drainCleanup(true);
}

ViewportProviderCleanupResult ViewportProviderBridge::releaseLease(quint64 leaseId)
{
    ViewportProviderCleanupResult result;
    const auto lease = leaseRegistry->takeLeaseForRelease(leaseId);
    if (!lease) {
        return result;
    }
    const auto outcome = lease->failureLease
        ? executor().releaseFailureHandle(lease->sessionControl, lease->failureHandle)
        : executor().releaseFrameHandle(lease->sessionControl, lease->frameHandle);
    if (!executorAccepted(outcome)) {
        leaseRegistry->releaseSchedulingFailed(leaseId);
        result.diagnostics.append(providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Release, {}, {}, false, true,
            lease->generation, lease->sessionSerial, leaseId));
        result.pending = true;
        return result;
    }
    leaseRegistry->erase(leaseId);
    result.progress = true;
    result.pending = hasPendingCleanup();
    return result;
}

void ViewportProviderBridge::retrySessionCleanup(
    ViewportProviderCleanupResult& result, bool retryPendingSessions)
{
    const auto sessionKeys = sessions.keys();
    for (ImageSequenceProviderSession* session : sessionKeys) {
        sessions.detach();
        auto recordIt = sessions.find(session); // clazy:exclude=detaching-member
        if (recordIt == sessions.end()) {
            continue;
        }
        if (!recordIt->session) {
            if (activeSession == session) {
                activeSession.clear();
            }
            sessions.erase(recordIt);
            result.progress = true;
            continue;
        }
        if (recordIt->lifecycle == SessionLifecycle::CleanupPending && retryPendingSessions) {
            const SessionRecord& record = recordIt.value();
            const auto outcome
                = queueSessionClose(record.control, record.metadataToken, record.frameToken);
            if (!executorAccepted(outcome)) {
                result.diagnostics.append(providerTransportDiagnostic(role,
                    ImageViewportInternal::ProviderTransportOperation::Close, record.metadataToken,
                    record.frameToken, false, true, record.generation, record.sessionSerial));
                result.pending = true;
                continue;
            }
            result.progress = true;
            recordIt->lifecycle = SessionLifecycle::Closing;
        }
    }
    pruneExpiredEventEndpoints();
}

bool ViewportProviderBridge::takeForcedDeliveryFailureForTest()
{
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    if (forceNextCommandDeliveryFailure) {
        forceNextCommandDeliveryFailure = false;
        return true;
    }
#endif
    return false;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportProviderBridge::failNextCommandDeliveryForTest()
{
    forceNextCommandDeliveryFailure = true;
}

void ViewportProviderBridge::failNextSessionCloseDeliveriesForTest(qsizetype count)
{
    sessionCleanupRegistry->failNextCloseDeliveries(count);
}

void ViewportProviderBridge::useSynchronousEventDeliveryForTest()
{
    synchronousEventDelivery = true;
}

qsizetype ViewportProviderBridge::retainedEventEndpointCountForTest() const
{
    return eventEndpoints.size();
}

ViewportProviderExecutor& synchronousViewportProviderExecutorForTest()
{
    static SynchronousViewportProviderExecutor executor;
    return executor;
}
#endif
