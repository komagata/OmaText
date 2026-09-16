# Implementation and verification

OmaText is an Omarchy shell plugin with a regular editor window and no bar widget. The public repository contains the implementation, tests, and demo screenshots.

## Architecture

`ui/Editor.qml` receives the host's `open` and `close` calls. `ui/EditorView.qml` provides the normal Qt Quick window. The plugin stays loaded so reopening it retains the document while the shell remains running.

The UI directly imports Omarchy's `qs.Commons` and `qs.Ui`. Colors, typography, square borders, buttons, and font selectors come from those modules. File dialogs use Qt Quick Dialogs with the native platform picker disabled. The clean VM reproduced a GTK/GVfs abort before the file helper ran; keeping the picker inside Qt avoids that crashing path. Text and filenames are rendered as plain text.

File handling accepts local regular UTF-8 files up to 2 MiB, rejects special files and invalid UTF-8, preserves CRLF and UTF-8 BOM, and saves atomically. A failed save retains the editor buffer. Unsaved-change confirmation supports Save, Don't Save, and Cancel.

QML owns the document state; `scripts/file_io.py` performs each file operation in a short-lived Python process. Production has no C++ module or architecture-specific binary. A C++ transport adapter is used only by the optional Qt UI tests. The VM exercises the actual Quickshell process transport.

## Source research

- [Percius04/omafiles at d8bde32](https://github.com/Percius04/omafiles/tree/d8bde32df743b9a9fa3e64ed728f636837d6f0f3) builds a standalone Qt Quick executable with a C++ backend. Its `app/qml_modules/qs` directory contains adapted components; `Commons/ThemeSource.qml` polls theme files without Quickshell.Io. It is an adaptation, not evidence that standalone Qt applications can directly import the running shell's modules.
- Omawrite at commit `8f98892b26768236b2c20f4e637cf4b102d898bf` uses Qt Quick and C++, as shown by `omawrite.pro`, `src/main.cpp`, and `src/Main.qml`.
- OmaText follows Omarchy's hosted plugin contract rather than launching another Quickshell process.

The Omarchy components under `tests/support/vendor` are test-only copies. Their license and upstream hashes are included alongside them.

## Verified behavior

The original implementation was tested on Omarchy 4.0.3, Quickshell 0.3.1, and Qt 6.11.2. Tests cover file round trips, invalid input, special-file rejection, failed saves, unsaved-change decisions, editor input, hover controls, modal focus, font previews, cancellation, and preference persistence.

Live checks confirmed Japanese input through Fcitx5/Mozc, saving and reopening a Unicode filename, Save As, all three unsaved-change choices, undo, reopening the editor, and preserving modal focus when summoned again. Unicode regression fixtures use escaped code points so source files remain English.

The minimal UI was also checked live: one gear at rest; four icons on hover; font search; size preview; apply/cancel; and New, Open, Save, and Settings shortcuts. The settings test applied Liberation Mono at 20 px, previewed 22 px, cancelled back to 20 px, and restored the Omarchy default.

## Screenshots

Public screenshots must come from the dedicated VM and contain fictional English text. See [capture instructions](screenshots.md) for environment and provenance. Earlier host captures and the Japanese IME screenshot have been removed. The four current screenshots show the QML/Python version running in the VM.

## Limits

- No tabs, Markdown preview, autosave, or external-file change monitoring.
- Unsaved documents are not recovered after shell restart, plugin reload/disable, or logout.
- UTF-8 only. Mixed line endings are not guaranteed to retain their exact original bytes.
- Large pasted buffers and extremely long lines have not been performance-tested.
- Live theme switching and multi-monitor behavior have not been tested.
- QML lint reports dynamic type and unqualified-reference warnings; runtime testing is not a claim of a warning-free static check.

The host can retain cached QML after a plugin update. Save documents before updating. Never restart a user's shell to collect a screenshot or refresh a development build.
