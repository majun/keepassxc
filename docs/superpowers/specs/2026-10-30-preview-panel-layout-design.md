# Preview panel layout: position and sections mode

Date: 2026-10-30
Status: Draft (awaiting review)
Related upstream: keepassxreboot/keepassxc#4244, keepassxreboot/keepassxc#13224

## Goal

Make the entry/group preview panel usable as a 1Password-style detail pane:

1. The preview panel can be placed to the **right** of the entry list instead of below it.
2. The preview can show all of its content as **stacked sections with headings**
   instead of tabs, so no information is hidden behind a tab.

The work lives in a fork first but must be upstream-ready: opt-in settings,
default behavior unchanged, tests included, small reviewable commits.

## Non-goals

- Two-line entry list rows or large icons (separate part of #4244).
- Changing the edit entry dialog (`EditEntryWidget`).
- Restructuring which fields belong to which tab (e.g. #13224, #8228).
- Changes to `.ui` files beyond adding menu actions.

## User-facing behavior

Two independent settings, both in the **View** menu, next to the existing
"Show Preview Panel" action (`MainWindow.ui`, `menuView`):

| Setting | UI | Values | Default |
|---|---|---|---|
| Preview panel position | Submenu "Preview Panel Position" with two exclusive checkable actions | Bottom / Right | Bottom |
| Sections mode | Checkable action "Show Preview as Sections" | on / off | off |

(Change against the chat discussion: the settings were originally placed in
Settings → General → User Interface. The existing "Show Preview Panel" and
"Compact Mode" toggles live in the View menu, so the new ones go there too.)

Both settings apply immediately to all open databases, without restart.

### Position

- **Bottom**: current behavior, vertical splitter.
- **Right**: horizontal splitter, entry list left, preview right.
- When the orientation actually changes, stored splitter sizes are reset to the
  default ratio (heights are not valid as widths).

### Sections mode

Applies to both previews:

- Entry preview: sections **General**, **Advanced**, **Auto-Type**.
- Group preview: sections **General**, **Share**.

Each section is a bold heading with a horizontal separator line, followed by
the content of the former tab. All sections are in one vertically scrolling area.

Empty sections mirror disabled tabs: whenever the tab would be disabled
(e.g. Advanced without attributes/attachments, Share for a non-shared group),
the heading stays visible but greyed out (disabled), and its content is hidden.

The entry header (title, icon, TOTP, etc. above the tab widget) is unchanged.

## Design

Approach: **move the existing tab pages at runtime** between the `QTabWidget`
and a sections container. Data filling code (`updateEntryGeneralTab()`, etc.)
stays untouched.

### Config (`src/core/Config.h`, `src/core/Config.cpp`)

- `GUI_PreviewPanelPosition` → `GUI/PreviewPanelPosition`, Roaming, string
  `"bottom"` | `"right"`, default `"bottom"`. Unknown value is treated as bottom.
- `GUI_PreviewShowSections` → `GUI/PreviewShowSections`, Roaming, bool, default `false`.

### Menu (`src/gui/MainWindow.ui`, `src/gui/MainWindow.cpp`)

- New actions `actionPreviewPositionBottom`, `actionPreviewPositionRight`
  in a `QActionGroup` (exclusive), in submenu `menuPreviewPanelPosition`.
- New checkable action `actionShowPreviewSections`.
- Wired like `actionShowPreviewPanel`: initial state from config, `toggled`
  writes config.

### Splitter (`src/gui/DatabaseWidget.cpp/.h`)

- New private `applyPreviewPanelPosition()`: reads config, sets
  `m_previewSplitter->setOrientation()`. If the orientation changed compared
  to the current one, sets sizes to the default ratio.
- Called from the constructor (replacing the hard-coded `Qt::Vertical`)
  and from a `Config::changed` handler for `GUI_PreviewPanelPosition`.
- `GUI_PreviewSplitterState` stays a single value. On startup the stored sizes
  are applied after the orientation is set; a mismatch only results in an
  odd initial ratio, never a broken layout.

### Sections (`src/gui/EntryPreviewWidget.cpp/.h`)

- New private helper class or struct `SectionsView` per tab widget, holding:
  the `QScrollArea`, its container with `QVBoxLayout`, and a list of
  `{QLabel* heading, QWidget* page, QString title}`.
- `setSectionsMode(bool enabled)`:
  - **on**: for each page of `entryTabWidget` / `groupTabWidget` (in order),
    remember the tab title, remove the page from the tab widget, add heading +
    separator + page to the container; hide the tab widget, show the scroll area
    in its place (same parent layout, same position).
  - **off**: reverse — remove pages from the container, re-insert into the tab
    widget with original titles and order; restore enabled states.
- Inner scroll area of the General page (`scrollAreaWidgetContents`) is set to
  `Qt::ScrollBarAlwaysOff` and its size policy adjusted so its content takes its
  full height in sections mode; restored when sections mode is turned off.
- `setTabEnabled(QTabWidget*, QWidget*, bool)` becomes mode-aware: in sections
  mode it disables the heading and hides the page; in tabs mode it behaves as now.
  The enabled state is stored per page so switching modes preserves it.
- `updateTabIndexes()` and the restore of the last selected tab are skipped in
  sections mode.
- Focus proxy (`setFocusProxy(m_ui->entryTabWidget)`) points to the scroll area
  in sections mode.
- Called from the constructor and a `Config::changed` handler for
  `GUI_PreviewShowSections`.

Group preview uses the same mechanism; it is added in a separate commit.

## Error handling

No new failure modes: settings are UI-only. Invalid config values fall back to
defaults. Mode switching is idempotent (calling `setSectionsMode(true)` twice is
a no-op).

## Testing (`tests/gui/TestGui.cpp`)

- Position: set config to right → splitter `orientation() == Qt::Horizontal`;
  back to bottom → `Qt::Vertical`; default is vertical.
- Entry sections: enable sections, select an entry with Auto-Type and without
  attributes → General and Auto-Type pages are visible at the same time,
  tab widget hidden, Advanced heading disabled and its page hidden.
  Select an entry with attributes → Advanced heading enabled, page visible.
  Disable sections → pages are back in the tab widget in original order with
  original titles.
- Group sections: same for General / Share with a non-shared group.
- Existing preview tests (around `TestGui.cpp:2317`) keep passing in default mode.

## Commits

Each commit builds, is formatted (`cmake --build . --target format`) and passes tests.

1. Add preview panel position setting (bottom/right) + test.
2. Add sections mode for the entry preview + test.
3. Extend sections mode to the group preview + test.
4. Update translations (`release-tool.py i18n lupdate`) and user guide
   (`docs/topics/UserInterface.adoc`, section "(D) Preview").

Commit messages follow CONTRIBUTING.md: imperative, present tense, ≤72 chars.
The PR description must state that the code was written with AI assistance.

## Open risks

- Reparenting widgets out of a `QTabWidget` built from `.ui` may affect
  stylesheets or minimum sizes; verified by manual run with both modes and
  both themes.
- The General page's inner scroll area may need extra size-policy tweaking
  to avoid clipped content in sections mode.
