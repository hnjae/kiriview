// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEVIEWPORTDECODESOURCE_H
#define KIRIVIEW_IMAGEVIEWPORTDECODESOURCE_H

#include "async/imageasyncworker.h"
#include "decoding/decodedimageresult.h"
#include "decoding/imagedecodedependencies.h"
#include "decoding/imagedecodejob.h"
#include "document/imageloadtypes.h"
#include "imageviewportproviderresource.h"
#include "metadata/embeddedmetadata.h"
#include "presentation/imageanimationplaybacksource.h"

#include <QObject>
#include <QString>
#include <memory>
#include <optional>
#include <vector>

namespace kiriview {
class ImageViewportDecodeProviderSource final : public QObject, public ImageViewportProviderSource
{
public:
    ImageViewportDecodeProviderSource(
        ImageLoadSession session, ImageDecodeDependencies dependencies);
    ~ImageViewportDecodeProviderSource() override;

    [[nodiscard]] const EmbeddedMetadata& embeddedMetadata() const;
    [[nodiscard]] ImageSequenceProviderMetadata constructionMetadata() const override;
    void requestMetadata(
        const ImageViewportProviderWorkIdentity& identity, MetadataCompletion completion) override;
    void requestFrame(const ImageViewportProviderWorkIdentity& identity,
        ImageViewportProviderFrameRequest request, FrameCompletion completion) override;
    void cancel(const QVector<ImageSequenceProviderRequestToken>& tokens) override;
    void close() override;

private:
    struct PendingMetadata
    {
        ImageViewportProviderWorkIdentity identity;
        MetadataCompletion completion;
    };

    struct PendingFrame
    {
        ImageViewportProviderWorkIdentity identity;
        ImageViewportProviderFrameRequest request;
        FrameCompletion completion;
    };

    struct AnimationState
    {
        ImageAnimationPlaybackRequest playbackRequest;
        ImageSequenceProviderMetadata metadata;
        QString sourceIdentity;
        QString formatIdentifier;
    };

    void ensureDecoded();
    void finishDecode(ImageDecodeRequest request, DecodedImageResult result);
    void finishDataLoadError(const ImageDecodeRequest& request, const QString& errorString);
    void finishThumbnail(const ImageDecodeRequest& request, StaticDisplayImagePayload displayImage);
    void finishDecodedImage(DecodedImage image);
    void finishStaticImage(StaticDecodedImage image);
    void finishAnimationImage(ImageAnimationPlaybackRequest playbackRequest, QString sourceIdentity,
        QString formatIdentifier);
    void finishFailure(ImageSequenceProviderFailureCause cause, ImageLoadFailure failure);
    void publishMetadata();
    void publishFrames();
    void publishStaticFrame(PendingFrame pending);
    void publishAnimationFrame(PendingFrame pending);

    ImageLoadSession m_session;
    ImageDecodeDependencies m_dependencies;
    ImageDecodeJob m_decodeJob;
    EmbeddedMetadata m_embeddedMetadata;
    std::optional<ImageSequenceProviderMetadata> m_metadata;
    std::optional<StaticDisplayImagePayload> m_staticDisplayImage;
    std::optional<AnimationState> m_animation;
    std::optional<ImageLoadFailure> m_failure;
    ImageSequenceProviderFailureCause m_failureCause
        = ImageSequenceProviderFailureCause::Unavailable;
    std::vector<PendingMetadata> m_pendingMetadata;
    std::vector<PendingFrame> m_pendingFrames;
    std::vector<ImageWorkerTask> m_workerTasks;
    bool m_decodeStarted = false;
    bool m_decodeComplete = false;
    bool m_closed = false;
};
}

#endif
