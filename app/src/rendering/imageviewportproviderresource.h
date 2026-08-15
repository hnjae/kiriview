// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEVIEWPORTPROVIDERRESOURCE_H
#define KIRIVIEW_IMAGEVIEWPORTPROVIDERRESOURCE_H

#include "async/imageworkerscheduler.h"
#include "decoding/imagedecodeworkspace.h"
#include "decoding/staticimage.h"
#include "document/imageloadfailure.h"
#include "imageviewportfailureregistry.h"
#include "rendering/displayimagestore.h"

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
    std::shared_ptr<DisplayImageStore> outputStore;
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
    ImageViewportProviderFrameResult() = default;
    ImageViewportProviderFrameResult(const ImageViewportProviderFrameResult&) = default;
    ImageViewportProviderFrameResult(ImageViewportProviderFrameResult&&) noexcept = default;
    ~ImageViewportProviderFrameResult() = default;
    ImageViewportProviderFrameResult& operator=(const ImageViewportProviderFrameResult& other);
    ImageViewportProviderFrameResult& operator=(ImageViewportProviderFrameResult&& other) noexcept;

    std::shared_ptr<DisplayImageOutputAdmission> outputAdmission;
    std::optional<StaticDisplayImagePayload> displayImage;
    ImageSequenceProviderFrameEnvelope envelope;
    QString formatIdentifier;
    ImageSequenceProviderFailureCause failureCause = ImageSequenceProviderFailureCause::Unavailable;
    std::optional<ImageLoadFailure> failure;
    std::optional<ImageSequenceProviderUnsupportedCause> unsupportedCause;
    ImageViewportProviderFrameStage stage = ImageViewportProviderFrameStage::Authoritative;

    static ImageViewportProviderFrameResult ready(StaticDisplayImagePayload displayImage,
        ImageSequenceProviderFrameEnvelope envelope, QString formatIdentifier,
        std::shared_ptr<DisplayImageOutputAdmission> outputAdmission = {});
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
    ImageViewportProviderPreparedFrame() = default;
    ImageViewportProviderPreparedFrame(const ImageViewportProviderPreparedFrame&) = default;
    ImageViewportProviderPreparedFrame(ImageViewportProviderPreparedFrame&&) noexcept = default;
    ~ImageViewportProviderPreparedFrame() = default;
    ImageViewportProviderPreparedFrame& operator=(const ImageViewportProviderPreparedFrame& other);
    ImageViewportProviderPreparedFrame& operator=(
        ImageViewportProviderPreparedFrame&& other) noexcept;

    QString storeEntryId;
    ImageSequenceProviderFrameEnvelope envelope;
    QString formatIdentifier;
    ImageSequenceProviderFailureCause failureCause = ImageSequenceProviderFailureCause::Unavailable;
    std::optional<ImageLoadFailure> failure;
    std::optional<ImageSequenceProviderUnsupportedCause> unsupportedCause;
    ImageViewportProviderFrameStage stage = ImageViewportProviderFrameStage::Authoritative;
    std::shared_ptr<DisplayImageOutputAdmission> outputAdmission;
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
    using FrameHandleCompletion = std::function<void(
        ImageViewportProviderWorkIdentity, std::unique_ptr<ImageSequenceProviderFrameHandle>)>;

    ImageViewportProviderResource(quint64 sourceGeneration, QString locationIdentity,
        std::shared_ptr<ImageViewportProviderSource> source,
        std::shared_ptr<DisplayImageStore> displayStore,
        std::shared_ptr<ImageViewportFailureRegistry> failureRegistry = {},
        std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget = {},
        ImageWorkerScheduler frameConstructionScheduler = {});
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
    void requestFrameHandle(QObject* receiver, const ImageViewportProviderWorkIdentity& identity,
        const ImageViewportProviderPreparedFrame& preparedFrame, FrameHandleCompletion completion);
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
    [[nodiscard]] bool frameConstructionIsActive(quint64 constructionId) const;
    [[nodiscard]] bool claimFrameConstruction(quint64 constructionId);
    [[nodiscard]] bool beginFrameConstructionWorker(quint64 constructionId);
    [[nodiscard]] bool completeFrameConstruction(quint64 constructionId);
    void cancelFrameConstruction(quint64 constructionId);
    void installFrameConstructionTask(quint64 constructionId, ImageWorkerTask task);
    void retireFrameConstruction(quint64 constructionId);

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

    struct ActiveFrameConstruction;

    quint64 m_sourceGeneration = 0;
    QString m_locationIdentity;
    QString m_displayLocationIdentity;
    std::shared_ptr<ImageViewportProviderSource> m_source;
    std::shared_ptr<DisplayImageStore> m_displayStore;
    std::shared_ptr<ImageViewportFailureRegistry> m_failureRegistry;
    std::shared_ptr<ImageDecodeWorkspaceBudget> m_workspaceBudget;
    ImageWorkerScheduler m_frameConstructionScheduler;
    mutable QMutex m_stateMutex;
    std::vector<ImageViewportProviderWorkIdentity> m_activeMetadataWork;
    std::vector<ImageViewportProviderWorkIdentity> m_activeFrameWork;
    std::vector<AuthoritativeFrameCandidate> m_authoritativeFrameCandidates;
    std::optional<AuthoritativeStillDisplayImage> m_authoritativeStillDisplayImageCandidate;
    std::optional<AuthoritativeStillDisplayImage> m_currentStillDisplayImage;
    std::vector<std::unique_ptr<ActiveFrameConstruction>> m_activeFrameConstructions;
    quint64 m_nextFrameConstructionId = 1;
    bool m_displayLocationIdentityBound = false;
    bool m_payloadPreparationStarted = false;
    bool m_closed = false;
};
}

#endif
