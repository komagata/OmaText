#!/usr/bin/env python3
"""Install the built plugin locally, without a bar widget."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

root = Path(__file__).resolve().parent.parent
plugin_id = json.loads((root / "manifest.json").read_text())["id"]
destination = Path.home() / ".config/omarchy/plugins" / plugin_id
if destination.exists() or destination.is_symlink():
    raise SystemExit(f"Already installed: {destination}. Save your work and remove it before reinstalling.")
subprocess.run(["omarchy", "plugin", "validate", str(root)], check=True)
backend = root / "lib/OmaText/Backend"
required = ["qmldir", "omatext-backend.qmltypes", "libomatext-backend.so", "libomatext-backendplugin.so"]
for name in required:
    if not (backend / name).is_file():
        raise SystemExit("Build first: cmake -S . -B build && cmake --build build")
destination.mkdir(parents=True)
for name in ["LICENSE", "README.md"]:
    shutil.copy2(root / name, destination / name)
for directory in ["ui", "docs", "artifacts"]:
    shutil.copytree(root / directory, destination / directory)
target_lib = destination / "lib/OmaText/Backend"
target_lib.mkdir(parents=True)
for name in required:
    shutil.copy2(backend / name, target_lib / name)
shutil.copy2(root / "manifest.json", destination / "manifest.json")
applications = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "applications"
applications.mkdir(parents=True, exist_ok=True)
(applications / f"{plugin_id}.desktop").write_text(
    "[Desktop Entry]\nType=Application\nName=OmaText\n"
    "Comment=Simple text editor for Omarchy\n"
    f"Exec=omarchy-shell shell summon {plugin_id} {{}}\n"
    "Icon=accessories-text-editor\nTerminal=false\nCategories=Utility;TextEditor;\n")
subprocess.run(["omarchy-shell", "shell", "rescanPlugins"], check=True)
for attempt in range(50):
    catalog = subprocess.run(["omarchy", "plugin", "list", "--json"], check=True, capture_output=True, text=True)
    if any(item["id"] == plugin_id for item in json.loads(catalog.stdout)):
        break
    time.sleep(0.1)
else:
    raise SystemExit(f"Installed files, but discovery timed out: {destination}")
subprocess.run(["omarchy", "plugin", "enable", plugin_id], check=True)
print(f"Installed: {destination}")
