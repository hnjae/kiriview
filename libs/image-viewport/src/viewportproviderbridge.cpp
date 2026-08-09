// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportproviderbridge_p.h"

#include "imageviewportprovidersubmission_p.h"
#include "imageviewporttoken_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMetaObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QPointer>
#include <QtCore/QScopeGuard>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(imageViewportProviderLog, "org.hnjae.imageviewport.provider", QtWarningMsg)

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
constexpr qsizetype maximumRetainedProviderSessionCountPerRole = 2;

bool hasCurrentThreadAffinity(const QObject* object)
{
    const QThread* thread = object == nullptr ? nullptr : object->thread();
    return thread != nullptr && thread->isCurrentThread();
}

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

quint64 allocateProviderDeliveryId()
{
    static QMutex mutex;
    static quint64 nextId = 0;
    QMutexLocker locker(&mutex);
    if (nextId == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport provider event delivery identity exhausted");
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
        if (hasCurrentThreadAffinity(session)) {
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
            if (hasCurrentThreadAffinity(session)) {
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
            if (!session || hasCurrentThreadAffinity(session)) {
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
        if (hasCurrentThreadAffinity(session)) {
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
            if (!session || hasCurrentThreadAffinity(session)) {
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
        if (hasCurrentThreadAffinity(session)) {
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
    struct AdmissionResult
    {
        ImageSequenceProviderEventSubmissionOutcome outcome
            = ImageSequenceProviderEventSubmissionOutcome::Rejected;
        bool scheduleDelivery = false;
    };

    explicit ViewportProviderEventEndpoint(std::shared_ptr<ViewportProviderSessionControl> control,
        std::function<void(const ViewportProviderEvent&)> sink)
        : sessionControl(std::move(control))
        , eventSink(std::move(sink))
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

    AdmissionResult admit(ViewportProviderEvent event,
        const std::function<void(ViewportProviderEvent&)>& claimOwnership)
    {
        QMutexLocker locker(&mutex);
        if (closed) {
            return { ImageSequenceProviderEventSubmissionOutcome::Closed, false };
        }
        const std::optional<DeliveryCredit> credit = admitCreditLocked(event);
        if (!credit) {
            return { ImageSequenceProviderEventSubmissionOutcome::Rejected, false };
        }
        event.deliveryId = allocateProviderDeliveryId();
        claimOwnership(event);
        sessionControl->claimEventDelivery();
        deliveryCredits.insert(event.deliveryId, *credit);
        pendingDeliveries.append(std::move(event));
        if (deliveryScheduled) {
            return { ImageSequenceProviderEventSubmissionOutcome::Accepted, false };
        }
        deliveryScheduled = true;
        return { ImageSequenceProviderEventSubmissionOutcome::Accepted, true };
    }

    std::optional<ViewportProviderEvent> takeNextDelivery()
    {
        QMutexLocker locker(&mutex);
        if (pendingDeliveries.isEmpty()) {
            return std::nullopt;
        }
        return pendingDeliveries.takeFirst();
    }

    bool completeScheduledDeliveryDrain()
    {
        QMutexLocker locker(&mutex);
        if (!pendingDeliveries.isEmpty()) {
            return false;
        }
        deliveryScheduled = false;
        return true;
    }

    bool completeDelivery(quint64 deliveryId)
    {
        if (deliveryId == 0) {
            return false;
        }
        bool completed = false;
        {
            QMutexLocker locker(&mutex);
            auto creditIt = deliveryCredits.find(deliveryId); // clazy:exclude=detaching-member
            if (creditIt == deliveryCredits.end()) {
                return false;
            }
            switch (creditIt.value()) {
            case DeliveryCredit::Normal:
                break;
            case DeliveryCredit::FaultRepresentative:
                faultRepresentativeOutstanding = false;
                break;
            case DeliveryCredit::StaleRepresentative:
                staleRepresentativeOutstanding = false;
                break;
            case DeliveryCredit::MismatchRepresentative:
                mismatchRepresentativeOutstanding = false;
                break;
            }
            deliveryCredits.erase(creditIt);
            completed = true;
        }
        if (completed) {
            sessionControl->completeEventDelivery();
        }
        return completed;
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
            if (activeTokenIndexLocked(request.token()) < 0) {
                activeTokens.append({ request.token(), request.kind(), false, false });
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

    AdmissionResult enqueueAdvisory(ViewportProviderEvent event)
    {
        QMutexLocker locker(&mutex);
        if (closed) {
            return { ImageSequenceProviderEventSubmissionOutcome::Closed, false };
        }
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
            return { ImageSequenceProviderEventSubmissionOutcome::Accepted, false };
        }
        advisoryDeliveryScheduled = true;
        return { ImageSequenceProviderEventSubmissionOutcome::Accepted, true };
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

    QVector<ViewportProviderEvent> deliverySchedulingFailed()
    {
        QMutexLocker locker(&mutex);
        deliveryScheduled = false;
        return std::exchange(pendingDeliveries, {});
    }

    QVector<ViewportProviderEvent> revoke()
    {
        QMutexLocker locker(&mutex);
        closed = true;
        eventSink = {};
        activeTokens.clear();
        pendingAdvisories.clear();
        advisoryDeliveryScheduled = false;
        deliveryScheduled = false;
        return std::exchange(pendingDeliveries, {});
    }

    void closeAcceptance()
    {
        QMutexLocker locker(&mutex);
        closed = true;
        retireAllTokensLocked();
    }

    QVector<quint64> outstandingDeliveryIds() const
    {
        QMutexLocker locker(&mutex);
        return deliveryCredits.keys();
    }

private:
    enum class DeliveryCredit {
        Normal,
        FaultRepresentative,
        StaleRepresentative,
        MismatchRepresentative,
    };

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

    struct ActiveToken
    {
        ImageSequenceProviderRequestToken token;
        ImageSequenceProviderRequestKind requestKind = ImageSequenceProviderRequestKind::Metadata;
        bool provisionalAccepted = false;
        bool terminalAccepted = false;
    };

    [[nodiscard]] static bool terminalCompatible(
        ImageSequenceProviderEventKind eventKind, ImageSequenceProviderRequestKind requestKind)
    {
        switch (eventKind) {
        case ImageSequenceProviderEventKind::MetadataReady:
            return requestKind == ImageSequenceProviderRequestKind::Metadata;
        case ImageSequenceProviderEventKind::FrameReady:
            return requestKind == ImageSequenceProviderRequestKind::Frame
                || requestKind == ImageSequenceProviderRequestKind::Position
                || requestKind == ImageSequenceProviderRequestKind::Playback;
        case ImageSequenceProviderEventKind::EndOfSequence:
            return requestKind == ImageSequenceProviderRequestKind::Playback;
        case ImageSequenceProviderEventKind::Unsupported:
        case ImageSequenceProviderEventKind::Failed:
        case ImageSequenceProviderEventKind::Cancelled:
            return true;
        case ImageSequenceProviderEventKind::ProvisionalFrameReady:
        case ImageSequenceProviderEventKind::Waiting:
        case ImageSequenceProviderEventKind::Progress:
            return false;
        }
        return false;
    }

    [[nodiscard]] std::optional<DeliveryCredit> representativeCreditLocked(DeliveryCredit credit)
    {
        bool* outstanding = nullptr;
        switch (credit) {
        case DeliveryCredit::Normal:
            return credit;
        case DeliveryCredit::FaultRepresentative:
            outstanding = &faultRepresentativeOutstanding;
            break;
        case DeliveryCredit::StaleRepresentative:
            outstanding = &staleRepresentativeOutstanding;
            break;
        case DeliveryCredit::MismatchRepresentative:
            outstanding = &mismatchRepresentativeOutstanding;
            break;
        }
        if (*outstanding) {
            return std::nullopt;
        }
        *outstanding = true;
        return credit;
    }

    [[nodiscard]] std::optional<DeliveryCredit> admitCreditLocked(
        const ViewportProviderEvent& event)
    {
        const qsizetype activeIndex = activeTokenIndexLocked(event.token);
        if (activeIndex >= 0) {
            auto& active = activeTokens[activeIndex]; // clazy:exclude=detaching-member
            if (event.kind == ImageSequenceProviderEventKind::ProvisionalFrameReady
                && active.requestKind == ImageSequenceProviderRequestKind::Frame
                && !active.provisionalAccepted && !active.terminalAccepted) {
                active.provisionalAccepted = true;
                return DeliveryCredit::Normal;
            }
            if (terminalCompatible(event.kind, active.requestKind) && !active.terminalAccepted) {
                active.terminalAccepted = true;
                activeTokens.removeAt(activeIndex);
                return DeliveryCredit::Normal;
            }
            return representativeCreditLocked(DeliveryCredit::FaultRepresentative);
        }

        const quint64 tokenValue
            = ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(event.token);
        if (activeTokens.isEmpty() || (event.token.isValid() && tokenValue <= highestIssuedToken)) {
            return representativeCreditLocked(DeliveryCredit::StaleRepresentative);
        }
        return representativeCreditLocked(DeliveryCredit::MismatchRepresentative);
    }

    [[nodiscard]] qsizetype activeTokenIndexLocked(ImageSequenceProviderRequestToken token) const
    {
        for (qsizetype index = 0; index < activeTokens.size(); ++index) {
            if (activeTokens.at(index).token == token) {
                return index;
            }
        }
        return -1;
    }

    static bool usefulActiveAdvisory(const ViewportProviderEvent& event)
    {
        return event.kind == ImageSequenceProviderEventKind::Waiting
            || (event.kind == ImageSequenceProviderEventKind::Progress
                && std::isfinite(event.progress) && event.progress >= 0.0 && event.progress <= 1.0);
    }

    [[nodiscard]] AdvisoryCategory classifyAdvisoryLocked(
        ImageSequenceProviderRequestToken token) const
    {
        if (activeTokenIndexLocked(token) >= 0) {
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
        const qsizetype activeIndex = activeTokenIndexLocked(token);
        if (activeIndex >= 0) {
            activeTokens.removeAt(activeIndex);
        }
        bool staleRepresentativeExists
            = std::ranges::any_of(pendingAdvisories, [](const PendingAdvisory& candidate) {
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
        QVector<ImageSequenceProviderRequestToken> tokens;
        tokens.reserve(activeTokens.size());
        for (const auto& active : std::as_const(activeTokens)) {
            tokens.append(active.token);
        }
        for (ImageSequenceProviderRequestToken token : std::as_const(tokens)) {
            retireTokenLocked(token);
        }
    }

    mutable QMutex mutex;
    std::shared_ptr<ViewportProviderSessionControl> sessionControl;
    std::function<void(const ViewportProviderEvent&)> eventSink;
    QVector<ActiveToken> activeTokens;
    QVector<PendingAdvisory> pendingAdvisories;
    QVector<ViewportProviderEvent> pendingDeliveries;
    QHash<quint64, DeliveryCredit> deliveryCredits;
    quint64 highestIssuedToken = 0;
    bool advisoryDeliveryScheduled = false;
    bool deliveryScheduled = false;
    bool faultRepresentativeOutstanding = false;
    bool staleRepresentativeOutstanding = false;
    bool mismatchRepresentativeOutstanding = false;
    bool closed = false;
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
        for (const auto& [leaseId, lease] : std::as_const(leases).asKeyValueRange()) {
            if (!lease.pendingEngineDelivery && !liveLeaseIds.contains(leaseId)) {
                retiredLeases.insert(leaseId);
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
            QTimer::singleShot(10ms, cleanupDispatchTarget,
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

namespace {
void drainProviderEventDeliveries(
    const std::shared_ptr<ViewportProviderEventEndpoint>& eventEndpoint,
    const std::shared_ptr<ViewportProviderLeaseRegistry>& leaseRegistry,
    bool completeScheduledDrain)
{
    if (!eventEndpoint || !leaseRegistry) {
        return;
    }
    for (;;) {
        while (const auto pendingEvent = eventEndpoint->takeNextDelivery()) {
            if (eventEndpoint->deliver(*pendingEvent)) {
                continue;
            }
            leaseRegistry->retire(pendingEvent->frameLeaseId);
            leaseRegistry->retire(pendingEvent->failureLeaseId);
            eventEndpoint->completeDelivery(pendingEvent->deliveryId);
        }
        if (!completeScheduledDrain || eventEndpoint->completeScheduledDeliveryDrain()) {
            return;
        }
    }
}
}

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
    static constexpr std::array retryDelays { 0ms, 10ms, 50ms, 250ms, 1000ms };

    bool hasUnscheduledCloseLocked() const
    {
        return std::ranges::any_of(
            sessions, [](const SessionSnapshot& session) { return !session.closeScheduled; });
    }

    void scheduleRetry()
    {
        std::shared_ptr<ViewportProviderSessionCleanupRegistry> self;
        QPointer<QObject> target;
        std::chrono::milliseconds delay {};
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
            for (const auto& [session, snapshot] : std::as_const(sessions).asKeyValueRange()) {
                if (!snapshot.closeScheduled) {
                    sessionKeys.append(session);
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
            } else if (failed && std::cmp_less(retryDelayIndex + 1, retryDelays.size())) {
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

void ViewportProviderSessionControl::claimEventDelivery()
{
    QMutexLocker locker(&mutex);
    ++acceptedDeliveryCount;
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
        scheduleCheck = closeCompleted && activeIngressCount == 0 && acceptedDeliveryCount == 0
            && handleLeaseCount == 0;
    }
    if (scheduleCheck) {
        scheduleDestructionCheck();
    }
}

void ViewportProviderSessionControl::completeEventDelivery()
{
    bool scheduleCheck = false;
    {
        QMutexLocker locker(&mutex);
        Q_ASSERT(acceptedDeliveryCount > 0);
        --acceptedDeliveryCount;
        scheduleCheck = closeCompleted && acceptedDeliveryCount == 0 && activeIngressCount == 0
            && handleLeaseCount == 0;
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
            || acceptedDeliveryCount != 0 || handleLeaseCount != 0) {
            return;
        }
        destructionStarted = true;
        sessionToDestroy = providerSession;
        providerSession = nullptr;
    }
    Q_ASSERT(hasCurrentThreadAffinity(sessionToDestroy));
    sessionToDestroy->setParent(nullptr);
    delete sessionToDestroy;
}

void ViewportProviderSessionControl::scheduleDestructionCheck()
{
    ImageSequenceProviderSession* currentSession = session();
    if (!currentSession) {
        return;
    }
    if (hasCurrentThreadAffinity(currentSession)) {
        destroySessionIfReadyOnSessionAffinity();
        return;
    }
    const auto self = weak_from_this().lock();
    if (!self) {
        return;
    }
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
            const auto undelivered = value->revoke();
            for (const auto& event : undelivered) {
                leaseRegistry->retire(event.frameLeaseId);
                leaseRegistry->retire(event.failureLeaseId);
                value->completeDelivery(event.deliveryId);
            }
            const auto outstandingDeliveryIds = value->outstandingDeliveryIds();
            for (const quint64 deliveryId : outstandingDeliveryIds) {
                value->completeDelivery(deliveryId);
            }
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
        for (const auto& [candidate, record] : std::as_const(sessions).asKeyValueRange()) {
            if (record.generation == generation && record.sessionSerial == sessionSerial) {
                session = candidate;
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
    qCDebug(imageViewportProviderLog)
        << "provider session close requested"
        << "role" << static_cast<int>(role) << "generation" << record.generation << "sessionSerial"
        << record.sessionSerial << "retainedSessions" << sessions.size() << "metadataTokenValid"
        << metadataToken.isValid() << "frameTokenValid" << frameToken.isValid();
    record.metadataToken = metadataToken;
    record.frameToken = frameToken;
    record.eventEndpoint->closeAcceptance();
    if (activeSession == session) {
        activeSession.clear();
    }

    if (takeForcedDeliveryFailureForTest()) {
        result.diagnostic = providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Close, metadataToken, frameToken,
            false, true, record.generation, record.sessionSerial);
        record.lifecycle = SessionLifecycle::CleanupPending;
        qCWarning(imageViewportProviderLog)
            << "provider session close delivery failed"
            << "role" << static_cast<int>(role) << "generation" << record.generation
            << "sessionSerial" << record.sessionSerial << "lifecycle"
            << "cleanup-pending";
        return result;
    }

    result.delivered
        = executorAccepted(queueSessionClose(record.control, metadataToken, frameToken));
    if (!result.delivered) {
        result.diagnostic = providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Close, metadataToken, frameToken,
            false, true, record.generation, record.sessionSerial);
        record.lifecycle = SessionLifecycle::CleanupPending;
        qCWarning(imageViewportProviderLog)
            << "provider session close delivery failed"
            << "role" << static_cast<int>(role) << "generation" << record.generation
            << "sessionSerial" << record.sessionSerial << "lifecycle"
            << "cleanup-pending";
        return result;
    }
    record.lifecycle = SessionLifecycle::Closing;
    qCDebug(imageViewportProviderLog)
        << "provider session close queued"
        << "role" << static_cast<int>(role) << "generation" << record.generation << "sessionSerial"
        << record.sessionSerial << "lifecycle"
        << "closing";
    return result;
}

ViewportProviderTransportResult ViewportProviderBridge::activateSession(
    quint64 generation, quint64 sessionSerial)
{
    for (const auto& [session, record] : std::as_const(sessions).asKeyValueRange()) {
        if (record.generation == generation && record.sessionSerial == sessionSerial
            && record.lifecycle == SessionLifecycle::Active && record.session) {
            activeSession = session;
            return { true };
        }
    }
    return {};
}

bool ViewportProviderBridge::canAdmitSession()
{
    pruneDestroyedSessions();
    return sessions.size() < maximumRetainedProviderSessionCountPerRole;
}

ViewportProviderSessionOpenTransportResult ViewportProviderBridge::openSession(
    const ViewportProviderSessionOpenInput& input)
{
    if (!input.factory || !input.callbackTarget || !input.eventSink || input.generation == 0
        || input.sessionSerial == 0) {
        return {};
    }

    const auto logRetainedSessions = [this, &input](const char* event) {
        qsizetype activeCount = 0;
        qsizetype cleanupPendingCount = 0;
        qsizetype closingCount = 0;
        qsizetype destroyedCount = 0;
        for (const SessionRecord& record : std::as_const(sessions)) {
            if (!record.session) {
                ++destroyedCount;
            }
            switch (record.lifecycle) {
            case SessionLifecycle::Active:
                ++activeCount;
                break;
            case SessionLifecycle::CleanupPending:
                ++cleanupPendingCount;
                break;
            case SessionLifecycle::Closing:
                ++closingCount;
                break;
            }
        }
        qCDebug(imageViewportProviderLog)
            << event << "role" << static_cast<int>(role) << "generation" << input.generation
            << "sessionSerial" << input.sessionSerial << "retainedSessions" << sessions.size()
            << "active" << activeCount << "cleanupPending" << cleanupPendingCount << "closing"
            << closingCount << "destroyed" << destroyedCount;
    };

    logRetainedSessions("provider session open requested");
    pruneDestroyedSessions();
    logRetainedSessions("provider session open after prune");
    if (sessions.size() >= maximumRetainedProviderSessionCountPerRole) {
        qCDebug(imageViewportProviderLog)
            << "provider session admission deferred"
            << "role" << static_cast<int>(role) << "generation" << input.generation
            << "sessionSerial" << input.sessionSerial << "retainedSessions" << sessions.size()
            << "limit" << maximumRetainedProviderSessionCountPerRole;
        ViewportProviderSessionOpenTransportResult result;
        result.outcome = ViewportProviderSessionOpenTransportOutcome::Deferred;
        return result;
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
        ViewportProviderSessionOpenTransportResult result;
        result.providerFailureAvailable = admitted;
        result.providerCause
            = admitted ? failure.cause() : ImageSequenceProviderFailureCause::Unavailable;
        result.providerReference
            = admitted && handle ? handle->reference() : ImageSequenceProviderFailureReference {};
        result.providerFailureLeaseId = leaseId;
        return result;
    }
    if (!claimProviderSession(session)) {
        return {};
    }
    if (session->parent() && hasCurrentThreadAffinity(session)) {
        session->setParent(nullptr);
    }
    const auto sessionControl = std::make_shared<ViewportProviderSessionControl>(
        session, input.threadingContract, input.generation, input.sessionSerial);
    const auto eventEndpoint
        = std::make_shared<ViewportProviderEventEndpoint>(sessionControl, input.eventSink);
    QObject::connect(session, &QObject::destroyed, session,
        [session, sessionControl, cleanupRegistry = sessionCleanupRegistry, eventRole = role,
            generation = input.generation, sessionSerial = input.sessionSerial]() {
            qCDebug(imageViewportProviderLog)
                << "provider session destroyed"
                << "role" << static_cast<int>(eventRole) << "generation" << generation
                << "sessionSerial" << sessionSerial;
            sessionControl->markSessionDestroyed();
            cleanupRegistry->sessionDestroyed(session);
            releaseProviderSession(session);
        });
    activeSession = session;
    sessions.insert(session,
        { session, input.threadingContract, input.generation, input.sessionSerial,
            SessionLifecycle::Active, {}, {}, sessionControl, eventEndpoint,
            input.callbackTarget });
    qCDebug(imageViewportProviderLog)
        << "provider session opened"
        << "role" << static_cast<int>(role) << "generation" << input.generation << "sessionSerial"
        << input.sessionSerial << "retainedSessions" << sessions.size();
    eventEndpoints.append(eventEndpoint);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    const bool deliverSynchronously = synchronousEventDelivery;
#else
    constexpr bool deliverSynchronously = false;
#endif

    ImageViewportInternal::ProviderEventSubmissionPrivateAccess::install(*session,
        [sessionControl, eventEndpoint, leaseRegistry = leaseRegistry,
            callbackTarget = QPointer<QObject>(input.callbackTarget), eventRole = role,
            sessionSerial = input.sessionSerial, generation = input.generation,
            deliverSynchronously](const ImageSequenceProviderEvent& typedEvent) {
            if (!sessionControl->beginEventIngress()) {
                return ImageSequenceProviderEventSubmissionOutcome::Closed;
            }
            const auto ingressGuard
                = qScopeGuard([sessionControl]() { sessionControl->endEventIngress(); });
            ViewportProviderEvent event
                = viewportProviderEventFromTyped(eventRole, sessionSerial, generation, typedEvent);
            const bool advisory = event.kind == ImageSequenceProviderEventKind::Waiting
                || event.kind == ImageSequenceProviderEventKind::Progress;
            if (advisory) {
                const auto admission = eventEndpoint->enqueueAdvisory(std::move(event));
                if (!admission.scheduleDelivery) {
                    return admission.outcome;
                }
                auto deliverAdvisories = [eventEndpoint]() {
                    const auto events = eventEndpoint->takePendingAdvisories();
                    for (const auto& pendingEvent : events) {
                        eventEndpoint->deliver(pendingEvent);
                    }
                };
                if (deliverSynchronously) {
                    deliverAdvisories();
                } else if (!callbackTarget
                    || !QMetaObject::invokeMethod(callbackTarget.data(),
                        std::move(deliverAdvisories), Qt::QueuedConnection)) {
                    eventEndpoint->advisoryDeliverySchedulingFailed();
                }
                return admission.outcome;
            }

            const auto admission = eventEndpoint->admit(std::move(event),
                [sessionControl, leaseRegistry](ViewportProviderEvent& acceptedEvent) {
                    if (acceptedEvent.frameHandle) {
                        acceptedEvent.frameLeaseId
                            = leaseRegistry->claim(sessionControl, acceptedEvent.frameHandle);
                    }
                    if (acceptedEvent.failureHandle) {
                        acceptedEvent.failureLeaseId
                            = leaseRegistry->claim(sessionControl, acceptedEvent.failureHandle);
                    }
                });
            if (!admission.scheduleDelivery) {
                return admission.outcome;
            }
            auto deliverPending = [eventEndpoint, leaseRegistry]() {
                drainProviderEventDeliveries(eventEndpoint, leaseRegistry, true);
            };
            if (deliverSynchronously) {
                deliverPending();
            } else if (!callbackTarget
                || !QMetaObject::invokeMethod(
                    callbackTarget.data(), std::move(deliverPending), Qt::QueuedConnection)) {
                const auto undelivered = eventEndpoint->deliverySchedulingFailed();
                for (const auto& pendingEvent : undelivered) {
                    leaseRegistry->retire(pendingEvent.frameLeaseId);
                    leaseRegistry->retire(pendingEvent.failureLeaseId);
                    eventEndpoint->completeDelivery(pendingEvent.deliveryId);
                }
            }
            return admission.outcome;
        });

    ViewportProviderSessionOpenTransportResult result;
    result.outcome = ViewportProviderSessionOpenTransportOutcome::Opened;
    return result;
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
    const bool canDrainSynchronously
        = recordIt != sessions.cend() && hasCurrentThreadAffinity(recordIt->callbackTarget);
    if (eventEndpoint) {
        if (canDrainSynchronously) {
            drainProviderEventDeliveries(eventEndpoint, leaseRegistry, false);
        }
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

bool ViewportProviderBridge::pruneDestroyedSessions()
{
    bool pruned = false;
    const auto sessionKeys = sessions.keys();
    for (ImageSequenceProviderSession* session : sessionKeys) {
        sessions.detach();
        auto recordIt = sessions.find(session); // clazy:exclude=detaching-member
        if (recordIt == sessions.end() || recordIt->session) {
            continue;
        }
        if (activeSession == session) {
            activeSession.clear();
        }
        qCDebug(imageViewportProviderLog)
            << "provider session pruned"
            << "role" << static_cast<int>(role) << "generation" << recordIt->generation
            << "sessionSerial" << recordIt->sessionSerial << "retainedSessionsBefore"
            << sessions.size();
        sessions.erase(recordIt);
        pruned = true;
    }
    pruneExpiredEventEndpoints();
    return pruned;
}

void ViewportProviderBridge::completeFrameEventDelivery(quint64 leaseId)
{
    leaseRegistry->completeEventDelivery(leaseId);
}

void ViewportProviderBridge::completeFailureEventDelivery(quint64 leaseId)
{
    leaseRegistry->completeEventDelivery(leaseId);
}

void ViewportProviderBridge::completeProviderEventDelivery(quint64 deliveryId)
{
    if (deliveryId == 0) {
        return;
    }
    for (const auto& endpoint : std::as_const(eventEndpoints)) {
        if (const auto value = endpoint.lock(); value && value->completeDelivery(deliveryId)) {
            return;
        }
    }
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
    result.progress = pruneDestroyedSessions() || result.progress;
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
