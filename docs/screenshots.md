# VM screenshots

Screenshots are captured from a dedicated QEMU/KVM virtual machine, not the developer's desktop. Use a fresh account with stock Omarchy settings, an English locale, and no personal files, accounts, notifications, or other plugins.

## Environment

- Omarchy 4.0.3
- Quickshell 0.3.1
- Qt 6.11.2
- Linux x86_64
- A disposable copy-on-write disk and a new `demo` user
- Stock Tokyo Night theme, 1280 x 800 display, scale 1
- A normal 1000 x 650 editor window

The disposable disk is based on an existing Omarchy installation-test image. The new user starts from `/etc/skel`; previous test accounts are not used for the screenshots. All configuration changes remain inside the VM.

## Capture procedure

1. Install OmaText through `omarchy plugin add` and enable it, without building inside the VM.
2. Open OmaText with `omarchy-shell shell summon io.github.komagata.omatext '{}'`.
3. Enter the fictional English text in `demo/note.txt`.
4. Capture the editor with controls collapsed, controls expanded, Settings open, and the unsaved-change confirmation open.
5. Save and reopen the demo file, and test the New, Open, Save, and Settings shortcuts.
6. Copy the guest's unedited PNG screenshots into `artifacts/`. Inspect every image for English text and unrelated content.
7. Shut down the VM normally. Keep the base image unchanged.

Do not generate or retouch screenshots to simulate application behavior. The verification record distinguishes captured states from unrun checks.
