# UnrealBridge UMG Library API

Module: `unreal.UnrealBridgeUMGLibrary`

## Widget Tree

### get_widget_tree(widget_blueprint_path) -> list[FBridgeWidgetInfo]

Get the widget hierarchy of a Widget Blueprint as a flat list with parent references.

```python
widgets = unreal.UnrealBridgeUMGLibrary.get_widget_tree('/Game/UI/WBP_MainMenu')
for w in widgets:
    indent = '  ' if w.parent_name else ''
    var_tag = ' [var]' if w.is_variable else ''
    print(f'{indent}{w.name} ({w.widget_class}) slot={w.slot_type} vis={w.visibility}{var_tag}')
```

### FBridgeWidgetInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Widget name |
| `widget_class` | str | e.g. "CanvasPanel", "TextBlock", "Button" |
| `parent_name` | str | Parent widget name (empty for root) |
| `slot_type` | str | Slot class if parented, e.g. "CanvasPanelSlot" |
| `is_variable` | bool | Exposed as a variable in the Blueprint |
| `visibility` | str | "Visible", "Collapsed", "Hidden", "HitTestInvisible", "SelfHitTestInvisible" |

---

## Widget Properties

### get_widget_properties(widget_blueprint_path, widget_name) -> list[FBridgeWidgetPropertyValue]

Get non-default property values for a specific widget. Only returns properties that differ from class defaults.

```python
props = unreal.UnrealBridgeUMGLibrary.get_widget_properties('/Game/UI/WBP_Main', 'TitleText')
for p in props:
    print(f'{p.name} ({p.type}) = {p.value}')
```

### FBridgeWidgetPropertyValue fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Property name |
| `type` | str | C++ type string |
| `value` | str | Exported text value |

---

## Widget Animations

### get_widget_animations(widget_blueprint_path) -> list[FBridgeWidgetAnimationInfo]

Get all widget animations with tracks, durations, and which widgets they target.

```python
anims = unreal.UnrealBridgeUMGLibrary.get_widget_animations('/Game/UI/WBP_Main')
for a in anims:
    print(f'{a.name} ({a.duration}s)')
    for t in a.tracks:
        print(f'  {t.widget_name}: {t.display_name} ({t.track_type})')
```

### FBridgeWidgetAnimationInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Animation display name |
| `duration` | float | Duration in seconds |
| `tracks` | list[FBridgeWidgetAnimTrack] | Animated tracks |

### FBridgeWidgetAnimTrack fields

| Field | Type | Description |
|-------|------|-------------|
| `widget_name` | str | Target widget name |
| `track_type` | str | Track class name |
| `display_name` | str | Human-readable track name |

---

## Widget Bindings

### get_widget_bindings(widget_blueprint_path) -> list[FBridgeWidgetBindingInfo]

Get all property bindings (e.g. Text bound to a function, Visibility bound to a property).

```python
bindings = unreal.UnrealBridgeUMGLibrary.get_widget_bindings('/Game/UI/WBP_Main')
for b in bindings:
    print(f'{b.widget_name}.{b.property_name} -> {b.function_name} ({b.kind})')
```

### FBridgeWidgetBindingInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `widget_name` | str | The widget this binding is on |
| `property_name` | str | Bound property, e.g. "Text", "Visibility" |
| `function_name` | str | Function or property providing the value |
| `kind` | str | "Function" or "Property" |

---

## Widget Events

### get_widget_events(widget_blueprint_path) -> list[FBridgeWidgetEventInfo]

Get widget event bindings from the event graph (OnClicked, OnHovered, etc.).

```python
events = unreal.UnrealBridgeUMGLibrary.get_widget_events('/Game/UI/WBP_Main')
for e in events:
    print(f'{e.widget_name}.{e.event_name} -> {e.handler_name}')
```

### FBridgeWidgetEventInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `widget_name` | str | The widget this event is on |
| `event_name` | str | Event name, e.g. "OnClicked", "OnHovered" |
| `handler_name` | str | Bound function or node description |

---

## Search Widgets

### search_widgets(widget_blueprint_path, query) -> list[FBridgeWidgetInfo]

Search widgets by name or class substring. Returns FBridgeWidgetInfo (same as GetWidgetTree).

```python
buttons = unreal.UnrealBridgeUMGLibrary.search_widgets('/Game/UI/WBP_Main', 'Button')
texts = unreal.UnrealBridgeUMGLibrary.search_widgets('/Game/UI/WBP_Main', 'Health')
```

---

## Set Widget Property

### set_widget_property(widget_blueprint_path, widget_name, property_name, value) -> bool

Set a design-time property on a widget. Value is parsed as text.

```python
ok = unreal.UnrealBridgeUMGLibrary.set_widget_property(
    '/Game/UI/WBP_Main', 'TitleText', 'Text', 'Hello World'
)
```
