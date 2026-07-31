# Video Playback

## Product Boundary

KiriView supports the direct video files and eligible video entries inside directly opened collections defined in [File Access](file-access.md#supported-sources) as moving, sound-capable image-like media items.

Supported direct video inputs may come from the startup argument, drop, open dialog selection, or ordinary adjacent direct media navigation from a direct media URL scope.

Direct KDE archive-entry URLs remain direct media URLs, while videos reached through a directly opened archive or directory remain collection navigation items. Eligibility and unsupported-placeholder behavior are defined in [File Access](file-access.md#supported-sources).

Video files do not support editable image zoom, image pan, image rotation, or two-page spread pairing. Moving through a video does not prevent nearby still images from remaining available for quick image navigation.

For opened collection videos, collection-internal video metadata, playlists, subtitles, track selection, frame stepping, and timeline preview thumbnails are not provided.

## Source URL Identity

Direct video source identity follows [Direct Media Source Identity](file-access.md#direct-media-source-identity).

When playback preparation fails, the user-facing error text is a stable KiriView message that the selected video could not be opened. Platform and playback diagnostic messages are not displayed as the primary error text.

Direct video embedded metadata may populate the Info Panel when available while keeping the original direct media URL as the user-facing source identity.

Collection-internal video metadata is not parsed for the Info Panel.

## Direct Media Scope

Video mode does not create a video-only navigation scope. It uses the active scope, ordering, non-wrapping behavior, and boundary feedback defined in [Navigation](navigation.md#active-navigation-scope), whether that scope is ordinary direct media or an opened collection.

## Playback

Opening a video starts playback automatically.

When a video load or playback error is superseded by a later accepted non-error playback state for the same video, KiriView clears the public video error text. Public video states such as loading, ready, or empty must not expose stale error text from an earlier failed playback state.

Video mode shows a video viewport and a Breeze-style playback control panel at the bottom edge of the video viewport.

The regular toolbar remains available in video mode and follows the active-scope layout and collection-control visibility defined in [Toolbar](toolbar.md). Active navigation remains the direct media or opened collection scope that contains the video. Fit is disabled, zoom is read-only, and Two-Page Spread never pairs a video item.

Video mode shows a read-only zoom percentage when the video frame size, displayed video content rectangle, and target window effective device pixel ratio are known. The value is the current fitted display size in physical pixels relative to the video's intrinsic frame size. When the percentage is unknown, the read-only zoom control displays `? %`. Users cannot edit this value or use image zoom actions for video.

The playback control panel is shown only when the current video is ready and has a video track.

The playback control panel includes icon-only play/pause and mute/unmute buttons, current time, timeline position selection and scrubbing, total duration, and a disabled non-interactive timeline state when the media is not seekable. The timeline row order is play/pause, current time, timeline, total duration, and mute/unmute. Time readouts use fixed-width digits and are the only visible text in the panel.

Toggling mute affects the current video audio output and persists across video source changes during the app session.

In a spacious regular windowed or fullscreen video viewport, the playback controls use a floating panel aligned to the bottom edge with a large-spacing bottom margin. The floating panel uses the active Kirigami background color with theme-aware translucency, the Kirigami corner radius, a weak shadow, and centered content. Its nominal width is 75% of the viewport; it may grow to the complete control row's natural width but must preserve a large-spacing margin on each side.

Responsive control mode is stateful. Before valid layout facts are available, the controls use fixed mode. Fixed controls enter floating mode only when the viewport is at least 33 grid units wide and 17 grid units high and the complete control row, both side margins, and one additional grid unit of horizontal headroom fit. Floating controls remain floating at widths of at least 32 grid units and heights of at least 16 grid units while the complete control row and both side margins fit. Between those entry and exit thresholds, the current mode is retained.

The playback controls use fixed mode when touch/mobile input is active or system animations are disabled or reduced. The fixed bottom bar is full-width, has no floating bottom margin, has no shadow, has square outer bottom corners, and reserves its height from the video display area so the video is not covered by the controls.

While the user is dragging the timeline, changes in viewport geometry, input modality, animation capability, or control-row measurements do not move the controls between floating and fixed mode. After the drag is committed, cancelled, or invalidated, KiriView applies the latest complete environment once.

When video playback reaches the natural end of the media, KiriView keeps the video output as the active presentation and must not clear the video output into an empty or null-like visual state. Playback stops at the final position. Pressing Play from that ended state restarts playback from the beginning when seeking is available.

The floating playback panel must not reserve page layout height and must remain usable in fullscreen.

Auto-hide applies only in floating control mode while video is playing. Pointer movement, hover, focus, slider drag, button press, tap, or paused playback reveals the controls and keeps them visible while interaction continues. When controls are eligible to hide, they fade after a human-moment delay. If system animations are disabled or reduced, fixed bottom bar mode is used and controls do not auto-hide.

In Flatpak, KiriView supports video playback initialization and audio output using the sandbox multimedia access required for the current video source, without requiring broader filesystem access than the File Access contract allows.

During timeline dragging, the user's drag position remains visually stable instead of being overwritten by ordinary playback position updates.

Unknown duration, invalid duration, and non-seekable media produce a stable disabled timeline state rather than invalid or flickering values.

## Video Navigation And Seeking

In direct video mode, viewer Left and Right and existing adjacent navigation actions move to the previous or next supported media item in the ordinary parent location. They do not seek within the video.

In opened collection video mode, viewer Left and Right and existing adjacent navigation actions move to the previous or next supported media item in the opened archive or directory collection. They do not seek within the video.

In video mode, the shared scan shortcuts also use adjacent navigation in the active navigation scope: `.` and `Space` move to the next supported media item, while `,` and `Shift+Space` move to the previous supported media item. They do not pan or seek within the video.

Video mode supports the shared configurable shortcuts for Open, Move to Trash, Delete Permanently, Previous Media Item, Next Media Item, First Media Item, Last Media Item, Current Content Start, Current Content End, Fullscreen, Keyboard Shortcuts, Configure Shortcuts, and Quit. Shared navigation shortcuts use active-navigation dispatch and availability.

The fixed Show Menubar shortcut follows the Menu Presentation contract in video mode.

The Current Content Start and Current Content End shortcuts are viewer-local configurable video seek actions. By default, `Shift+,` and `Alt+Home` seek to position `0 ms` in the current video, and `Shift+.` and `Alt+End` seek to the video's known positive duration. They are distinct from the fixed `Alt+Arrow` video seek shortcuts.

`P` toggles play/pause for the current video using the same playback command as the playback panel button.

When an image-only configurable shortcut is pressed in video mode, KiriView does not trigger the image action and shows the in-app toast `This action is not available for videos`. Repeated unsupported video shortcut presses update the same toast instance.

When the video play/pause configurable shortcut is pressed while a ready image is displayed, KiriView does not trigger video playback and shows the in-app toast `This action is not available for images`. Empty, loading, error, and unsupported-placeholder states do not show this image unavailable-action toast.

Activating disabled image-only action placements in video mode has no effect. Only activating an image-only shortcut produces the unavailable-action toast.

Timeline dragging and scrubbing is the primary way to seek within the current video.

If keyboard focus is inside the timeline control, that control may handle its own keyboard interaction.

Video mode also supports fixed viewer-local seek shortcuts: `Alt+Left` seeks backward 5 seconds, `Alt+Right` seeks forward 5 seconds, `Alt+Up` seeks forward 45 seconds, and `Alt+Down` seeks backward 45 seconds. These intervals apply consistently to direct videos and playable opened-collection videos.

The configurable current-content start and end shortcuts follow the same media seekability gates as timeline seeking. Current Content End is unavailable when the duration is unknown, zero, or invalid.

Video seek shortcuts are active only while viewer-local shortcuts are active, the accepted media item is a ready video with an active video track, and the video is seekable. They are video-mode-only and must not affect image mode or unsupported-video placeholders.

Video seek shortcuts are best-effort time seeks. They clamp to the valid `[0, duration]` range when duration is known, never change the active navigation item, and must not promise frame-accurate seeking. If their readiness or seekability gates are not satisfied, they have no effect and do not fall back to adjacent-media navigation.

The actual landed position may be adjusted by the playback engine, commonly to a nearby decodable or keyframe position.

## Deletion

Video deletion follows [File Access](file-access.md#deletion).

After a successful direct-video deletion, playback stops before KiriView applies the direct-media follow-up item or empty-state fallback.
