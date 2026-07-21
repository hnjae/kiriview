// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportsequenceprovider.h"

#include <QPointer>
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace {
bool containsToken(const QVector<ImageSequenceProviderRequestToken>& tokens,
    ImageSequenceProviderRequestToken token)
{
    return std::find(tokens.cbegin(), tokens.cend(), token) != tokens.cend();
}

class ImageViewportProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit ImageViewportProviderSession(
        std::shared_ptr<kiriview::ImageViewportProviderResource> resource)
        : m_resource(std::move(resource))
    {
    }

    ~ImageViewportProviderSession() override { close(); }

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (m_closed || !request.isValid()) {
            return;
        }
        switch (request.kind()) {
        case ImageSequenceProviderRequestKind::Metadata:
            requestMetadata(request);
            return;
        case ImageSequenceProviderRequestKind::Frame:
        case ImageSequenceProviderRequestKind::Position:
        case ImageSequenceProviderRequestKind::Playback:
            requestFrame(request);
            return;
        case ImageSequenceProviderRequestKind::Cancel:
            cancel(request.tokens());
            return;
        case ImageSequenceProviderRequestKind::Close:
            close();
            return;
        }
    }

private:
    kiriview::ImageViewportProviderWorkIdentity identity(
        const ImageSequenceProviderRequest& request) const
    {
        return {
            m_resource->sourceGeneration(),
            request.role(),
            request.token(),
            request.demand().demandRevision(),
            m_resource->locationIdentity(),
        };
    }

    void requestMetadata(const ImageSequenceProviderRequest& request)
    {
        kiriview::ImageViewportProviderWorkIdentity work {
            m_resource->sourceGeneration(),
            request.role(),
            request.token(),
            {},
            m_resource->locationIdentity(),
        };
        m_metadataWork = work;
        const QPointer<ImageViewportProviderSession> guard(this);
        m_resource->requestMetadata(work,
            [guard](kiriview::ImageViewportProviderWorkIdentity completed,
                kiriview::ImageViewportProviderMetadataResult result) {
                if (guard) {
                    guard->completeMetadata(std::move(completed), std::move(result));
                }
            });
    }

    void requestFrame(const ImageSequenceProviderRequest& request)
    {
        const kiriview::ImageViewportProviderWorkIdentity work = identity(request);
        m_frameWork = work;
        const int resolvedFrame = request.kind() == ImageSequenceProviderRequestKind::Frame
            ? request.frame()
            : request.resolvedFrame();
        const kiriview::ImageViewportProviderFrameRequest providerRequest {
            resolvedFrame,
            request.demand(),
        };
        const QPointer<ImageViewportProviderSession> guard(this);
        m_resource->requestFrame(work, providerRequest,
            [guard](kiriview::ImageViewportProviderWorkIdentity completed,
                kiriview::ImageViewportProviderPreparedFrame result) {
                if (guard) {
                    guard->completeFrame(std::move(completed), std::move(result));
                }
            });
    }

    void completeMetadata(kiriview::ImageViewportProviderWorkIdentity identity,
        kiriview::ImageViewportProviderMetadataResult result)
    {
        if (m_closed || !m_metadataWork.has_value() || identity != *m_metadataWork) {
            return;
        }
        m_metadataWork.reset();
        if (result.metadata.has_value() && result.metadata->isValid()) {
            emit providerEvent(ImageSequenceProviderEvent::metadataReady(
                identity.requestToken, std::move(*result.metadata)));
            return;
        }
        const ImageSequenceProviderFailureCause cause
            = result.failureCause == ImageSequenceProviderFailureCause::Unavailable
            ? ImageSequenceProviderFailureCause::ProviderInternal
            : result.failureCause;
        emit providerEvent(ImageSequenceProviderEvent::failed(
            identity.requestToken, m_resource->failure(cause, std::move(result.failure))));
    }

    void completeFrame(kiriview::ImageViewportProviderWorkIdentity identity,
        kiriview::ImageViewportProviderPreparedFrame result)
    {
        if (m_closed || !m_frameWork.has_value() || identity != *m_frameWork) {
            return;
        }
        m_frameWork.reset();
        if (!result.isReady()) {
            const ImageSequenceProviderFailureCause cause
                = result.failureCause == ImageSequenceProviderFailureCause::Unavailable
                ? ImageSequenceProviderFailureCause::ProviderInternal
                : result.failureCause;
            emit providerEvent(ImageSequenceProviderEvent::failed(
                identity.requestToken, m_resource->failure(cause, std::move(result.failure))));
            return;
        }

        ImageSequenceProviderFrameHandle* handle = m_resource->acquireFrameHandle(result);
        if (handle == nullptr) {
            emit providerEvent(ImageSequenceProviderEvent::failed(identity.requestToken,
                m_resource->failure(
                    ImageSequenceProviderFailureCause::ResourceExhausted, std::nullopt)));
            return;
        }
        emit providerEvent(
            ImageSequenceProviderEvent::frameReady(identity.requestToken, handle, result.envelope));
    }

    void cancel(const QVector<ImageSequenceProviderRequestToken>& tokens)
    {
        m_resource->cancel(tokens);
        if (m_metadataWork.has_value() && containsToken(tokens, m_metadataWork->requestToken)) {
            const ImageSequenceProviderRequestToken token = m_metadataWork->requestToken;
            m_metadataWork.reset();
            emit providerEvent(ImageSequenceProviderEvent::cancelled(token));
        }
        if (m_frameWork.has_value() && containsToken(tokens, m_frameWork->requestToken)) {
            const ImageSequenceProviderRequestToken token = m_frameWork->requestToken;
            m_frameWork.reset();
            emit providerEvent(ImageSequenceProviderEvent::cancelled(token));
        }
    }

    void close()
    {
        if (m_closed) {
            return;
        }
        m_closed = true;
        m_metadataWork.reset();
        m_frameWork.reset();
        m_resource->close();
    }

    std::shared_ptr<kiriview::ImageViewportProviderResource> m_resource;
    std::optional<kiriview::ImageViewportProviderWorkIdentity> m_metadataWork;
    std::optional<kiriview::ImageViewportProviderWorkIdentity> m_frameWork;
    bool m_closed = false;
};
}

namespace kiriview {
ImageViewportSequenceProvider::ImageViewportSequenceProvider(
    std::shared_ptr<ImageViewportProviderResource> resource, QObject* parent)
    : ImageSequenceProviderAdapter(parent)
    , m_resource(std::move(resource))
{
}

ImageSequenceProviderDescriptor ImageViewportSequenceProvider::descriptor() const
{
    if (m_resource == nullptr) {
        return {};
    }
    const std::shared_ptr<ImageViewportProviderResource> resource = m_resource;
    return ImageSequenceProviderDescriptor(resource->constructionMetadata(),
        ImageSequenceProviderThreadingContract::AffinityBound, [resource]() {
            return ImageSequenceProviderSessionFactoryResult::created(
                new ImageViewportProviderSession(resource));
        });
}
}
