// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEPAGESURFACECONTROLLER_H
#define KIRIVIEW_IMAGEPAGESURFACECONTROLLER_H

#include "async/imageworkerscheduler.h"
#include "cache/imagecachepolicy.h"
#include "document/imagedocumenttypes.h"
#include "presentation/imageanimationplaybacksource.h"
#include "presentation/imagepresentationruntime.h"
#include "rendering/displayimagestore.h"
#include "rendering/imagerendercontext.h"
#include "rendering/staticimage.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {
class ImageAnimationPlayer;
class ImageDisplayEntryLeaseController;
class RasterDisplayRefinementCoordinator;

class ImagePageSurfaceController final
{
public:
    using ChangeCallback = std::function<void(ImageDocumentChange)>;
    using AnimationErrorCallback = std::function<void(const QString&)>;

    struct Callbacks
    {
        ChangeCallback change;
        AnimationErrorCallback animationError;
    };

    ImagePageSurfaceController(QObject* context, Callbacks callbacks,
        ImageCacheBudgets cacheBudgets, std::shared_ptr<DisplayImageStore> displayImageStore = {},
        DisplayedPageRole pageRole = DisplayedPageRole::Primary,
        ImageWorkerScheduler workerScheduler = {});
    ~ImagePageSurfaceController();

    QSize imageSize() const;
    quint64 imageRevision() const;
    bool hasImage() const;
    bool isPredecodeCacheable() const;
    qsizetype predecodeCacheByteBudget() const;
    std::optional<StaticDisplayImagePayload> displayImage() const;
    ImagePresentationPageSlotSnapshot snapshot() const;

    void setImage(const QImage& image, bool predecodeCacheable);
    void setAnimationFrame(const QImage& image, const QString& sourceIdentity);
    void setStaticDisplayImage(StaticDisplayImagePayload displayImage, bool predecodeCacheable,
        const ImageDocumentRenderContext& renderContext);
    QString publishShadowDisplayImage(StaticDisplayImagePayload displayImage);
    void clearShadowDisplayImage();
    void retainCurrentStaticDisplayImageForSameScopeNavigation();
    void clearSameScopeImageNavigationRetention();
    void updateDisplayProjection(const ImagePresentationRenderProjection& projection);
    void clearImage();

    void startAnimation(ImageAnimationPlaybackRequest request);
    void stopAnimation();
    bool acknowledgeDisplayImageLoad(const QUrl& providerUrl, quint64 revision,
        const QString& sourceIdentity, ImageDisplayLoadOutcome outcome);
    bool acknowledgeStillImageDisplayLoad(const QUrl& providerUrl, quint64 revision,
        const QString& sourceIdentity, ImageDisplayLoadOutcome outcome);
    bool acknowledgeAnimationFrameDisplayLoad(const QUrl& providerUrl, quint64 revision,
        const QString& sourceIdentity, ImageDisplayLoadOutcome outcome);

private:
    void acceptImageState(QSize imageSize, bool predecodeCacheable,
        std::optional<StaticDisplayImagePayload> displayImage);
    void publishDisplaySource(const StaticDisplayImagePayload& displayImage);
    void publishAnimationFrameDisplaySource(const QImage& image, const QString& sourceIdentity);
    void clearDisplaySource();
    void notify(ImageDocumentChange change);

    Callbacks m_callbacks;
    qsizetype m_predecodeCacheByteBudget = 0;
    DisplayedPageRole m_pageRole = DisplayedPageRole::Primary;
    QSize m_imageSize;
    quint64 m_imageRevision = 0;
    bool m_hasImage = false;
    bool m_predecodeCacheable = false;
    std::optional<StaticDisplayImagePayload> m_displayImage;
    QString m_animationFrameSourceIdentity;
    ImageDisplaySourceSlot m_displaySource;
    quint64 m_displaySourceRevision = 0;
    std::unique_ptr<ImageDisplayEntryLeaseController> m_displayEntryLeases;
    std::unique_ptr<RasterDisplayRefinementCoordinator> m_refinementCoordinator;
    std::unique_ptr<ImageAnimationPlayer> m_animationPlayer;
};
}

#endif
