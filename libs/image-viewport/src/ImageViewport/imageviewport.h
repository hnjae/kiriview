/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/imagesequence.h>
#include <ImageViewport/imageviewportstate.h>
#include <ImageViewport/imageviewporttypes.h>

#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtQuick/QQuickItem>

#include <memory>

class ImageViewportPrivate;
class ImageViewportPresentationCommand;
class PresentationTargetTransitionPolicy;

class ImageViewportDisplayLimits : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(double minimumManualZoomPercent READ getMinimumManualZoomPercent CONSTANT)
    Q_PROPERTY(double manualZoomStepFactor READ getManualZoomStepFactor CONSTANT)
    Q_PROPERTY(double maximumPageGap READ getMaximumPageGap CONSTANT)
    Q_PROPERTY(double minimumCheckerboardCellSize READ getMinimumCheckerboardCellSize CONSTANT)
    Q_PROPERTY(double maximumCheckerboardCellSize READ getMaximumCheckerboardCellSize CONSTANT)

public:
    explicit ImageViewportDisplayLimits(QObject* parent = nullptr);

    double getMinimumManualZoomPercent() const;
    double getManualZoomStepFactor() const;
    double getMaximumPageGap() const;
    double getMinimumCheckerboardCellSize() const;
    double getMaximumCheckerboardCellSize() const;

    static double minimumManualZoomPercent();
    static double manualZoomStepFactor();
    static double maximumPageGap();
    static double minimumCheckerboardCellSize();
    static double maximumCheckerboardCellSize();
};

class ImageViewportPresentationTarget
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportPresentationTarget)
    Q_PROPERTY(ImageSequence* primary READ primary WRITE setPrimary)
    Q_PROPERTY(ImageSequence* secondary READ secondary WRITE setSecondary)
    Q_PROPERTY(bool clear READ isClear CONSTANT)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportPresentationTarget() = default;
    explicit ImageViewportPresentationTarget(ImageSequence* primary)
        : m_primary(primary)
    {
    }
    ImageViewportPresentationTarget(ImageSequence* primary, ImageSequence* secondary)
        : m_primary(primary)
        , m_secondary(secondary)
    {
    }

    Q_INVOKABLE static ImageViewportPresentationTarget clear() { return {}; }

    ImageSequence* primary() const { return m_primary; }
    void setPrimary(ImageSequence* primary) { m_primary = primary; }
    ImageSequence* secondary() const { return m_secondary; }
    void setSecondary(ImageSequence* secondary) { m_secondary = secondary; }
    bool isClear() const { return !m_primary && !m_secondary; }
    bool isValid() const { return m_primary || !m_secondary; }

    friend bool operator==(
        const ImageViewportPresentationTarget& lhs, const ImageViewportPresentationTarget& rhs)
    {
        return lhs.m_primary == rhs.m_primary && lhs.m_secondary == rhs.m_secondary;
    }
    friend bool operator!=(
        const ImageViewportPresentationTarget& lhs, const ImageViewportPresentationTarget& rhs)
    {
        return !(lhs == rhs);
    }

private:
    QPointer<ImageSequence> m_primary;
    QPointer<ImageSequence> m_secondary;
};

class ImageViewport : public QQuickItem
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    QML_ELEMENT
    QML_EXTENDED_NAMESPACE(ImageViewportEnums)
    Q_PROPERTY(ImageViewportStateSnapshot state READ state NOTIFY stateChanged)

public:
    explicit ImageViewport(QQuickItem* parent = nullptr);
    ~ImageViewport() override;

    ImageViewportStateSnapshot state() const;

    Q_INVOKABLE ImageViewportCommandResult clear();
    Q_INVOKABLE ImageViewportCommandResult play(
        ImageViewportPageRole role); // clazy:exclude=fully-qualified-moc-types
    Q_INVOKABLE ImageViewportCommandResult pause(
        ImageViewportPageRole role); // clazy:exclude=fully-qualified-moc-types
    Q_INVOKABLE ImageViewportCommandResult stop(
        ImageViewportPageRole role); // clazy:exclude=fully-qualified-moc-types
    Q_INVOKABLE ImageViewportCommandResult seek(
        ImageViewportPageRole role, int frame); // clazy:exclude=fully-qualified-moc-types
    Q_INVOKABLE ImageViewportCommandResult seekToPosition(
        ImageViewportPageRole role, // clazy:exclude=fully-qualified-moc-types
        int milliseconds);
    Q_INVOKABLE ImageViewportCommandResult setPresentationTarget(
        ImageViewportPresentationTarget presentationTarget,
        PresentationTargetTransitionPolicy policy);
    Q_INVOKABLE ImageViewportCommandResult resetView();
    Q_INVOKABLE ImageViewportCommandResult setPresentation(
        ImageViewportPresentationCommand command);
    Q_INVOKABLE ImageViewportCoordinateResult mapPoint(ImageViewportCoordinateInput input) const;
    Q_INVOKABLE bool containsPoint(ImageViewportCoordinateInput input) const;

signals:
    void stateChanged(); // clazy:exclude=overloaded-signal

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& data) override;

private:
    friend ImageViewportPrivate;

    std::unique_ptr<ImageViewportPrivate> d;
};

class ImageViewportPresentationCommand
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportPresentationCommand)
    Q_PROPERTY(bool resetView READ resetView WRITE setResetView)
    Q_PROPERTY(bool fitModeSet READ hasFitMode CONSTANT)
    Q_PROPERTY(ImageViewportFitMode fitMode READ fitMode WRITE setFitMode)
    Q_PROPERTY(bool preferredManualZoomPercentSet READ hasPreferredManualZoomPercent CONSTANT)
    Q_PROPERTY(double preferredManualZoomPercent READ preferredManualZoomPercent WRITE
            setPreferredManualZoomPercent)
    Q_PROPERTY(bool zoomStepDeltaSet READ hasZoomStepDelta CONSTANT)
    Q_PROPERTY(double zoomStepDelta READ zoomStepDelta WRITE setZoomStepDelta)
    Q_PROPERTY(bool zoomAnchorSet READ hasZoomAnchor CONSTANT)
    Q_PROPERTY(QPointF zoomAnchor READ zoomAnchor WRITE setZoomAnchor)
    Q_PROPERTY(bool contentPositionSet READ hasContentPosition CONSTANT)
    Q_PROPERTY(QPointF contentPosition READ contentPosition WRITE setContentPosition)
    Q_PROPERTY(bool panDeltaSet READ hasPanDelta CONSTANT)
    Q_PROPERTY(QPointF panDelta READ panDelta WRITE setPanDelta)
    Q_PROPERTY(bool contentAnchorSet READ hasContentAnchor CONSTANT)
    Q_PROPERTY(ImageViewportContentAnchor contentAnchor READ contentAnchor WRITE setContentAnchor)
    Q_PROPERTY(bool rotationDegreesSet READ hasRotationDegrees CONSTANT)
    Q_PROPERTY(int rotationDegrees READ rotationDegrees WRITE setRotationDegrees)
    Q_PROPERTY(bool mirrorHorizontallySet READ hasMirrorHorizontally CONSTANT)
    Q_PROPERTY(bool mirrorHorizontally READ mirrorHorizontally WRITE setMirrorHorizontally)
    Q_PROPERTY(bool mirrorVerticallySet READ hasMirrorVertically CONSTANT)
    Q_PROPERTY(bool mirrorVertically READ mirrorVertically WRITE setMirrorVertically)
    Q_PROPERTY(bool spreadDirectionSet READ hasSpreadDirection CONSTANT)
    Q_PROPERTY(
        ImageViewportSpreadDirection spreadDirection READ spreadDirection WRITE setSpreadDirection)
    Q_PROPERTY(bool pageGapSet READ hasPageGap CONSTANT)
    Q_PROPERTY(double pageGap READ pageGap WRITE setPageGap)
    Q_PROPERTY(bool backgroundModeSet READ hasBackgroundMode CONSTANT)
    Q_PROPERTY(
        ImageViewportBackgroundMode backgroundMode READ backgroundMode WRITE setBackgroundMode)
    Q_PROPERTY(bool backgroundColorSet READ hasBackgroundColor CONSTANT)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(bool checkerboardLightColorSet READ hasCheckerboardLightColor CONSTANT)
    Q_PROPERTY(
        QColor checkerboardLightColor READ checkerboardLightColor WRITE setCheckerboardLightColor)
    Q_PROPERTY(bool checkerboardDarkColorSet READ hasCheckerboardDarkColor CONSTANT)
    Q_PROPERTY(
        QColor checkerboardDarkColor READ checkerboardDarkColor WRITE setCheckerboardDarkColor)
    Q_PROPERTY(bool checkerboardCellSizeSet READ hasCheckerboardCellSize CONSTANT)
    Q_PROPERTY(double checkerboardCellSize READ checkerboardCellSize WRITE setCheckerboardCellSize)
    Q_PROPERTY(bool smoothingSet READ hasSmoothing CONSTANT)
    Q_PROPERTY(bool smoothing READ smoothing WRITE setSmoothing)
    Q_PROPERTY(bool mipmapSet READ hasMipmap CONSTANT)
    Q_PROPERTY(bool mipmap READ mipmap WRITE setMipmap)
    Q_PROPERTY(bool loopingSet READ hasLooping CONSTANT)
    Q_PROPERTY(bool looping READ looping WRITE setLooping)
    Q_PROPERTY(bool qualityPreferenceSet READ hasQualityPreference CONSTANT)
    Q_PROPERTY(ImageViewportQualityPreference qualityPreference READ qualityPreference WRITE
            setQualityPreference)
    Q_PROPERTY(bool exactnessPreferenceSet READ hasExactnessPreference CONSTANT)
    Q_PROPERTY(ImageViewportExactnessPreference exactnessPreference READ exactnessPreference WRITE
            setExactnessPreference)

public:
    ImageViewportPresentationCommand() = default;

    static ImageViewportPresentationCommand resetViewCommand()
    {
        ImageViewportPresentationCommand command;
        command.setResetView(true);
        return command;
    }

    bool resetView() const { return m_resetView; }
    void setResetView(bool reset) { m_resetView = reset; }
    bool hasFitMode() const { return m_hasFitMode; }
    ImageViewportFitMode fitMode() const { return m_fitMode; }
    void setFitMode(ImageViewportFitMode mode)
    {
        m_fitMode = mode;
        m_hasFitMode = true;
    }
    bool hasPreferredManualZoomPercent() const { return m_hasPreferredManualZoomPercent; }
    double preferredManualZoomPercent() const { return m_preferredManualZoomPercent; }
    void setPreferredManualZoomPercent(double percent)
    {
        m_preferredManualZoomPercent = percent;
        m_hasPreferredManualZoomPercent = true;
    }
    bool hasZoomStepDelta() const { return m_hasZoomStepDelta; }
    double zoomStepDelta() const { return m_zoomStepDelta; }
    void setZoomStepDelta(double delta)
    {
        m_zoomStepDelta = delta;
        m_hasZoomStepDelta = true;
    }
    bool hasZoomAnchor() const { return m_hasZoomAnchor; }
    QPointF zoomAnchor() const { return m_zoomAnchor; }
    void setZoomAnchor(QPointF anchor)
    {
        m_zoomAnchor = anchor;
        m_hasZoomAnchor = true;
    }
    bool hasContentPosition() const { return m_hasContentPosition; }
    QPointF contentPosition() const { return m_contentPosition; }
    void setContentPosition(QPointF position)
    {
        m_contentPosition = position;
        m_hasContentPosition = true;
    }
    bool hasPanDelta() const { return m_hasPanDelta; }
    QPointF panDelta() const { return m_panDelta; }
    void setPanDelta(QPointF delta)
    {
        m_panDelta = delta;
        m_hasPanDelta = true;
    }
    bool hasContentAnchor() const { return m_hasContentAnchor; }
    ImageViewportContentAnchor contentAnchor() const { return m_contentAnchor; }
    void setContentAnchor(ImageViewportContentAnchor direction)
    {
        m_contentAnchor = direction;
        m_hasContentAnchor = true;
    }
    bool hasRotationDegrees() const { return m_hasRotationDegrees; }
    int rotationDegrees() const { return m_rotationDegrees; }
    void setRotationDegrees(int degrees)
    {
        m_rotationDegrees = degrees;
        m_hasRotationDegrees = true;
    }
    bool hasMirrorHorizontally() const { return m_hasMirrorHorizontally; }
    bool mirrorHorizontally() const { return m_mirrorHorizontally; }
    void setMirrorHorizontally(bool mirror)
    {
        m_mirrorHorizontally = mirror;
        m_hasMirrorHorizontally = true;
    }
    bool hasMirrorVertically() const { return m_hasMirrorVertically; }
    bool mirrorVertically() const { return m_mirrorVertically; }
    void setMirrorVertically(bool mirror)
    {
        m_mirrorVertically = mirror;
        m_hasMirrorVertically = true;
    }
    bool hasSpreadDirection() const { return m_hasSpreadDirection; }
    ImageViewportSpreadDirection spreadDirection() const { return m_spreadDirection; }
    void setSpreadDirection(ImageViewportSpreadDirection direction)
    {
        m_spreadDirection = direction;
        m_hasSpreadDirection = true;
    }
    bool hasPageGap() const { return m_hasPageGap; }
    double pageGap() const { return m_pageGap; }
    void setPageGap(double gap)
    {
        m_pageGap = gap;
        m_hasPageGap = true;
    }
    bool hasBackgroundMode() const { return m_hasBackgroundMode; }
    ImageViewportBackgroundMode backgroundMode() const { return m_backgroundMode; }
    void setBackgroundMode(ImageViewportBackgroundMode mode)
    {
        m_backgroundMode = mode;
        m_hasBackgroundMode = true;
    }
    bool hasBackgroundColor() const { return m_hasBackgroundColor; }
    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor& color)
    {
        m_backgroundColor = color;
        m_hasBackgroundColor = true;
    }
    bool hasCheckerboardLightColor() const { return m_hasCheckerboardLightColor; }
    QColor checkerboardLightColor() const { return m_checkerboardLightColor; }
    void setCheckerboardLightColor(const QColor& color)
    {
        m_checkerboardLightColor = color;
        m_hasCheckerboardLightColor = true;
    }
    bool hasCheckerboardDarkColor() const { return m_hasCheckerboardDarkColor; }
    QColor checkerboardDarkColor() const { return m_checkerboardDarkColor; }
    void setCheckerboardDarkColor(const QColor& color)
    {
        m_checkerboardDarkColor = color;
        m_hasCheckerboardDarkColor = true;
    }
    bool hasCheckerboardCellSize() const { return m_hasCheckerboardCellSize; }
    double checkerboardCellSize() const { return m_checkerboardCellSize; }
    void setCheckerboardCellSize(double size)
    {
        m_checkerboardCellSize = size;
        m_hasCheckerboardCellSize = true;
    }
    bool hasSmoothing() const { return m_hasSmoothing; }
    bool smoothing() const { return m_smoothing; }
    void setSmoothing(bool smoothing)
    {
        m_smoothing = smoothing;
        m_hasSmoothing = true;
    }
    bool hasMipmap() const { return m_hasMipmap; }
    bool mipmap() const { return m_mipmap; }
    void setMipmap(bool mipmap)
    {
        m_mipmap = mipmap;
        m_hasMipmap = true;
    }
    bool hasLooping() const { return m_hasLooping; }
    bool looping() const { return m_looping; }
    void setLooping(bool looping)
    {
        m_looping = looping;
        m_hasLooping = true;
    }
    bool hasQualityPreference() const { return m_hasQualityPreference; }
    ImageViewportQualityPreference qualityPreference() const { return m_qualityPreference; }
    void setQualityPreference(ImageViewportQualityPreference preference)
    {
        m_qualityPreference = preference;
        m_hasQualityPreference = true;
    }
    bool hasExactnessPreference() const { return m_hasExactnessPreference; }
    ImageViewportExactnessPreference exactnessPreference() const { return m_exactnessPreference; }
    void setExactnessPreference(ImageViewportExactnessPreference preference)
    {
        m_exactnessPreference = preference;
        m_hasExactnessPreference = true;
    }

private:
    bool m_resetView = false;
    bool m_hasFitMode = false;
    ImageViewportFitMode m_fitMode = ImageViewportFitMode::Contain;
    bool m_hasPreferredManualZoomPercent = false;
    double m_preferredManualZoomPercent = 100.0;
    bool m_hasZoomStepDelta = false;
    double m_zoomStepDelta = 0.0;
    bool m_hasZoomAnchor = false;
    QPointF m_zoomAnchor;
    bool m_hasContentPosition = false;
    QPointF m_contentPosition;
    bool m_hasPanDelta = false;
    QPointF m_panDelta;
    bool m_hasContentAnchor = false;
    ImageViewportContentAnchor m_contentAnchor = ImageViewportContentAnchor::Start;
    bool m_hasRotationDegrees = false;
    int m_rotationDegrees = 0;
    bool m_hasMirrorHorizontally = false;
    bool m_mirrorHorizontally = false;
    bool m_hasMirrorVertically = false;
    bool m_mirrorVertically = false;
    bool m_hasSpreadDirection = false;
    ImageViewportSpreadDirection m_spreadDirection = ImageViewportSpreadDirection::LeftToRight;
    bool m_hasPageGap = false;
    double m_pageGap = 0.0;
    bool m_hasBackgroundMode = false;
    ImageViewportBackgroundMode m_backgroundMode = ImageViewportBackgroundMode::Transparent;
    bool m_hasBackgroundColor = false;
    QColor m_backgroundColor = Qt::white;
    bool m_hasCheckerboardLightColor = false;
    QColor m_checkerboardLightColor = Qt::white;
    bool m_hasCheckerboardDarkColor = false;
    QColor m_checkerboardDarkColor = QColor(220, 220, 220);
    bool m_hasCheckerboardCellSize = false;
    double m_checkerboardCellSize = 8.0;
    bool m_hasSmoothing = false;
    bool m_smoothing = true;
    bool m_hasMipmap = false;
    bool m_mipmap = false;
    bool m_hasLooping = false;
    bool m_looping = false;
    bool m_hasQualityPreference = false;
    ImageViewportQualityPreference m_qualityPreference = ImageViewportQualityPreference::Default;
    bool m_hasExactnessPreference = false;
    ImageViewportExactnessPreference m_exactnessPreference
        = ImageViewportExactnessPreference::Default;
};

class PresentationTargetTransitionPolicy
{
    Q_GADGET
    QML_VALUE_TYPE(presentationTargetTransitionPolicy)
    QML_STRUCTURED_VALUE
    Q_PROPERTY(
        DisplayTransition displayTransition READ displayTransition WRITE setDisplayTransition)
    Q_PROPERTY(
        FailureTransition failureTransition READ failureTransition WRITE setFailureTransition)
    Q_PROPERTY(ZoomTransition zoomTransition READ zoomTransition WRITE setZoomTransition)
    Q_PROPERTY(ContentPositionTransition contentPositionTransition READ contentPositionTransition
            WRITE setContentPositionTransition)
    Q_PROPERTY(
        RotationTransition rotationTransition READ rotationTransition WRITE setRotationTransition)
    Q_PROPERTY(MirrorTransition mirrorTransition READ mirrorTransition WRITE setMirrorTransition)
    Q_PROPERTY(
        FitModeTransition fitModeTransition READ fitModeTransition WRITE setFitModeTransition)
    Q_PROPERTY(ImageViewportFitMode fitMode READ fitMode WRITE setFitMode)
    Q_PROPERTY(SpreadDirectionTransition spreadDirectionTransition READ spreadDirectionTransition
            WRITE setSpreadDirectionTransition)
    Q_PROPERTY(
        ImageViewportSpreadDirection spreadDirection READ spreadDirection WRITE setSpreadDirection)
    Q_PROPERTY(
        PageGapTransition pageGapTransition READ pageGapTransition WRITE setPageGapTransition)
    Q_PROPERTY(double pageGap READ pageGap WRITE setPageGap)
    Q_PROPERTY(
        ReplacementIntent replacementIntent READ replacementIntent WRITE setReplacementIntent)

public:
    enum class DisplayTransition {
        RetainPrevious,
        ClearBeforeLoad,
    };
    Q_ENUM(DisplayTransition)

    enum class ZoomTransition {
        Preserve,
        ResetToContain,
    };
    Q_ENUM(ZoomTransition)

    enum class FailureTransition {
        KeepFailedTarget,
        RestorePrevious,
    };
    Q_ENUM(FailureTransition)

    enum class ContentPositionTransition {
        Clamp,
        AnchorStart,
        AnchorEnd,
    };
    Q_ENUM(ContentPositionTransition)

    enum class RotationTransition {
        Preserve,
        Reset,
    };
    Q_ENUM(RotationTransition)

    enum class MirrorTransition {
        Preserve,
        Reset,
    };
    Q_ENUM(MirrorTransition)

    enum class FitModeTransition {
        Preserve,
        SetExplicit,
    };
    Q_ENUM(FitModeTransition)

    enum class SpreadDirectionTransition {
        Preserve,
        SetExplicit,
    };
    Q_ENUM(SpreadDirectionTransition)

    enum class PageGapTransition {
        Preserve,
        SetExplicit,
    };
    Q_ENUM(PageGapTransition)

    enum class ReplacementIntent {
        NewTarget,
        SameTargetRefinement,
    };
    Q_ENUM(ReplacementIntent)

    PresentationTargetTransitionPolicy() = default;

    Q_INVOKABLE static PresentationTargetTransitionPolicy defaultClear()
    {
        PresentationTargetTransitionPolicy policy;
        policy.setDisplayTransition(DisplayTransition::ClearBeforeLoad);
        return policy;
    }

    DisplayTransition displayTransition() const { return m_displayTransition; }
    void setDisplayTransition(DisplayTransition transition) { m_displayTransition = transition; }
    FailureTransition failureTransition() const { return m_failureTransition; }
    void setFailureTransition(FailureTransition transition) { m_failureTransition = transition; }
    ZoomTransition zoomTransition() const { return m_zoomTransition; }
    void setZoomTransition(ZoomTransition transition) { m_zoomTransition = transition; }
    ContentPositionTransition contentPositionTransition() const
    {
        return m_contentPositionTransition;
    }
    void setContentPositionTransition(ContentPositionTransition transition)
    {
        m_contentPositionTransition = transition;
    }
    RotationTransition rotationTransition() const { return m_rotationTransition; }
    void setRotationTransition(RotationTransition transition) { m_rotationTransition = transition; }
    MirrorTransition mirrorTransition() const { return m_mirrorTransition; }
    void setMirrorTransition(MirrorTransition transition) { m_mirrorTransition = transition; }
    FitModeTransition fitModeTransition() const { return m_fitModeTransition; }
    void setFitModeTransition(FitModeTransition transition) { m_fitModeTransition = transition; }
    ImageViewportFitMode fitMode() const { return m_fitMode; }
    void setFitMode(ImageViewportFitMode mode)
    {
        m_fitMode = mode;
        m_fitModeSet = true;
    }
    SpreadDirectionTransition spreadDirectionTransition() const
    {
        return m_spreadDirectionTransition;
    }
    void setSpreadDirectionTransition(SpreadDirectionTransition transition)
    {
        m_spreadDirectionTransition = transition;
    }
    ImageViewportSpreadDirection spreadDirection() const { return m_spreadDirection; }
    void setSpreadDirection(ImageViewportSpreadDirection direction)
    {
        m_spreadDirection = direction;
        m_spreadDirectionSet = true;
    }
    PageGapTransition pageGapTransition() const { return m_pageGapTransition; }
    void setPageGapTransition(PageGapTransition transition) { m_pageGapTransition = transition; }
    double pageGap() const { return m_pageGap; }
    void setPageGap(double gap)
    {
        m_pageGap = gap;
        m_pageGapSet = true;
    }
    ReplacementIntent replacementIntent() const { return m_replacementIntent; }
    void setReplacementIntent(ReplacementIntent intent) { m_replacementIntent = intent; }

    bool hasExplicitFitMode() const { return m_fitModeSet; }
    bool hasExplicitSpreadDirection() const { return m_spreadDirectionSet; }
    bool hasExplicitPageGap() const { return m_pageGapSet; }
    bool isValid() const;

    friend bool operator==(
        PresentationTargetTransitionPolicy lhs, PresentationTargetTransitionPolicy rhs)
    {
        return lhs.m_displayTransition == rhs.m_displayTransition
            && lhs.m_failureTransition == rhs.m_failureTransition
            && lhs.m_zoomTransition == rhs.m_zoomTransition
            && lhs.m_contentPositionTransition == rhs.m_contentPositionTransition
            && lhs.m_rotationTransition == rhs.m_rotationTransition
            && lhs.m_mirrorTransition == rhs.m_mirrorTransition
            && lhs.m_fitModeTransition == rhs.m_fitModeTransition && lhs.m_fitMode == rhs.m_fitMode
            && lhs.m_fitModeSet == rhs.m_fitModeSet
            && lhs.m_spreadDirectionTransition == rhs.m_spreadDirectionTransition
            && lhs.m_spreadDirection == rhs.m_spreadDirection
            && lhs.m_spreadDirectionSet == rhs.m_spreadDirectionSet
            && lhs.m_pageGapTransition == rhs.m_pageGapTransition && lhs.m_pageGap == rhs.m_pageGap
            && lhs.m_pageGapSet == rhs.m_pageGapSet
            && lhs.m_replacementIntent == rhs.m_replacementIntent;
    }
    friend bool operator!=(
        PresentationTargetTransitionPolicy lhs, PresentationTargetTransitionPolicy rhs)
    {
        return !(lhs == rhs);
    }

private:
    DisplayTransition m_displayTransition = DisplayTransition::RetainPrevious;
    FailureTransition m_failureTransition = FailureTransition::KeepFailedTarget;
    ZoomTransition m_zoomTransition = ZoomTransition::Preserve;
    ContentPositionTransition m_contentPositionTransition = ContentPositionTransition::Clamp;
    RotationTransition m_rotationTransition = RotationTransition::Preserve;
    MirrorTransition m_mirrorTransition = MirrorTransition::Preserve;
    FitModeTransition m_fitModeTransition = FitModeTransition::Preserve;
    ImageViewportFitMode m_fitMode = ImageViewportFitMode::Contain;
    bool m_fitModeSet = false;
    SpreadDirectionTransition m_spreadDirectionTransition = SpreadDirectionTransition::Preserve;
    ImageViewportSpreadDirection m_spreadDirection = ImageViewportSpreadDirection::LeftToRight;
    bool m_spreadDirectionSet = false;
    PageGapTransition m_pageGapTransition = PageGapTransition::Preserve;
    double m_pageGap = 0.0;
    bool m_pageGapSet = false;
    ReplacementIntent m_replacementIntent = ReplacementIntent::NewTarget;
};

Q_DECLARE_METATYPE(ImageViewportPresentationTarget)
Q_DECLARE_METATYPE(ImageViewportPresentationCommand)
Q_DECLARE_METATYPE(PresentationTargetTransitionPolicy)
