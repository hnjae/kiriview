#pragma once

#include "imageviewport.h"

#include <functional>
#include <memory>

class QObject;

struct ViewportProviderEvent
{
    enum class Kind {
        MetadataReady,
        ImageFrameReady,
        ImageFrameWithMetadataReady,
        FrameHandleReady,
        FrameHandleWithMetadataReady,
        Waiting,
        Progress,
        EndOfSequence,
        Failure,
        Unsupported,
        Cancellation
    };

    Kind kind = Kind::Waiting;
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    quint64 sessionSerial = 0;
    ImageSequenceProviderRequestToken token;
    ImageSequenceProviderMetadata metadata;
    ImageFrame* imageFrame = nullptr;
    ImageSequenceProviderFrameHandle* frameHandle = nullptr;
    ImageSequenceProviderFrameMetadata frameMetadata;
    double progress = 0.0;
    ImageSequenceProviderSession::UnsupportedCause unsupportedCause
        = ImageSequenceProviderSession::UnsupportedCause::PayloadRejection;
    bool unsupportedCauseExplicit = false;
    QString diagnostic;
};

class ViewportProviderBridgeClient
{
public:
    ViewportProviderBridgeClient() = default;
    ViewportProviderBridgeClient(const ViewportProviderBridgeClient&) = delete;
    ViewportProviderBridgeClient& operator=(const ViewportProviderBridgeClient&) = delete;
    virtual ~ViewportProviderBridgeClient() = default;

    virtual QObject* providerCallbackTarget() const = 0;
    virtual std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory(
        ImageViewport::PageRole role) const
        = 0;
    virtual quint64 installProviderSession(
        ImageViewport::PageRole role, ImageSequenceProviderSession* session)
        = 0;
    virtual ImageSequenceProviderSession* takeProviderSession(ImageViewport::PageRole role) = 0;
    virtual ImageSequenceProviderSession* currentProviderSession(ImageViewport::PageRole role) const
        = 0;
    virtual ImageSequenceProviderThreadingContract providerThreadingContract(
        ImageViewport::PageRole role) const
        = 0;
    virtual void handleProviderEvent(const ViewportProviderEvent& event) = 0;
};

class ViewportProviderExecutor
{
public:
    ViewportProviderExecutor() = default;
    ViewportProviderExecutor(const ViewportProviderExecutor&) = delete;
    ViewportProviderExecutor& operator=(const ViewportProviderExecutor&) = delete;
    virtual ~ViewportProviderExecutor() = default;

    virtual bool invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract, std::function<void()> command)
        = 0;
    virtual bool queueSessionCleanup(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken)
        = 0;
};

class ViewportProviderBridge
{
public:
    explicit ViewportProviderBridge(ViewportProviderBridgeClient& client,
        ImageViewport::PageRole role = ImageViewport::PageRole::Primary);

    bool closeSession(ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken);
    bool openSession();
    bool requestMetadata(ImageSequenceProviderRequestToken token);
    bool requestFrame(ImageSequenceProviderRequestToken token, int frame);
    bool requestPosition(ImageSequenceProviderRequestToken token, int frame, int position);
    bool requestPlayback(ImageSequenceProviderRequestToken token, int frame, int position);
    bool cancelRequest(ImageSequenceProviderRequestToken token);
    void setExecutor(ViewportProviderExecutor& executor);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void failNextCommandDeliveryForTest();
#endif

private:
    ImageSequenceProviderThreadingContract threadingContract() const;
    bool takeForcedDeliveryFailureForTest();
    ViewportProviderExecutor& executor() const;

    ViewportProviderBridgeClient& client;
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ViewportProviderExecutor* providerExecutor = nullptr;
    bool forceNextCommandDeliveryFailure = false;
};

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ViewportProviderExecutor& synchronousViewportProviderExecutorForTest();
#endif
