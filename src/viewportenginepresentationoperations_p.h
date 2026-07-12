#pragma once

#include "viewportenginecontracts_p.h"
#include "viewportenginestate_p.h"

#include <optional>

struct ViewportEnginePresentationCommandInput
{
    ImageViewportPresentationCommand command;
    ViewportEngineGeometryInput geometry;
    QPointF anchor;
    int quarterTurnDelta = 0;
};

struct ViewportEnginePresentationTargetTransitionInput
{
    PresentationTargetTransitionPolicy::ZoomTransition zoomTransition
        = PresentationTargetTransitionPolicy::ZoomTransition::Preserve;
    PresentationTargetTransitionPolicy::ContentPositionTransition contentPositionTransition
        = PresentationTargetTransitionPolicy::ContentPositionTransition::Clamp;
    PresentationTargetTransitionPolicy::RotationTransition rotationTransition
        = PresentationTargetTransitionPolicy::RotationTransition::Preserve;
    PresentationTargetTransitionPolicy::MirrorTransition mirrorTransition
        = PresentationTargetTransitionPolicy::MirrorTransition::Preserve;
    std::optional<ImageViewport::FitMode> explicitFitMode;
    std::optional<ImageViewport::SpreadDirection> explicitSpreadDirection;
    std::optional<double> explicitPageGap;
    ViewportEngineGeometryInput acceptedGeometry;
    QPointF previousContentPosition;
    double previousZoomPercent = 100.0;
    bool readyDisplay = false;
};

class ViewportEnginePresentationCommandStateView
{
    friend class ViewportEngine;
    ViewportEnginePresentationCommandStateView(
        const ImageViewportInternal::PresentationState& presentation, bool looping,
        bool readyDisplay)
        : m_presentation(presentation)
        , m_looping(looping)
        , m_readyDisplay(readyDisplay)
    {
    }

public:
    ViewportEnginePresentationCommandStateView(const ViewportEnginePresentationCommandStateView&)
        = delete;
    ViewportEnginePresentationCommandStateView(
        ViewportEnginePresentationCommandStateView&&) noexcept
        = default;
    ViewportEnginePresentationCommandStateView& operator=(
        const ViewportEnginePresentationCommandStateView&)
        = delete;

    const ImageViewportInternal::PresentationState& presentation() const { return m_presentation; }
    bool looping() const { return m_looping; }
    bool readyDisplay() const { return m_readyDisplay; }

private:
    const ImageViewportInternal::PresentationState& m_presentation;
    bool m_looping = false;
    bool m_readyDisplay = false;
};

class ViewportEnginePresentationTargetTransitionStateView
{
    friend class ViewportEngine;
    friend class ViewportEnginePresentationTargetAssignmentAccess;
    explicit ViewportEnginePresentationTargetTransitionStateView(
        const ImageViewportInternal::PresentationState& presentation)
        : m_presentation(presentation)
    {
    }

public:
    ViewportEnginePresentationTargetTransitionStateView(
        const ViewportEnginePresentationTargetTransitionStateView&)
        = delete;
    ViewportEnginePresentationTargetTransitionStateView(
        ViewportEnginePresentationTargetTransitionStateView&&) noexcept
        = default;
    ViewportEnginePresentationTargetTransitionStateView& operator=(
        const ViewportEnginePresentationTargetTransitionStateView&)
        = delete;

    const ImageViewportInternal::PresentationState& presentation() const { return m_presentation; }

private:
    const ImageViewportInternal::PresentationState& m_presentation;
};

struct ViewportEnginePresentationCommandReduction
{
    std::optional<ImageViewportInternal::PresentationState> presentation;
    std::optional<bool> looping;
    ImageViewportInternal::ViewportChangeSet changes;
    bool restageProviderDemands = false;
};

struct ViewportEnginePresentationTargetTransitionReduction
{
    std::optional<ImageViewportInternal::PresentationState> presentation;
    ImageViewportInternal::ViewportChangeSet changes;
};

bool validateViewportEnginePresentationCommand(const ViewportEnginePresentationCommandInput&);
ViewportEnginePresentationCommandReduction reduceViewportEnginePresentationCommand(
    ViewportEnginePresentationCommandInput, ViewportEnginePresentationCommandStateView);
ViewportEnginePresentationTargetTransitionReduction
    reduceViewportEnginePresentationTargetTransition(
        ViewportEnginePresentationTargetTransitionInput,
        ViewportEnginePresentationTargetTransitionStateView);
