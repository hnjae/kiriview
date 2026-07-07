# Panels

## Info Panel Content

KiriView provides an Info Panel with user-visible media information for the current media state.

The Info Panel header shows an information icon, the title `Information`, and a close button that hides the panel.

When no media item is available, the Info Panel shows an unavailable state rather than stale media details.

When a media item is available, the Info Panel shows the current file name, a summary line, General and media-specific metadata sections, a Camera section only when camera metadata rows are available, and an Advanced Metadata section only when additional parsed metadata rows are available. File names and displayed paths decode percent-encoded URL path text for user readability. The current file name, summary line, and metadata row values are rendered in a fixed-width font.

## Info Panel Metadata

The General section shows available non-placeholder file identity rows such as type and path.

For media items displayed inside an opened archive or local directory collection, the General section path is the item path relative to the collection root.

The Image section uses the current image dimensions when they are known and omits unavailable rows.

The Video section uses the current video frame dimensions when they are known. It may also show embedded video metadata such as duration and frame size when available, and omits unavailable rows.

For images, embedded metadata is parsed from the same media content used for image display, so direct files, directory collections, and archive collections expose metadata consistently with the displayed image source.

For direct videos, the Info Panel may show embedded video metadata when available while preserving the original direct media URL as the displayed source identity. Collection-internal video metadata is not shown for playable collection videos or unsupported-video placeholders.

Playable collection videos and unsupported-video placeholders keep the current media item's video identity in the Info Panel. Their General section type is Video and their media-specific section is Video, but collection-internal video metadata rows are omitted.

The Camera section shows curated embedded metadata rows only when the values are available: Camera, Taken, Location, Lens, Exposure, ISO, Focal Length, and Software. Camera combines make and model when both exist. Location is shown as coordinates only.

The Advanced Metadata section is collapsed by default and contains parsed embedded tags not already consumed by curated rows, excluding empty, binary, and unprintable values.

## Info Panel Actions And Layout

The Info Panel provides icon buttons to copy the current file path and open the containing folder when those operations have a valid target for the current media item.

The Info Panel content is vertically scrollable and keeps labels and values on one elided line per row.

On wide windows, the Info Panel is an inline layout-reserving right-side panel that spans the full content height below the normal toolbar outside fullscreen, or the full window content height in fullscreen.

On narrow windows, the Info Panel is a right-edge overlay drawer that does not shrink the media viewport.

The Info Panel uses the same width bounds in inline and overlay modes: minimum 16 Kirigami grid units, preferred 18 Kirigami grid units, and maximum 20 Kirigami grid units.

## Thumbnail Panel Content

The Thumbnail Panel is a compact, layout-reserving bottom filmstrip in the remaining media area to the left of the Info Panel. It uses the same dark viewer surface and matching foreground colors as the media viewport rather than a light page-panel surface.

The Thumbnail Panel shows a horizontal, scrollable active-navigation strip when the active navigation list is known. Each strip item renders according to the thumbnail eligibility and fallback behavior defined in [Navigation](navigation.md#page-controls). The candidate name is rendered in a fixed-width font on one elided line. The horizontal scrollbar occupies a dedicated lane below the strip items and must not overlap or obscure candidate names.

The Thumbnail Panel has a subtle top separator using the viewer foreground color at reduced opacity. Strip items use compact spacing, a small corner radius, and a subtle hover fill without shadow, glow, or card treatment. The selected strip item is indicated with a 2-pixel border using the theme highlight color.

The Thumbnail Panel uses image and video icons to distinguish supported still images from supported videos.

The number of visible strip items matches the active navigation total count. When active navigation is unavailable or unknown, the strip is empty.

## Thumbnail Panel Selection And Scrolling

The Thumbnail Panel follows the active-navigation thumbnail selection, activation, and scrolling behavior defined in [Navigation](navigation.md#thumbnail-strip-scrolling).

The Thumbnail Panel does not remap vertical mouse-wheel events to horizontal movement.

## Panel Resizing, Shortcuts, And State

When both panels are visible, the Info Panel occupies the right side for the full content height, and the Thumbnail Panel occupies only the bottom of the media area that remains to its left.

The panels are resizable with splitters. The Thumbnail Panel minimum height is tall enough to show the media-type icon, one-line candidate name, and dedicated horizontal scrollbar lane without clipping. Its default resizable height range is compact, roughly 6 to 7.5 Kirigami grid units.

I toggles the Info Panel in viewer context, and T toggles the Thumbnail Panel in viewer context.

The panel toggle shortcuts are user-configurable application action shortcuts, not fixed shortcuts.

The panel toggle actions are available from the application menu, menubar, Keyboard Shortcuts configuration, and Keyboard Shortcuts help.

The panels are closed by default. Panel open state and splitter sizes are runtime-only and are not remembered across launches.

The panels remain available in fullscreen.

The overlay Info Panel closes when the user presses Escape, clicks outside the drawer, or activates its close button.
