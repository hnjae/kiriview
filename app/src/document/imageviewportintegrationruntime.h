// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEVIEWPORTINTEGRATIONRUNTIME_H
#define KIRIVIEW_IMAGEVIEWPORTINTEGRATIONRUNTIME_H

#include "imagedocumenttypes.h"
#include "imageloadfailure.h"
#include "rendering/imageviewportproviderresource.h"

#include <ImageViewport/imageviewport.h>

#include <QObject>
#include <QPointer>
#include <QSize>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace kiriview {
enum class ImageViewportTargetTransitionIntent {
    SameNavigationScope,
    OutsideNavigationScope,
    RetainedDirectImage,
    PresentationShapeChange,
};

using ImageViewportProviderResourceFactory
    = std::function<std::shared_ptr<ImageViewportProviderResource>()>;

struct ImageViewportIntegrationTarget
{
    quint64 sourceGeneration = 0;
    QUrl primaryUrl;
    QUrl secondaryUrl;
    ImageViewportTargetTransitionIntent transitionIntent
        = ImageViewportTargetTransitionIntent::OutsideNavigationScope;
    bool rightToLeft = false;
    bool anchorAtEnd = false;
    std::optional<bool> priorTwoPageModeEnabled;
    ImageViewportProviderResourceFactory primaryResource;
    ImageViewportProviderResourceFactory secondaryResource;

    bool isValid() const;
};

struct ImageViewportIntegrationProjection
{
    bool correlated = false;
    quint64 sourceGeneration = 0;
    QUrl secondaryUrl;
    QUrl displayedUrl;
    ImageDocumentStatus status = ImageDocumentStatus::Null;
    bool loading = false;
    QString errorString;
    std::optional<ImageLoadFailure> failure;
    QSize primaryImageSize;
    QSize secondaryImageSize;
    bool secondaryVisible = false;
    ImageViewportFitMode fitMode = ImageViewportFitMode::Contain;
    qreal zoomPercent = 0.0;
    qreal preferredManualZoomPercent = 100.0;
    qreal minimumManualZoomPercent = 0.0;
    qreal maximumManualZoomPercent = 0.0;
    qreal manualZoomStepFactor = 1.0;
    int rotationDegrees = 0;
    bool horizontallyPannable = false;
    bool verticallyPannable = false;
    QSizeF viewportSize;
    QRectF contentRect;
    QPointF contentPosition;
    QPointF maximumContentPosition;
    qreal horizontalScrollPosition = 0.0;
    qreal horizontalScrollPageSize = 1.0;
    qreal verticalScrollPosition = 0.0;
    qreal verticalScrollPageSize = 1.0;
    bool restoredTransition = false;
    ImageViewportPresentationTargetGenerationToken displayedTargetGeneration;
};

class ImageViewportIntegrationRuntime final : public QObject
{
public:
    struct Callbacks
    {
        std::function<void(const ImageViewportIntegrationProjection&)> projectionChanged;
        std::function<void(bool)> restoreTwoPageModeEnabled;
    };

    explicit ImageViewportIntegrationRuntime(Callbacks callbacks = {});
    ~ImageViewportIntegrationRuntime() override;

    void attach(ImageViewport* viewport);
    void detach(ImageViewport* viewport);

    bool submitTarget(ImageViewportIntegrationTarget target);
    void clearTarget();
    const ImageViewportIntegrationProjection& projection() const;
    std::optional<StaticDisplayImagePayload> displayedImage(ImageViewportPageRole role) const;

    bool resetView();
    bool setFitMode(ImageViewportFitMode fitMode);
    bool setPreferredManualZoomPercent(qreal percent, std::optional<QPointF> anchor = std::nullopt);
    bool zoomBySteps(qreal steps, std::optional<QPointF> anchor = std::nullopt);
    bool panBy(QPointF delta);
    bool setContentPosition(QPointF position);
    bool setRotationDegrees(int degrees);
    bool setSpreadDirection(ImageViewportSpreadDirection direction);
    bool submitHorizontalScrollPosition(qreal position);
    bool submitVerticalScrollPosition(qreal position);
    ImageViewportCoordinateResult mapPoint(ImageViewportCoordinateInput input) const;

private:
    struct TargetRecord;

    bool submitCurrentTarget();
    void invalidateAttachment(ImageViewport* viewport);
    void handleStateChanged();
    void acceptSnapshot(const ImageViewportStateSnapshot& snapshot);
    TargetRecord* recordForGeneration(
        ImageViewportPresentationTargetGenerationToken generation) const;
    void pruneRecords(ImageViewportPresentationTargetGenerationToken acceptedGeneration,
        ImageViewportPresentationTargetGenerationToken displayedGeneration);
    std::optional<ImageLoadFailure> resolveFailure(
        const TargetRecord& record, const ImageViewportFailureSnapshot& failure) const;
    void publishProjection(ImageViewportIntegrationProjection projection);
    bool submitPresentation(ImageViewportPresentationCommand command);

    Callbacks m_callbacks;
    QPointer<ImageViewport> m_viewport;
    QMetaObject::Connection m_stateConnection;
    QMetaObject::Connection m_destroyedConnection;
    std::optional<ImageViewportIntegrationTarget> m_target;
    std::vector<std::unique_ptr<TargetRecord>> m_records;
    TargetRecord* m_activeRecord = nullptr;
    ImageViewportIntegrationProjection m_projection;
};
}

#endif
