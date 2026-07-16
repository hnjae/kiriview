#pragma once

#include <ImageViewport/imagesequence.h>
#include <ImageViewport/imageviewporttypes.h>

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtGui/QColor>

#include <optional>

class ImageViewportRequestSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRequestSnapshot)
    Q_PROPERTY(ImageViewportRequestStatus status READ status CONSTANT)
    Q_PROPERTY(ImageViewportRequestReason reason READ reason CONSTANT)
    Q_PROPERTY(ImageViewportPlaybackPhase playbackPhase READ playbackPhase CONSTANT)
    Q_PROPERTY(ImageViewportPresentationTargetGenerationToken acceptedPresentationTargetGeneration
            READ acceptedPresentationTargetGeneration CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet acceptedRoleSet READ acceptedRoleSet CONSTANT)
    Q_PROPERTY(QVariant playbackRole READ playbackRole CONSTANT)

public:
    ImageViewportRequestSnapshot() = default;
    ImageViewportRequestSnapshot(ImageViewportRequestStatus status,
        ImageViewportRequestReason reason, ImageViewportPlaybackPhase playbackPhase,
        ImageViewportPresentationTargetGenerationToken acceptedPresentationTargetGeneration,
        ImageViewportRoleSet acceptedRoleSet, QVariant playbackRole)
        : m_status(status)
        , m_reason(reason)
        , m_playbackPhase(playbackPhase)
        , m_acceptedPresentationTargetGeneration(acceptedPresentationTargetGeneration)
        , m_acceptedRoleSet(acceptedRoleSet)
        , m_playbackRole(std::move(playbackRole))
    {
    }

    ImageViewportRequestStatus status() const { return m_status; }
    ImageViewportRequestReason reason() const { return m_reason; }
    ImageViewportPlaybackPhase playbackPhase() const { return m_playbackPhase; }
    ImageViewportPresentationTargetGenerationToken acceptedPresentationTargetGeneration() const
    {
        return m_acceptedPresentationTargetGeneration;
    }
    ImageViewportRoleSet acceptedRoleSet() const { return m_acceptedRoleSet; }
    QVariant playbackRole() const { return m_playbackRole; }

    friend bool operator==(
        const ImageViewportRequestSnapshot& lhs, const ImageViewportRequestSnapshot& rhs)
    {
        return lhs.m_status == rhs.m_status && lhs.m_reason == rhs.m_reason
            && lhs.m_playbackPhase == rhs.m_playbackPhase
            && lhs.m_acceptedPresentationTargetGeneration
            == rhs.m_acceptedPresentationTargetGeneration
            && lhs.m_acceptedRoleSet == rhs.m_acceptedRoleSet
            && lhs.m_playbackRole == rhs.m_playbackRole;
    }
    friend bool operator!=(
        const ImageViewportRequestSnapshot& lhs, const ImageViewportRequestSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportRequestStatus m_status = ImageViewportRequestStatus::NoRequest;
    ImageViewportRequestReason m_reason = ImageViewportRequestReason::NoRequest;
    ImageViewportPlaybackPhase m_playbackPhase = ImageViewportPlaybackPhase::Stopped;
    ImageViewportPresentationTargetGenerationToken m_acceptedPresentationTargetGeneration;
    ImageViewportRoleSet m_acceptedRoleSet;
    QVariant m_playbackRole;
};

class ImageViewportDisplaySnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportDisplaySnapshot)
    Q_PROPERTY(ImageViewportDisplayStatus status READ status CONSTANT)
    Q_PROPERTY(ImageViewportDisplayPhase phase READ phase CONSTANT)
    Q_PROPERTY(ImageViewportPresentationTargetGenerationToken displayedPresentationTargetGeneration
            READ displayedPresentationTargetGeneration CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet displayedRoleSet READ displayedRoleSet CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet targetRoleSet READ targetRoleSet CONSTANT)
    Q_PROPERTY(
        bool belongsToAcceptedPresentationTarget READ belongsToAcceptedPresentationTarget CONSTANT)
    Q_PROPERTY(bool retained READ retained CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken displayedPresentationRevision READ
            displayedPresentationRevision CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken targetPresentationRevision READ targetPresentationRevision
            CONSTANT)
    Q_PROPERTY(QSizeF spreadSize READ spreadSize CONSTANT)
    Q_PROPERTY(QRectF contentRect READ contentRect CONSTANT)
    Q_PROPERTY(QSizeF contentSize READ contentSize CONSTANT)
    Q_PROPERTY(QPointF contentPosition READ contentPosition CONSTANT)
    Q_PROPERTY(QPointF maximumContentPosition READ maximumContentPosition CONSTANT)
    Q_PROPERTY(QRectF visibleSpreadRect READ visibleSpreadRect CONSTANT)
    Q_PROPERTY(bool horizontalPannable READ horizontalPannable CONSTANT)
    Q_PROPERTY(bool verticalPannable READ verticalPannable CONSTANT)

public:
    ImageViewportDisplaySnapshot() = default;
    ImageViewportDisplaySnapshot(ImageViewportDisplayStatus status, ImageViewportDisplayPhase phase,
        ImageViewportPresentationTargetGenerationToken displayedPresentationTargetGeneration,
        ImageViewportRoleSet displayedRoleSet, ImageViewportRoleSet targetRoleSet,
        bool belongsToAcceptedPresentationTarget, bool retained,
        ImageViewportRevisionToken displayedPresentationRevision,
        ImageViewportRevisionToken targetPresentationRevision, QSizeF spreadSize,
        QRectF contentRect, QSizeF contentSize, QPointF contentPosition,
        QPointF maximumContentPosition, QRectF visibleSpreadRect, bool horizontalPannable,
        bool verticalPannable)
        : m_status(status)
        , m_phase(phase)
        , m_displayedPresentationTargetGeneration(displayedPresentationTargetGeneration)
        , m_displayedRoleSet(displayedRoleSet)
        , m_targetRoleSet(targetRoleSet)
        , m_belongsToAcceptedPresentationTarget(belongsToAcceptedPresentationTarget)
        , m_retained(retained)
        , m_displayedPresentationRevision(displayedPresentationRevision)
        , m_targetPresentationRevision(targetPresentationRevision)
        , m_spreadSize(spreadSize)
        , m_contentRect(contentRect)
        , m_contentSize(contentSize)
        , m_contentPosition(contentPosition)
        , m_maximumContentPosition(maximumContentPosition)
        , m_visibleSpreadRect(visibleSpreadRect)
        , m_horizontalPannable(horizontalPannable)
        , m_verticalPannable(verticalPannable)
    {
    }

    ImageViewportDisplayStatus status() const { return m_status; }
    ImageViewportDisplayPhase phase() const { return m_phase; }
    ImageViewportPresentationTargetGenerationToken displayedPresentationTargetGeneration() const
    {
        return m_displayedPresentationTargetGeneration;
    }
    ImageViewportRoleSet displayedRoleSet() const { return m_displayedRoleSet; }
    ImageViewportRoleSet targetRoleSet() const { return m_targetRoleSet; }
    bool belongsToAcceptedPresentationTarget() const
    {
        return m_belongsToAcceptedPresentationTarget;
    }
    bool retained() const { return m_retained; }
    ImageViewportRevisionToken displayedPresentationRevision() const
    {
        return m_displayedPresentationRevision;
    }
    ImageViewportRevisionToken targetPresentationRevision() const
    {
        return m_targetPresentationRevision;
    }
    QSizeF spreadSize() const { return m_spreadSize; }
    QRectF contentRect() const { return m_contentRect; }
    QSizeF contentSize() const { return m_contentSize; }
    QPointF contentPosition() const { return m_contentPosition; }
    QPointF maximumContentPosition() const { return m_maximumContentPosition; }
    QRectF visibleSpreadRect() const { return m_visibleSpreadRect; }
    bool horizontalPannable() const { return m_horizontalPannable; }
    bool verticalPannable() const { return m_verticalPannable; }

    friend bool operator==(
        const ImageViewportDisplaySnapshot& lhs, const ImageViewportDisplaySnapshot& rhs)
    {
        return lhs.m_status == rhs.m_status && lhs.m_phase == rhs.m_phase
            && lhs.m_displayedPresentationTargetGeneration
            == rhs.m_displayedPresentationTargetGeneration
            && lhs.m_displayedRoleSet == rhs.m_displayedRoleSet
            && lhs.m_targetRoleSet == rhs.m_targetRoleSet
            && lhs.m_belongsToAcceptedPresentationTarget
            == rhs.m_belongsToAcceptedPresentationTarget
            && lhs.m_retained == rhs.m_retained
            && lhs.m_displayedPresentationRevision == rhs.m_displayedPresentationRevision
            && lhs.m_targetPresentationRevision == rhs.m_targetPresentationRevision
            && lhs.m_spreadSize == rhs.m_spreadSize && lhs.m_contentRect == rhs.m_contentRect
            && lhs.m_contentSize == rhs.m_contentSize
            && lhs.m_contentPosition == rhs.m_contentPosition
            && lhs.m_maximumContentPosition == rhs.m_maximumContentPosition
            && lhs.m_visibleSpreadRect == rhs.m_visibleSpreadRect
            && lhs.m_horizontalPannable == rhs.m_horizontalPannable
            && lhs.m_verticalPannable == rhs.m_verticalPannable;
    }
    friend bool operator!=(
        const ImageViewportDisplaySnapshot& lhs, const ImageViewportDisplaySnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportDisplayStatus m_status = ImageViewportDisplayStatus::Empty;
    ImageViewportDisplayPhase m_phase = ImageViewportDisplayPhase::NoPresentation;
    ImageViewportPresentationTargetGenerationToken m_displayedPresentationTargetGeneration;
    ImageViewportRoleSet m_displayedRoleSet;
    ImageViewportRoleSet m_targetRoleSet;
    bool m_belongsToAcceptedPresentationTarget = false;
    bool m_retained = false;
    ImageViewportRevisionToken m_displayedPresentationRevision;
    ImageViewportRevisionToken m_targetPresentationRevision;
    QSizeF m_spreadSize;
    QRectF m_contentRect;
    QSizeF m_contentSize;
    QPointF m_contentPosition;
    QPointF m_maximumContentPosition;
    QRectF m_visibleSpreadRect;
    bool m_horizontalPannable = false;
    bool m_verticalPannable = false;
};

class ImageViewportPresentationSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportPresentationSnapshot)
    Q_PROPERTY(ImageViewportFitMode fitMode READ fitMode CONSTANT)
    Q_PROPERTY(double zoomPercent READ zoomPercent CONSTANT)
    Q_PROPERTY(double manualZoomPercent READ manualZoomPercent CONSTANT)
    Q_PROPERTY(double minimumManualZoomPercent READ minimumManualZoomPercent CONSTANT)
    Q_PROPERTY(double maximumManualZoomPercent READ maximumManualZoomPercent CONSTANT)
    Q_PROPERTY(double manualZoomStepFactor READ manualZoomStepFactor CONSTANT)
    Q_PROPERTY(int rotationDegrees READ rotationDegrees CONSTANT)
    Q_PROPERTY(bool mirrorHorizontally READ mirrorHorizontally CONSTANT)
    Q_PROPERTY(bool mirrorVertically READ mirrorVertically CONSTANT)
    Q_PROPERTY(ImageViewportSpreadDirection spreadDirection READ spreadDirection CONSTANT)
    Q_PROPERTY(double pageGap READ pageGap CONSTANT)
    Q_PROPERTY(ImageViewportBackgroundMode backgroundMode READ backgroundMode CONSTANT)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor CONSTANT)
    Q_PROPERTY(QColor checkerboardLightColor READ checkerboardLightColor CONSTANT)
    Q_PROPERTY(QColor checkerboardDarkColor READ checkerboardDarkColor CONSTANT)
    Q_PROPERTY(double checkerboardCellSize READ checkerboardCellSize CONSTANT)
    Q_PROPERTY(bool smoothing READ smoothing CONSTANT)
    Q_PROPERTY(bool mipmap READ mipmap CONSTANT)
    Q_PROPERTY(bool looping READ looping CONSTANT)
    Q_PROPERTY(ImageViewportQualityPreference qualityPreference READ qualityPreference CONSTANT)
    Q_PROPERTY(
        ImageViewportExactnessPreference exactnessPreference READ exactnessPreference CONSTANT)

public:
    ImageViewportPresentationSnapshot() = default;
    ImageViewportPresentationSnapshot(ImageViewportFitMode fitMode, double zoomPercent,
        double manualZoomPercent, double minimumManualZoomPercent, double maximumManualZoomPercent,
        double manualZoomStepFactor, int rotationDegrees, bool mirrorHorizontally,
        bool mirrorVertically, ImageViewportSpreadDirection spreadDirection, double pageGap,
        ImageViewportBackgroundMode backgroundMode, QColor backgroundColor,
        QColor checkerboardLightColor, QColor checkerboardDarkColor, double checkerboardCellSize,
        bool smoothing, bool mipmap, bool looping, ImageViewportQualityPreference qualityPreference,
        ImageViewportExactnessPreference exactnessPreference)
        : m_fitMode(fitMode)
        , m_zoomPercent(zoomPercent)
        , m_manualZoomPercent(manualZoomPercent)
        , m_minimumManualZoomPercent(minimumManualZoomPercent)
        , m_maximumManualZoomPercent(maximumManualZoomPercent)
        , m_manualZoomStepFactor(manualZoomStepFactor)
        , m_rotationDegrees(rotationDegrees)
        , m_mirrorHorizontally(mirrorHorizontally)
        , m_mirrorVertically(mirrorVertically)
        , m_spreadDirection(spreadDirection)
        , m_pageGap(pageGap)
        , m_backgroundMode(backgroundMode)
        , m_backgroundColor(backgroundColor)
        , m_checkerboardLightColor(checkerboardLightColor)
        , m_checkerboardDarkColor(checkerboardDarkColor)
        , m_checkerboardCellSize(checkerboardCellSize)
        , m_smoothing(smoothing)
        , m_mipmap(mipmap)
        , m_looping(looping)
        , m_qualityPreference(qualityPreference)
        , m_exactnessPreference(exactnessPreference)
    {
    }

    ImageViewportFitMode fitMode() const { return m_fitMode; }
    double zoomPercent() const { return m_zoomPercent; }
    double manualZoomPercent() const { return m_manualZoomPercent; }
    double minimumManualZoomPercent() const { return m_minimumManualZoomPercent; }
    double maximumManualZoomPercent() const { return m_maximumManualZoomPercent; }
    double manualZoomStepFactor() const { return m_manualZoomStepFactor; }
    int rotationDegrees() const { return m_rotationDegrees; }
    bool mirrorHorizontally() const { return m_mirrorHorizontally; }
    bool mirrorVertically() const { return m_mirrorVertically; }
    ImageViewportSpreadDirection spreadDirection() const { return m_spreadDirection; }
    double pageGap() const { return m_pageGap; }
    ImageViewportBackgroundMode backgroundMode() const { return m_backgroundMode; }
    QColor backgroundColor() const { return m_backgroundColor; }
    QColor checkerboardLightColor() const { return m_checkerboardLightColor; }
    QColor checkerboardDarkColor() const { return m_checkerboardDarkColor; }
    double checkerboardCellSize() const { return m_checkerboardCellSize; }
    bool smoothing() const { return m_smoothing; }
    bool mipmap() const { return m_mipmap; }
    bool looping() const { return m_looping; }
    ImageViewportQualityPreference qualityPreference() const { return m_qualityPreference; }
    ImageViewportExactnessPreference exactnessPreference() const { return m_exactnessPreference; }

    friend bool operator==(
        const ImageViewportPresentationSnapshot& lhs, const ImageViewportPresentationSnapshot& rhs)
    {
        return lhs.m_fitMode == rhs.m_fitMode && lhs.m_zoomPercent == rhs.m_zoomPercent
            && lhs.m_manualZoomPercent == rhs.m_manualZoomPercent
            && lhs.m_minimumManualZoomPercent == rhs.m_minimumManualZoomPercent
            && lhs.m_maximumManualZoomPercent == rhs.m_maximumManualZoomPercent
            && lhs.m_manualZoomStepFactor == rhs.m_manualZoomStepFactor
            && lhs.m_rotationDegrees == rhs.m_rotationDegrees
            && lhs.m_mirrorHorizontally == rhs.m_mirrorHorizontally
            && lhs.m_mirrorVertically == rhs.m_mirrorVertically
            && lhs.m_spreadDirection == rhs.m_spreadDirection && lhs.m_pageGap == rhs.m_pageGap
            && lhs.m_backgroundMode == rhs.m_backgroundMode
            && lhs.m_backgroundColor == rhs.m_backgroundColor
            && lhs.m_checkerboardLightColor == rhs.m_checkerboardLightColor
            && lhs.m_checkerboardDarkColor == rhs.m_checkerboardDarkColor
            && lhs.m_checkerboardCellSize == rhs.m_checkerboardCellSize
            && lhs.m_smoothing == rhs.m_smoothing && lhs.m_mipmap == rhs.m_mipmap
            && lhs.m_looping == rhs.m_looping && lhs.m_qualityPreference == rhs.m_qualityPreference
            && lhs.m_exactnessPreference == rhs.m_exactnessPreference;
    }
    friend bool operator!=(
        const ImageViewportPresentationSnapshot& lhs, const ImageViewportPresentationSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportFitMode m_fitMode = ImageViewportFitMode::Contain;
    double m_zoomPercent = 0.0;
    double m_manualZoomPercent = 100.0;
    double m_minimumManualZoomPercent = 0.0;
    double m_maximumManualZoomPercent = 0.0;
    double m_manualZoomStepFactor = 1.0;
    int m_rotationDegrees = 0;
    bool m_mirrorHorizontally = false;
    bool m_mirrorVertically = false;
    ImageViewportSpreadDirection m_spreadDirection = ImageViewportSpreadDirection::LeftToRight;
    double m_pageGap = 0.0;
    ImageViewportBackgroundMode m_backgroundMode = ImageViewportBackgroundMode::Transparent;
    QColor m_backgroundColor = Qt::white;
    QColor m_checkerboardLightColor = Qt::white;
    QColor m_checkerboardDarkColor = QColor(220, 220, 220);
    double m_checkerboardCellSize = 8.0;
    bool m_smoothing = true;
    bool m_mipmap = false;
    bool m_looping = false;
    ImageViewportQualityPreference m_qualityPreference = ImageViewportQualityPreference::Default;
    ImageViewportExactnessPreference m_exactnessPreference
        = ImageViewportExactnessPreference::Default;
};

class ImageViewportRoleRequestSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleRequestSnapshot)
    Q_PROPERTY(
        bool belongsToAcceptedPresentationTarget READ belongsToAcceptedPresentationTarget CONSTANT)
    Q_PROPERTY(ImageViewportPresentationTargetGenerationToken presentationTargetGeneration READ
            presentationTargetGeneration CONSTANT)
    Q_PROPERTY(ImageViewportPageRole role READ role CONSTANT)
    Q_PROPERTY(int frame READ frame CONSTANT)
    Q_PROPERTY(int position READ position CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(ImageViewportDemandRevisionToken demandRevision READ demandRevision CONSTANT)

public:
    ImageViewportRoleRequestSnapshot() = default;
    ImageViewportRoleRequestSnapshot(bool belongsToAcceptedPresentationTarget,
        ImageViewportPresentationTargetGenerationToken presentationTargetGeneration,
        ImageViewportPageRole role, int frame, int position, QSizeF sourceLogicalSize,
        ImageViewportDemandRevisionToken demandRevision)
        : m_belongsToAcceptedPresentationTarget(belongsToAcceptedPresentationTarget)
        , m_presentationTargetGeneration(presentationTargetGeneration)
        , m_role(role)
        , m_frame(frame)
        , m_position(position)
        , m_sourceLogicalSize(sourceLogicalSize)
        , m_demandRevision(demandRevision)
    {
    }

    bool belongsToAcceptedPresentationTarget() const
    {
        return m_belongsToAcceptedPresentationTarget;
    }
    ImageViewportPresentationTargetGenerationToken presentationTargetGeneration() const
    {
        return m_presentationTargetGeneration;
    }
    ImageViewportPageRole role() const { return m_role; }
    int frame() const { return m_frame; }
    int position() const { return m_position; }
    QSizeF sourceLogicalSize() const { return m_sourceLogicalSize; }
    ImageViewportDemandRevisionToken demandRevision() const { return m_demandRevision; }

    friend bool operator==(
        const ImageViewportRoleRequestSnapshot& lhs, const ImageViewportRoleRequestSnapshot& rhs)
    {
        return lhs.m_belongsToAcceptedPresentationTarget
            == rhs.m_belongsToAcceptedPresentationTarget
            && lhs.m_presentationTargetGeneration == rhs.m_presentationTargetGeneration
            && lhs.m_role == rhs.m_role && lhs.m_frame == rhs.m_frame
            && lhs.m_position == rhs.m_position
            && lhs.m_sourceLogicalSize == rhs.m_sourceLogicalSize
            && lhs.m_demandRevision == rhs.m_demandRevision;
    }
    friend bool operator!=(
        const ImageViewportRoleRequestSnapshot& lhs, const ImageViewportRoleRequestSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_belongsToAcceptedPresentationTarget = false;
    ImageViewportPresentationTargetGenerationToken m_presentationTargetGeneration;
    ImageViewportPageRole m_role = ImageViewportPageRole::Primary;
    int m_frame = -1;
    int m_position = -1;
    QSizeF m_sourceLogicalSize;
    ImageViewportDemandRevisionToken m_demandRevision;
};

class ImageViewportRoleDisplaySnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleDisplaySnapshot)
    Q_PROPERTY(
        bool belongsToAcceptedPresentationTarget READ belongsToAcceptedPresentationTarget CONSTANT)
    Q_PROPERTY(bool retained READ retained CONSTANT)
    Q_PROPERTY(int frame READ frame CONSTANT)
    Q_PROPERTY(int position READ position CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(QSizeF payloadRasterSize READ payloadRasterSize CONSTANT)
    Q_PROPERTY(QSizeF sourceToPayloadScale READ sourceToPayloadScale CONSTANT)
    Q_PROPERTY(ImageViewportPayloadQuality quality READ quality CONSTANT)
    Q_PROPERTY(ImageViewportPayloadExactness exactness READ exactness CONSTANT)
    Q_PROPERTY(bool currentForDemand READ currentForDemand CONSTANT)
    Q_PROPERTY(ImageViewportDemandRevisionToken demandRevision READ demandRevision CONSTANT)

public:
    ImageViewportRoleDisplaySnapshot() = default;
    ImageViewportRoleDisplaySnapshot(bool belongsToAcceptedPresentationTarget, bool retained,
        int frame, int position, QSizeF sourceLogicalSize, QSizeF payloadRasterSize,
        QSizeF sourceToPayloadScale, ImageViewportPayloadQuality quality,
        ImageViewportPayloadExactness exactness, bool currentForDemand,
        ImageViewportDemandRevisionToken demandRevision)
        : m_belongsToAcceptedPresentationTarget(belongsToAcceptedPresentationTarget)
        , m_retained(retained)
        , m_frame(frame)
        , m_position(position)
        , m_sourceLogicalSize(sourceLogicalSize)
        , m_payloadRasterSize(payloadRasterSize)
        , m_sourceToPayloadScale(sourceToPayloadScale)
        , m_quality(quality)
        , m_exactness(exactness)
        , m_currentForDemand(currentForDemand)
        , m_demandRevision(demandRevision)
    {
    }

    bool belongsToAcceptedPresentationTarget() const
    {
        return m_belongsToAcceptedPresentationTarget;
    }
    bool retained() const { return m_retained; }
    int frame() const { return m_frame; }
    int position() const { return m_position; }
    QSizeF sourceLogicalSize() const { return m_sourceLogicalSize; }
    QSizeF payloadRasterSize() const { return m_payloadRasterSize; }
    QSizeF sourceToPayloadScale() const { return m_sourceToPayloadScale; }
    ImageViewportPayloadQuality quality() const { return m_quality; }
    ImageViewportPayloadExactness exactness() const { return m_exactness; }
    bool currentForDemand() const { return m_currentForDemand; }
    ImageViewportDemandRevisionToken demandRevision() const { return m_demandRevision; }

    friend bool operator==(
        const ImageViewportRoleDisplaySnapshot& lhs, const ImageViewportRoleDisplaySnapshot& rhs)
    {
        return lhs.m_belongsToAcceptedPresentationTarget
            == rhs.m_belongsToAcceptedPresentationTarget
            && lhs.m_retained == rhs.m_retained && lhs.m_frame == rhs.m_frame
            && lhs.m_position == rhs.m_position
            && lhs.m_sourceLogicalSize == rhs.m_sourceLogicalSize
            && lhs.m_payloadRasterSize == rhs.m_payloadRasterSize
            && lhs.m_sourceToPayloadScale == rhs.m_sourceToPayloadScale
            && lhs.m_quality == rhs.m_quality && lhs.m_exactness == rhs.m_exactness
            && lhs.m_currentForDemand == rhs.m_currentForDemand
            && lhs.m_demandRevision == rhs.m_demandRevision;
    }
    friend bool operator!=(
        const ImageViewportRoleDisplaySnapshot& lhs, const ImageViewportRoleDisplaySnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_belongsToAcceptedPresentationTarget = false;
    bool m_retained = false;
    int m_frame = -1;
    int m_position = -1;
    QSizeF m_sourceLogicalSize;
    QSizeF m_payloadRasterSize;
    QSizeF m_sourceToPayloadScale;
    ImageViewportPayloadQuality m_quality = ImageViewportPayloadQuality::Unknown;
    ImageViewportPayloadExactness m_exactness = ImageViewportPayloadExactness::Unknown;
    bool m_currentForDemand = false;
    ImageViewportDemandRevisionToken m_demandRevision;
};

class ImageViewportRoleMetadataSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleMetadataSnapshot)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(int frameCount READ frameCount CONSTANT)
    Q_PROPERTY(int totalDuration READ totalDuration CONSTANT)
    Q_PROPERTY(ImageViewportRange frameSeekBounds READ frameSeekBounds CONSTANT)
    Q_PROPERTY(ImageViewportRange positionSeekBounds READ positionSeekBounds CONSTANT)
    Q_PROPERTY(ImageViewportCapabilitySupport frameSeekSupport READ frameSeekSupport CONSTANT)
    Q_PROPERTY(ImageViewportCapabilitySupport positionSeekSupport READ positionSeekSupport CONSTANT)
    Q_PROPERTY(
        ImageViewportCapabilitySupport timedPlaybackSupport READ timedPlaybackSupport CONSTANT)
    Q_PROPERTY(bool autoplay READ autoplay CONSTANT)
    Q_PROPERTY(bool progressiveAnimationReadiness READ progressiveAnimationReadiness CONSTANT)
    Q_PROPERTY(ImageSequenceAuthoredAnimationLoopMode loopMode READ loopMode CONSTANT)
    Q_PROPERTY(int loopCount READ loopCount CONSTANT)

public:
    ImageViewportRoleMetadataSnapshot() = default;
    ImageViewportRoleMetadataSnapshot(bool available, QSizeF sourceLogicalSize, int frameCount,
        int totalDuration, ImageViewportRange frameSeekBounds,
        ImageViewportRange positionSeekBounds, ImageViewportCapabilitySupport frameSeekSupport,
        ImageViewportCapabilitySupport positionSeekSupport,
        ImageViewportCapabilitySupport timedPlaybackSupport, bool autoplay,
        bool progressiveAnimationReadiness, ImageSequenceAuthoredAnimationLoopMode loopMode,
        int loopCount)
        : m_available(available)
        , m_sourceLogicalSize(sourceLogicalSize)
        , m_frameCount(frameCount)
        , m_totalDuration(totalDuration)
        , m_frameSeekBounds(frameSeekBounds)
        , m_positionSeekBounds(positionSeekBounds)
        , m_frameSeekSupport(frameSeekSupport)
        , m_positionSeekSupport(positionSeekSupport)
        , m_timedPlaybackSupport(timedPlaybackSupport)
        , m_autoplay(autoplay)
        , m_progressiveAnimationReadiness(progressiveAnimationReadiness)
        , m_loopMode(loopMode)
        , m_loopCount(loopCount)
    {
    }

    bool available() const { return m_available; }
    QSizeF sourceLogicalSize() const { return m_sourceLogicalSize; }
    int frameCount() const { return m_frameCount; }
    int totalDuration() const { return m_totalDuration; }
    ImageViewportRange frameSeekBounds() const { return m_frameSeekBounds; }
    ImageViewportRange positionSeekBounds() const { return m_positionSeekBounds; }
    ImageViewportCapabilitySupport frameSeekSupport() const { return m_frameSeekSupport; }
    ImageViewportCapabilitySupport positionSeekSupport() const { return m_positionSeekSupport; }
    ImageViewportCapabilitySupport timedPlaybackSupport() const { return m_timedPlaybackSupport; }
    bool autoplay() const { return m_autoplay; }
    bool progressiveAnimationReadiness() const { return m_progressiveAnimationReadiness; }
    ImageSequenceAuthoredAnimationLoopMode loopMode() const { return m_loopMode; }
    int loopCount() const { return m_loopCount; }

    friend bool operator==(
        const ImageViewportRoleMetadataSnapshot& lhs, const ImageViewportRoleMetadataSnapshot& rhs)
    {
        return lhs.m_available == rhs.m_available
            && lhs.m_sourceLogicalSize == rhs.m_sourceLogicalSize
            && lhs.m_frameCount == rhs.m_frameCount && lhs.m_totalDuration == rhs.m_totalDuration
            && lhs.m_frameSeekBounds == rhs.m_frameSeekBounds
            && lhs.m_positionSeekBounds == rhs.m_positionSeekBounds
            && lhs.m_frameSeekSupport == rhs.m_frameSeekSupport
            && lhs.m_positionSeekSupport == rhs.m_positionSeekSupport
            && lhs.m_timedPlaybackSupport == rhs.m_timedPlaybackSupport
            && lhs.m_autoplay == rhs.m_autoplay
            && lhs.m_progressiveAnimationReadiness == rhs.m_progressiveAnimationReadiness
            && lhs.m_loopMode == rhs.m_loopMode && lhs.m_loopCount == rhs.m_loopCount;
    }
    friend bool operator!=(
        const ImageViewportRoleMetadataSnapshot& lhs, const ImageViewportRoleMetadataSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_available = false;
    QSizeF m_sourceLogicalSize;
    int m_frameCount = -1;
    int m_totalDuration = -1;
    ImageViewportRange m_frameSeekBounds;
    ImageViewportRange m_positionSeekBounds;
    ImageViewportCapabilitySupport m_frameSeekSupport = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport m_positionSeekSupport
        = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport m_timedPlaybackSupport
        = ImageViewportCapabilitySupport::Unavailable;
    bool m_autoplay = false;
    bool m_progressiveAnimationReadiness = false;
    ImageSequenceAuthoredAnimationLoopMode m_loopMode
        = ImageSequenceAuthoredAnimationLoopMode::PlayOnce;
    int m_loopCount = -1;
};

class ImageViewportRoleGeometrySnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleGeometrySnapshot)
    Q_PROPERTY(QRectF acceptedPageRect READ acceptedPageRect CONSTANT)
    Q_PROPERTY(QRectF acceptedItemRect READ acceptedItemRect CONSTANT)
    Q_PROPERTY(QRectF acceptedVisiblePageRect READ acceptedVisiblePageRect CONSTANT)
    Q_PROPERTY(QRectF displayedPageRect READ displayedPageRect CONSTANT)
    Q_PROPERTY(QRectF displayedItemRect READ displayedItemRect CONSTANT)
    Q_PROPERTY(QRectF displayedVisiblePageRect READ displayedVisiblePageRect CONSTANT)

public:
    ImageViewportRoleGeometrySnapshot() = default;
    ImageViewportRoleGeometrySnapshot(QRectF acceptedPageRect, QRectF acceptedItemRect,
        QRectF acceptedVisiblePageRect, QRectF displayedPageRect, QRectF displayedItemRect,
        QRectF displayedVisiblePageRect)
        : m_acceptedPageRect(acceptedPageRect)
        , m_acceptedItemRect(acceptedItemRect)
        , m_acceptedVisiblePageRect(acceptedVisiblePageRect)
        , m_displayedPageRect(displayedPageRect)
        , m_displayedItemRect(displayedItemRect)
        , m_displayedVisiblePageRect(displayedVisiblePageRect)
    {
    }

    QRectF acceptedPageRect() const { return m_acceptedPageRect; }
    QRectF acceptedItemRect() const { return m_acceptedItemRect; }
    QRectF acceptedVisiblePageRect() const { return m_acceptedVisiblePageRect; }
    QRectF displayedPageRect() const { return m_displayedPageRect; }
    QRectF displayedItemRect() const { return m_displayedItemRect; }
    QRectF displayedVisiblePageRect() const { return m_displayedVisiblePageRect; }

    friend bool operator==(
        const ImageViewportRoleGeometrySnapshot& lhs, const ImageViewportRoleGeometrySnapshot& rhs)
    {
        return lhs.m_acceptedPageRect == rhs.m_acceptedPageRect
            && lhs.m_acceptedItemRect == rhs.m_acceptedItemRect
            && lhs.m_acceptedVisiblePageRect == rhs.m_acceptedVisiblePageRect
            && lhs.m_displayedPageRect == rhs.m_displayedPageRect
            && lhs.m_displayedItemRect == rhs.m_displayedItemRect
            && lhs.m_displayedVisiblePageRect == rhs.m_displayedVisiblePageRect;
    }
    friend bool operator!=(
        const ImageViewportRoleGeometrySnapshot& lhs, const ImageViewportRoleGeometrySnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    QRectF m_acceptedPageRect;
    QRectF m_acceptedItemRect;
    QRectF m_acceptedVisiblePageRect;
    QRectF m_displayedPageRect;
    QRectF m_displayedItemRect;
    QRectF m_displayedVisiblePageRect;
};

class ImageViewportRoleSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleSnapshot)
    Q_PROPERTY(bool present READ present CONSTANT)
    Q_PROPERTY(ImageSequence* sequence READ sequence CONSTANT)
    Q_PROPERTY(ImageViewportRoleRequestSnapshot request READ request CONSTANT)
    Q_PROPERTY(ImageViewportRoleDisplaySnapshot display READ display CONSTANT)
    Q_PROPERTY(ImageViewportRoleMetadataSnapshot metadata READ metadata CONSTANT)
    Q_PROPERTY(ImageViewportRoleGeometrySnapshot geometry READ geometry CONSTANT)

public:
    ImageViewportRoleSnapshot() = default;
    ImageViewportRoleSnapshot(bool present, ImageSequence* sequence,
        ImageViewportRoleRequestSnapshot request, ImageViewportRoleDisplaySnapshot display,
        ImageViewportRoleMetadataSnapshot metadata, ImageViewportRoleGeometrySnapshot geometry)
        : m_present(present)
        , m_sequence(sequence)
        , m_request(request)
        , m_display(display)
        , m_metadata(metadata)
        , m_geometry(geometry)
    {
    }

    bool present() const { return m_present; }
    ImageSequence* sequence() const { return m_sequence; }
    ImageViewportRoleRequestSnapshot request() const { return m_request; }
    ImageViewportRoleDisplaySnapshot display() const { return m_display; }
    ImageViewportRoleMetadataSnapshot metadata() const { return m_metadata; }
    ImageViewportRoleGeometrySnapshot geometry() const { return m_geometry; }

    friend bool operator==(
        const ImageViewportRoleSnapshot& lhs, const ImageViewportRoleSnapshot& rhs)
    {
        return lhs.m_present == rhs.m_present && lhs.m_sequence == rhs.m_sequence
            && lhs.m_request == rhs.m_request && lhs.m_display == rhs.m_display
            && lhs.m_metadata == rhs.m_metadata && lhs.m_geometry == rhs.m_geometry;
    }
    friend bool operator!=(
        const ImageViewportRoleSnapshot& lhs, const ImageViewportRoleSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_present = false;
    QPointer<ImageSequence> m_sequence;
    ImageViewportRoleRequestSnapshot m_request;
    ImageViewportRoleDisplaySnapshot m_display;
    ImageViewportRoleMetadataSnapshot m_metadata;
    ImageViewportRoleGeometrySnapshot m_geometry;
};

class ImageViewportDiagnosticsSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportDiagnosticsSnapshot)
    Q_PROPERTY(QString errorString READ errorString CONSTANT)
    Q_PROPERTY(QString warningString READ warningString CONSTANT)
    Q_PROPERTY(ImageViewportCommandReason commandReason READ commandReason CONSTANT)

public:
    ImageViewportDiagnosticsSnapshot() = default;
    ImageViewportDiagnosticsSnapshot(
        QString errorString, QString warningString, ImageViewportCommandReason commandReason)
        : m_errorString(std::move(errorString))
        , m_warningString(std::move(warningString))
        , m_commandReason(commandReason)
    {
    }

    QString errorString() const { return m_errorString; }
    QString warningString() const { return m_warningString; }
    ImageViewportCommandReason commandReason() const { return m_commandReason; }

    friend bool operator==(
        const ImageViewportDiagnosticsSnapshot& lhs, const ImageViewportDiagnosticsSnapshot& rhs)
    {
        return lhs.m_errorString == rhs.m_errorString && lhs.m_warningString == rhs.m_warningString
            && lhs.m_commandReason == rhs.m_commandReason;
    }
    friend bool operator!=(
        const ImageViewportDiagnosticsSnapshot& lhs, const ImageViewportDiagnosticsSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    QString m_errorString;
    QString m_warningString;
    ImageViewportCommandReason m_commandReason = ImageViewportCommandReason::NoCommand;
};

class ImageViewportRevisionsSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRevisionsSnapshot)
    Q_PROPERTY(ImageViewportRevisionToken request READ request CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken display READ display CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken presentation READ presentation CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken command READ command CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken snapshot READ snapshot CONSTANT)

public:
    ImageViewportRevisionsSnapshot() = default;
    ImageViewportRevisionsSnapshot(ImageViewportRevisionToken request,
        ImageViewportRevisionToken display, ImageViewportRevisionToken presentation,
        ImageViewportRevisionToken command, ImageViewportRevisionToken snapshot)
        : m_request(request)
        , m_display(display)
        , m_presentation(presentation)
        , m_command(command)
        , m_snapshot(snapshot)
    {
    }

    ImageViewportRevisionToken request() const { return m_request; }
    ImageViewportRevisionToken display() const { return m_display; }
    ImageViewportRevisionToken presentation() const { return m_presentation; }
    ImageViewportRevisionToken command() const { return m_command; }
    ImageViewportRevisionToken snapshot() const { return m_snapshot; }

    friend bool operator==(
        const ImageViewportRevisionsSnapshot& lhs, const ImageViewportRevisionsSnapshot& rhs)
    {
        return lhs.m_request == rhs.m_request && lhs.m_display == rhs.m_display
            && lhs.m_presentation == rhs.m_presentation && lhs.m_command == rhs.m_command
            && lhs.m_snapshot == rhs.m_snapshot;
    }
    friend bool operator!=(
        const ImageViewportRevisionsSnapshot& lhs, const ImageViewportRevisionsSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportRevisionToken m_request;
    ImageViewportRevisionToken m_display;
    ImageViewportRevisionToken m_presentation;
    ImageViewportRevisionToken m_command;
    ImageViewportRevisionToken m_snapshot;
};

class ImageViewportStateSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportStateSnapshot)
    Q_PROPERTY(ImageViewportRequestSnapshot request READ request CONSTANT)
    Q_PROPERTY(ImageViewportDisplaySnapshot display READ display CONSTANT)
    Q_PROPERTY(ImageViewportPresentationSnapshot presentation READ presentation CONSTANT)
    Q_PROPERTY(ImageViewportRoleSnapshot primary READ primary CONSTANT)
    Q_PROPERTY(ImageViewportRoleSnapshot secondary READ secondary CONSTANT)
    Q_PROPERTY(ImageViewportDiagnosticsSnapshot diagnostics READ diagnostics CONSTANT)
    Q_PROPERTY(ImageViewportRevisionsSnapshot revisions READ revisions CONSTANT)

public:
    ImageViewportStateSnapshot() = default;
    ImageViewportStateSnapshot(ImageViewportRequestSnapshot request,
        ImageViewportDisplaySnapshot display, ImageViewportPresentationSnapshot presentation,
        ImageViewportRoleSnapshot primary, ImageViewportRoleSnapshot secondary,
        ImageViewportDiagnosticsSnapshot diagnostics, ImageViewportRevisionsSnapshot revisions)
        : m_request(request)
        , m_display(display)
        , m_presentation(presentation)
        , m_primary(primary)
        , m_secondary(secondary)
        , m_diagnostics(diagnostics)
        , m_revisions(revisions)
    {
    }

    ImageViewportRequestSnapshot request() const { return m_request; }
    ImageViewportDisplaySnapshot display() const { return m_display; }
    ImageViewportPresentationSnapshot presentation() const { return m_presentation; }
    ImageViewportRoleSnapshot primary() const { return m_primary; }
    ImageViewportRoleSnapshot secondary() const { return m_secondary; }
    ImageViewportDiagnosticsSnapshot diagnostics() const { return m_diagnostics; }
    ImageViewportRevisionsSnapshot revisions() const { return m_revisions; }

    friend bool operator==(
        const ImageViewportStateSnapshot& lhs, const ImageViewportStateSnapshot& rhs)
    {
        return lhs.m_request == rhs.m_request && lhs.m_display == rhs.m_display
            && lhs.m_presentation == rhs.m_presentation && lhs.m_primary == rhs.m_primary
            && lhs.m_secondary == rhs.m_secondary && lhs.m_diagnostics == rhs.m_diagnostics
            && lhs.m_revisions == rhs.m_revisions;
    }
    friend bool operator!=(
        const ImageViewportStateSnapshot& lhs, const ImageViewportStateSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportRequestSnapshot m_request;
    ImageViewportDisplaySnapshot m_display;
    ImageViewportPresentationSnapshot m_presentation;
    ImageViewportRoleSnapshot m_primary;
    ImageViewportRoleSnapshot m_secondary;
    ImageViewportDiagnosticsSnapshot m_diagnostics;
    ImageViewportRevisionsSnapshot m_revisions;
};

class ImageViewportCommandResult
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportCommandResult)
    Q_PROPERTY(ImageViewportCommandOutcome outcome READ outcome CONSTANT)
    Q_PROPERTY(ImageViewportCommandReason reason READ reason CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken commandRevision READ commandRevision CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken snapshotRevision READ snapshotRevision CONSTANT)

public:
    ImageViewportCommandResult() = default;
    ImageViewportCommandResult(ImageViewportCommandOutcome outcome,
        ImageViewportCommandReason reason, ImageViewportRevisionToken commandRevision,
        ImageViewportRevisionToken snapshotRevision)
        : m_outcome(outcome)
        , m_reason(reason)
        , m_commandRevision(commandRevision)
        , m_snapshotRevision(snapshotRevision)
    {
    }

    ImageViewportCommandOutcome outcome() const { return m_outcome; }
    ImageViewportCommandReason reason() const { return m_reason; }
    ImageViewportRevisionToken commandRevision() const { return m_commandRevision; }
    ImageViewportRevisionToken snapshotRevision() const { return m_snapshotRevision; }

    friend bool operator==(
        const ImageViewportCommandResult& lhs, const ImageViewportCommandResult& rhs)
    {
        return lhs.m_outcome == rhs.m_outcome && lhs.m_reason == rhs.m_reason
            && lhs.m_commandRevision == rhs.m_commandRevision
            && lhs.m_snapshotRevision == rhs.m_snapshotRevision;
    }
    friend bool operator!=(
        const ImageViewportCommandResult& lhs, const ImageViewportCommandResult& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportCommandOutcome m_outcome = ImageViewportCommandOutcome::Accepted;
    ImageViewportCommandReason m_reason = ImageViewportCommandReason::NoCommand;
    ImageViewportRevisionToken m_commandRevision;
    ImageViewportRevisionToken m_snapshotRevision;
};

class ImageViewportCoordinateInput
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportCoordinateInput)
    QML_STRUCTURED_VALUE
    Q_PROPERTY(ImageViewportCoordinateSpace sourceSpace READ sourceSpace WRITE setSourceSpace)
    Q_PROPERTY(ImageViewportCoordinateSpace targetSpace READ targetSpace WRITE setTargetSpace)
    Q_PROPERTY(QVariant role READ role WRITE setRole)
    Q_PROPERTY(QPointF point READ point WRITE setPoint)

public:
    ImageViewportCoordinateInput() = default;

    ImageViewportCoordinateSpace sourceSpace() const { return m_sourceSpace; }
    void setSourceSpace(ImageViewportCoordinateSpace sourceSpace) { m_sourceSpace = sourceSpace; }
    ImageViewportCoordinateSpace targetSpace() const { return m_targetSpace; }
    void setTargetSpace(ImageViewportCoordinateSpace targetSpace) { m_targetSpace = targetSpace; }
    QVariant role() const { return m_role; }
    void setRole(QVariant role) { m_role = std::move(role); }
    QPointF point() const { return m_point; }
    void setPoint(QPointF point) { m_point = point; }

    friend bool operator==(
        const ImageViewportCoordinateInput& lhs, const ImageViewportCoordinateInput& rhs)
    {
        return lhs.m_sourceSpace == rhs.m_sourceSpace && lhs.m_targetSpace == rhs.m_targetSpace
            && lhs.m_role == rhs.m_role && lhs.m_point == rhs.m_point;
    }
    friend bool operator!=(
        const ImageViewportCoordinateInput& lhs, const ImageViewportCoordinateInput& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportCoordinateSpace m_sourceSpace = ImageViewportCoordinateSpace::Item;
    ImageViewportCoordinateSpace m_targetSpace = ImageViewportCoordinateSpace::DisplayedSpread;
    QVariant m_role;
    QPointF m_point;
};

class ImageViewportCoordinateResult
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportCoordinateResult)
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(QPointF point READ point CONSTANT)
    Q_PROPERTY(ImageViewportCoordinateSpace space READ space CONSTANT)
    Q_PROPERTY(QVariant role READ role CONSTANT)

public:
    ImageViewportCoordinateResult() = default;
    ImageViewportCoordinateResult(
        bool valid, QPointF point, ImageViewportCoordinateSpace space, QVariant role = {})
        : m_valid(valid)
        , m_point(point)
        , m_space(space)
        , m_role(std::move(role))
    {
    }

    bool isValid() const { return m_valid; }
    QPointF point() const { return m_point; }
    ImageViewportCoordinateSpace space() const { return m_space; }
    QVariant role() const { return m_role; }

    friend bool operator==(
        const ImageViewportCoordinateResult& lhs, const ImageViewportCoordinateResult& rhs)
    {
        return lhs.m_valid == rhs.m_valid && lhs.m_point == rhs.m_point
            && lhs.m_space == rhs.m_space && lhs.m_role == rhs.m_role;
    }
    friend bool operator!=(
        const ImageViewportCoordinateResult& lhs, const ImageViewportCoordinateResult& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_valid = false;
    QPointF m_point;
    ImageViewportCoordinateSpace m_space = ImageViewportCoordinateSpace::Item;
    QVariant m_role;
};

Q_DECLARE_METATYPE(ImageViewportRequestSnapshot)
Q_DECLARE_METATYPE(ImageViewportDisplaySnapshot)
Q_DECLARE_METATYPE(ImageViewportPresentationSnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleRequestSnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleDisplaySnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleMetadataSnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleGeometrySnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleSnapshot)
Q_DECLARE_METATYPE(ImageViewportDiagnosticsSnapshot)
Q_DECLARE_METATYPE(ImageViewportRevisionsSnapshot)
Q_DECLARE_METATYPE(ImageViewportStateSnapshot)
Q_DECLARE_METATYPE(ImageViewportCommandResult)
Q_DECLARE_METATYPE(ImageViewportCoordinateInput)
Q_DECLARE_METATYPE(ImageViewportCoordinateResult)
