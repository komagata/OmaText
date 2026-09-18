import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SETUP = ROOT / 'scripts/setup-launcher.py'


class LauncherTest(unittest.TestCase):
    def test_install_launch_and_remove_with_spaces(self):
        self.assertTrue(SETUP.exists(), 'Launcher setup is missing')
        with tempfile.TemporaryDirectory(prefix='omatext launcher ') as directory:
            home = Path(directory)
            env = dict(os.environ, HOME=str(home), XDG_DATA_HOME=str(home / 'data'))
            subprocess.run(['python3', str(SETUP)], env=env, check=True)
            launcher = home / '.local/bin/omatext'
            desktop = home / 'data/applications/io.github.komagata.omatext.desktop'
            self.assertIn('Name=OmaText', desktop.read_text())
            self.assertIn(f'Exec="{launcher}"', desktop.read_text())
            helper = home / '.local/bin/omarchy-shell'
            helper.write_text('#!/usr/bin/env python3\nimport json, sys\nprint(json.dumps(sys.argv[1:]))\n')
            helper.chmod(0o755)
            env['PATH'] = str(helper.parent) + os.pathsep + env['PATH']
            result = subprocess.run(['omatext'], env=env, check=True, capture_output=True, text=True)
            self.assertEqual(json.loads(result.stdout), ['shell', 'summon', 'io.github.komagata.omatext', '{}'])
            subprocess.run(['python3', str(SETUP)], env=env, check=True)
            subprocess.run(['python3', str(SETUP), '--remove'], env=env, check=True)
            self.assertFalse(launcher.exists())
            self.assertFalse(desktop.exists())

    def test_file_argument_and_mime_registration(self):
        with tempfile.TemporaryDirectory(prefix='omatext file ') as directory:
            home = Path(directory)
            env = dict(os.environ, HOME=str(home), XDG_DATA_HOME=str(home / 'data'))
            subprocess.run(['python3', str(SETUP)], env=env, check=True)
            launcher = home / '.local/bin/omatext'
            helper = launcher.with_name('omarchy-shell')
            helper.write_text('#!/usr/bin/env python3\nimport json, sys\nprint(json.dumps(sys.argv[1:]))\n')
            helper.chmod(0o755)
            env['PATH'] = str(helper.parent) + os.pathsep + env['PATH']
            name = '日本語 space # $(echo test).md'
            result = subprocess.run([str(launcher), name], cwd=home, env=env, check=True, capture_output=True, text=True)
            args = json.loads(result.stdout)
            self.assertEqual(args[:3], ['shell', 'summon', 'io.github.komagata.omatext'])
            self.assertEqual(json.loads(args[3]), {'fileUrl': (home / name).as_uri()})
            desktop = (home / 'data/applications/io.github.komagata.omatext.desktop').read_text()
            self.assertIn(f'Exec="{launcher}" %f', desktop)
            self.assertIn('MimeType=text/plain;text/markdown;text/x-markdown;', desktop)
            self.assertFalse((home / '.config/mimeapps.list').exists())

    def test_refuses_unrelated_command(self):
        self.assertTrue(SETUP.exists(), 'Launcher setup is missing')
        with tempfile.TemporaryDirectory() as directory:
            home = Path(directory)
            launcher = home / '.local/bin/omatext'
            launcher.parent.mkdir(parents=True)
            launcher.write_text('unrelated command')
            env = dict(os.environ, HOME=str(home), XDG_DATA_HOME=str(home / 'data'))
            for args in ([], ['--remove']):
                result = subprocess.run(['python3', str(SETUP), *args], env=env, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(launcher.read_text(), 'unrelated command')

    def test_upgrades_legacy_desktop_entry(self):
        with tempfile.TemporaryDirectory() as directory:
            home = Path(directory)
            desktop = home / 'data/applications/io.github.komagata.omatext.desktop'
            desktop.parent.mkdir(parents=True)
            desktop.write_text('[Desktop Entry]\nType=Application\nName=OmaText\nComment=Simple text editor for Omarchy\nExec=omarchy-shell shell summon io.github.komagata.omatext {}\nIcon=accessories-text-editor\nTerminal=false\nCategories=Utility;TextEditor;\n')
            env = dict(os.environ, HOME=str(home), XDG_DATA_HOME=str(home / 'data'))
            result = subprocess.run(['python3', str(SETUP)], env=env, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('Managed by OmaText', desktop.read_text())
