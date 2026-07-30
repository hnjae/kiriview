# Menus

## Application Menu and Menubar

KiriView shows its application menu through a toolbar application menu button by default outside fullscreen.

Users can switch the application menu presentation between Hamburger Menu and a conventional menubar. KiriView remembers the selected presentation as application state across launches.

In Hamburger Menu mode outside fullscreen, the toolbar application menu contains application actions such as Open, Open With, archive navigation, Full Screen, shortcut configuration, shortcut help, and Quit.

In menubar mode outside fullscreen, those application actions are available from the menubar. Any toolbar overflow menu appears only when toolbar controls do not fit.

The conventional menubar is an in-window menubar. KiriView does not integrate with native or global menubars.

In fullscreen, KiriView hides both the menubar and toolbar application menu button. Actions with configured shortcuts remain available through those shortcuts.

The menubar and toolbar application menu keep each action's identity, text, shortcut, and enabled state unchanged.

The Open With action is labeled `Open With...` in menus and is placed immediately after Open in the File menu and toolbar application menu.

Adjacent navigation actions are projected in reading progression order. When Right-to-Left Reading is active, the adjacent page navigation pair is displayed as Next before Previous, and the adjacent archive navigation pair is displayed as Next Archive before Previous Archive. First and Last keep their normal order because their page-index meaning does not change with reading direction.

The menubar Go menu projects directional navigation icons to match the displayed reading progression meaning without changing the underlying action identity. When Right-to-Left Reading is active, Next uses the previous-direction icon, Previous uses the next-direction icon, First uses the last-boundary icon, and Last uses the first-boundary icon.

The toolbar application menu is a single popup menu surface. When Hamburger Menu presentation is active outside fullscreen, activating the toolbar application menu button or pressing F10 opens that surface with the same width, actions, access keys, and shortcut column.

In that presentation, activating the toolbar application menu button while the menu is open closes it. Pressing F10 opens the menu and leaves it open when it is already open.

The menubar and toolbar application menu display one representative configured program-wide shortcut for actions with user-configurable program-wide shortcuts through the menu action's shortcut column.

The representative shortcut is the first of the action's current configured shortcuts that is safe to display in menus. Delete shortcuts, arrow and navigation-key shortcuts, and unmodified printable shortcuts are not menu-display safe because they can affect focused text input.

Actions without a menu-display-safe program-wide representative shortcut do not show configured shortcut text in menus.

Fixed shortcuts that users cannot configure may be shown only as display-only menu or tooltip text for the control they activate. They are not user-configurable action shortcuts.

Fixed shortcuts include arrow pan/navigation, Shift+arrow two-page stepping, video Alt+Arrow seeking, F10 for the toolbar application menu, and Ctrl+M for menubar presentation.

In the menubar, representative shortcut text is visually deemphasized from the menu item label when the item is not pressed. In the toolbar application menu, representative shortcut text is displayed separately on the trailing side of the menu item.

When the menubar or toolbar application menu is open, underlined menu access keys are activatable with either the displayed mnemonic letter alone or Alt plus that mnemonic letter.

When an access key opens a submenu, the parent menu remains open and the opened menu chain continues to accept access keys for the deepest open submenu.

Pressing and releasing Alt alone while the menubar or toolbar application menu is open keeps the menu open. Releasing Alt after an access-key interaction is not treated as a request to toggle or close the menu.

## Viewer Context Menu

Right-clicking the main media viewport opens a viewer context menu at the pointer position.

Holding the right mouse button and using the mouse wheel over an image viewport performs image zoom instead of opening the viewer context menu when the button is released.

The viewer context menu is available in image and video mode, including fullscreen.

Right-clicking the toolbar, menubar, Info Panel, or Thumbnail Panel does not open the viewer context menu.

The viewer context menu contains a concise set of viewer actions: Open, Open With, active navigation, image-only view actions such as zoom, fit, and rotation, panel toggles, and Fullscreen.

The viewer context menu uses the same application actions as the toolbar application menu and menubar. Action identity, text, shortcut display, enabled state, and checked state remain consistent across all menus.

Image-only actions remain visible but disabled when their normal availability rules disable them, including video mode and empty state.

The right-click gesture is a fixed mouse gesture. It is not listed in Keyboard Shortcuts configuration or Keyboard Shortcuts help.

## Menu Presentation

Ctrl+M toggles the application menu presentation between Hamburger Menu and Menubar.

KiriView may show Ctrl+M as display-only shortcut text on the Show Menubar menu item or tooltip.

KiriView may show F10 as display-only shortcut text on the toolbar application menu button tooltip.

Ctrl+M and F10 are fixed, are not user-configurable, and are not listed in Keyboard Shortcuts configuration or shortcut help.
