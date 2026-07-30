// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEOPENCONTROLLER_H
#define KIRIVIEW_IMAGEOPENCONTROLLER_H

#include "imagedocumentruntimeplan.h"
#include "imageloadfailure.h"
#include "imageloadtypes.h"
#include "metadata/embeddedmetadata.h"
#include "navigation/imagedocumentpagecandidatelistsource.h"
#include "predecode/predecodedimage.h"

#include <QSize>
#include <QString>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {
class ImageDocumentState;
class ImageLoader;
struct ImageOpenApplicationPlan;

class ImageOpenController final
{
public:
    using FindPredecodedImageCallback
        = std::function<std::optional<PredecodedImage>(const DisplayedImageLocation&)>;
    using RuntimePlanCallback = std::function<void(const ImageDocumentRuntimePlan&)>;
    using UnsupportedOpenedCollectionVideoEnteredCallback = std::function<void(const QString&)>;
    using OpenedCollectionVideoPlaybackAvailableCallback
        = std::function<bool(const OpenedCollectionScopeLocation&, const QUrl&)>;
    using CommitViewportPresentationCallback = std::function<bool(const ImageLoadSession&, QSize)>;
    using InvalidatePendingViewportImageLoadCallback = std::function<void()>;
    using EnsurePageCandidateSnapshotCallback = std::function<void(
        ImageDocumentPageCandidateListContext, ImageDocumentPageCandidateListSnapshotCallback)>;
    using StartViewportImageTargetCallback = std::function<bool(ImageLoadSession)>;
    using ResolveViewportImageTargetCallback
        = std::function<bool(ImageLoadSession, std::optional<PredecodedImage>)>;
    using FirstDisplayDecodeContextCallback = std::function<ImageFirstDisplayDecodeContext()>;
    using HasAuthoritativeDisplayCallback = std::function<bool()>;

    struct Callbacks
    {
        FindPredecodedImageCallback findPredecodedImage;
        RuntimePlanCallback runtimePlan;
        UnsupportedOpenedCollectionVideoEnteredCallback unsupportedOpenedCollectionVideoEntered;
        OpenedCollectionVideoPlaybackAvailableCallback openedCollectionVideoPlaybackAvailable;
        CommitViewportPresentationCallback commitViewportPresentation;
        InvalidatePendingViewportImageLoadCallback invalidatePendingViewportImageLoad;
        EnsurePageCandidateSnapshotCallback ensurePageCandidateSnapshot;
        StartViewportImageTargetCallback startViewportImageTarget;
        ResolveViewportImageTargetCallback resolveViewportImageTarget;
        FirstDisplayDecodeContextCallback firstDisplayDecodeContext;
        HasAuthoritativeDisplayCallback hasAuthoritativeDisplay;
    };

    ImageOpenController(ImageDocumentState& state, Callbacks callbacks);
    ~ImageOpenController();
    Q_DISABLE_COPY_MOVE(ImageOpenController)

    void open();
    void prepareSourceLoad(const ImageDocumentSourceLoadRequest& request);
    void cancel();
    void finishEmptySourceLoad();
    void finishViewportImageLoadReady(
        const ImageLoadSession& session, QSize imageSize, EmbeddedMetadata metadata);
    void finishViewportImageLoadWithError(
        const ImageLoadSession& session, ImageLoadFailure failure);
    void finishContainerNavigationWithEmptyContainer(const QUrl& containerUrl);
    void finishContainerNavigationLoadWithError(
        const QUrl& containerUrl, const QString& errorString);

private:
    [[nodiscard]] quint64 beginOperation();
    [[nodiscard]] bool operationIsCurrent(quint64 revision) const;
    void cancelActiveLoad();
    [[nodiscard]] bool applyAndReportIfCurrent(ImageOpenApplicationPlan plan, quint64 revision);
    [[nodiscard]] bool beginSourceLoad(bool sameScopePageNavigation, quint64 revision);
    void finishSourcePrepared(const ImageLoadSession& session);
    void finishUnsupportedOpenedCollectionVideoLoad(const ImageLoadSession& session);
    void finishPlayableOpenedCollectionVideoLoad(const ImageLoadSession& session);
    void finishStartedViewportImageLoad(const ImageLoadSession& session);
    void finishResolvedViewportImageLoad(
        const ImageLoadSession& session, std::optional<PredecodedImage> predecoded);
    void finishLoadWithError(const ImageLoadSession& session, ImageLoadFailure failure);
    void finishSuccessfulImageLoad(const ImageLoadSession& session, EmbeddedMetadata metadata);
    void reportRuntimePlan(const ImageDocumentRuntimePlan& plan);

    ImageDocumentState& m_state;
    Callbacks m_callbacks;
    std::unique_ptr<ImageLoader> m_imageLoader;
    std::optional<ImageDocumentSourceLoadRequest> m_sourceLoadRequest;
    quint64 m_operationRevision = 0;
};
}

#endif
