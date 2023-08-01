# Menu [![Build Status](https://api.cirrus-ci.com/github/helloSystem/Menu.svg)](https://cirrus-ci.com/github/helloSystem/Menu) [![Translation status](https://hosted.weblate.org/widgets/hellosystem/-/menu/svg-badge.svg)](https://hosted.weblate.org/engage/hellosystem/) [![Help Wanted Issues](https://img.shields.io/github/issues/helloSystem/Menu/help%20wanted)](https://github.com/helloSystem/Menu/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22)

![](Menubar.png)

Global menu bar written in Qt with as few Kf5 dependencies as possible

On FreeBSD:

![](https://user-images.githubusercontent.com/2480569/95656631-f4898400-0b0f-11eb-8337-f9041f75cb20.png)

![](https://user-images.githubusercontent.com/2480569/94789725-96ed8d00-03d5-11eb-95e8-7f17f6166de4.png)

On FreeBSD-based LIVEstep:

![](https://user-images.githubusercontent.com/2480569/94831116-d8912e80-03fb-11eb-9f89-e11f50a49571.png)

## Dependencies

On Alpine Linux:

```
apk add --no-cache qt5-qtbase-dev kwindowsystem-dev qt5-qttools-dev kdbusaddons-dev baloo-dev xcb-util-wm-dev libdbusmenu-qt-dev procps-dev git cmake musl-dev alpine-sdk clang
```

On Arch Linux:

```
sudo pacman -Sy
sudo pacman -S cmake gcc qt5-base qt5-tools qt5-svg qt5-x11extras kwindowsystem baloo libxtst libxdamage libxcomposite libxrender libxcb xcb-util libdbusmenu-qt5 kdbusaddons libpulse glibc make pkgconf icu
```

__Note__ that not all functionality is implemented for anything else than FreeBSD at this point. So building the Menu for operating systems other than FreeBSD will result in partly non-functional menu, currently only usable for development.

On FreeBSD:

```
sudo pkg install -y cmake pkgconf qt5-x11extras qt5-qmake qt5-widgets qt5-buildtools kf5-kdbusaddons kf5-kwindowsystem kf5-baloo libdbusmenu-qt5 qt5-concurrent qt5-qtmultimedia-dev libfm-dev libfm-extra-dev menu-cache-dev
```

## Build

```
mkdir build
cd build
cmake ..
make
sudo make install
```

## How to use

### Qt applications (KDEPlasmaPlatformTheme.so and libqgtk3.so work, plain Qt and qt5ct don't work yet)

Currently only works with `QT_QPA_PLATFORMTHEME=kde` and https://github.com/helloSystem/QtPlugin using `QT_QPA_PLATFORMTHEME=panda`. FIXME: Make it work with e.g., `QT_QPA_PLATFORMTHEME=qt5ct`.

For this, it would probably need to support the __Unity protocol__?

### Gtk applications (Inkscape, GIMP, Audacity work)

Firefox, Thunderbird need special [patches](https://aur.archlinux.org/packages/firefox-appmenu?O=50) to enable the global menu functionality. The Firefox derivative [Pale Moon](https://www.palemoon.org/) has it built it, but it needs to be specifically [enabled](https://github.com/helloSystem/Utilities/issues/183#issuecomment-1656432129).

Prerequisite: https://www.freshports.org/x11/gtk-app-menu/

E.g., Firefox and Chrome use

```
/usr/local/lib/gtk-2.0/modules/libappmenu-gtk-module.so
/usr/local/lib/gtk-3.0/modules/libappmenu-gtk-module.so
```

On FreeBSD:

```
git clone https://github.com/NorwegianRockCat/FreeBSD-my-ports
cd FreeBSD-my-ports/
cd x11/gtk-app-menu/
sudo make
sudo make install
```

`gmenudbusmenuproxy` https://phabricator.kde.org/D10461?id=28255

```
user@FreeBSD$ sudo pkg which /usr/local/bin/gmenudbusmenuproxy 
/usr/local/bin/gmenudbusmenuproxy was installed by package plasma5-plasma-workspace-5.19.2
```

Would be nice if it was packaged "standalone", without KDE Plasma.


## Baloo file system search

If `baloosearch` returns results, then they are shown in Menu.


## License

menubar is licensed under GPLv3.


## Also see

https://github.com/cutefishos/statusbar/
