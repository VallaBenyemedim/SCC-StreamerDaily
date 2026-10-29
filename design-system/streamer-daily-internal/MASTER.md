# Streamer Daily Internal — Design System

## Direction

Premium dark desktop control panel with dense bento cards, layered glass-like surfaces and short functional motion. The interface must remain readable over a moving game scene and must never depend on color alone.

## Tokens

| Role | Value |
|---|---|
| Background | `#050609` |
| Sidebar | `#07080D` |
| Surface | `#0B0D14` |
| Raised surface | `#11131C` |
| Border | `#252A38` |
| Primary text | `#F4F6FB` |
| Muted text | `#999FAD` |
| Violet accent | `#7D63FA` |
| Cyan accent | `#29C2EF` |
| Success | `#38D18C` |
| Warning | `#F5B343` |
| Danger | `#F54F61` |

Alternative accent pairs are Digital Cyan (`#1AC2E0` + `#42E096`) and Premium Gold (`#E8A845` + `#F06E4F`). Semantic success, warning and danger colors do not change with the accent theme.

## Typography

- Segoe UI, 16 px: body and controls
- Segoe UI Semibold, 16 px: navigation and control titles
- Segoe UI Semibold, 25 px: page titles and key metrics
- Normal body text must retain at least 4.5:1 contrast against its actual composed surface.

## Geometry and rhythm

- Spacing follows a 4/8 px rhythm: 8, 16, 24 and 32 px tiers.
- Main window: default `1020 × 660`, minimum `900 × 580`, resizable.
- Sidebar: 218 px; header: 88 px.
- Cards use 13 px rounding; the main window uses 16 px.
- Primary controls are at least 34 px high. Toggle rows expose the full 54 px row as the hit area.

## Interaction

- Navigation, buttons and toggle rows expose hover, pressed and keyboard-focus states without changing layout bounds.
- Page changes use a short fade/vertical transition. Toggle thumbs and hover fills use exponential interpolation.
- Reduced Motion immediately resolves all non-essential transitions and disables status pulsing.
- `Insert` and the header close control both hide the menu. The System page owns save, reload and clean unload actions.

## Visual discipline

- Structural icons are vector primitives with a consistent 1.65 px outline; no emoji.
- Status always has a text label in addition to its colored indicator.
- Destructive actions remain isolated in a danger card and use the fixed danger color.
- Accent color is reserved for active navigation, primary actions, selected themes and key values.
