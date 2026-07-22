// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageactionavailabilitypolicy.h"

ImageActionAvailabilityProjection imageActionAvailabilityProjection(
    ImageActionAvailabilityInput input)
{
    const bool canUseReadyActions
        = input.imageReady && !input.fileDeletionInProgress && !input.helpDialogOpen;
    const bool rightToLeftReadingActive
        = input.rightToLeftReadingEnabled && input.rightToLeftReadingAvailable;
    const bool twoPageModeActive = input.twoPageModeEnabled && input.twoPageModeAvailable;
    const bool helpShortcutsEnabled = !input.helpDialogOpen;
    const bool viewerShortcutsEnabled = !input.textInputFocused && helpShortcutsEnabled;
    const bool readyShortcutsEnabled
        = input.imageReady && !input.fileDeletionInProgress && helpShortcutsEnabled;
    const bool readyViewerShortcutsEnabled
        = input.imageReady && !input.fileDeletionInProgress && viewerShortcutsEnabled;

    return {
        canUseReadyActions,
        canUseReadyActions && !twoPageModeActive,
        canUseReadyActions && input.twoPageModeAvailable,
        canUseReadyActions && input.rightToLeftReadingAvailable,
        rightToLeftReadingActive,
        twoPageModeActive,
        helpShortcutsEnabled,
        viewerShortcutsEnabled,
        readyShortcutsEnabled,
        readyViewerShortcutsEnabled,
        readyViewerShortcutsEnabled && twoPageModeActive,
        readyShortcutsEnabled && input.rightToLeftReadingAvailable,
        readyViewerShortcutsEnabled && input.rightToLeftReadingAvailable,
        readyShortcutsEnabled && !twoPageModeActive,
        readyViewerShortcutsEnabled && !twoPageModeActive,
        input.imagePannable && !input.fileDeletionInProgress && helpShortcutsEnabled,
        input.imagePannable && !input.fileDeletionInProgress && viewerShortcutsEnabled,
        input.containerNavigationAvailable && !input.fileDeletionInProgress && helpShortcutsEnabled,
        input.containerNavigationAvailable && !input.fileDeletionInProgress
            && viewerShortcutsEnabled,
    };
}

bool imageActionAvailabilityShortcutsEnabledForScope(
    const ImageActionAvailabilityProjection& projection,
    kiriview::ApplicationActions::ImageShortcutScope scope)
{
    if (!kiriview::ApplicationActions::imageShortcutScopeKnown(scope)) {
        return false;
    }

    using Scope = kiriview::ApplicationActions::ImageShortcutScope;
    switch (scope) {
    case Scope::HelpShortcutScope:
        return projection.helpShortcutsEnabled;
    case Scope::ViewerShortcutScope:
        return projection.viewerShortcutsEnabled;
    case Scope::ReadyShortcutScope:
        return projection.readyShortcutsEnabled;
    case Scope::ReadyViewerShortcutScope:
        return projection.readyViewerShortcutsEnabled;
    case Scope::RightToLeftReadingShortcutScope:
        return projection.rightToLeftReadingShortcutsEnabled;
    case Scope::RightToLeftReadingViewerShortcutScope:
        return projection.rightToLeftReadingViewerShortcutsEnabled;
    case Scope::RotateShortcutScope:
        return projection.rotateShortcutsEnabled;
    case Scope::RotateViewerShortcutScope:
        return projection.rotateViewerShortcutsEnabled;
    case Scope::PannableShortcutScope:
        return projection.pannableShortcutsEnabled;
    case Scope::PannableViewerShortcutScope:
    case Scope::MediaStartEndViewerShortcutScope:
        return projection.pannableViewerShortcutsEnabled;
    case Scope::ContainerShortcutScope:
        return projection.containerShortcutsEnabled;
    case Scope::ContainerViewerShortcutScope:
        return projection.containerViewerShortcutsEnabled;
    case Scope::ImageSelectionShortcutScope:
    case Scope::ImageSelectionViewerShortcutScope:
    case Scope::PageShortcutScope:
    case Scope::PageViewerShortcutScope:
    case Scope::ImageShortcutScopeCount:
        return false;
    }
    return false;
}

bool activeMediaShortcutsEnabledForScope(const ActiveMediaShortcutAvailabilityInput& input,
    kiriview::ApplicationActions::ImageShortcutScope scope)
{
    if (!kiriview::ApplicationActions::imageShortcutScopeKnown(scope)) {
        return false;
    }

    using Scope = kiriview::ApplicationActions::ImageShortcutScope;
    switch (scope) {
    case Scope::ImageSelectionShortcutScope:
    case Scope::PageShortcutScope:
        return input.activeNavigationActionsAvailable;
    case Scope::ImageSelectionViewerShortcutScope:
    case Scope::PageViewerShortcutScope:
        return input.activeNavigationActionsAvailable
            && input.imageProjection.viewerShortcutsEnabled;
    default:
        if (input.videoMode) {
            return kiriview::ApplicationActions::videoShortcutsEnabledForScope(
                { input.imageProjection.helpShortcutsEnabled,
                    input.imageProjection.viewerShortcutsEnabled, input.videoFileDeletionInProgress,
                    input.activeNavigationActionsAvailable },
                scope);
        }
        return imageActionAvailabilityShortcutsEnabledForScope(input.imageProjection, scope);
    }
}

namespace kiriview::ApplicationActions {
bool videoShortcutsEnabledForScope(VideoShortcutAvailabilityInput input, ImageShortcutScope scope)
{
    if (!imageShortcutScopeKnown(scope)) {
        return false;
    }

    const bool ready = input.helpShortcutsEnabled && !input.fileDeletionInProgress;
    const bool readyViewer = input.viewerShortcutsEnabled && !input.fileDeletionInProgress;
    const bool media = input.directMediaNavigationActive && ready;
    const bool mediaViewer = input.directMediaNavigationActive && readyViewer;
    switch (scope) {
    case ImageShortcutScope::HelpShortcutScope:
        return input.helpShortcutsEnabled;
    case ImageShortcutScope::ViewerShortcutScope:
        return input.viewerShortcutsEnabled;
    case ImageShortcutScope::ReadyShortcutScope:
    case ImageShortcutScope::RotateShortcutScope:
    case ImageShortcutScope::PannableShortcutScope:
    case ImageShortcutScope::ContainerShortcutScope:
    case ImageShortcutScope::RightToLeftReadingShortcutScope:
        return ready;
    case ImageShortcutScope::ReadyViewerShortcutScope:
    case ImageShortcutScope::RotateViewerShortcutScope:
    case ImageShortcutScope::PannableViewerShortcutScope:
    case ImageShortcutScope::MediaStartEndViewerShortcutScope:
    case ImageShortcutScope::ContainerViewerShortcutScope:
    case ImageShortcutScope::RightToLeftReadingViewerShortcutScope:
        return readyViewer;
    case ImageShortcutScope::ImageSelectionShortcutScope:
    case ImageShortcutScope::PageShortcutScope:
        return media;
    case ImageShortcutScope::ImageSelectionViewerShortcutScope:
    case ImageShortcutScope::PageViewerShortcutScope:
        return mediaViewer;
    case ImageShortcutScope::ImageShortcutScopeCount:
        return false;
    }
    return false;
}

bool mediaHorizontalArrowShortcutsEnabled(bool videoMode, bool imageReadyViewerShortcutsEnabled,
    VideoShortcutAvailabilityInput videoInput)
{
    if (!videoMode) {
        return imageReadyViewerShortcutsEnabled;
    }
    return videoInput.directMediaNavigationActive && !videoInput.fileDeletionInProgress
        && videoInput.viewerShortcutsEnabled;
}
}
