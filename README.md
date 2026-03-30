# waybar-undertale

### what is this?

uhh idk i was bored, i have no idea why i'm putting this on github<br>
basically this puts a simple undertale fight into your waybar for no reason

[Preview](preview.mp4)

#### Configuration

This fork adds only one module: `undertale`.

It has the following configuration options (configured like any other waybar module in `~/.config/waybar/config.jsonc`:

```jsonc
"undertale": {
  "path": "/path/to/assets/", // path to a folder containing all the PNGs (they are not provided in this repo cause idk about licensing)
  "interval": 0.05, // amount of seconds a tick takes (I use this with 0.05)
  "signal": 4, // when you send the SIGRTMIN+{signal} to the waybar process, it will update this module (used mostly for unpause)
  "width": 200 // I didn't test it with anything other than 200, this option may not even work lol
}
```

You should also add this to your `~/.config/waybar/config.jsonc` because reasons:

```css
#undertale-overlay {
  background: transparent;
}
```

By the way here's the list of assets that are required to be in the asset folder:

- act.png
- act_selected.png
- fight.png
- fight_selected.png
- heartbreak.png
- hp.png
- item.png
- item_selected.png
- mercy.png
- mercy_selected.png
- soul.png
- Determination.ttf

Oh, and this only runs on hyprland with some very specific configs because I'm lazy.
Anyways, here's the relevant part of my `~/.config/hypr/hyprland.conf`:
(It doesn't have to be these specific binds)

```
submap = undertale
bind = $mainMod, C, exec, hyprctl setcursor Adwaita 16 # insert your cursor theme here
bind = $mainMod, C, submap, reset
submap = reset


bind = $mainMod, C, exec, hyprctl setcursor Blank 16 # this theme just makes the cursor invisible
bind = $mainMod, C, exec, hyprctl dispatch submap undertale && pkill -SIGRTMIN+4 waybar

windowrule {
  name = undertale-waybar-module
  match:title = waybar-undertale
  no_blur = on
  no_focus = on
  move = 0 0
  float = on
  border_size = 0
  pin = on
}
```

### Installation

I have no idea why you would even want this.
Just build from source.

#### Building from source

```bash
$ git clone https://github.com/TarLaboratories/waybar-undertale
$ cd Waybar
$ meson setup build
$ ninja -C build
$ ./build/waybar
# If you want to install it
$ ninja -C build install
$ waybar
```

**Dependencies**

```
gtkmm3
jsoncpp
libsigc++
fmt
wayland
chrono-date
spdlog
libgtk-3-dev [gtk-layer-shell]
gobject-introspection [gtk-layer-shell]
libgirepository1.0-dev [gtk-layer-shell]
libpulse [Pulseaudio module]
libnl [Network module]
libappindicator-gtk3 [Tray module]
libdbusmenu-gtk3 [Tray module]
libmpdclient [MPD module]
libsndio [sndio module]
libevdev [KeyboardState module]
xkbregistry
upower [UPower battery module]
```

**Build dependencies**

```
cmake
meson
scdoc
wayland-protocols
```

On Ubuntu, you can install all the relevant dependencies using this command (tested with 19.10 and 20.04):

```
sudo apt install \
  clang-tidy \
  gobject-introspection \
  libdbusmenu-gtk3-dev \
  libevdev-dev \
  libfmt-dev \
  libgirepository1.0-dev \
  libgtk-3-dev \
  libgtkmm-3.0-dev \
  libinput-dev \
  libjsoncpp-dev \
  libmpdclient-dev \
  libnl-3-dev \
  libnl-genl-3-dev \
  libpulse-dev \
  libsigc++-2.0-dev \
  libspdlog-dev \
  libwayland-dev \
  scdoc \
  upower \
  libxkbregistry-dev
```

On Arch, you can use this command:

```
pacman -S --asdeps \
  gtkmm3 \
  jsoncpp \
  libsigc++ \
  fmt \
  wayland \
  chrono-date \
  spdlog \
  gtk3 \
  gobject-introspection \
  libgirepository \
  libpulse \
  libnl \
  libappindicator-gtk3 \
  libdbusmenu-gtk3 \
  libmpdclient \
  sndio \
  libevdev \
  libxkbcommon \
  upower \
  meson \
  cmake \
  scdoc \
  wayland-protocols \
  glib2-devel
```
