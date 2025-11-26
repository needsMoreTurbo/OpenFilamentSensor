#!/usr/bin/env python3
"""
Build the lightweight (HTML-only) Web UI, sync it into the data/ filesystem image, and
flash firmware + filesystem to an ESP32 using PlatformIO.

Usage (run from repo root):

  python tools/build_and_flash.py                    # Build Lite UI + firmware

Options:
  --env ENV            PlatformIO env to use (default: esp32-s3-dev)
  --local              Only build Lite UI/filesystem; skip PlatformIO upload steps
  --ignore-secrets     Do not merge or package data/secrets.json
  --build-mode MODE    Build mode: (default) full build with merge, 'nofs' firmware only, 'nobin' filesystem only
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile
from contextlib import contextmanager
from datetime import datetime
from typing import Iterator, List, Optional

SECRET_FILENAME = "secrets.json"


@contextmanager
def temporarily_hide_files(paths: List[str]):
    """Temporarily move sensitive files out of data/ so LittleFS skips them."""
    backups = []
    staging_dir: Optional[str] = None
    try:
        for path in paths:
            if os.path.exists(path):
                if staging_dir is None:
                    staging_dir = tempfile.mkdtemp(prefix="fs_secrets_")
                backup = os.path.join(staging_dir, os.path.basename(path))
                shutil.move(path, backup)
                backups.append((path, backup))
        if backups:
            print("Excluding secret files from filesystem image for this build.")
        yield
    finally:
        for original, backup in reversed(backups):
            if os.path.exists(backup):
                shutil.move(backup, original)
        if staging_dir and os.path.isdir(staging_dir):
            shutil.rmtree(staging_dir, ignore_errors=True)


@contextmanager
def temporarily_merge_secrets(settings_path: str, secrets_path: str, ignore: bool) -> Iterator[bool]:
    """
    Merge secrets into user_settings.json for the duration of the context.
    Always restores the original contents, even if later steps fail.
    """
    if ignore:
        yield False
        return

    if not os.path.exists(secrets_path):
        raise FileNotFoundError(
            f"Required secrets file not found at {secrets_path}. "
            "Create it or pass --ignore-secrets."
        )

    try:
        with open(settings_path, "r", encoding="utf-8") as f:
            original_text = f.read()
            base_settings = json.loads(original_text or "{}")
    except FileNotFoundError:
        original_text = ""
        base_settings = {}
    except Exception as e:  # pragma: no cover
        print(f"WARNING: Failed to read base user_settings.json: {e}")
        original_text = ""
        base_settings = {}

    try:
        with open(secrets_path, "r", encoding="utf-8") as f:
            secrets = json.load(f)
    except Exception as e:  # pragma: no cover
        print(f"WARNING: Failed to read {SECRET_FILENAME}: {e}")
        secrets = {}

    for key in ("ssid", "passwd", "elegooip"):
        if key in secrets:
            base_settings[key] = secrets[key]

    with open(settings_path, "w", encoding="utf-8") as f:
        json.dump(base_settings, f, indent=2)
        f.write("\n")
    print("Merged secrets into data/user_settings.json for this build.")

    try:
        yield True
    finally:
        with open(settings_path, "w", encoding="utf-8") as f:
            f.write(original_text)
        print("Restored original data/user_settings.json without secrets after build.")


def run(cmd: List[str], cwd: Optional[str] = None) -> None:
    print(f"> {' '.join(cmd)} (cwd={cwd or os.getcwd()})")
    subprocess.run(cmd, cwd=cwd, check=True)


def ensure_executable(name: str) -> None:
    if shutil.which(name) is None:
        print(f"ERROR: `{name}` is not on PATH.")
        if name == "npm":
            print("Please install Node.js (which includes npm) and try again.")
        elif name in ("python", "python3"):
            print("Please ensure Python is installed and available as `python`.")
        else:
            print(f"Install `{name}` or update your PATH.")
        sys.exit(1)


def create_build_timestamp(data_dir: str) -> str:
    """Create build_timestamp.txt with format MMDDYYHHMMSS for filesystem identification."""
    now = datetime.now()  # Use local time instead of UTC
    # Format: MMDDYYHHMMSS
    thumbprint = now.strftime("%m%d%y%H%M%S")
    timestamp_path = os.path.join(data_dir, "build_timestamp.txt")
    with open(timestamp_path, "w", encoding="utf-8") as f:
        f.write(thumbprint)
    print(f"Created filesystem build thumbprint: {thumbprint}")
    return timestamp_path


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Build Lite WebUI + firmware and flash to ESP32 via PlatformIO."
    )
    parser.add_argument(
        "--env",
        default="esp32-s3-dev",
        help="PlatformIO environment to use (default: esp32-s3-dev)",
    )
    parser.add_argument(
        "--local",
        action="store_true",
        help="Only build the Lite UI/filesystem; skip PlatformIO upload steps.",
    )
    parser.add_argument(
        "--ignore-secrets",
        action="store_true",
        help="Skip merging data/secrets.json and omit secrets files from the filesystem image.",
    )
    parser.add_argument(
        "--build-mode",
        choices=["nofs", "nobin"],
        default=None,
        help="Build mode: 'nofs' = firmware only (no merge), 'nobin' = filesystem only (no merge), default = full build with merge",
    )
    args = parser.parse_args()

    # Resolve paths relative to this file (tools/ -> repo root)
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(tools_dir)
    webui_lite_dir = os.path.join(repo_root, "webui_lite")
    data_dir = os.path.join(repo_root, "data")
    data_lite_subdir = os.path.join(data_dir, "lite")
    secret_file_paths: List[str] = [os.path.join(data_dir, SECRET_FILENAME)]
    legacy_secret_paths = glob.glob(os.path.join(data_dir, "user_settings.secrets.json*"))
    if legacy_secret_paths:
        print("ERROR: Legacy secrets files detected in data/:")
        for path in legacy_secret_paths:
            print(f"  - {path}")
        print("Rename or move secrets into data/secrets.json and remove the old files.")
        sys.exit(1)

    # Ensure a base user_settings.json exists; this file should be safe to commit and can
    # leave SSID/password/IP blank. Per-run secrets are provided via
    # data/secrets.json and merged only for the filesystem image.
    settings_path = os.path.join(data_dir, "user_settings.json")
    template_path = os.path.join(data_dir, "user_settings.template.json")
    if not os.path.exists(settings_path) and os.path.exists(template_path):
        shutil.copyfile(template_path, settings_path)
        print("Created data/user_settings.json from template (no secrets).")

    # Build WebUI(s) and sync into data/
    npm_cmd = "npm.cmd" if os.name == "nt" else "npm"
    node_cmd = "node.exe" if os.name == "nt" else "node"
    ensure_executable(npm_cmd)
    ensure_executable(node_cmd)

    # Always build lightweight webui_lite so data/lite stays in sync (unless nofs mode)
    if args.build_mode != "nofs":
        print("\n=== Building Lightweight WebUI ===")
        if not os.path.exists(webui_lite_dir):
            print(f"WARNING: webui_lite/ directory not found at {webui_lite_dir}")
            print("Skipping lightweight UI build.")
        else:
            # Auto-detect if npm install is needed
            node_modules_dir = os.path.join(webui_lite_dir, "node_modules")
            if not os.path.exists(node_modules_dir):
                print("node_modules not found, running npm install...")
                run([npm_cmd, "install"], cwd=webui_lite_dir)
            else:
                print("node_modules found, skipping npm install")

            run([node_cmd, "build.js"], cwd=webui_lite_dir)

            # Ensure staging artifacts land in data/lite/
            staging_output = os.path.join(repo_root, "data_lite")
            if os.path.exists(staging_output):
                shutil.copytree(staging_output, data_lite_subdir, dirs_exist_ok=True)
                print(f"Lightweight UI deployed to {data_lite_subdir}")
            else:
                print(f"WARNING: Build output not found at {staging_output}")
                print("Lightweight UI build may have failed.")

    # Flash firmware + filesystem using PlatformIO CLI (pio or platformio)
    pio_cmd = shutil.which("pio") or shutil.which("platformio")
    if not pio_cmd:
        print(
            "ERROR: Neither `pio` nor `platformio` is on PATH.\n"
            "Install PlatformIO Core (pip install platformio) "
            "and/or add its Scripts directory to PATH."
        )
        sys.exit(1)

    secrets_path = secret_file_paths[0]

    if args.ignore_secrets:
        print("Skipping data/secrets.json merge (--ignore-secrets).")

    timestamp_path: Optional[str] = None
    try:
        # Build mode: nofs = firmware only (no merge)
        if args.build_mode == "nofs":
            print("\n=== Build Mode: Firmware Only (no merge) ===")
            if args.local:
                run([pio_cmd, "run", "-e", args.env], cwd=repo_root)
                print("\nAll done. Firmware binary ready at `.pio/build/{}/firmware.bin`.".format(args.env))
            else:
                run([pio_cmd, "run", "-e", args.env, "-t", "upload"], cwd=repo_root)
                print("\nAll done. Firmware has been flashed.")

        # Build mode: nobin = filesystem only (no merge)
        elif args.build_mode == "nobin":
            print("\n=== Build Mode: Filesystem Only (no merge) ===")
            with temporarily_merge_secrets(settings_path, secrets_path, args.ignore_secrets):
                timestamp_path = create_build_timestamp(data_dir)
                fs_target = "uploadfs" if not args.local else "buildfs"
                with temporarily_hide_files(secret_file_paths):
                    run([pio_cmd, "run", "-e", args.env, "-t", fs_target], cwd=repo_root)

            if args.local:
                print("\nAll done. Filesystem binary ready at `.pio/build/{}/littlefs.bin`.".format(args.env))
            else:
                print("\nAll done. Filesystem has been flashed.")

        # Default mode: full build with merge
        else:
            print("\n=== Build Mode: Full Build (with merge) ===")
            with temporarily_merge_secrets(settings_path, secrets_path, args.ignore_secrets):
                # Create filesystem build timestamp before building filesystem
                timestamp_path = create_build_timestamp(data_dir)

                # Filesystem upload/build (uses merged settings if present)
                fs_target = "uploadfs" if not args.local else "buildfs"
                with temporarily_hide_files(secret_file_paths):
                    run([pio_cmd, "run", "-e", args.env, "-t", fs_target], cwd=repo_root)

                if args.local:
                    run([pio_cmd, "run", "-e", args.env], cwd=repo_root)
                    print("\nAll done. Build artifacts are ready in `data/` and `.pio/build`.")
                else:
                    # Firmware upload (will build firmware first if needed)
                    run([pio_cmd, "run", "-e", args.env, "-t", "upload"], cwd=repo_root)
                    print("\nAll done. Firmware and filesystem have been flashed.")
    except FileNotFoundError as exc:
        print(f"ERROR: {exc}")
        sys.exit(1)
    finally:
        if timestamp_path and os.path.exists(timestamp_path):
            os.remove(timestamp_path)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        print(f"\nCommand failed with exit code {exc.returncode}: {exc.cmd}")
        sys.exit(exc.returncode)
