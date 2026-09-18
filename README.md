# OmaText

A minimal text editor for Omarchy. A normal window, your current theme, and one small gear in the corner. No bar widget, tabs, or Markdown preview.

![OmaText running in a clean virtual machine](preview.png)

## Install

On **Omarchy 4** with **Python 3**, install and enable the plugin:

```sh
omarchy plugin add https://github.com/komagata/OmaText.git --enable
```

No compiler, build command, or Python package installation is required. The plugin uses QML and a Python 3 standard-library helper. There are no bundled native binaries or CPU-specific builds.

Verified on Omarchy 4.0.3 / Quickshell 0.3.1 / Qt 6.11.2 in an x86_64 VM. ARM64 has not been tested; it still requires a compatible Omarchy and Quickshell installation.

Register the application launcher and terminal command once:

```sh
python3 "${XDG_CONFIG_HOME:-$HOME/.config}/omarchy/plugins/io.github.komagata.omatext/scripts/setup-launcher.py"
```

Then choose **OmaText** in your application launcher, or run:

```sh
omatext
omatext "notes/my document.md"
```

The setup creates `~/.local/bin/omatext` and an application entry under `${XDG_DATA_HOME:-$HOME/.local/share}/applications`. Omarchy normally includes `~/.local/bin` in `PATH`; setup reports if it is missing. Registration is optional and requires no build. The standard plugin installer does not register these launchers automatically.

## Open text and Markdown files

The launcher accepts one local filename. Its desktop entry registers support for `.txt` and Markdown files, so you can select **OmaText** in your file manager's **Open With** menu. To make it your default explicitly:

```sh
xdg-mime default io.github.komagata.omatext.desktop text/plain
xdg-mime default io.github.komagata.omatext.desktop text/markdown
xdg-mime default io.github.komagata.omatext.desktop text/x-markdown
```

Launcher setup does not change your default applications. The commands above change the user MIME associations. To undo them, choose another default editor in your file manager before removing OmaText.

Opening a file uses the existing editor window. Unsaved changes offer Save, Don't Save, or Cancel; opening the same file again preserves your edits. Finish any open dialog or file operation before opening another file externally. With no filename, the launcher only shows the current document.

## Use

Hover over the bottom-left gear to reveal **Settings**, **New**, **Open**, and **Save**. All four controls are icons; their tooltips show the action and shortcut.

| Action | Shortcut |
| --- | --- |
| Settings | Ctrl+, |
| New | Ctrl+N |
| Open | Ctrl+O |
| Save | Ctrl+S |
| Save As | Ctrl+Shift+S |
| Close | Ctrl+Q |
| Undo / Redo | Ctrl+Z / Ctrl+Shift+Z |
| Select all / Copy / Cut / Paste | Ctrl+A / Ctrl+C / Ctrl+X / Ctrl+V |

Settings lets you search for a font and choose a font size. Changes preview in the document. Select the checkmark to apply or the cross to cancel. **Omarchy default** follows the shell's font and text size. Preferences are saved in `~/.config/omatext/editor.ini`.

The editor opens regular UTF-8 files up to 2 MiB, preserves UTF-8 BOM and CRLF line endings, and saves atomically. Unsaved changes trigger a confirmation before creating, opening, or closing a document. Japanese IME input is supported.

## Update and remove

Save your document before updating or removing the plugin:

```sh
omarchy plugin update io.github.komagata.omatext
python3 "${XDG_CONFIG_HOME:-$HOME/.config}/omarchy/plugins/io.github.komagata.omatext/scripts/setup-launcher.py"
```

For an early version copied with `scripts/install-local.py`, remove the old plugin after saving your document, then use the Install command above. That early copy is not a Git checkout.

If the shell retains an older QML component after an update, save your work and log out and back in. When upgrading from the early C++ version, a new session also unloads its old native library.

To remove:

```sh
python3 "${XDG_CONFIG_HOME:-$HOME/.config}/omarchy/plugins/io.github.komagata.omatext/scripts/setup-launcher.py" --remove
omarchy plugin remove io.github.komagata.omatext
```

Saved documents and `~/.config/omatext/editor.ini` are kept. If you installed an older version using `scripts/install-local.py`, also remove its optional launcher:

```sh
rm -f -- "${XDG_DATA_HOME:-$HOME/.local/share}/applications/io.github.komagata.omatext.desktop"
```

## Development and tests

Installing and using OmaText does not require a build. The optional UI test harness uses Qt Test and a C++17 compiler; the file-helper tests use Python's standard library.

From a local source checkout:

```sh
./tests/run
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The UI tests require Qt 6 Core, Gui, Qml, Quick, QuickControls2 and Test, plus CMake. These are developer dependencies only.

## How it works

OmaText runs inside Omarchy's existing Quickshell process and opens a regular Qt Quick window. It directly uses `qs.Commons` and `qs.Ui` for theme colors, typography, square borders, and controls. QML maintains document and unsaved-change state. A short-lived Python helper reads or atomically saves a file, communicating through JSON on stdin and stdout. Document content is never placed in command-line arguments. The Omarchy component copies under `tests/support` are for headless tests only.

The `omatext` command internally invokes the shell IPC command to show the editor and optionally pass a local file URL as JSON. Filenames are never interpreted as shell code; no plugin ID or JSON argument is needed from the user. The optional launcher setup only creates the two launcher files described above and refuses to overwrite unrelated files.

The plugin does not start a persistent background service or make network requests. Python runs only during a file operation. It writes documents only when you save, plus its font preferences. Closing the window retains the current document while the shell is running; restarting the shell, reloading/disabling the plugin, or logging out does not restore unsaved work. There is no autosave or external-file change monitoring.

See [implementation and verification](docs/implementation.md) and [VM screenshot instructions](docs/screenshots.md) for the tested scope.

## License and support

[MIT License](LICENSE). The test-only Omarchy components retain their [upstream license](tests/support/vendor/OMARCHY-LICENSE).

Report bugs and suggestions in [Issues](https://github.com/komagata/OmaText/issues).
