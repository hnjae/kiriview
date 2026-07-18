/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/imagesequence.h>
#include <ImageViewport/imageviewporttypes.h>

#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtGui/QColor>

struct ViewportEnginePresentationTarget
{
    ViewportEnginePresentationTarget() = default;
    ViewportEnginePresentationTarget(ImageSequence* primary)
        : primarySequence(primary)
    {
    }
    ViewportEnginePresentationTarget(ImageSequence* primary, ImageSequence* secondary)
        : primarySequence(primary)
        , secondarySequence(secondary)
    {
    }
    ViewportEnginePresentationTarget(ImageSequence* primary, ImageSequence* secondary, bool valid)
        : primarySequence(primary)
        , secondarySequence(secondary)
        , shapeValid(valid)
    {
    }

    template <typename Target>
    ViewportEnginePresentationTarget(const Target& target)
        : primarySequence(target.primary())
        , secondarySequence(target.secondary())
        , shapeValid(target.isValid())
    {
    }

    QPointer<ImageSequence> primarySequence;
    QPointer<ImageSequence> secondarySequence;
    bool shapeValid = true;

    static ViewportEnginePresentationTarget clear() { return {}; }
    ImageSequence* primary() const { return primarySequence; }
    ImageSequence* secondary() const { return secondarySequence; }
    bool isClear() const { return !primarySequence && !secondarySequence; }
    bool isValid() const { return shapeValid && (primarySequence || !secondarySequence); }

    friend bool operator==(
        const ViewportEnginePresentationTarget& lhs, const ViewportEnginePresentationTarget& rhs)
    {
        return lhs.primarySequence == rhs.primarySequence
            && lhs.secondarySequence == rhs.secondarySequence && lhs.shapeValid == rhs.shapeValid;
    }
    friend bool operator!=(
        const ViewportEnginePresentationTarget& lhs, const ViewportEnginePresentationTarget& rhs)
    {
        return !(lhs == rhs);
    }
};

struct ViewportEnginePresentationTargetTransitionPolicy
{
    enum class DisplayTransition {
        RetainPrevious,
        ClearBeforeLoad,
        Invalid,
    };
    enum class ZoomTransition {
        Preserve,
        ResetToContain,
        Invalid,
    };
    enum class ContentPositionTransition {
        Clamp,
        AnchorStart,
        AnchorEnd,
        Invalid,
    };
    enum class RotationTransition {
        Preserve,
        Reset,
        Invalid,
    };
    enum class MirrorTransition {
        Preserve,
        Reset,
        Invalid,
    };
    enum class FitModeTransition {
        Preserve,
        SetExplicit,
        Invalid,
    };
    enum class SpreadDirectionTransition {
        Preserve,
        SetExplicit,
        Invalid,
    };
    enum class PageGapTransition {
        Preserve,
        SetExplicit,
        Invalid,
    };
    enum class ReplacementIntent {
        NewTarget,
        SameTargetRefinement,
        Invalid,
    };

    ViewportEnginePresentationTargetTransitionPolicy() = default;

    template <typename Policy>
    ViewportEnginePresentationTargetTransitionPolicy(const Policy& policy)
    {
        displayTransitionValue = static_cast<DisplayTransition>(policy.displayTransition());
        zoomTransitionValue = static_cast<ZoomTransition>(policy.zoomTransition());
        contentPositionTransitionValue
            = static_cast<ContentPositionTransition>(policy.contentPositionTransition());
        rotationTransitionValue = static_cast<RotationTransition>(policy.rotationTransition());
        mirrorTransitionValue = static_cast<MirrorTransition>(policy.mirrorTransition());
        fitModeTransitionValue = static_cast<FitModeTransition>(policy.fitModeTransition());
        fitModeValue = policy.fitMode();
        fitModeSet = policy.hasExplicitFitMode();
        spreadDirectionTransitionValue
            = static_cast<SpreadDirectionTransition>(policy.spreadDirectionTransition());
        spreadDirectionValue = policy.spreadDirection();
        spreadDirectionSet = policy.hasExplicitSpreadDirection();
        pageGapTransitionValue = static_cast<PageGapTransition>(policy.pageGapTransition());
        pageGapValue = policy.pageGap();
        pageGapSet = policy.hasExplicitPageGap();
        replacementIntentValue = static_cast<ReplacementIntent>(policy.replacementIntent());
        shapeValid = policy.isValid();
    }

    DisplayTransition displayTransitionValue = DisplayTransition::RetainPrevious;
    ZoomTransition zoomTransitionValue = ZoomTransition::Preserve;
    ContentPositionTransition contentPositionTransitionValue = ContentPositionTransition::Clamp;
    RotationTransition rotationTransitionValue = RotationTransition::Preserve;
    MirrorTransition mirrorTransitionValue = MirrorTransition::Preserve;
    FitModeTransition fitModeTransitionValue = FitModeTransition::Preserve;
    ImageViewportFitMode fitModeValue = ImageViewportFitMode::Contain;
    bool fitModeSet = false;
    SpreadDirectionTransition spreadDirectionTransitionValue = SpreadDirectionTransition::Preserve;
    ImageViewportSpreadDirection spreadDirectionValue = ImageViewportSpreadDirection::LeftToRight;
    bool spreadDirectionSet = false;
    PageGapTransition pageGapTransitionValue = PageGapTransition::Preserve;
    double pageGapValue = 0.0;
    bool pageGapSet = false;
    ReplacementIntent replacementIntentValue = ReplacementIntent::NewTarget;
    bool shapeValid = true;

    DisplayTransition displayTransition() const { return displayTransitionValue; }
    ZoomTransition zoomTransition() const { return zoomTransitionValue; }
    ContentPositionTransition contentPositionTransition() const
    {
        return contentPositionTransitionValue;
    }
    RotationTransition rotationTransition() const { return rotationTransitionValue; }
    MirrorTransition mirrorTransition() const { return mirrorTransitionValue; }
    FitModeTransition fitModeTransition() const { return fitModeTransitionValue; }
    ImageViewportFitMode fitMode() const { return fitModeValue; }
    SpreadDirectionTransition spreadDirectionTransition() const
    {
        return spreadDirectionTransitionValue;
    }
    ImageViewportSpreadDirection spreadDirection() const { return spreadDirectionValue; }
    PageGapTransition pageGapTransition() const { return pageGapTransitionValue; }
    double pageGap() const { return pageGapValue; }
    ReplacementIntent replacementIntent() const { return replacementIntentValue; }
    bool hasExplicitFitMode() const { return fitModeSet; }
    bool hasExplicitSpreadDirection() const { return spreadDirectionSet; }
    bool hasExplicitPageGap() const { return pageGapSet; }

    bool isValid() const
    {
        return shapeValid && displayTransitionValue != DisplayTransition::Invalid
            && zoomTransitionValue != ZoomTransition::Invalid
            && contentPositionTransitionValue != ContentPositionTransition::Invalid
            && rotationTransitionValue != RotationTransition::Invalid
            && mirrorTransitionValue != MirrorTransition::Invalid
            && fitModeTransitionValue != FitModeTransition::Invalid
            && spreadDirectionTransitionValue != SpreadDirectionTransition::Invalid
            && pageGapTransitionValue != PageGapTransition::Invalid
            && replacementIntentValue != ReplacementIntent::Invalid;
    }
};

struct ViewportEnginePresentationCommand
{
    ViewportEnginePresentationCommand() = default;

    template <typename Command> ViewportEnginePresentationCommand(const Command& command)
    {
        resetViewValue = command.resetView();
        fitModeSet = command.hasFitMode();
        fitModeValue = command.fitMode();
        manualZoomPercentSet = command.hasManualZoomPercent();
        manualZoomPercentValue = command.manualZoomPercent();
        zoomStepDeltaSet = command.hasZoomStepDelta();
        zoomStepDeltaValue = command.zoomStepDelta();
        contentPositionSet = command.hasContentPosition();
        contentPositionValue = command.contentPosition();
        panDeltaSet = command.hasPanDelta();
        panDeltaValue = command.panDelta();
        contentAnchorSet = command.hasContentAnchor();
        contentAnchorValue = command.contentAnchor();
        rotationDegreesSet = command.hasRotationDegrees();
        rotationDegreesValue = command.rotationDegrees();
        mirrorHorizontallySet = command.hasMirrorHorizontally();
        mirrorHorizontallyValue = command.mirrorHorizontally();
        mirrorVerticallySet = command.hasMirrorVertically();
        mirrorVerticallyValue = command.mirrorVertically();
        spreadDirectionSet = command.hasSpreadDirection();
        spreadDirectionValue = command.spreadDirection();
        pageGapSet = command.hasPageGap();
        pageGapValue = command.pageGap();
        backgroundModeSet = command.hasBackgroundMode();
        backgroundModeValue = command.backgroundMode();
        backgroundColorSet = command.hasBackgroundColor();
        backgroundColorValue = command.backgroundColor();
        checkerboardLightColorSet = command.hasCheckerboardLightColor();
        checkerboardLightColorValue = command.checkerboardLightColor();
        checkerboardDarkColorSet = command.hasCheckerboardDarkColor();
        checkerboardDarkColorValue = command.checkerboardDarkColor();
        checkerboardCellSizeSet = command.hasCheckerboardCellSize();
        checkerboardCellSizeValue = command.checkerboardCellSize();
        smoothingSet = command.hasSmoothing();
        smoothingValue = command.smoothing();
        mipmapSet = command.hasMipmap();
        mipmapValue = command.mipmap();
        loopingSet = command.hasLooping();
        loopingValue = command.looping();
        qualityPreferenceSet = command.hasQualityPreference();
        qualityPreferenceValue = command.qualityPreference();
        exactnessPreferenceSet = command.hasExactnessPreference();
        exactnessPreferenceValue = command.exactnessPreference();
    }

    bool resetViewValue = false;
    bool fitModeSet = false;
    ImageViewportFitMode fitModeValue = ImageViewportFitMode::Contain;
    bool manualZoomPercentSet = false;
    double manualZoomPercentValue = 100.0;
    bool zoomStepDeltaSet = false;
    int zoomStepDeltaValue = 0;
    bool contentPositionSet = false;
    QPointF contentPositionValue;
    bool panDeltaSet = false;
    QPointF panDeltaValue;
    bool contentAnchorSet = false;
    ImageViewportContentAnchor contentAnchorValue = ImageViewportContentAnchor::Start;
    bool rotationDegreesSet = false;
    int rotationDegreesValue = 0;
    bool mirrorHorizontallySet = false;
    bool mirrorHorizontallyValue = false;
    bool mirrorVerticallySet = false;
    bool mirrorVerticallyValue = false;
    bool spreadDirectionSet = false;
    ImageViewportSpreadDirection spreadDirectionValue = ImageViewportSpreadDirection::LeftToRight;
    bool pageGapSet = false;
    double pageGapValue = 0.0;
    bool backgroundModeSet = false;
    ImageViewportBackgroundMode backgroundModeValue = ImageViewportBackgroundMode::Transparent;
    bool backgroundColorSet = false;
    QColor backgroundColorValue = Qt::white;
    bool checkerboardLightColorSet = false;
    QColor checkerboardLightColorValue = Qt::white;
    bool checkerboardDarkColorSet = false;
    QColor checkerboardDarkColorValue = QColor(220, 220, 220);
    bool checkerboardCellSizeSet = false;
    double checkerboardCellSizeValue = 8.0;
    bool smoothingSet = false;
    bool smoothingValue = true;
    bool mipmapSet = false;
    bool mipmapValue = false;
    bool loopingSet = false;
    bool loopingValue = false;
    bool qualityPreferenceSet = false;
    ImageViewportQualityPreference qualityPreferenceValue = ImageViewportQualityPreference::Default;
    bool exactnessPreferenceSet = false;
    ImageViewportExactnessPreference exactnessPreferenceValue
        = ImageViewportExactnessPreference::Default;

    bool resetView() const { return resetViewValue; }
    bool hasFitMode() const { return fitModeSet; }
    ImageViewportFitMode fitMode() const { return fitModeValue; }
    bool hasManualZoomPercent() const { return manualZoomPercentSet; }
    double manualZoomPercent() const { return manualZoomPercentValue; }
    bool hasZoomStepDelta() const { return zoomStepDeltaSet; }
    int zoomStepDelta() const { return zoomStepDeltaValue; }
    bool hasContentPosition() const { return contentPositionSet; }
    QPointF contentPosition() const { return contentPositionValue; }
    bool hasPanDelta() const { return panDeltaSet; }
    QPointF panDelta() const { return panDeltaValue; }
    bool hasContentAnchor() const { return contentAnchorSet; }
    ImageViewportContentAnchor contentAnchor() const { return contentAnchorValue; }
    bool hasRotationDegrees() const { return rotationDegreesSet; }
    int rotationDegrees() const { return rotationDegreesValue; }
    bool hasMirrorHorizontally() const { return mirrorHorizontallySet; }
    bool mirrorHorizontally() const { return mirrorHorizontallyValue; }
    bool hasMirrorVertically() const { return mirrorVerticallySet; }
    bool mirrorVertically() const { return mirrorVerticallyValue; }
    bool hasSpreadDirection() const { return spreadDirectionSet; }
    ImageViewportSpreadDirection spreadDirection() const { return spreadDirectionValue; }
    bool hasPageGap() const { return pageGapSet; }
    double pageGap() const { return pageGapValue; }
    bool hasBackgroundMode() const { return backgroundModeSet; }
    ImageViewportBackgroundMode backgroundMode() const { return backgroundModeValue; }
    bool hasBackgroundColor() const { return backgroundColorSet; }
    QColor backgroundColor() const { return backgroundColorValue; }
    bool hasCheckerboardLightColor() const { return checkerboardLightColorSet; }
    QColor checkerboardLightColor() const { return checkerboardLightColorValue; }
    bool hasCheckerboardDarkColor() const { return checkerboardDarkColorSet; }
    QColor checkerboardDarkColor() const { return checkerboardDarkColorValue; }
    bool hasCheckerboardCellSize() const { return checkerboardCellSizeSet; }
    double checkerboardCellSize() const { return checkerboardCellSizeValue; }
    bool hasSmoothing() const { return smoothingSet; }
    bool smoothing() const { return smoothingValue; }
    bool hasMipmap() const { return mipmapSet; }
    bool mipmap() const { return mipmapValue; }
    bool hasLooping() const { return loopingSet; }
    bool looping() const { return loopingValue; }
    bool hasQualityPreference() const { return qualityPreferenceSet; }
    ImageViewportQualityPreference qualityPreference() const { return qualityPreferenceValue; }
    bool hasExactnessPreference() const { return exactnessPreferenceSet; }
    ImageViewportExactnessPreference exactnessPreference() const
    {
        return exactnessPreferenceValue;
    }
};
