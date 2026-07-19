# Actions and Shortcuts

## Action Availability And Shortcuts

Controls that require selected, navigable, or ready media are disabled until the corresponding program state is available.

Configurable application actions and their placements use one shared availability decision. If an action is unavailable for the current media, mode, or interaction context, activating its menu item, toolbar placement, context-menu placement, or shortcut has no effect.

Known navigation boundaries are a dispatch outcome rather than media unavailability. Previous, Next, First, and Last placements are disabled when they have no target, but configured and fixed viewer-navigation shortcuts may still request the command so KiriView can provide the documented boundary feedback.

Configurable shortcuts have a declared activation scope. Program-wide shortcuts are active throughout the KiriView window subject to the action's normal enabled state. Viewer-local shortcuts are active only in viewer context after the viewer shortcut gates for the action are enabled.

Users may edit a shortcut slot's key sequence but may not change that slot's activation scope.

Program-wide configurable shortcuts appear as ordinary action shortcuts in menus, Keyboard Shortcuts configuration, and Keyboard Shortcuts help.

Viewer-local configurable shortcuts are shown in Keyboard Shortcuts help and KiriView shortcut configuration as viewer-local shortcuts. They do not appear as ordinary global action shortcuts.

Viewer-local shortcuts are inactive while text input, input-method-sensitive UI, shortcut help, modal UI, inactive windows, or other viewer-suppressed states are active.

Unmodified ASCII printable key sequences are allowed for viewer-local configurable shortcuts but disallowed for program-wide configurable shortcuts.

Viewer commands use viewer-local shortcuts by default unless the action explicitly declares a program-wide shortcut slot. KiriView does not derive hidden viewer aliases by dropping Ctrl from program-wide shortcuts and does not keep program-wide Ctrl fallbacks for viewer-local commands.

Toolbar controls, menus, context menus, shortcut help, and shortcut handling expose one coherent current-media, navigation, viewport, and interaction-context state, subject only to the documented navigation-boundary feedback exception. During media replacement, mode switches, deletion, modal dialogs, or focus changes, KiriView must not display or trigger state from an older media item, viewport, or interaction context after a newer state is active.

## Shortcut Help

Ctrl+? and F1 open the modal Keyboard Shortcuts help dialog. The viewer-local Keyboard Shortcuts help shortcut is ?.

The Keyboard Shortcuts help is shown as a modal dialog over the main window.

The Keyboard Shortcuts help content remains contained within the modal dialog. When shortcut rows exceed the available dialog content height, the shortcut list scrolls vertically and does not overflow the dialog window.

It lists user-configurable KiriView actions and their current configured shortcut text.

Program-wide and viewer-local configurable shortcuts are both listed. Viewer-local shortcuts are identified by scope text or grouping.

Shortcut help is grouped by app-menu category headers.

Each listed action is shown as a compact form-card delegate with the action text on the leading side and one or more rounded, fixed-width keycap badges for its configured shortcut sequences on the trailing side.

It does not list fixed shortcuts, mouse gestures, or mouse-wheel gestures.

It can be dismissed with standard dialog dismissal actions such as Enter/OK, Escape, the close button, or clicking outside the dialog.

While the shortcut help dialog is open, standard dialog dismissal actions close the dialog before any fullscreen handling.

Video seek shortcuts are fixed shortcuts and are not listed in Keyboard Shortcuts configuration or shortcut help.

## Escape and Quit

Escape first cancels an active page number or zoom input edit.

When no toolbar input is focused and the Info Panel is open, Escape closes the Info Panel before fullscreen handling.

When no toolbar input is focused and the Info Panel is closed, Escape leaves fullscreen if the main window is fullscreen.

Outside fullscreen, Escape does not close the main window.

Ctrl+Q closes the main window as the default program-wide configurable Quit shortcut.

The viewer-local configurable Quit shortcut is `q`; it closes the main window only while viewer-local shortcuts are active.

Quit shortcuts using Ctrl, Alt, or Meta remain active while those inputs are focused.

## Configurable Shortcuts

Users can open Keyboard Shortcuts configuration to configure KiriView's keyboard shortcuts.

Changing a shortcut updates the toolbar, application menu, menubar, shortcut help, and active keyboard handling consistently.

Shortcut changes apply immediately and persist across launches.

Changing a program-wide shortcut does not create a viewer-local alias, and changing a viewer-local shortcut does not create a program-wide fallback.

Unmodified ASCII printable shortcuts are not kept as program-wide user-configurable action shortcuts.
