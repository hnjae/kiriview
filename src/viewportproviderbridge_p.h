#pragma once

#include "imageviewport.h"

#include <memory>

class QObject;

class ViewportProviderBridgeClient
{
public:
    ViewportProviderBridgeClient() = default;
    ViewportProviderBridgeClient(const ViewportProviderBridgeClient&) = delete;
    ViewportProviderBridgeClient& operator=(const ViewportProviderBridgeClient&) = delete;
    virtual ~ViewportProviderBridgeClient() = default;

    virtual QObject* providerCallbackTarget() const = 0;
    virtual std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory() const = 0;
    virtual quint64 installProviderSession(ImageSequenceProviderSession* session) = 0;
    virtual ImageSequenceProviderSession* takeProviderSession() = 0;
    virtual ImageSequenceProviderSession* currentProviderSession() const = 0;
    virtual bool acceptsProviderSessionResult(quint64 sessionSerial) const = 0;
    virtual ImageSequenceProviderThreadingContract providerThreadingContract() const = 0;

    virtual void handleProviderMetadataReady(
        ImageSequenceProviderRequestToken token, const ImageSequenceProviderMetadata& metadata) = 0;
    virtual void handleProviderFrameReady(ImageSequenceProviderRequestToken token, ImageFrame* frame)
        = 0;
    virtual void handleProviderFrameReadyWithMetadata(ImageSequenceProviderRequestToken token,
        ImageFrame* frame, ImageSequenceProviderFrameMetadata metadata)
        = 0;
    virtual void handleProviderFrameReady(
        ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame)
        = 0;
    virtual void handleProviderFrameReadyWithMetadata(ImageSequenceProviderRequestToken token,
        ImageSequenceProviderFrameHandle* frame, ImageSequenceProviderFrameMetadata metadata)
        = 0;
    virtual void handleProviderWaiting(ImageSequenceProviderRequestToken token) = 0;
    virtual void handleProviderProgress(ImageSequenceProviderRequestToken token, double progress) = 0;
    virtual void handleProviderEndOfSequence(ImageSequenceProviderRequestToken token) = 0;
    virtual void handleProviderFailure(
        ImageSequenceProviderRequestToken token, const QString& diagnostic) = 0;
    virtual void handleProviderUnsupported(ImageSequenceProviderRequestToken token,
        ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic)
        = 0;
    virtual void handleProviderCancellation(
        ImageSequenceProviderRequestToken token, const QString& diagnostic) = 0;
};

class ViewportProviderBridge
{
public:
    explicit ViewportProviderBridge(ViewportProviderBridgeClient& client);

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
};
