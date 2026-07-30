# Toolbar

## Toolbar Layout

The main window has exactly one toolbar instance.

The main window toolbar shows media controls without a page title.

The leading side of the toolbar contains Previous, the current page number, `of`, the total item count, and Next.

The current page number, `of`, and total item count use the system fixed-width font.

The trailing action toolbar shows as many trailing controls as fit and moves the rest into an overflow menu. When it runs out of horizontal space, KiriView keeps the zoom percentage visible the longest, then the Fit menu button. The Fit menu button shows its selected fit label when there is enough toolbar space and collapses to icon-only when space is constrained.

Visible trailing toolbar controls align to a common vertical center and use consistent outer spacing between adjacent top-level controls. The Fit menu button is rendered as one top-level toolbar control with the same vertical alignment and spacing as adjacent visible trailing toolbar controls.

Outside fullscreen, the toolbar uses normal application header placement, reserves layout space above the image viewing area, and remains visible even when no image or video is open.

Open, Open With, Previous Archive, and Next Archive are provided by the application menu and shortcuts rather than fixed toolbar buttons. Previous Archive and Next Archive use visually distinct previous/next-use icons so they are not confused with page Previous and Next navigation.

## Collection Controls

When the active navigation scope is a directly opened archive or local directory collection, the trailing action toolbar contains Right-to-Left Reading, Two-Page Spread, a Fit menu button, zoom, and, when Hamburger Menu presentation is active outside fullscreen, a toolbar application menu button.

When the active navigation scope is not an opened archive or directory collection, including empty state, ordinary direct image files, direct video files, and direct KDE archive-entry URLs, the trailing action toolbar does not show Right-to-Left Reading or Two-Page Spread.

Right-to-Left Reading and Two-Page Spread visibility is determined by the active navigation scope, not by whether the current media item is an image, direct video, playable collection video, or unsupported-video placeholder. When a playable video is displayed, Fit and zoom remain in their image-mode positions; Fit is disabled and zoom is read-only. When an unsupported-video placeholder is displayed, Fit is disabled and the zoom control has no active readout.

When Right-to-Left Reading or Two-Page Spread is visible for an opened collection that is not a [comic book archive](comic-archives.md#two-page-spread-and-reading-direction), the control is disabled.

When full trailing toolbar controls fit and Right-to-Left Reading and Two-Page Spread are visible, they are text-beside-icon buttons with the toolbar labels `Right-to-Left` and `Two-Page Spread`. If the toolbar cannot fit the text-bearing controls, KiriView may collapse them to icon-only controls or move them into overflow.

Visible text-bearing Right-to-Left Reading and Two-Page Spread toolbar buttons expose control mnemonics through the toolbar button labels. Their menu labels, tooltips, action identity, shortcut configuration, checked state, and enabled state remain unchanged.

When visible, the Right-to-Left Reading control is immediately to the left of the Two-Page Spread control. It toggles archive binding between left-to-right and right-to-left reading when that option is available.

## Page Navigation Controls

The toolbar page navigation readout and page-number entry use the current active navigation scope. The toolbar does not show a mixed readout from more than one scope.

When active navigation is unavailable or unknown, the toolbar page navigation readout displays `– of –` and keeps the page-number entry and navigation buttons disabled.

The toolbar page navigation arrow buttons keep their physical affordance. The left arrow button triggers Previous in Left-to-Right Reading mode and Next in Right-to-Left Reading mode. The right arrow button triggers Next in Left-to-Right Reading mode and Previous in Right-to-Left Reading mode. Each button's tooltip and accessible text follow the action that button triggers.

The toolbar page navigation arrow buttons, page-number entry, shared Previous, Next, First, and Last actions, menus, and shortcuts all target the same active navigation scope. Visible placements share the scope's enabled state; configured and fixed viewer-navigation shortcuts may still request Previous or Next at a known boundary so KiriView can provide the documented boundary feedback. First and Last follow their visible availability at the corresponding boundary.
