#!/usr/bin/env python3
"""Install or remove OmaText's per-user command and application entry."""

import argparse
import os
from pathlib import Path
import sys

MARKER = '# Managed by OmaText setup-launcher.py'
LEGACY_DESKTOP = """[Desktop Entry]
Type=Application
Name=OmaText
Comment=Simple text editor for Omarchy
Exec=omarchy-shell shell summon io.github.komagata.omatext {}
Icon=accessories-text-editor
Terminal=false
Categories=Utility;TextEditor;
"""
COMMAND = f'''#!/usr/bin/env python3
{MARKER}
import argparse
import json
import os
from pathlib import Path

parser = argparse.ArgumentParser(description="Open a text file in OmaText.")
parser.add_argument("file", nargs="?")
args = parser.parse_args()
payload = {{"fileUrl": Path(args.file).absolute().as_uri()}} if args.file else {{}}
os.execvp("omarchy-shell", ["omarchy-shell", "shell", "summon",
                            "io.github.komagata.omatext", json.dumps(payload)])
'''


def desktop_quote(value):
    # Desktop Entry string escaping is applied after Exec argument escaping.
    value = value.replace('%', '%%')
    for character in ('\\', '"', '`', '$'):
        value = value.replace(character, '\\' + character)
    return '"' + value.replace('\\', '\\\\') + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--remove', action='store_true')
    args = parser.parse_args()
    launcher = Path.home() / '.local/bin/omatext'
    data = Path(os.environ.get('XDG_DATA_HOME') or Path.home() / '.local/share')
    if not data.is_absolute() or any(c in str(launcher) + str(data) for c in '\n\r\t'):
        parser.error('Launcher paths must be absolute and contain no control whitespace.')
    desktop = data / 'applications/io.github.komagata.omatext.desktop'
    entry = f'''[Desktop Entry]
{MARKER}
Type=Application
Name=OmaText
Comment=A minimal text editor for Omarchy
Exec={desktop_quote(str(launcher))} %f
Icon=accessories-text-editor
Terminal=false
Categories=Utility;TextEditor;
MimeType=text/plain;text/markdown;text/x-markdown;
'''
    files = [(launcher, COMMAND, 0o755), (desktop, entry, 0o644)]
    # Check both destinations before making changes; never replace another app.
    for path, _, _ in files:
        existing = path.read_text() if path.is_file() else ''
        legacy = path == desktop and existing == LEGACY_DESKTOP
        if path.is_symlink() or (path.exists() and MARKER not in existing and not legacy):
            parser.exit(1, f'Refusing to change an unmanaged file: {path}\n')
    for path, content, mode in files:
        if args.remove:
            path.unlink(missing_ok=True)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
            path.chmod(mode)
    print('OmaText launchers removed.' if args.remove else 'OmaText is available in your application launcher and as omatext.')
    if not args.remove and str(launcher.parent) not in os.get_exec_path():
        print(f'Add {launcher.parent} to PATH to use the omatext command in your terminal.')


if __name__ == '__main__':
    try:
        main()
    except OSError as error:
        sys.exit(f'Could not update OmaText launchers: {error}')
