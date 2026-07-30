# Main Window Shell

## Startup and Input

When KiriView is launched with one or more file path or URL arguments, including from a file manager's Open With action, it processes only the first argument in the supplied order and opens it at startup.

When KiriView is launched with `--verbose` or `-v`, it enables diagnostic and performance logging for the launch while otherwise following normal GUI startup and source-opening behavior.

Activating the open action shows the XDG portal file chooser, which accepts a single selection only.

Dropping one or more file or URL items onto the running main window opens only the first item in the order supplied by the desktop environment.

If the first startup argument is a local file path or file URL and that file does not exist, KiriView prints a clear error message naming the path and reason to standard error, does not open the main window, and exits with code 2.

Startup arguments, drop, and open dialog selection may open supported direct video files.

## Window Size and Title

KiriView permits compact window sizes down to 14 by 12 Kirigami grid units.

When no saved window geometry overrides the launch size, KiriView opens at 24 by 20 Kirigami grid units.

When a direct image file is displayed and its intrinsic image size is known, the main window title is the displayed image file name, a spaced en dash, the intrinsic size as `width×height` using the multiplication sign with no spaces, a spaced em dash, and `KiriView`.

When a direct image file is displayed and its intrinsic image size is unknown, the title omits the size and uses the displayed image file name, a spaced em dash, and `KiriView`.

When a direct video file is displayed and its intrinsic video frame size is known, the main window title is the original direct media URL's file name, a spaced en dash, the intrinsic size as `width×height` using the multiplication sign with no spaces, a spaced em dash, and `KiriView`.

When a direct video file is displayed and its intrinsic video frame size is unknown, the title omits the size and uses the original direct media URL's file name, a spaced em dash, and `KiriView`.

When a directly opened archive collection is displayed and the active page position is known, the title is the archive file name, a spaced en dash, the current primary page number, `/`, the total supported item count, a spaced em dash, and `KiriView`.

When a directly opened local directory collection is displayed and the active page position is known, the title is the directory name, a spaced en dash, the current primary page number, `/`, the total supported item count, a spaced em dash, and `KiriView`.

When an archive collection or directly opened local directory collection is displayed and the active page position is unknown, the title omits the page counter and uses the archive file name or directory name, a spaced em dash, and `KiriView`.

KiriView does not show file paths in the window title.

While an accepted selected media target is loading or has a target-specific error, these title rules use that selected target and its accepted collection context; intrinsic size, page position, and other details that are not yet known are omitted.

When no accepted media target or opened collection is selected, the window title is `KiriView`.

## Fullscreen

Ctrl+F and F11 toggle the main window between normal windowed display and system fullscreen. The viewer-local Fullscreen shortcut is F.

Fullscreen hides the system titlebar and window decorations and shows the app toolbar as a top-attached overlay toolbar above the image viewing area without reserving layout space.

The fullscreen overlay toolbar uses the normal toolbar background and padding, attaches to the top, left, and right window edges, and does not use an outer margin, rounded floating-card background, or shadow.

The fullscreen overlay toolbar contains media controls without the toolbar application menu button.

The fullscreen overlay toolbar is shown when entering fullscreen and when the pointer enters the top reveal area near the toolbar.

Fullscreen hides the pointer when entering fullscreen. Moving the pointer shows it again, and KiriView hides it again after 1.0 seconds without pointer movement.

The fullscreen overlay toolbar hides after 1.0 seconds without pointer movement unless the user is actively interacting with toolbar controls or a toolbar input is focused. Pointer hover over the toolbar or top reveal area does not keep the toolbar visible by itself.

Leaving fullscreen restores the window's previous windowed, maximized, or minimized state and restores the normal header toolbar.

## Video Playback Panel

Video mode shows a video viewport with playback controls governed by [Video Playback](video-playback.md#playback).
