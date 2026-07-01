#pragma once

#include "imageviewport.h"

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

class ViewportProviderBridge
{
public:
    explicit ViewportProviderBridge(ViewportProviderBridgeClient& client,
        ImageViewport::PageRole role = ImageViewport::PageRole::Primary);

    void closeSession(ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken);
    bool openSession();
    void requestMetadata(ImageSequenceProviderRequestToken token);
    void requestFrame(ImageSequenceProviderRequestToken token, int frame);
    void requestPosition(ImageSequenceProviderRequestToken token, int frame, int position);
    void requestPlayback(ImageSequenceProviderRequestToken token, int frame, int position);
    void cancelRequest(ImageSequenceProviderRequestToken token);

private:
    ImageSequenceProviderThreadingContract threadingContract() const;

    ViewportProviderBridgeClient& client;
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
};
