#!/usr/bin/env python3
"""One JSON request on stdin, one JSON result on stdout; no shell commands."""
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
from urllib.parse import unquote, urlsplit

MAX_BYTES = 2 * 1024 * 1024


def local_path(url):
    if not isinstance(url, str):
        raise ValueError("Choose a local file.")
    parts = urlsplit(url)
    if parts.scheme != "file" or parts.netloc not in ("", "localhost") or parts.query or parts.fragment:
        raise ValueError("Choose a local file.")
    path = unquote(parts.path, errors="strict")
    if not os.path.isabs(path) or "\0" in path:
        raise ValueError("Choose a local file.")
    return Path(path)


def read_file(path):
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
    with os.fdopen(fd, "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > MAX_BYTES:
            raise ValueError("Choose a regular text file no larger than 2 MiB.")
        data = stream.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        raise ValueError("The file exceeds 2 MiB.")
    try:
        text = data.decode("utf-8-sig")
    except UnicodeError as error:
        raise ValueError("This is not a UTF-8 text file. The original file has not been changed.") from error
    if "\0" in text:
        raise ValueError("This is not a UTF-8 text file. The original file has not been changed.")
    return {"ok": True, "text": text.replace("\r\n", "\n"), "crlf": "\r\n" in text, "bom": data.startswith(b"\xef\xbb\xbf")}


def write_file(path, text, crlf=False, bom=False):
    if not isinstance(text, str):
        raise ValueError("The document text is missing.")
    # Follow a selected symlink without replacing the link itself.
    path = path.resolve()
    mode = 0o600
    try:
        info = path.stat()
        if not stat.S_ISREG(info.st_mode):
            raise ValueError("Choose a regular file to save to.")
        mode = stat.S_IMODE(info.st_mode) & 0o777
    except FileNotFoundError:
        pass
    data = (text.replace("\n", "\r\n") if crlf else text).encode("utf-8")
    if bom:
        data = b"\xef\xbb\xbf" + data
    fd, temporary = tempfile.mkstemp(prefix=".omatext-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fchmod(stream.fileno(), mode)
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    return {"ok": True}


def handle(request):
    try:
        if not isinstance(request, dict):
            raise ValueError("Invalid file request.")
        path = local_path(request.get("url"))
        if request.get("action") == "read":
            return read_file(path)
        if request.get("action") == "write":
            return write_file(path, request.get("text"), request.get("crlf", False), request.get("bom", False))
        raise ValueError("Unknown file operation.")
    except (OSError, ValueError, UnicodeError) as error:
        # Do not echo document content or a traceback into shell logs.
        message = str(error) if not isinstance(error, OSError) else "Could not access the file. Check its location, permissions, and available disk space."
        return {"ok": False, "error": message}


def main():
    try:
        request = json.loads(sys.stdin.readline())
        result = handle(request)
    except (ValueError, UnicodeError):
        result = {"ok": False, "error": "Invalid file request."}
    print(json.dumps(result, ensure_ascii=True), flush=True)


if __name__ == "__main__":
    main()
