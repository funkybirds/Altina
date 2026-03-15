# Spec.EditorUiModularization

## Summary

This spec defines the Editor UI modularization refactor for `AltinaEditor::UI`.

Goals:
- Keep `FEditorUiModule` as lifecycle/composition entry.
- Split runtime behaviors by root/layout controller and panel controllers.
- Replace legacy public polling API with frame-oriented API.
- Keep runtime behavior aligned with existing editor interactions.

## Public API

`FEditorUiModule` exposes:
- `Initialize(const FEditorUiInitDesc&)`
- `TickUi(const FEditorUiFrameContext&) -> FEditorUiFrameOutput`
- `Shutdown()`

Data contracts:
- `FEditorUiInitDesc` carries debug GUI system and optional panel descriptors.
- `FEditorUiFrameContext` carries optional hierarchy snapshot and command-clear control.
- `FEditorUiFrameOutput` carries viewport request and queued UI commands.

Legacy debug/test methods are removed from the main public API surface and are exposed through `EditorUI/EditorUiTesting.h` helper accessors.

## Internal Architecture

### Module composition
- `FEditorUiModule` owns:
  - `FEditorUiRootController`
  - `FHierarchyPanelController`
  - `FAssetPanelController`
  - `FInspectorPanelController`
  - `FEditorUiStateStore`

### Responsibilities
- Root controller:
  - Menu bar interactions
  - Dock regions and splitter drag logic
  - Panel tab activation/drag-drop docking
  - Viewport focus and UI blocking state
- Panel controllers:
  - Hierarchy panel draw + interaction
  - Asset panel draw + interaction
  - Inspector panel draw
- State store:
  - Frame counter
  - Pending command queue
  - Cached frame output
  - Viewport request

## Lifecycle and data flow

1. `Initialize` registers the root overlay callback and seeds default panels when descriptors are absent.
2. Debug GUI callback draws the root and panel UIs each frame and updates state.
3. Host calls `TickUi` after GUI frame processing to fetch `FEditorUiFrameOutput`.
4. Host consumes `mCommands` and optionally clears internal command buffer via `bClearCommandBuffer`.
5. `Shutdown` clears registration state and cached state.

## Test matrix

- Viewport request availability after initialize + first UI frame.
- Asset list scan excludes `.meta` files and supports directory navigation.
- Play menu command enqueue is visible in frame output and clear path is deterministic.
- Hierarchy snapshot ordering and depth handling remains stable for world-id/generation edge cases.
- Dock drag from center to left changes viewport content width.

## Compatibility notes

- Host integration must migrate from:
  - `RegisterDefaultPanels`, `GetViewportRequest`, `ConsumeUiCommands`
  to:
  - `Initialize`, `TickUi`, `Shutdown`.
- Test integration must migrate from direct debug methods on `FEditorUiModule` to `Testing::FEditorUiTestingAccess`.
