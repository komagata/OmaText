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

## File associations (0.3.0)

The optional launcher accepts one filename, converts it to an absolute file URL, and sends it as a JSON IPC payload using an argument array. The desktop entry advertises plain text and Markdown MIME types and forwards a single local file through `%f`. Registering the launcher does not change MIME defaults; the README gives explicit opt-in `xdg-mime` commands.

The document state machine holds a requested file through unsaved-change confirmation. Cancel and failed saves clear that request; Save finishes before opening the requested file. Encoded and decoded forms of the current URL are normalized so reopening the same file keeps edits. Non-file URLs are ignored. Requests received while a modal dialog or file operation is active do not replace that operation.

Current test scope is recorded in `artifacts/file-associations-0.3.0.json`. The existing preview and clean-VM evidence remain historical and are not claimed as 0.3.0 VM validation.

### xdg-utils compatibility (0.3.1)

The desktop entry uses `Exec=omatext %f`, resolved through Omarchy's PATH. The generic xdg-utils backend splits Exec fields without decoding quotes, so a quoted absolute executable path is not reliably recognized or launched. A regression test installs the launcher into a temporary home containing spaces, queries its MIME default, and opens a Japanese text filename through actual `xdg-mime` and `xdg-open` commands with a recording IPC helper. Launcher setup still reports when `~/.local/bin` is absent from PATH.

On Omarchy 4.0.4, both `.txt` and `.md` were opened through `xdg-open` in the daily-use shell. The editor reported each expected URL and content length, with no pending operation or unsaved modification. All three user MIME defaults resolve to OmaText.

## Folder sidebar

`FileTree.qml` uses its own `FileIo` instance, independent of document reads and saves. It requests one directory at a time. A generation number discards old root responses and keeps only the latest queued root. The IO timeout keeps the request occupied until the process exits, so a late exit cannot consume a newer queued request. Opening a file commits IME input and routes through `Document.requestFile`; selection follows the successfully loaded document URL, including encoded Unicode filenames. Folder changes, errors, and sidebar visibility do not modify document state. Ctrl+B and the Settings file-tree action restore editor focus when hiding the sidebar; modal dialogs disable both folder and sidebar shortcuts. The sidebar is hidden on first launch, and explicit visible and hidden preferences survive plugin recreation. Showing it with Ctrl+B focuses the tree; Tab traversal also reaches it. A thin border marks the keyboard cursor independently of the loaded-file selection fill. Folder/file outlines occupy a fixed icon column beside the expansion glyph. Row indices are clamped after collapse or root replacement. Sidebar visibility joins the existing Settings preferences; folders and document content are not persisted.

The helper scans at most 1,024 entries per directory, returns at most 256 entries and approximately 120 KB of JSON entry data, and never recursively scans. The tree cache holds at most 4,096 entries. Hidden dotfiles, all symbolic links, and special files are omitted. Each directory path component is opened with `O_NOFOLLOW` and `O_DIRECTORY` before scanning, preventing symlink-directory traversal even during replacement races. Listings sort folders first within the bounded subset. Permission and deleted-folder errors leave document text untouched and offer Retry. No file management operations are provided.

The sidebar starts at 240 logical pixels. Its one-pixel divider has a transparent eight-pixel horizontal drag area. Dragging commits IME input without moving focus or changing text selection; modal dialogs and file operations disable resizing. Displayed width is clamped to a 160-pixel tree minimum and a 280-pixel editor minimum, including at 520×360. Requested width survives hide/show and window shrink/expand within the session, but is not persisted across restarts. There is no editor path header or permanent tree-toggle icon. The 36-pixel Show/Hide file tree button appears in the bottom-right gear hover row and remains available through keyboard focus. The sidebar status area uses a 12-pixel bottom margin. Text starts at the original 24-pixel top padding, and the title contains only OmaText with an optional unsaved marker. Settings provides labeled Show/Hide file tree and Open folder actions. They close Settings without applying font drafts, then defer the action until the modal guard clears. Folder dialog acceptance and `folderUrl` payloads share a guarded `openFolderUrl` function that commits IME input, reveals the tree, and preserves the current document. Canceling the folder dialog preserves visibility. The launcher sends `folderUrl` for existing directories and `fileUrl` otherwise, using `Path.is_dir()` without shell interpolation; directory MIME defaults are not registered. Bottom space keeps text clear of actions. The rightmost gear stays anchored while New, Open, Save, and the tree toggle appear to its left; the expanded row uses five 36-pixel buttons with 4-pixel gaps.

### Synthetic development preview

Build the existing test harness, then run this standalone window (no shell restart or installed plugin changes):

```sh
QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME= ./build/ui-test --preview
```

All files are fictional fixtures in a temporary directory. The harness uses test-only Omarchy component copies and reads current theme colors; this is not live-shell integration evidence. For an offscreen PNG and minimum-width check:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME= OMATEXT_PREVIEW_SNAPSHOT=/tmp/omatext-960.png ./build/ui-test --preview
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME= OMATEXT_PREVIEW_WIDTH=520 OMATEXT_PREVIEW_SNAPSHOT=/tmp/omatext-520.png ./build/ui-test --preview
```

Set `OMATEXT_PREVIEW_SIDEBAR_WIDTH=320` to preview a resized tree. Set `OMATEXT_PREVIEW_HIDDEN=1` to capture the default editor layout and add `OMATEXT_PREVIEW_SETTINGS=1` to open Settings through the gear. These flags can be combined with the width and snapshot variables above. Set `OMATEXT_PREVIEW_HEIGHT=360` with width 520 for the minimum-size window.

Earlier VM screenshots describe the previous layout; they are not evidence for this sidebar change.
