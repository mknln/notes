## QtNotes

A notes app that aims to be simple, written in Qt (QML)/C++.

Features include:
* Making notes (auto-saved while typing)
* Formatting and export to Markdown
* Zoom in/out
* Common keyboard shortcuts (press `Ctrl`+`/` or use Menu>Help>Keyboard Shortcuts to see them)
* Import/Export (saved as a ZIP file)

<img width="1521" height="956" alt="NotseApp" src="https://github.com/user-attachments/assets/4ddc7b33-aaaa-4ef8-9505-5d7fd42c92b9" />

## Building

You need:

* Qt 6.8 or later, with Qt Quick, Qt Quick Controls and Qt's private headers. The [Qt Online Installer](https://www.qt.io/download-qt-installer-oss) includes all of these. If you use your Linux distribution's Qt instead, the private headers are usually a separate package, such as `qt6-base-private-dev` on Debian and Ubuntu or `qt6-qtbase-private-devel` on Fedora.
* CMake 3.21 or later and [Ninja](https://ninja-build.org/).
* A C++17 compiler: GCC or Clang on Linux, Xcode's Clang on macOS, or Visual Studio 2022 (MSVC) on Windows. On Windows, run the commands below from a *Developer PowerShell for VS 2022* or *x64 Native Tools Command Prompt*, so CMake finds the compiler.

Then, from the repository's folder, configure, build and run with one of the `debug`, `relwithdebinfo` or `release` presets:

```sh
cmake --preset release -DCMAKE_PREFIX_PATH=<path to Qt>
cmake --build --preset release
```

`<path to Qt>` is the folder for your compiler inside the Qt installation, for example `~/Qt/6.8.3/gcc_64` on Linux, `~/Qt/6.8.3/macos` on macOS or `C:\Qt\6.8.3\msvc2022_64` on Windows. You can leave it out if CMake finds Qt by itself, e.g. a distribution's Qt on Linux.

The app is built in `build/release`: `qtnotes` on Linux, `qtnotes.app` on macOS, `qtnotes.exe` on Windows. On Windows, it only runs from there if Qt's `bin` folder is on your `PATH`.

To keep your Qt path out of each command, put it in a `CMakeUserPresets.json` next to `CMakePresets.json`. Git ignores this file, so it's yours alone. For example:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "my-release",
      "inherits": "release",
      "cacheVariables": { "CMAKE_PREFIX_PATH": "/home/me/Qt/6.8.3/gcc_64" }
    }
  ],
  "buildPresets": [{ "name": "my-release", "configurePreset": "my-release" }]
}
```

Qt Creator, VS Code's CMake Tools and Visual Studio list these presets too.

### Installing

```sh
cmake --install build/release --prefix <folder>
```

On Windows and macOS, this also copies the Qt libraries the app needs, so the installed app runs on machines without Qt. On Linux, it only installs the app, to use with the system's Qt; add `-DQTNOTES_DEPLOY_QT=ON` when configuring to bundle Qt there too.

### Debian and Ubuntu package

The `debian` folder builds a `.deb` package that uses the system's Qt. That needs Qt 6.8 or later, so it works on Debian 13 (trixie) and Ubuntu 25.04 or later, but not on Debian 12 or Ubuntu 24.04.

From the repository's folder:

```sh
sudo apt install dpkg-dev
sudo apt build-dep ./
dpkg-buildpackage -us -uc -b
sudo apt install ../qtnotes_*.deb
```

The package is written to the folder above the repository. To release a new version, add an entry to `debian/changelog` (e.g. with `dch -v 0.2.0` from the `devscripts` package) and change the version in `CMakeLists.txt` to match.
