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
    PresentationShapeChange,
};

struct ImageViewportIntegrationTarget
{
    quint64 sourceGeneration = 0;
    QUrl selectedSourceUrl;
    QUrl resolvedPrimaryUrl;
    QUrl secondaryUrl;
    quint64 secondarySessionId = 0;
    ImageViewportTargetTransitionIntent transitionIntent
        = ImageViewportTargetTransitionIntent::OutsideNavigationScope;
    bool rightToLeft = false;
    bool anchorAtEnd = false;
    ImageViewportProviderResourceFactory primaryResource;
    ImageViewportProviderResourceFactory secondaryResource;

    [[nodiscard]] bool isValid() const;
};

struct ImageViewportIntegrationProjection
{
    bool correlated = false;
    bool completeAuthoritativeDisplayAvailable = false;
    bool loading = false;
    bool viewportFailureAvailable = false;
    bool providerFailureAvailable = false;
    bool secondaryVisible = false;
    bool horizontallyPannable = false;
    bool verticallyPannable = false;
    quint64 sourceGeneration = 0;
    QUrl secondaryUrl;
    quint64 secondarySessionId = 0;
    QUrl displayedUrl;
    ImageDocumentStatus status = ImageDocumentStatus::Null;
    ImageViewportFitMode fitMode = ImageViewportFitMode::Contain;
    QString errorString;
    QString diagnosticDetail;
    std::optional<ImageLoadFailure> failure;
    ImageViewportFailureContext viewportFailureContext = ImageViewportFailureContext::Unavailable;
    ImageViewportRequestReason viewportFailureReason = ImageViewportRequestReason::NoRequest;
    std::optional<ImageViewportPageRole> viewportFailureRole;
    ImageViewportFailureScope viewportFailureScope = ImageViewportFailureScope::Unavailable;
    ImageSequenceProviderFailureCause providerFailureCause
        = ImageSequenceProviderFailureCause::Unavailable;
    QSize primaryImageSize;
    QSize secondaryImageSize;
    qreal zoomPercent = 0.0;
    qreal preferredManualZoomPercent = 100.0;
    qreal minimumManualZoomPercent = 0.0;
    qreal maximumManualZoomPercent = 0.0;
    qreal manualZoomStepFactor = 1.0;
    QSizeF viewportSize;
    QRectF contentRect;
    QPointF contentPosition;
    QPointF maximumContentPosition;
    qreal horizontalScrollPosition = 0.0;
    qreal horizontalScrollPageSize = 1.0;
    qreal verticalScrollPosition = 0.0;
    qreal verticalScrollPageSize = 1.0;
    ImageViewportPresentationTargetGenerationToken displayedTargetGeneration;
};

class ImageViewportIntegrationRuntime final : public QObject
{
public:
    struct Callbacks
    {
        std::function<void(const ImageViewportIntegrationProjection&)> projectionChanged;
    };

    explicit ImageViewportIntegrationRuntime(Callbacks callbacks = {});
    ~ImageViewportIntegrationRuntime() override;
    Q_DISABLE_COPY_MOVE(ImageViewportIntegrationRuntime)

    void attach(ImageViewport* viewport);
    void detach(ImageViewport* viewport);

    bool submitTarget(ImageViewportIntegrationTarget target);
    bool resolvePrimaryTargetUrl(quint64 sourceGeneration, const QUrl& resolvedPrimaryUrl);
    void clearTarget();
    void stopPlayback();
    [[nodiscard]] bool hasAuthoritativeDisplay() const;
    const ImageViewportIntegrationProjection& projection() const;
    std::optional<StaticDisplayImagePayload> displayedImage(ImageViewportPageRole role) const;

    bool resetView();
    bool setFitMode(ImageViewportFitMode fitMode);
    bool setPreferredManualZoomPercent(qreal percent, std::optional<QPointF> anchor = std::nullopt);
    bool zoomBySteps(qreal steps, std::optional<QPointF> anchor = std::nullopt);
    bool panBy(QPointF delta);
    bool setContentPosition(QPointF position);
    bool resetImageTransforms();
    bool rotateByQuarterTurns(int delta);
    bool toggleMirrorHorizontally();
    bool toggleMirrorVertically();
    bool setSpreadDirection(ImageViewportSpreadDirection direction);
    bool submitHorizontalScrollPosition(qreal position);
    bool submitVerticalScrollPosition(qreal position);
    ImageViewportCoordinateResult mapPoint(ImageViewportCoordinateInput input) const;

private:
    struct TargetRecord;
    struct SubmissionStamp;
    enum class SubmissionOutcome;

    SubmissionOutcome submitCurrentTarget();
    [[nodiscard]] quint64 beginTargetRevision();
    [[nodiscard]] quint64 beginAttachmentRevision();
    [[nodiscard]] bool submissionIsCurrent(const SubmissionStamp& stamp) const;
    [[nodiscard]] bool containsRecord(const TargetRecord* record) const;
    void retireRecord(TargetRecord* record);
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
    quint64 m_targetRevision = 0;
    quint64 m_attachmentRevision = 0;
};
}

#endif
