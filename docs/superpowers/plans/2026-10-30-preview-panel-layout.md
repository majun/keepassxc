# Preview Panel Position and Sections Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let users put the preview panel to the right of the entry list and show the entry/group preview as stacked sections instead of tabs.

**Architecture:** Two opt-in config keys toggled from the View menu. `DatabaseWidget` switches the orientation of `m_previewSplitter`; `EntryPreviewWidget` moves the existing tab pages at runtime between their `QTabWidget` and a `QScrollArea` with headings. Data-filling code is untouched.

**Tech Stack:** C++20, Qt 6 (Widgets, QTest), CMake, clang-format.

**Spec:** `docs/superpowers/specs/2026-10-30-preview-panel-layout-design.md`

## Global Constraints

- Default behavior unchanged: position `"bottom"`, sections `false`.
- Config keys: `GUI_PreviewPanelPosition` → `GUI/PreviewPanelPosition`, Roaming, `"bottom"` | `"right"`; `GUI_PreviewShowSections` → `GUI/PreviewShowSections`, Roaming, `false`. Unknown position value = bottom.
- Menu texts (exact): submenu "Preview Panel Position", actions "Bottom", "Right"; action "Show Preview as Sections". Placed in `menuView` directly after `actionShowPreviewPanel`.
- Settings apply immediately to all open databases (via `Config::changed`), no restart.
- No `.ui` edits except `MainWindow.ui` menu actions. `EntryPreviewWidget.ui` stays unchanged.
- Style: `lowerCamelCase`, run `cmake --build . --target format` before each commit.
- Commit messages: imperative, present tense, first line ≤72 chars, no `feat:` prefixes. End with the session's Co-Authored-By / Claude-Session trailers.
- Work on branch `claude/ecstatic-hypatia-ml6ysn`.

## Review Focus

1. Switching sections mode while an entry is selected and the Advanced/Auto-Type tab is the current tab → after switching back, tab widget shows a valid enabled tab, no assert from `Q_ASSERT(isTabEnabled(GeneralTabIndex))`.
2. Toggling sections on/off repeatedly (idempotency) → no duplicated headings, pages in original order with original titles. Test in Task 2.
3. Switching from entry to group selection in sections mode (stacked widget `pageEntry`/`pageGroup`) → group preview also in sections, enabled states per page correct. Test in Task 3.
4. Changing position while preview is hidden (`GUI_HidePreviewPanel = true`) → orientation still changes, no crash when shown later. Test in Task 1.
5. Existing `entryAutotypeTab->isEnabled()` checks in `TestGui::testAutoType` keep working in sections mode, because page `setEnabled()` is kept. Test in Task 2.

---

### Task 0: Build environment

No Qt in the container currently (`dpkg -l | grep qt6` is empty).

- [ ] **Step 1:** Install build deps (Debian/Ubuntu): `qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools qt6-svg-dev libbotan-3-dev libargon2-dev zlib1g-dev libqrencode-dev libminizip-dev libreadline-dev libxi-dev libxtst-dev libpcsclite-dev libusb-1.0-0-dev clang-format xvfb`. If the package manager or network blocks this, stop and report.
- [ ] **Step 2:** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DWITH_TESTS=ON -DWITH_GUI_TESTS=ON && cmake --build build -j$(nproc)`. Expected: build succeeds.
- [ ] **Step 3:** `cd build && QT_QPA_PLATFORM=offscreen ctest -R testgui -V`. Expected: PASS (baseline). Record any pre-existing failures; they are not ours.

No commit.

### Task 1: Preview panel position (bottom/right)

**Files:**
- Modify: `src/core/Config.h` (enum, next to `GUI_HidePreviewPanel`), `src/core/Config.cpp` (~line 112)
- Modify: `src/gui/DatabaseWidget.h`, `src/gui/DatabaseWidget.cpp:83,156,205`
- Modify: `src/gui/MainWindow.ui` (`menuView`), `src/gui/MainWindow.cpp` (~line 2023)
- Test: `tests/gui/TestGui.cpp`, `tests/gui/TestGui.h`

**Interfaces:**
- Produces: `Config::GUI_PreviewPanelPosition`; `m_previewSplitter->setObjectName("previewSplitter")` (tests find it via `findChild<QSplitter*>("previewSplitter")`); private `void DatabaseWidget::applyPreviewPanelPosition()`.

- [ ] **Step 1: Write failing test `TestGui::testPreviewPanelPosition()`**

```cpp
auto* splitter = m_dbWidget->findChild<QSplitter*>("previewSplitter");
QVERIFY(splitter);
QCOMPARE(splitter->orientation(), Qt::Vertical);
config()->set(Config::GUI_PreviewPanelPosition, "right");
QCOMPARE(splitter->orientation(), Qt::Horizontal);
config()->set(Config::GUI_PreviewPanelPosition, "bogus");
QCOMPARE(splitter->orientation(), Qt::Vertical);
// Review Focus 4: hidden preview
config()->set(Config::GUI_HidePreviewPanel, true);
config()->set(Config::GUI_PreviewPanelPosition, "right");
QCOMPARE(splitter->orientation(), Qt::Horizontal);
config()->set(Config::GUI_HidePreviewPanel, false);
// Menu action reflects config
auto* actionRight = m_mainWindow->findChild<QAction*>("actionPreviewPositionRight");
QVERIFY(actionRight && actionRight->isChecked());
```

- [ ] **Step 2:** Build, run `ctest -R testgui -V`. Expected: FAIL (compile error on `GUI_PreviewPanelPosition`).
- [ ] **Step 3: Implement**
  - Config key as in Global Constraints.
  - `applyPreviewPanelPosition()`: `"right"` → `Qt::Horizontal`, else `Qt::Vertical`; if the orientation differs from the current one, `setOrientation()` then `setSizes({1, 1})` (same default as constructor). Replace the hard-coded `setOrientation(Qt::Vertical)` at line 156 with the call. Ensure it runs before stored `GUI_PreviewSplitterState` is applied (line ~401).
  - Connect `Config::changed` in the constructor; on `GUI_PreviewPanelPosition` call `applyPreviewPanelPosition()`.
  - `MainWindow.ui`: `QMenu menuPreviewPanelPosition` ("Preview Panel Position") with checkable `actionPreviewPositionBottom` ("Bottom"), `actionPreviewPositionRight` ("Right"), added after `actionShowPreviewPanel`.
  - `MainWindow.cpp`: `QActionGroup` (exclusive) for both; initial check from config; `triggered` writes `"bottom"` / `"right"`.
- [ ] **Step 4:** Run tests. Expected: `testPreviewPanelPosition` PASS, rest of testgui unchanged vs. baseline.
- [ ] **Step 5:** Manual check: `QT_QPA_PLATFORM=offscreen` cannot show it; if a display is available (`xvfb-run`), take a screenshot with both positions. Otherwise note it as unverified visually.
- [ ] **Step 6: Commit** — `Add option to show the preview panel on the right`

### Task 2: Sections mode for entry preview

**Files:**
- Modify: `src/core/Config.h`, `src/core/Config.cpp`
- Modify: `src/gui/EntryPreviewWidget.h`, `src/gui/EntryPreviewWidget.cpp` (constructor ~77–106, `setEntry` ~195–200, `updateTabIndexes` ~563, `setTabEnabled` ~576)
- Modify: `src/gui/MainWindow.ui`, `src/gui/MainWindow.cpp`
- Test: `tests/gui/TestGui.cpp`, `tests/gui/TestGui.h`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces:
  - `Config::GUI_PreviewShowSections`
  - In `EntryPreviewWidget.h`, private:
    ```cpp
    struct SectionsView {
        QTabWidget* tabWidget = nullptr;
        QScrollArea* scrollArea = nullptr;   // objectName: tabWidget->objectName() + "Sections"
        QVBoxLayout* layout = nullptr;
        struct Section { QWidget* page; QLabel* heading; QString title; bool enabled; };
        QList<Section> sections;             // in original tab order
        bool active = false;
    };
    void initSectionsView(SectionsView& view, QTabWidget* tabWidget);
    void setSectionsMode(SectionsView& view, bool enabled);
    void applySectionsMode();               // reads config, applies to all views
    SectionsView m_entrySections;
    ```
  - Heading objectName: `page->objectName() + "Heading"` (e.g. `entryAdvancedTabHeading`).
  - Mode-aware `setTabEnabled(QTabWidget*, QWidget*, bool)` — same signature as today.

- [ ] **Step 1: Write failing test `TestGui::testEntryPreviewSections()`**

Select an entry with Auto-Type enabled and no attributes/attachments (reuse the selection pattern from `testAutoType`, entry index 1 after sorting by title), then:

```cpp
auto* preview = m_dbWidget->findChild<EntryPreviewWidget*>("previewWidget");
auto* tabs = preview->findChild<QTabWidget*>("entryTabWidget");
auto* general = preview->findChild<QWidget*>("entryGeneralTab");
auto* advanced = preview->findChild<QWidget*>("entryAdvancedTab");
auto* autotype = preview->findChild<QWidget*>("entryAutotypeTab");
config()->set(Config::GUI_PreviewShowSections, true);
auto* sections = preview->findChild<QScrollArea*>("entryTabWidgetSections");
QVERIFY(sections && sections->isVisible());
QVERIFY(!tabs->isVisible());
QVERIFY(general->isVisible() && autotype->isVisible());
auto* advHeading = preview->findChild<QLabel*>("entryAdvancedTabHeading");
QVERIFY(advHeading->isVisible() && !advHeading->isEnabled());
QVERIFY(!advanced->isVisible());
// Review Focus 5: page enabled state still mirrors tab state
QVERIFY(autotype->isEnabled() && !advanced->isEnabled());
// Review Focus 2: idempotent toggle
config()->set(Config::GUI_PreviewShowSections, true);
config()->set(Config::GUI_PreviewShowSections, false);
config()->set(Config::GUI_PreviewShowSections, true);
QCOMPARE(preview->findChildren<QLabel*>("entryAdvancedTabHeading").size(), 1);
config()->set(Config::GUI_PreviewShowSections, false);
QVERIFY(tabs->isVisible() && !sections->isVisible());
QCOMPARE(tabs->count(), 3);
QCOMPARE(tabs->widget(0), general);
QCOMPARE(tabs->widget(1), advanced);
QCOMPARE(tabs->widget(2), autotype);
QVERIFY(!tabs->isTabEnabled(1));
```

Second part: add an attribute to the selected entry (`entry->attributes()->set("foo", "bar")`), call `preview->refresh()` with sections on → `advHeading->isEnabled()` and `advanced->isVisible()`.

Third part (Review Focus 1): sections off, `tabs->setCurrentWidget(autotype)`, sections on, select entry index 0 (Auto-Type disabled), sections off → `tabs->isTabEnabled(tabs->currentIndex())`.

- [ ] **Step 2:** Run. Expected: FAIL (compile error on `GUI_PreviewShowSections`).
- [ ] **Step 3: Implement**
  - `initSectionsView()`: create `QScrollArea` (`setWidgetResizable(true)`, `QFrame::NoFrame`) inserted into the tab widget's parent layout right after it, hidden; container widget + `QVBoxLayout` + trailing stretch. Record each page's title from `tabWidget->tabText(i)`.
  - `setSectionsMode(view, true)`: no-op if active. Remove pages from the tab widget in reverse order, add per section: heading `QLabel` (bold font, objectName as above), a `QFrame::HLine` separator, the page. Hide tab widget, show scroll area. Apply stored `enabled`: heading `setEnabled(enabled)`, page `setVisible(enabled)` and `setEnabled(enabled)`.
  - `setSectionsMode(view, false)`: no-op if not active. Remove page from layout, `tabWidget->insertTab(i, page, title)` in original order, `setTabEnabled(i, enabled)`, `page->setVisible(true)`; delete headings/separators; if current tab is disabled, set current index to 0.
  - General page inner scroll area: in sections mode `setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff)` and `setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents)`; restore previous values when leaving. Find it as the `QScrollArea` ancestor of `scrollAreaWidgetContents` inside `entryGeneralTab`.
  - `setTabEnabled()`: update `Section::enabled`; if view active apply as above, else current behavior.
  - Skip `updateTabIndexes()` and the tab-restore block in `setEntry()` when `m_entrySections.active`.
  - `setFocusProxy()` → scroll area when active, tab widget otherwise.
  - Constructor: `initSectionsView(m_entrySections, m_ui->entryTabWidget)`, `applySectionsMode()`; `Config::changed` handler (existing lambda ~line 92) calls `applySectionsMode()` on `GUI_PreviewShowSections`.
  - Menu: checkable `actionShowPreviewSections` ("Show Preview as Sections") after the position submenu, wired like `actionShowPreviewPanel`.
- [ ] **Step 4:** Run `ctest -R testgui -V`. Expected: new test PASS, `testAutoType` PASS, rest unchanged vs. baseline.
- [ ] **Step 5:** Visual check as in Task 1 Step 5, both modes, light and dark theme (spec "Open risks"). Fix clipped General content if seen.
- [ ] **Step 6: Commit** — `Add option to show the entry preview as sections`

### Task 3: Sections mode for group preview

**Files:**
- Modify: `src/gui/EntryPreviewWidget.h`, `src/gui/EntryPreviewWidget.cpp` (`setGroup` ~207–212)
- Test: `tests/gui/TestGui.cpp`, `tests/gui/TestGui.h`

**Interfaces:**
- Consumes: `SectionsView`, `initSectionsView()`, `setSectionsMode()`, `applySectionsMode()` from Task 2.
- Produces: `SectionsView m_groupSections`; scroll area `groupTabWidgetSections`; headings `groupGeneralTabHeading`, `groupShareTabHeading`.

- [ ] **Step 1: Write failing test `TestGui::testGroupPreviewSections()`**

Enable sections, select a non-shared group in `groupView` (root group), then:

```cpp
auto* preview = m_dbWidget->findChild<EntryPreviewWidget*>("previewWidget");
auto* sections = preview->findChild<QScrollArea*>("groupTabWidgetSections");
QVERIFY(sections && sections->isVisible());
QVERIFY(preview->findChild<QWidget*>("groupGeneralTab")->isVisible());
auto* shareHeading = preview->findChild<QLabel*>("groupShareTabHeading");
QVERIFY(shareHeading->isVisible() && !shareHeading->isEnabled());
QVERIFY(!preview->findChild<QWidget*>("groupShareTab")->isVisible());
```

Review Focus 3: select an entry → `entryTabWidgetSections` visible; select the group again → `groupTabWidgetSections` still visible. Sections off → `groupTabWidget->count() == 2`, original order.

KeeShare is always compiled in (`EntryPreviewWidget.cpp:28`), so no `#ifdef` guard is needed.

- [ ] **Step 2:** Run. Expected: FAIL (`groupTabWidgetSections` is null).
- [ ] **Step 3: Implement** — `initSectionsView(m_groupSections, m_ui->groupTabWidget)`; `applySectionsMode()` applies to both views; skip group tab restore in `setGroup()` and in `updateTabIndexes()` when active.
- [ ] **Step 4:** Run testgui. Expected: PASS.
- [ ] **Step 5: Commit** — `Show the group preview as sections when enabled`

### Task 4: Translations and user guide

**Files:**
- Modify: `share/translations/keepassxc_en.ts` (generated)
- Modify: `docs/topics/UserInterface.adoc:19`

- [ ] **Step 1:** `python ./release-tool.py i18n lupdate`. Expected: only new strings ("Preview Panel Position", "Bottom", "Right", "Show Preview as Sections") added to `keepassxc_en.ts`; revert unrelated churn if any.
- [ ] **Step 2:** In `UserInterface.adoc`, extend the "(D) Preview" paragraph: the preview can be moved to the right via *View → Preview Panel Position*, and shown as sections instead of tabs via *View → Show Preview as Sections*.
- [ ] **Step 3:** Full run: `cmake --build build --target format`, `git diff --stat` shows no format changes left, `ctest` in build dir all green (vs. baseline).
- [ ] **Step 4: Commit** — `Update translations and user guide for preview layout options`
- [ ] **Step 5:** `git push -u origin claude/ecstatic-hypatia-ml6ysn`.
