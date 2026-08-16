# Image Display

## Loading and Replacement

The UI remains responsive while a selected image is being opened.

When an image target is selected, KiriView accepts that selected target immediately. A previous complete image may remain visible as a non-interactive fallback while the matching replacement presentation is pending.

If the matching replacement presentation becomes display-ready within 150 milliseconds, it replaces the fallback without an intervening blank viewport or loading state. Otherwise a loading surface covers the image viewport until the target is display-ready. Loading feedback does not change the presentation of toolbar controls.

While the selected image target is loading or showing its target-specific error, the window title identifies that selected file or collection. Collection-specific toolbar controls remain placed when the selected target remains in that collection, but controls that require a ready displayed image stay non-interactive until the matching target is ready. While a previous complete image remains as the replacement fallback, valid toolbar controls retain their last complete visual appearance for that fallback's entire lifetime, including while loading feedback is visible, and transition directly to the matching target's presentation when it becomes ready. If no complete fallback exists or the fallback is discarded without a ready replacement, those controls show their current unavailable appearance.

If no media item is selected, the empty state says that no file is selected and offers Open.

If the selected image cannot be opened while no media item is displayed, the error state explains that the selected file or URL could not be opened, shows a stable localized load error when available, and offers Open. Backend-authored diagnostic text is not user-visible. Initial video failures use the stable video error behavior defined in [Video Playback](video-playback.md#source-url-identity) and also offer Open.

If a media item is already displayed and users select a different image, the requested image becomes the selected target immediately, whether or not it belongs to the current active navigation scope. Any retained fallback pixels continue to identify the previous committed presentation and are not treated as a ready presentation of the selected target.

If a media item is already displayed and users select a video, KiriView leaves image mode immediately and shows the video loading state.

If another file or navigation target is selected before the previous load finishes, only the most recent selection is displayed.

When a different image target is selected while an image is already displayed, any running animation stops immediately even when its last complete frame remains temporarily visible as the fallback.

If the selected URL cannot be read or the file is not a decodable image or playable video, KiriView keeps the selected target active and shows the target's error state instead of restoring the previous media item.

If animation playback fails for the displayed image, the UI shows an error state and remains ready for another open action.

## Rendering

### Static Image Rendering

Opened images are displayed centered in the available page area while preserving their aspect ratio.

When KiriView is started with a direct image source, the initial image appears in the main viewport once it is display-ready. The main viewport does not require Thumbnail Panel visibility, Info Panel visibility, or another layout side effect before showing that accepted image.

Image zoom is expressed in physical display pixels. At 100%, one image pixel maps to one physical monitor pixel.

Static image files, including bitmap images and SVG files, appear at full resolution when they are small enough to display directly.

When matching current-detail output for adjacent images is already available, Previous and Next navigation can replace the view immediately.

If a static image exceeds the supported decode or display size, KiriView reports an error or unsupported state for the selected target instead of restoring a previously displayed image.

### Preview And Refinement

Large static images may first appear as a lower-detail preview and then become sharper when matching current-detail output is ready.

KiriView may display source-derived provisional pixels while the selected image is still loading only when no complete authoritative image is retained as its replacement fallback. When a complete authoritative display is retained, it remains the sole visual fallback and provisional pixels for the replacement are not displayed. Provisional pixels do not make the selected image ready or enable readiness-dependent controls, and they are replaced automatically when authoritative decoded output becomes available without requiring zoom, resize, or another user action.

If authoritative decoding fails after provisional pixels were shown, KiriView removes those pixels and shows the selected target's error state instead of treating the preview as a successfully opened image.

KiriView prefers authoritative output with enough detail for the current physical display size. When that output is already available, including from preparation performed before navigation, it appears directly without a lower-detail predecessor.

When available authoritative output has less detail than the current display requires, KiriView prepares matching current-detail output before making the selected image ready when it can do so promptly.

If matching current-detail output is not available promptly, a valid lower-detail authoritative image may make the selected image ready as a latency fallback. The visible result then becomes sharper automatically when matching detail is available; no zoom, resize, or other presentation change is required.

While sharper detail is being prepared for the same image, KiriView may keep the current accepted image visible. Zooming, panning, resizing, rotation, and display scale changes must not expose blank regions while a replacement for the same image is pending.

When a sharper replacement becomes available for the current image, it replaces the previous lower-detail image without changing the user's selected media target, zoom mode, or pan position except where the existing viewport rules require clamping.

### SVG Safety And Detail

For SVG files, 100% uses the SVG's intrinsic size. SVGs remain sharp instead of pixelated when Fit mode, manual zoom, window resizing, or display scale changes the displayed size.

Static SVG rendering applies ordinary static SVG features such as clip paths. KiriView does not execute SVG scripts, play SVG animation, or load external network, file, or embedded data resources referenced from SVG content.

SVG preview images are placeholders. KiriView may prepare an SVG preview capped to the current physical viewport size so adjacent SVG images can appear immediately. That preview may be visible while current-detail rendering is missing, but stale low-resolution SVG output does not substitute for the current-detail image after zoom, viewport, rotation, pan position, or device-pixel-ratio changes.

After zooming far out and then back in, SVG display eventually returns to current-detail rendering. Stale lower-detail SVG output does not remain visible once KiriView can provide the current-detail image.

### HEIF And RAW

HEIF-family still images, including AVIF still images, are supported when the still image is encoded with AV1, HEVC, AVC/H.264, JPEG, JPEG 2000, or VVC/H.266.

If a recognized HEIF-family still image uses an unsupported compression format or cannot be decoded, KiriView reports the decode error for the selected target instead of restoring a previously displayed image.

Camera RAW files open as static images. KiriView displays supported RAW files as display-ready 8-bit sRGB images and does not expose RAW editing controls such as demosaic, white-balance, tone-curve, or embedded-preview selection.

TIFF-family RAW files such as DNG files follow RAW behavior, while ordinary TIFF files follow general raster-image behavior.

RAW files participate in the same open, adjacent navigation, archive, and directory workflows as other supported static image files.

### Viewer Surface And Readiness

The image viewing area behind empty, loading, ready, and error states uses a dark background color derived from the KDE/Kirigami View color scheme. Dark color schemes use the View background color; light color schemes use the View text color so the image viewing area remains dark.

Navigation transitions do not flash to a different page color.

The toolbar keeps its normal application styling.

When an image is ready, the image viewing viewport does not add page padding around the image area.

Readiness-dependent controls, overlays, panning affordances, zoom controls, and image-only actions all use the same current media readiness state. Except for the non-interactive toolbar appearance retained while a previous complete image remains the replacement fallback, empty, loading, pending-navigation, replacement, error, video, and unsupported-placeholder intervals must not expose stale ready-image affordances from the previously displayed image.

## Fit and Zoom State

KiriView starts in Fit mode.

Fit mode scales the image as large as possible while keeping the full image visible in the viewport, including upscaling small images when space is available.

Fit mode's displayed image is interpreted by the active viewport geometry frame. If a fitted axis is equal to the viewport within KiriView's geometry tolerance, that axis is not pannable and does not show a scrollbar.

Fit Height mode scales the image height to the viewport height while preserving aspect ratio.

Fit Width mode scales the image width to the viewport width while preserving aspect ratio.

Within the same directly opened archive or directory collection, KiriView preserves the current zoom state while users move between pages with Previous, Next, or page number navigation.

Switching between single-page display and Two-Page Spread preserves the user's active fit mode or preferred manual zoom. A target-specific maximum may temporarily lower the effective manual zoom, but changing presentation shape does not replace the preferred percentage with that temporary value.

If the user has selected Fit, Fit Height, or Fit Width, that fit mode remains selected and recalculates for each page, viewport size, and rotation change.

If the user has entered a manual zoom value, that exact percentage remains the preferred manual zoom while users navigate within the collection or switch between single-page and Two-Page Spread display. If the current image or spread has a lower dynamic maximum, KiriView temporarily uses that maximum as the effective zoom and returns to the preferred percentage when a later image or spread permits it.

When the displayed page changes inside the archive or directory collection through ordinary page navigation, any panning position from the previous page is cleared. The newly displayed page starts at its scan start at the preserved zoom level: top-left normally and top-right in Right-to-Left Reading mode.

The scan-backward shortcut may open the previous image at its final scan position instead: bottom-right normally and bottom-left in Right-to-Left Reading mode.

Starting KiriView, opening an ordinary direct image, moving between ordinary direct images in a direct media URL scope, opening a KDE archive URL image directly, opening a different archive or directory collection, or moving to a sibling archive resets zoom to Fit mode when the new image is displayed.

## Rotation

When an image is ready and Two-Page Spread is disabled, Rotate Clockwise turns the current image view by 90 degrees clockwise and Rotate Counterclockwise turns it by 90 degrees counterclockwise.

Successive rotation commands accumulate immediately in quarter turns and wrap after a full turn: four clockwise or four counterclockwise commands return the view to 0 degrees. A same-image detail replacement pending for an earlier rotation does not block a newer rotation command or revert its result.

Rotation is display-only. It does not modify image files, metadata, archive contents, remote URLs, or any saved file state.

Fit, Fit Height, Fit Width, manual zoom, scrollbars, drag panning, wheel zoom, keyboard panning, and scan shortcuts use the rotated logical image bounds.

`R` rotates clockwise, and `Shift+R` rotates counterclockwise.

Rotation resets to 0 degrees whenever a different image or page is displayed, and it also resets when the displayed image is cleared.

Rotation is unavailable while Two-Page Spread is enabled. Enabling Two-Page Spread resets rotation to 0 degrees.

## Flipping

When an image is ready and Two-Page Spread is disabled, Flip Horizontally reflects the current image view across the vertical item axis and Flip Vertically reflects it across the horizontal item axis. These axes remain the visible viewer axes after rotation: horizontal always exchanges the displayed left and right sides, and vertical always exchanges the displayed top and bottom sides.

Each flip command toggles its axis independently. Applying the same flip twice returns that axis to its original state, and horizontal flip, vertical flip, and rotation may be combined. Successive flip commands take effect immediately; a same-image detail replacement pending for an earlier flip does not block a newer flip command or revert its result.

Flipping is display-only. It does not modify image files, metadata, archive contents, remote URLs, or any saved file state.

Flip Horizontally and Flip Vertically have configurable viewer-local shortcut slots with no default key bindings.

Both flip axes reset whenever a different image or page is displayed or the displayed image is cleared. Flipping is unavailable while Two-Page Spread is enabled, and enabling Two-Page Spread clears both flip axes.

## Zoom Controls

The toolbar provides a fixed-width zoom percentage input with a separate percent suffix. It shows values below 10,000% as a rounded integer percentage without digit grouping, capped at `9999 %`. It shows values from 10,000% through 999,999% in thousands using `k`, adds `+` when the actual value is above the displayed thousand bucket, and shows values at or above 1,000,000% as `999k+ %`. The editable value text is right-aligned, uses a fixed-width font, and excludes `%`; the adjacent percent suffix provides `%` after a one-space visual gap and keeps toolbar spacing before the stepper buttons.

Ctrl+Y focuses the editable toolbar zoom-percentage entry and selects its complete numeric value. In fullscreen, KiriView reveals the toolbar first. When responsive toolbar presentation replaces the inline entry with a compact or overflow control, KiriView exposes that control before focusing it. The shortcut is unavailable when the zoom readout is not editable.

When neither a ready image nor a playable video has an active zoom readout, the toolbar zoom control displays `- %`. An image zoom readout exists only while a ready image with a displayed image size is active. Empty, loading, error, and unsupported-video placeholder states must not expose fallback image-geometry zoom values through the toolbar.

When a playable direct or opened-collection video is displayed, the toolbar zoom control remains in the same position as image mode and becomes read-only. It displays the fitted video zoom percentage when KiriView can determine the intrinsic video frame size and current displayed content size, and `? %` when the percentage is unavailable.

When an image is ready, users can enter manual zoom values from 10% through a dynamic maximum that keeps the displayed image within the application's supported display size.

The maximum manual zoom is never lower than the normal Fit zoom percentage.

Pressing Enter or clicking the image viewing area while editing the zoom input applies the nearest valid manual zoom value, returns focus to the image viewing area, and restores viewer keyboard shortcuts.

Pressing Escape while editing the zoom input cancels the edit, restores the currently applied zoom percentage in the toolbar, returns focus to the image viewing area, and does not leave fullscreen.

If the submitted zoom text cannot be parsed as a percentage, KiriView restores the currently applied zoom percentage, leaves the current zoom mode unchanged, and ends the edit.

Submitting a valid zoom value switches to manual zoom.

When an image is ready and the zoom input is not being edited, unmodified wheel or trackpad scrolling over the toolbar zoom percentage control switches to manual zoom and adjusts by half the normal multiplicative zoom step. One wheel detent zooms in by multiplying the current zoom by `2^(1/16)`, and one wheel detent zooms out by multiplying by `2^(-1/16)`. Toolbar wheel zoom uses the same dynamic manual zoom range as the toolbar zoom input and has no effect for read-only zoom readouts such as playable video.

The toolbar fit control is a menu button that starts with Fit to Window selected. The button displays the currently selected fit mode's icon and label and opens a menu when clicked; clicking the button itself does not apply a fit mode. The menu offers Fit to Window, Fit Width, and Fit Height; selecting a menu item immediately applies that fit mode and updates the button's icon, label, and tooltip. Manual zoom does not replace the selected fit mode displayed by the button.

When an image is ready, `=` or `+` zooms in by multiplying the current zoom by `2^(1/8)`, and `-` zooms out by multiplying the current zoom by `2^(-1/8)`.

Keyboard zoom uses the same dynamic manual zoom range as the toolbar zoom input.

Holding Ctrl and using the mouse wheel, or holding the right mouse button and using the mouse wheel, over the image viewport zooms around the cursor when the cursor is over the image, or around the nearest displayed image point when the cursor is in the viewport outside the image. Wheel up zooms in by the same multiplicative step and wheel down zooms out by its reciprocal.

Double-clicking the image viewport toggles between Fit mode and 100% manual zoom. If the current zoom mode is Fit, double-clicking switches to 100% manual zoom around the clicked viewport point when it is over the image, or around the nearest displayed image point when the click is inside the viewport outside the image. If the current zoom mode is Manual, Fit Height, or Fit Width, double-clicking switches to Fit mode.

When an image is ready, `` ` `` switches to 50% manual zoom, `1` switches to 100% manual zoom, `2` switches to 200% manual zoom, `8` selects Fit Height mode, `9` selects Fit Width mode, and `0` selects Fit to Window mode.

Viewport zoom, fit, panning, and scan results follow the currently visible image or spread. Delayed resize, display-scale, or gesture updates must not visibly revert newer zoom, pan, or rotation choices.

During active gestures, KiriView may update the immediate visual position before settling the final position. Settling must preserve the user's newest visible zoom, pan, rotation, resize, and display-scale result.

Switching between single-page display and Two-Page Spread clears the previous presentation immediately and is atomic from the user's perspective. During the transition, controls, zoom readout, panning availability, and render output must not combine properties from the previous and requested presentations. If the transition cannot be completed, the requested presentation remains selected and KiriView shows its error state instead of restoring the previous presentation.

## Animation

Animated image files, including GIF, APNG, animated WebP, animated JPEG XL, and HEIF-family image sequences such as `.heics` and `.avifs`, play when animation frames are available.

When KiriView can identify more than one authored frame in a supported animated image file, it presents the file as an animation instead of freezing it as a static still image.

The first frame is shown once loading succeeds. Later frames use the file's frame delays and loop count.

When both pages in a Two-Page Spread are animated, each page advances independently according to its own frame delays and loop count. One page finishing, waiting, pausing, or stopping does not freeze the other page.

Infinite loops continue until another image is selected or the view is cleared.

APNG animations may show the first displayable frame before all later frames are ready, so opening an APNG does not require the complete animation to be ready before the image becomes visible.

APNG animations and HEIF-family image sequences continue to use full-frame playback and play as authored.
