# Codex WeekUsage Tray Design System

## 1. Atmosphere & Identity

Codex WeekUsage Tray is a small, calm command display for a developer's weekly limit. It feels like a dark terminal panel lit by the supplied Codex mark: lavender light at the top fades into a clear electric blue at the bottom. The signature is a bright, large remaining number inside a thin illuminated frame, so the answer is easy to find in one glance.

## 2. Color

| Role | Token | Value | Usage |
| --- | --- | --- | --- |
| Surface | `Surface` | `#080A17` | Panel background |
| Surface raised | `SurfaceRaised` | `#101336` | Secondary button fill |
| Text | `Text` | `#F8F8FF` | Main copy |
| Text muted | `TextMuted` | `#B9C1E9` | Supporting copy |
| Text dim | `TextDim` | `#7E86B3` | Section label |
| Codex glow start | `GlowStart` | `#95A2FF` | Top of the supplied Codex-mark gradient |
| Codex glow middle | `GlowMiddle` | `#566AFF` | Frame, hover, and number gradient |
| Codex glow end | `GlowEnd` | `#4A59FE` | Bottom of the supplied Codex-mark gradient |
| Error | `Error` | `#FF9AA8` | Short error copy |

The three glow values are sampled from the user-supplied Codex icon. No green accent is used.

## 3. Typography

| Level | Font | Size | Weight | Usage |
| --- | --- | --- | --- | --- |
| Metric | Cascadia Mono, monospace fallback | 32px | Bold | Remaining percentage and sign-in state |
| Heading | Cascadia Mono, monospace fallback | 13px | Bold | `CODEX` |
| Label | Cascadia Mono, monospace fallback | 11px | Regular | Limit title and details |
| Action | Cascadia Mono, monospace fallback | 11px | Bold | Buttons |

Text uses short English sentences and familiar words. Numbers use a monospaced face so they do not jump when updated.

## 4. Spacing & Layout

The panel is a 368px by 282px DPI-scaled tray popover. A 16px outer inset creates a single vertical stack: product label, metric, three detail rows, then actions. The bottom action row keeps `Refresh` directly left of `Close`; sign-in states add `Sign in` before them. All spacing follows a 4px unit.

## 5. Components

### Neon panel

- **Structure:** `CODEX` identifier, `WEEKLY LIMIT` label, primary metric, details, action row.
- **States:** signed in, sign-in needed, no limit, short error.
- **Accessibility:** visible text labels; standard WinForms tab order; high-contrast primary text; borderless form is named `Codex weekly limit` for assistive tools.
- **Layout:** one fixed-width column, repositioned by the existing tray-anchor logic.

### Action button

- **Variants:** primary (`Sign in`), secondary (`Refresh`, `Check`, `Close`).
- **States:** default, hover, pressed, disabled.
- **Accessibility:** each action has a visible English text label and keyboard activation.
- **Layout:** 36px height; `Close` is always the right-most action.

## 6. Motion & Interaction

The panel has no decorative animation. Buttons use native hover, press, focus, and disabled states. This keeps a tray utility quick and avoids movement that could distract from the number.

## 7. Depth & Surface

The surface uses a dark tonal base with a two-pass blue frame: a soft translucent outer glow and a crisp inner line. Small corner bars and a low-opacity blue light field create terminal-like depth without shadows or a pasted image.

## 8. Accessibility Constraints & Accepted Debt

- Target: WCAG 2.2 AA contrast for readable text; primary white text and light-blue metric meet the dark surface contrast requirement.
- Keyboard: `Tab`, `Shift+Tab`, `Enter`, and `Space` reach and activate all buttons.
- Text: panel strings remain simple English and do not rely on color alone to convey a state.
- DPI: verify the popover at Windows 100%, 125%, and 150% scaling with no clipped controls.

### Accepted Debt

| Item | Location | Why accepted | Owner / Exit |
| --- | --- | --- | --- |
| No animated panel entrance | Tray popover | The user asked for a fast status panel; animation would not add meaning. | Revisit only if a user requests motion. |
