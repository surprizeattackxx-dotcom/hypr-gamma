# hyprgamma

> Per-monitor brightness, contrast, and gamma control for [Hyprland](https://hyprland.org) — with a companion GTK4 GUI for live tweaking.

![Hyprland](https://img.shields.io/badge/Hyprland-0.55%2B-blue?logo=wayland&logoColor=white)
![Language](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)
![GUI](https://img.shields.io/badge/GUI-GTK4-4A86CF?logo=gnome&logoColor=white)
![License](https://img.shields.io/badge/license-BSD--3--Clause-green)

hyprgamma applies a software color ramp to each output through Hyprland's
gamma LUT, letting you set **brightness**, **contrast**, and **per-channel
gamma** independently for every monitor. Settings are saved to disk and
re-applied automatically on startup and on monitor hotplug.

It's handy when:

- one of your monitors is dimmer or warmer than the others and you want them to match,
- your panel has no usable hardware brightness control (most desktop displays),
- you want a quick contrast/gamma curve without an ICC profile or redshift-style tool.

---

## Contents

- [Demo](#demo)
- [Requirements](#requirements)
- [Install (hyprpm)](#install-hyprpm)
- [Manual build](#manual-build)
- [Usage](#usage)
  - [hyprctl commands](#hyprctl-commands)
  - [Keybinds](#keybinds)
  - [The GTK4 GUI](#the-gtk4-gui)
- [Configuration file](#configuration-file)
- [How it works](#how-it-works)
- [Troubleshooting](#troubleshooting)
- [Uninstall](#uninstall)
- [License](#license)

---

## Demo

> _Screenshot / GIF of the GUI goes here._ Drop an image at `assets/gui.png` and it will render below:

<!-- ![hyprgamma GUI](assets/gui.png) -->

---

## Requirements

- **Hyprland 0.55.x** (the plugin is ABI-locked to the Hyprland it was built against — see [Troubleshooting](#troubleshooting))
- `hyprpm` (ships with Hyprland) for the easy install path
- For a manual build: `cmake` (≥ 3.19), a C++23 compiler, the Hyprland headers, and `nlohmann_json`
- For the optional GUI: `gtk4` and `json-glib`

On Arch:

```sh
sudo pacman -S --needed base-devel cmake nlohmann-json gtk4 json-glib
```

---

## Install (hyprpm)

This is the recommended path — `hyprpm` builds the plugin against your exact
Hyprland version, so it stays in sync across updates.

```sh
# add and build the plugin repo
hyprpm add https://github.com/surprizeattackxx-dotcom/hypr-gamma

# enable it
hyprpm enable hyprgamma
```

Then make sure your plugins load on login. Add this to `hyprland.conf` if it
isn't there already:

```ini
exec-once = hyprpm reload -n
```

After a Hyprland update, refresh the plugin so it's rebuilt against the new version:

```sh
hyprpm update
```

You should see a green **`[hyprgamma] Loaded!`** notification when it starts.

---

## Manual build

Useful for development or if you'd rather not use `hyprpm`.

```sh
git clone https://github.com/surprizeattackxx-dotcom/hypr-gamma
cd hypr-gamma

# build the plugin
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build -j"$(nproc)"
# -> build/libhyprgamma.so

# load it for the current session
hyprctl plugin load "$PWD/build/libhyprgamma.so"
```

> ⚠️ A manually built `.so` **must** be compiled against the headers of the
> exact Hyprland version you're running, or the plugin will refuse to load with
> a version-mismatch notification. `hyprpm` handles this for you; manual builds
> are on you.

---

## Usage

The plugin registers a set of `hyprctl` commands (all prefixed `hyprgamma:`)
and two dispatchers for keybinds. Per-monitor settings are persisted
automatically after every change.

### hyprctl commands

| Command | Description |
| --- | --- |
| `hyprctl hyprgamma:list` | List every monitor with its current settings (human-readable). |
| `hyprctl -j hyprgamma:list` | Same, as JSON (used by the GUI / scripts). |
| `hyprctl hyprgamma:set <monitor> <key> <value>` | Set one parameter on a monitor. |
| `hyprctl hyprgamma:enable <monitor>` | Re-apply this monitor's saved curve. |
| `hyprctl hyprgamma:disable <monitor>` | Drop this monitor back to an identity ramp. |
| `hyprctl hyprgamma:reset [monitor]` | Reset one monitor (or all, if omitted) to defaults. |
| `hyprctl hyprgamma:save` | Force-write the config file. |
| `hyprctl hyprgamma:reload` | Reload the config file from disk and re-apply. |

**Keys** accepted by `set`:

| Key | Range (typical) | Meaning |
| --- | --- | --- |
| `brightness` | `0.0` – `2.0` | Linear multiplier on output (1.0 = unchanged). |
| `contrast` | `0.0` – `2.0` | Expansion/compression around mid-grey (1.0 = unchanged). |
| `gamma` | `0.1` – `5.0` | Gamma exponent applied to **all three** channels at once. |
| `gamma_r` | `0.1` – `5.0` | Red-channel gamma only. |
| `gamma_g` | `0.1` – `5.0` | Green-channel gamma only. |
| `gamma_b` | `0.1` – `5.0` | Blue-channel gamma only. |

Find your monitor names with `hyprctl monitors` (e.g. `DP-1`, `HDMI-A-1`, `eDP-1`).

**Examples**

```sh
# dim the second monitor to 80%
hyprctl hyprgamma:set DP-2 brightness 0.8

# warm up a too-blue panel (lift red, drop blue)
hyprctl hyprgamma:set HDMI-A-1 gamma_r 0.9
hyprctl hyprgamma:set HDMI-A-1 gamma_b 1.2

# punch up contrast on the laptop screen
hyprctl hyprgamma:set eDP-1 contrast 1.15

# undo everything on every monitor
hyprctl hyprgamma:reset
```

### Keybinds

Two dispatchers are exposed for `bind`:

```ini
# hyprland.conf
bind = $mainMod SHIFT, G, hyprgamma:reset    # reset all monitors to defaults
bind = $mainMod SHIFT, R, hyprgamma:reload   # reload config from disk

# bind step adjustments straight to hyprctl, too:
bind = , XF86MonBrightnessUp,   exec, hyprctl hyprgamma:set eDP-1 brightness 1.1
bind = , XF86MonBrightnessDown, exec, hyprctl hyprgamma:set eDP-1 brightness 0.9
```

### The GTK4 GUI

A small companion app gives you live sliders for every monitor — it talks to
the plugin over `hyprctl`, so the plugin must be loaded for it to do anything.

```sh
cd gui
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build -j"$(nproc)"
./build/hyprgamma-gui
```

Each detected monitor gets a card with an enable toggle, brightness/contrast
sliders, per-channel gamma sliders, and a **Reset Monitor** button. Changes
apply instantly and are saved automatically.

---

## Configuration file

Settings live at:

```
$XDG_CONFIG_HOME/hypr/hyprgamma.json     # or ~/.config/hypr/hyprgamma.json
```

You normally never edit this by hand — the plugin and GUI manage it — but it's
plain JSON if you want to version it or sync it across machines:

```json
{
  "version": 1,
  "defaults": {
    "brightness": 1.0,
    "contrast": 1.0,
    "gamma_r": 1.0,
    "gamma_g": 1.0,
    "gamma_b": 1.0,
    "enabled": false
  },
  "monitors": {
    "DP-1": {
      "brightness": 0.85,
      "contrast": 1.1,
      "gamma_r": 1.0,
      "gamma_g": 1.0,
      "gamma_b": 1.05,
      "enabled": true
    }
  }
}
```

The config is loaded and applied on plugin init, and re-applied whenever a
monitor is hotplugged.

---

## How it works

For each monitor, hyprgamma builds a `gammaSize × 3` lookup table and pushes it
to the output via Hyprland's `setGammaLut`. For a normalized input `n ∈ [0,1]`
on each channel:

```
out = clamp( ( pow(n, gamma) - 0.5 ) * contrast + 0.5 ) * brightness , 0, 1 )
```

A disabled monitor is given a plain identity ramp, so toggling `enabled` is a
lossless on/off without discarding your saved values. Outputs that report a
gamma table smaller than 10 entries (i.e. no real gamma support) are skipped.

---

## Troubleshooting

**`[hyprgamma] Version mismatch` notification / plugin won't load.**
Hyprland plugins are tied to the exact compositor build. Run `hyprpm update` to
rebuild against your current Hyprland, or recompile a manual build against the
matching headers.

**A command returns `error: gamma manager not initialized`.**
The plugin didn't finish loading. Check `hyprctl plugin list` and your startup
notification.

**Nothing happens on a particular monitor.**
That output may not expose a usable gamma LUT (some virtual/remote/HDR-pipeline
outputs don't). hyprgamma silently skips outputs with no gamma support.

**Colors look washed out at high brightness.**
`brightness > 1.0` multiplies values and will clip highlights — that's expected.
Use it to brighten dim panels, not to push past 1.0 on an already-bright one.

---

## Uninstall

```sh
hyprpm disable hyprgamma
hyprpm remove hyprgamma
```

For a manual load, just unload it: `hyprctl plugin unload "$PWD/build/libhyprgamma.so"`.

---

## License

[BSD-3-Clause](LICENSE) © donnie
