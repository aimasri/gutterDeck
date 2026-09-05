# gutterDeck: Official Feature Roadmap

This document serves as the canonical feature roadmap and architectural plan for **gutterDeck v3.0+**. It tracks completed milestones, immediate priorities, and future capabilities.

---

## Architecture Principles
1. **Application-Agnostic:** Decks are arbitrary user-defined shell commands, never hardcoded to specific applications.
2. **Direct XCB Infrastructure:** No shell polling (`wmctrl`/`xdotool`); rely on event-driven X11/XCB mechanisms.
3. **Deadlock-Proof State Machine:** All animations and window swaps are guarded by explicit state transitions and automated watchdog timers.
4. **Non-Blocking Overlay:** A single-canvas transparent window using dynamic XShape masking to ensure click-through transparency when idle.

---

## Phase 1: Core Foundation & Engine (Completed ✅)
- [x] **XCB Connection & Event Loop:** Direct integration with Qt's event loop via `QSocketNotifier` on XCB file descriptor.
- [x] **"Holy Grail" Single-Canvas Overlay:** Top-level fullscreen transparent window bypassing window manager constraints (`Qt::X11BypassWindowManagerHint`).
- [x] **Dynamic XShape Masking:** Screen is 100% click-through in `IDLE` state except for the physical tab bounds; mask solidifies during curtain sweeps to prevent misclicks.
- [x] **Deadlock-Proof State Machine:** Enforces `IDLE` -> `SWITCHING_OUT` -> `SWITCHING_IN` -> `IDLE` lifecycle backed by emergency watchdog recovery.
- [x] **Two-Phase Curtain Sweep:** Single-shot connected slide-in/slide-out raster curtain with smooth easing curves.
- [x] **Multi-Monitor Target Geometry:** Explicit configuration support for `target_screen`, `screen_width`, and `screen_height` (e.g. targeting portrait display `1080x1920` without virtual screen spilling).
- [x] **Sequential Window Attachment:** Robust fallback mechanism attaching newly mapped application windows to unassigned deck slots.

---

## Phase 2: Core Engine Enhancements & Navigation Controls (Up Next 🎯)

### 1. Gutter Peek on Hover Delay (Core Engine Enhancement)
- **Concept:** Hovering the mouse over any inactive gutter tab for a configurable delay (e.g., 1.0s – 1.5s) initiates a non-destructive temporary preview of that deck.
- **Engine Requirements:**
  - Integrated into the `StateMachine` with a dedicated `PEEKING` sub-state or non-interfering preview layer.
  - Moving the cursor away immediately restores the active deck without triggering a full two-phase curtain transition.
  - Clicking while peeking immediately promotes the peeked deck to the active deck.
  - Watchdog-protected to prevent ghost peeks or stuck preview layers.

### 2. Left and Right Controls (Keyboard & Button Navigation)
- **Concept:** Enable direct sequential navigation between deck tabs using Left and Right controls without requiring pixel-precise tab clicking.
- **Controls Supported:**
  - **Keyboard Navigation:** Global or focused Left/Right arrow keys (or configurable keybindings) to step forward and backward through the deck stack.
  - **Dedicated UI Step Controls:** Left/Right navigation buttons or hit-zones to cycle decks in order.
- **Behavior:**
  - Stepping past the last deck smoothly wraps to the first (or stops, configurable).
  - Triggers the standard smooth curtain transition between adjacent decks.

---

## Phase 3: Deck Customization & Lifecycle Management

### 1. Right-Click Context Menu
- Right-clicking any gutter tab brings up an Openbox-styled dark context menu with options:
  - **Edit Deck Name:** Rename the tab label.
  - **Edit Launch Command:** Modify the executable shell command.
  - **Change Color & Opacity:** Open custom dark RGBA color picker.
  - **Add Deck Left / Right:** Prompt to configure and spawn a new deck slot on the fly.
  - **Close Deck:** Terminate the attached application process, release the slot, and readjust the accordion layout.

### 2. Custom Dark Dialogs
- **CustomInputDialog:** Clean dark modal with high-contrast text inputs, Enter-to-submit, and active display centering.
- **CustomColorDialog:** Dark color picker supporting RGBA hexadecimal input, preset swatches, and an alpha/opacity slider.

### 3. Real-Time Configuration Persistence
- Any additions, modifications, renames, or deletions made through the UI are immediately serialized back to `~/.config/gutter-deck/config.json`.

---

## Phase 4: Multi-Deck Workspace Layouts

### 1. Vertical Split-Pull View (50/50 Side-by-Side)
- **Concept:** Allows viewing two adjacent decks simultaneously.
- **Activation:** `Shift + Click` on an adjacent gutter tab.
- **Layout:**
  - Divides target display in half (e.g., two 540×1920 windows side-by-side on a 1080×1920 screen).
  - Both windows remain interactive and mapped to X11.
  - Clicking any gutter normally exits split mode and restores fullscreen focus.

### 2. Horizontal Split View (Top / Bottom)
- Stack two decks vertically (e.g., two 1080×960 windows on a 1080×1920 screen).

---

## Phase 5: Tab Organization & Onboarding

### 1. Drag-to-Reorder Tabs
- Grab any gutter tab and physically drag it up/down or across to reorder the deck hierarchy.
- Smooth tactile/visual indicator showing the target drop slot.
- Automatically reorganizes the deck array and persists the order to `config.json`.

### 2. First-Launch Onboarding Overlay
- Triggered when `"first_launch": true` in `config.json`.
- Displays subtle, elegant on-screen overlays highlighting:
  - How edge gutter tabs work.
  - Navigation shortcuts (Left/Right controls, Shift+Click split mode).
  - Right-click configuration options.
- Automatically toggles `"first_launch": false` once dismissed.

---

## Explicitly Excluded Features
- **Mouse-Wheel Rolodexing:** Excluded per user decision to prevent accidental deck cycling while scrolling document or browser viewports near screen edges.
