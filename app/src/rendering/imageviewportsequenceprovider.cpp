// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportsequenceprovider.h"

#include <QPointer>
#include <QThread>
#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace {
bool containsToken(const QVector<ImageSequenceProviderRequestToken>& tokens,
    ImageSequenceProviderRequestToken token)
{
    return std::ranges::contains(tokens, token);
}

bool constructionMetadataMatches(
    const ImageSequenceProviderMetadata& expected, const ImageSequenceProviderMetadata& candidate)
{
    if (!expected.isSpecified() || !candidate.isSpecified()) {
        return true;
    }
    if (expected.isStill() != candidate.isStill()
        || expected.isTimedFrameList() != candidate.isTimedFrameList()
        || expected.sourceLogicalSize() != candidate.sourceLogicalSize()
        || expected.frameCount() != candidate.frameCount()
        || expected.frameDurations() != candidate.frameDurations()
        || expected.timedPlaybackSupport() != candidate.timedPlaybackSupport()
        || expected.frameSeekSupport() != candidate.frameSeekSupport()
        || expected.positionSeekSupport() != candidate.positionSeekSupport()
        || expected.autoplay() != candidate.autoplay()
        || expected.hasAuthoredAnimationFacts() != candidate.hasAuthoredAnimationFacts()) {
        return false;
    }
    if (!expected.hasAuthoredAnimationFacts()) {
        return true;
    }
    const ImageSequenceAuthoredAnimationFacts expectedFacts = expected.authoredAnimationFacts();
    const ImageSequenceAuthoredAnimationFacts candidateFacts = candidate.authoredAnimationFacts();
    return expectedFacts.autoplay() == candidateFacts.autoplay()
        && expectedFacts.loopMode() == candidateFacts.loopMode()
        && expectedFacts.loopCount() == candidateFacts.loopCount();
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
    Q_DISABLE_COPY_MOVE(ImageViewportProviderSession)

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
    struct ActiveFrameWork
    {
        kiriview::ImageViewportProviderWorkIdentity identity;
        bool provisionalEmitted = false;
    };

    [[nodiscard]] kiriview::ImageViewportProviderWorkIdentity identity(
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
            [guard](const kiriview::ImageViewportProviderWorkIdentity& completed,
                kiriview::ImageViewportProviderMetadataResult result) {
                if (guard) {
                    guard->completeMetadata(completed, std::move(result));
                }
            });
    }

    void requestFrame(const ImageSequenceProviderRequest& request)
    {
        const kiriview::ImageViewportProviderWorkIdentity work = identity(request);
        m_frameWork = ActiveFrameWork {
            work,
            false,
        };
        const int resolvedFrame = request.kind() == ImageSequenceProviderRequestKind::Frame
            ? request.frame()
            : request.resolvedFrame();
        const kiriview::ImageViewportProviderFrameRequest providerRequest {
            resolvedFrame,
            request.demand(),
        };
        const QPointer<ImageViewportProviderSession> guard(this);
        m_resource->requestFrame(work, providerRequest,
            [guard](const kiriview::ImageViewportProviderWorkIdentity& completed,
                kiriview::ImageViewportProviderPreparedFrame result) {
                if (guard) {
                    guard->completeFrame(completed, std::move(result));
                }
            });
    }

    void completeMetadata(const kiriview::ImageViewportProviderWorkIdentity& identity,
        kiriview::ImageViewportProviderMetadataResult result)
    {
        if (m_closed || !m_metadataWork.has_value() || identity != *m_metadataWork) {
            return;
        }
        m_metadataWork.reset();
        if (result.metadata.has_value() && result.metadata->isValid()) {
            Q_EMIT providerEvent(ImageSequenceProviderEvent::metadataReady(
                identity.requestToken, std::move(*result.metadata)));
            return;
        }
        const ImageSequenceProviderFailureCause cause
            = result.failureCause == ImageSequenceProviderFailureCause::Unavailable
            ? ImageSequenceProviderFailureCause::ProviderInternal
            : result.failureCause;
        Q_EMIT providerEvent(ImageSequenceProviderEvent::failed(
            identity.requestToken, m_resource->failure(cause, std::move(result.failure))));
    }

    void completeFrame(const kiriview::ImageViewportProviderWorkIdentity& identity,
        kiriview::ImageViewportProviderPreparedFrame result)
    {
        if (m_closed || !m_frameWork.has_value() || identity != m_frameWork->identity) {
            return;
        }
        if (result.isUnsupported()) {
            m_frameWork.reset();
            Q_EMIT providerEvent(ImageSequenceProviderEvent::unsupported(
                identity.requestToken, *result.unsupportedCause));
            return;
        }
        if (result.isProvisional()) {
            if (m_frameWork->provisionalEmitted || !result.isReady()) {
                return;
            }
            ImageSequenceProviderFrameHandle* handle = m_resource->acquireFrameHandle(result);
            if (handle == nullptr) {
                return;
            }
            m_frameWork->provisionalEmitted = true;
            Q_EMIT providerEvent(ImageSequenceProviderEvent::provisionalFrameReady(
                identity.requestToken, handle, result.envelope));
            return;
        }

        if (!result.isReady()) {
            m_frameWork.reset();
            const ImageSequenceProviderFailureCause cause
                = result.failureCause == ImageSequenceProviderFailureCause::Unavailable
                ? ImageSequenceProviderFailureCause::ProviderInternal
                : result.failureCause;
            Q_EMIT providerEvent(ImageSequenceProviderEvent::failed(
                identity.requestToken, m_resource->failure(cause, std::move(result.failure))));
            return;
        }

        ImageSequenceProviderFrameHandle* handle = m_resource->acquireFrameHandle(result);
        if (handle == nullptr) {
            m_frameWork.reset();
            Q_EMIT providerEvent(ImageSequenceProviderEvent::failed(identity.requestToken,
                m_resource->failure(
                    ImageSequenceProviderFailureCause::ResourceExhausted, std::nullopt)));
            return;
        }
        if (result.authoritativeStillDisplayImage.has_value()
            && !m_resource->acceptAuthoritativeStillDisplayImage(identity, result)) {
            delete handle;
            m_frameWork.reset();
            Q_EMIT providerEvent(ImageSequenceProviderEvent::failed(identity.requestToken,
                m_resource->failure(
                    ImageSequenceProviderFailureCause::ProviderInternal, std::nullopt)));
            return;
        }
        m_frameWork.reset();
        Q_EMIT providerEvent(
            ImageSequenceProviderEvent::frameReady(identity.requestToken, handle, result.envelope));
    }

    void cancel(const QVector<ImageSequenceProviderRequestToken>& tokens)
    {
        m_resource->cancel(tokens);
        if (m_metadataWork.has_value() && containsToken(tokens, m_metadataWork->requestToken)) {
            const ImageSequenceProviderRequestToken token = m_metadataWork->requestToken;
            m_metadataWork.reset();
            Q_EMIT providerEvent(ImageSequenceProviderEvent::cancelled(token));
        }
        if (m_frameWork.has_value() && containsToken(tokens, m_frameWork->identity.requestToken)) {
            const ImageSequenceProviderRequestToken token = m_frameWork->identity.requestToken;
            m_frameWork.reset();
            Q_EMIT providerEvent(ImageSequenceProviderEvent::cancelled(token));
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
    std::optional<ActiveFrameWork> m_frameWork;
    bool m_closed = false;
};
}

namespace kiriview {
class ImageViewportSequenceProviderPrivate final : public QObject
{
public:
    ImageViewportSequenceProviderPrivate(
        std::shared_ptr<ImageViewportProviderResource> initialResource,
        ImageViewportProviderResourceFactory resourceFactory)
        : m_constructionMetadata(initialResource == nullptr
                  ? ImageSequenceProviderMetadata {}
                  : initialResource->constructionMetadata())
        , m_sourceGeneration(initialResource == nullptr ? 0 : initialResource->sourceGeneration())
        , m_locationIdentity(
              initialResource == nullptr ? QString {} : initialResource->locationIdentity())
        , m_projectionResource(initialResource)
        , m_projectionFailureRegistry(
              initialResource == nullptr ? nullptr : initialResource->failureRegistry())
        , m_initialResource(std::move(initialResource))
        , m_resourceFactory(std::move(resourceFactory))
    {
    }

    ~ImageViewportSequenceProviderPrivate() override
    {
        if (m_initialResource != nullptr) {
            m_initialResource->close();
        }
    }
    Q_DISABLE_COPY_MOVE(ImageViewportSequenceProviderPrivate)

    [[nodiscard]] ImageSequenceProviderMetadata constructionMetadata() const
    {
        return m_constructionMetadata;
    }

    ImageSequenceProviderSessionFactoryResult createSession()
    {
        const std::optional<ImageSequenceProviderSessionFactoryResult> result
            = invokeOnAffinity<ImageSequenceProviderSessionFactoryResult>(
                [this]() { return createSessionOnAffinity(); });
        return result.value_or(ImageSequenceProviderSessionFactoryResult::failed(
            ImageSequenceProviderFailure(ImageSequenceProviderFailureCause::ProviderInternal)));
    }

    [[nodiscard]] std::optional<StaticDisplayImagePayload> currentStillDisplayImage(
        ImageViewportDemandRevisionToken demandRevision)
    {
        const auto result
            = invokeOnAffinity<std::optional<StaticDisplayImagePayload>>([this, demandRevision]() {
                  const std::shared_ptr<ImageViewportProviderResource> resource
                      = m_projectionResource.lock();
                  return resource == nullptr ? std::optional<StaticDisplayImagePayload> {}
                                             : resource->currentStillDisplayImage(demandRevision);
              });
        return result.value_or(std::optional<StaticDisplayImagePayload> {});
    }

    bool acceptDisplayedStillDisplayImage(
        ImageViewportPageRole role, ImageViewportDemandRevisionToken demandRevision)
    {
        const std::optional<bool> result = invokeOnAffinity<bool>([this, role, demandRevision]() {
            const std::shared_ptr<ImageViewportProviderResource> resource
                = m_projectionResource.lock();
            return resource != nullptr
                && resource->acceptDisplayedStillDisplayImage(role, demandRevision);
        });
        return result.value_or(false);
    }

    [[nodiscard]] std::optional<ImageLoadFailure> resolveFailure(
        ImageSequenceProviderFailureReference reference)
    {
        const auto result = invokeOnAffinity<std::optional<ImageLoadFailure>>([this, reference]() {
            return m_projectionFailureRegistry == nullptr
                ? std::optional<ImageLoadFailure> {}
                : m_projectionFailureRegistry->resolve(reference);
        });
        return result.value_or(std::optional<ImageLoadFailure> {});
    }

private:
    struct IssuedResource
    {
        std::weak_ptr<ImageViewportProviderResource> resource;
        std::weak_ptr<ImageViewportProviderSource> source;
    };

    template <typename Result, typename Function>
    [[nodiscard]] std::optional<Result> invokeOnAffinity(Function&& function)
    {
        if (QThread::currentThread() == thread()) {
            return std::invoke(std::forward<Function>(function));
        }
        std::optional<Result> result;
        const bool invoked = QMetaObject::invokeMethod(
            this,
            [&result, function = std::forward<Function>(function)]() mutable {
                result = std::invoke(std::move(function));
            },
            Qt::BlockingQueuedConnection);
        return invoked ? std::move(result) : std::nullopt;
    }

    ImageSequenceProviderSessionFactoryResult createSessionOnAffinity()
    {
        pruneResources();
        std::shared_ptr<ImageViewportProviderResource> resource
            = std::exchange(m_initialResource, {});
        if (resource == nullptr && m_resourceFactory) {
            resource = m_resourceFactory();
        }
        if (resource == nullptr) {
            return ImageSequenceProviderSessionFactoryResult::failed(
                ImageSequenceProviderFailure(ImageSequenceProviderFailureCause::ProviderInternal));
        }

        const std::shared_ptr<ImageViewportProviderSource> source
            = resource->providerSource().lock();
        const bool aliasesLiveMutableState = std::ranges::any_of(
            m_issuedResources, [&resource, &source](const auto& issued) {
                const std::shared_ptr<ImageViewportProviderResource> live = issued.resource.lock();
                return live == resource || (source != nullptr && issued.source.lock() == source);
            });
        if (source == nullptr || aliasesLiveMutableState
            || resource->sourceGeneration() != m_sourceGeneration
            || resource->locationIdentity() != m_locationIdentity
            || !constructionMetadataMatches(
                m_constructionMetadata, resource->constructionMetadata())) {
            return ImageSequenceProviderSessionFactoryResult::failed(
                ImageSequenceProviderFailure(ImageSequenceProviderFailureCause::ProviderInternal));
        }

        m_issuedResources.push_back({ resource, source });
        return ImageSequenceProviderSessionFactoryResult::created(
            new ImageViewportProviderSession(std::move(resource)));
    }

    void pruneResources()
    {
        std::erase_if(m_issuedResources, [](const IssuedResource& issued) {
            return issued.resource.expired() && issued.source.expired();
        });
    }

    ImageSequenceProviderMetadata m_constructionMetadata;
    quint64 m_sourceGeneration = 0;
    QString m_locationIdentity;
    std::weak_ptr<ImageViewportProviderResource> m_projectionResource;
    std::shared_ptr<ImageViewportFailureRegistry> m_projectionFailureRegistry;
    std::shared_ptr<ImageViewportProviderResource> m_initialResource;
    ImageViewportProviderResourceFactory m_resourceFactory;
    std::vector<IssuedResource> m_issuedResources;
};

namespace {
    std::shared_ptr<ImageViewportSequenceProviderPrivate> makeProviderPrivate(
        std::shared_ptr<ImageViewportProviderResource> initialResource,
        ImageViewportProviderResourceFactory resourceFactory)
    {
        return std::shared_ptr<ImageViewportSequenceProviderPrivate>(
            new ImageViewportSequenceProviderPrivate(
                std::move(initialResource), std::move(resourceFactory)),
            [](ImageViewportSequenceProviderPrivate* providerPrivate) {
                if (providerPrivate->thread() == QThread::currentThread()) {
                    delete providerPrivate;
                    return;
                }
                providerPrivate->deleteLater();
            });
    }
}

ImageViewportSequenceProvider::ImageViewportSequenceProvider(
    std::shared_ptr<ImageViewportProviderResource> initialResource,
    ImageViewportProviderResourceFactory resourceFactory, QObject* parent)
    : ImageSequenceProviderAdapter(parent)
    , m_private(makeProviderPrivate(std::move(initialResource), std::move(resourceFactory)))
{
}

ImageSequenceProviderDescriptor ImageViewportSequenceProvider::descriptor() const
{
    if (m_private == nullptr) {
        return {};
    }
    const std::shared_ptr<ImageViewportSequenceProviderPrivate> providerPrivate = m_private;
    return ImageSequenceProviderDescriptor(providerPrivate->constructionMetadata(),
        ImageSequenceProviderThreadingContract::AffinityBound,
        [providerPrivate]() { return providerPrivate->createSession(); });
}

std::optional<StaticDisplayImagePayload> ImageViewportSequenceProvider::currentStillDisplayImage(
    ImageViewportDemandRevisionToken demandRevision) const
{
    return m_private == nullptr ? std::nullopt
                                : m_private->currentStillDisplayImage(demandRevision);
}

bool ImageViewportSequenceProvider::acceptDisplayedStillDisplayImage(
    ImageViewportPageRole role, ImageViewportDemandRevisionToken demandRevision)
{
    return m_private != nullptr
        && m_private->acceptDisplayedStillDisplayImage(role, demandRevision);
}

std::optional<ImageLoadFailure> ImageViewportSequenceProvider::resolveFailure(
    ImageSequenceProviderFailureReference reference) const
{
    return m_private == nullptr ? std::nullopt : m_private->resolveFailure(reference);
}
}
