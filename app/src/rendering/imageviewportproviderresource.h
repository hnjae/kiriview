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

namespace kiriview {
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

    static ImageViewportProviderFrameResult ready(StaticDisplayImagePayload displayImage,
        ImageSequenceProviderFrameEnvelope envelope, QString formatIdentifier);
    static ImageViewportProviderFrameResult failed(
        ImageSequenceProviderFailureCause cause, ImageLoadFailure failure);
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

    [[nodiscard]] bool isReady() const { return !storeEntryId.isEmpty(); }
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
        std::shared_ptr<ImageViewportFailureRegistry> failureRegistry = {},
        std::optional<StaticDisplayImagePayload> predecodedImage = std::nullopt);
    ~ImageViewportProviderResource();
    Q_DISABLE_COPY_MOVE(ImageViewportProviderResource)

    quint64 sourceGeneration() const { return m_sourceGeneration; }
    const QString& locationIdentity() const { return m_locationIdentity; }
    ImageSequenceProviderMetadata constructionMetadata() const;
    std::shared_ptr<ImageViewportFailureRegistry> failureRegistry() const
    {
        return m_failureRegistry;
    }

    void requestMetadata(
        const ImageViewportProviderWorkIdentity& identity, MetadataCompletion completion);
    void requestFrame(const ImageViewportProviderWorkIdentity& identity,
        ImageViewportProviderFrameRequest request, FrameCompletion completion);
    void cancel(const QVector<ImageSequenceProviderRequestToken>& tokens);
    void close();

    std::optional<StaticDisplayImagePayload> currentStillDisplayImage() const;
    ImageSequenceProviderFrameHandle* acquireFrameHandle(
        const ImageViewportProviderPreparedFrame& preparedFrame);
    ImageSequenceProviderFailure failure(
        ImageSequenceProviderFailureCause cause, std::optional<ImageLoadFailure> failure);

private:
    bool matchesResource(const ImageViewportProviderWorkIdentity& identity) const;
    ImageViewportProviderPreparedFrame prepareFrame(
        const ImageViewportProviderWorkIdentity& identity, ImageViewportProviderFrameResult result);

    quint64 m_sourceGeneration = 0;
    QString m_locationIdentity;
    std::shared_ptr<ImageViewportProviderSource> m_source;
    std::shared_ptr<DisplayImageStore> m_displayStore;
    std::shared_ptr<ImageViewportFailureRegistry> m_failureRegistry;
    std::optional<StaticDisplayImagePayload> m_predecodedImage;
    mutable QMutex m_currentPayloadMutex;
    std::optional<StaticDisplayImagePayload> m_currentStillDisplayImage;
};
}

#endif
