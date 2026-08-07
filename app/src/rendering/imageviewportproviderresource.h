// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEVIEWPORTPROVIDERRESOURCE_H
#define KIRIVIEW_IMAGEVIEWPORTPROVIDERRESOURCE_H

#include "document/imageloadfailure.h"
#include "imageviewportfailureregistry.h"
#include "rendering/displayimagestore.h"
#include "rendering/staticimage.h"

#include <ImageViewport/imagesequenceprovider.h>

#include <QMutex>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace kiriview {
class ImageViewportProviderResource;
using ImageViewportProviderResourceFactory
    = std::function<std::shared_ptr<ImageViewportProviderResource>()>;

struct ImageViewportProviderWorkIdentity
{
    quint64 sourceGeneration = 0;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken requestToken;
    ImageViewportDemandRevisionToken demandRevision;
    QString locationIdentity;
};

bool operator==(
    const ImageViewportProviderWorkIdentity& left, const ImageViewportProviderWorkIdentity& right);

struct ImageViewportProviderFrameRequest
{
    int frame = -1;
    ImageSequenceProviderDisplayDemand demand;
    qint64 maximumStoreEntryBytes = -1;
};

enum class ImageViewportProviderFrameStage {
    Provisional,
    Authoritative,
};

struct ImageViewportProviderMetadataResult
{
    std::optional<ImageSequenceProviderMetadata> metadata;
    ImageSequenceProviderFailureCause failureCause = ImageSequenceProviderFailureCause::Unavailable;
    std::optional<ImageLoadFailure> failure;

    static ImageViewportProviderMetadataResult ready(ImageSequenceProviderMetadata metadata);
    static ImageViewportProviderMetadataResult failed(
        ImageSequenceProviderFailureCause cause, ImageLoadFailure failure);
};

struct ImageViewportProviderFrameResult
{
    std::optional<StaticDisplayImagePayload> displayImage;
    ImageSequenceProviderFrameEnvelope envelope;
    QString formatIdentifier;
    ImageSequenceProviderFailureCause failureCause = ImageSequenceProviderFailureCause::Unavailable;
    std::optional<ImageLoadFailure> failure;
    std::optional<ImageSequenceProviderUnsupportedCause> unsupportedCause;
    ImageViewportProviderFrameStage stage = ImageViewportProviderFrameStage::Authoritative;

    static ImageViewportProviderFrameResult ready(StaticDisplayImagePayload displayImage,
        ImageSequenceProviderFrameEnvelope envelope, QString formatIdentifier);
    static ImageViewportProviderFrameResult provisional(StaticDisplayImagePayload displayImage,
        ImageSequenceProviderFrameEnvelope envelope, QString formatIdentifier);
    static ImageViewportProviderFrameResult unsupported(
        ImageSequenceProviderUnsupportedCause cause);
    static ImageViewportProviderFrameResult failed(
        ImageSequenceProviderFailureCause cause, ImageLoadFailure failure);

    [[nodiscard]] bool isProvisional() const
    {
        return stage == ImageViewportProviderFrameStage::Provisional;
    }
};

class ImageViewportProviderSource
{
public:
    using MetadataCompletion = std::function<void(
        ImageViewportProviderWorkIdentity, ImageViewportProviderMetadataResult)>;
    using FrameCompletion
        = std::function<void(ImageViewportProviderWorkIdentity, ImageViewportProviderFrameResult)>;

    ImageViewportProviderSource() = default;
    virtual ~ImageViewportProviderSource() = default;

    [[nodiscard]] virtual ImageSequenceProviderMetadata constructionMetadata() const = 0;
    virtual void requestMetadata(
        const ImageViewportProviderWorkIdentity& identity, MetadataCompletion completion)
        = 0;
    virtual void requestFrame(const ImageViewportProviderWorkIdentity& identity,
        ImageViewportProviderFrameRequest request, FrameCompletion completion)
        = 0;
    virtual void cancel(const QVector<ImageSequenceProviderRequestToken>& tokens) = 0;
    virtual void close() = 0;

    Q_DISABLE_COPY_MOVE(ImageViewportProviderSource)
};

struct ImageViewportProviderPreparedFrame
{
    QString storeEntryId;
    ImageSequenceProviderFrameEnvelope envelope;
    QString formatIdentifier;
    ImageSequenceProviderFailureCause failureCause = ImageSequenceProviderFailureCause::Unavailable;
    std::optional<ImageLoadFailure> failure;
    std::optional<ImageSequenceProviderUnsupportedCause> unsupportedCause;
    ImageViewportProviderFrameStage stage = ImageViewportProviderFrameStage::Authoritative;
    std::optional<StaticDisplayImagePayload> authoritativeStillDisplayImage;

    [[nodiscard]] bool isReady() const { return !storeEntryId.isEmpty(); }
    [[nodiscard]] bool isUnsupported() const { return unsupportedCause.has_value(); }
    [[nodiscard]] bool isProvisional() const
    {
        return stage == ImageViewportProviderFrameStage::Provisional;
    }
};

class ImageViewportProviderResource final
    : public std::enable_shared_from_this<ImageViewportProviderResource>
{
public:
    using MetadataCompletion = ImageViewportProviderSource::MetadataCompletion;
    using FrameCompletion = std::function<void(
        ImageViewportProviderWorkIdentity, ImageViewportProviderPreparedFrame)>;

    ImageViewportProviderResource(quint64 sourceGeneration, QString locationIdentity,
        std::shared_ptr<ImageViewportProviderSource> source,
        std::shared_ptr<DisplayImageStore> displayStore,
        std::shared_ptr<ImageViewportFailureRegistry> failureRegistry = {});
    ~ImageViewportProviderResource();
    Q_DISABLE_COPY_MOVE(ImageViewportProviderResource)

    quint64 sourceGeneration() const { return m_sourceGeneration; }
    const QString& locationIdentity() const { return m_locationIdentity; }
    bool bindDisplayLocationIdentity(QString locationIdentity);
    ImageSequenceProviderMetadata constructionMetadata() const;
    std::shared_ptr<ImageViewportFailureRegistry> failureRegistry() const
    {
        return m_failureRegistry;
    }
    [[nodiscard]] std::weak_ptr<ImageViewportProviderSource> providerSource() const
    {
        return m_source;
    }

    void requestMetadata(
        const ImageViewportProviderWorkIdentity& identity, MetadataCompletion completion);
    void requestFrame(const ImageViewportProviderWorkIdentity& identity,
        ImageViewportProviderFrameRequest request, FrameCompletion completion);
    void cancel(const QVector<ImageSequenceProviderRequestToken>& tokens);
    void close();

    std::optional<StaticDisplayImagePayload> currentStillDisplayImage(
        ImageViewportDemandRevisionToken demandRevision) const;
    bool acceptAuthoritativeStillDisplayImage(const ImageViewportProviderWorkIdentity& identity,
        const ImageViewportProviderPreparedFrame& preparedFrame);
    bool acceptDisplayedStillDisplayImage(
        ImageViewportPageRole role, ImageViewportDemandRevisionToken demandRevision);
    ImageSequenceProviderFrameHandle* acquireFrameHandle(
        const ImageViewportProviderPreparedFrame& preparedFrame);
    ImageSequenceProviderFailure failure(
        ImageSequenceProviderFailureCause cause, std::optional<ImageLoadFailure> failure);

private:
    bool matchesResource(const ImageViewportProviderWorkIdentity& identity) const;
    bool finalizeMetadata(const ImageViewportProviderWorkIdentity& identity);
    bool finalizePreparedFrame(const ImageViewportProviderWorkIdentity& identity,
        const ImageViewportProviderPreparedFrame& preparedFrame);
    ImageViewportProviderPreparedFrame prepareFrame(
        const ImageViewportProviderWorkIdentity& identity, ImageViewportProviderFrameResult result);
    [[nodiscard]] QString displayLocationIdentityForPayloadPreparation();

    struct AuthoritativeStillDisplayImage
    {
        ImageViewportProviderWorkIdentity identity;
        QString storeEntryId;
        StaticDisplayImagePayload displayImage;
    };

    struct AuthoritativeFrameCandidate
    {
        ImageViewportProviderWorkIdentity identity;
        QString storeEntryId;
    };

    quint64 m_sourceGeneration = 0;
    QString m_locationIdentity;
    QString m_displayLocationIdentity;
    std::shared_ptr<ImageViewportProviderSource> m_source;
    std::shared_ptr<DisplayImageStore> m_displayStore;
    std::shared_ptr<ImageViewportFailureRegistry> m_failureRegistry;
    mutable QMutex m_stateMutex;
    std::vector<ImageViewportProviderWorkIdentity> m_activeMetadataWork;
    std::vector<ImageViewportProviderWorkIdentity> m_activeFrameWork;
    std::vector<AuthoritativeFrameCandidate> m_authoritativeFrameCandidates;
    std::optional<AuthoritativeStillDisplayImage> m_authoritativeStillDisplayImageCandidate;
    std::optional<AuthoritativeStillDisplayImage> m_currentStillDisplayImage;
    bool m_displayLocationIdentityBound = false;
    bool m_payloadPreparationStarted = false;
    bool m_closed = false;
};
}

#endif
