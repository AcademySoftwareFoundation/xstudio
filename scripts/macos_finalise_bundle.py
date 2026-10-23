#!/usr/bin/env python3

import abc
import argparse
import os
import plistlib
import re
import subprocess
import sys
import shutil
import logging


logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

CODESIGN_IDENTITY = os.environ.get("CODESIGN_IDENTITY", "-")
KEYCHAIN_PASSWORD = os.environ.get("KEYCHAIN_PASSWORD")

def fatal_error(msg):
    """Print error to stderr and exit with code 1"""
    logging.critical(msg)
    sys.exit(1)


def finalize_framework(path):
    name = os.path.basename(path)
    lib_name, _ = os.path.splitext(name)
    lib_path = os.path.join(path, "Versions", "Current", lib_name)
    remove_absolute_rpaths(lib_path)
    sign_path(lib_path)


def unlock_keychain():
    
    if not CODESIGN_IDENTITY:
        logging.warning("No identity given. Skipping unlock_keychain")
        return
    if CODESIGN_IDENTITY == "-":
        logging.info("ad-hoc identity given. Skipping unlock_keychain")

    if not KEYCHAIN_PASSWORD:
        return

    try:
        subprocess.check_call(
            ["security", "unlock-keychain", "-p",
                KEYCHAIN_PASSWORD, "login.keychain"]
        )
    except subprocess.CalledProcessError as error:
        fatal_error(f"Error unlocking keychain: {error}")


def finalize_app(path):
    exe_path = os.path.join(path, "Contents", "MacOS")
    app_name = os.path.basename(path)
    info_plist_path = os.path.join(path, "Contents", "Info.plist")
    info = plistlib.load(open(info_plist_path, "rb"))
    app_name = info.get("CFBundleExecutable")
    # sign any extra exes but skip the bundle exe
    for name in os.listdir(exe_path):
        if name == app_name:
            continue
        exe = os.path.join(exe_path, name)
        remove_absolute_rpaths(exe)
        sign_path(exe)
    remove_absolute_rpaths(os.path.join(exe_path, app_name))
    sign_path(path)


def sign_path(path):
    if not CODESIGN_IDENTITY:
        logging.warning(
            f"No identity given. Skipping code sign step for {path}")
        return
    code_sign = "/usr/bin/codesign"
    logging.info(f"Signing {path} with identity {CODESIGN_IDENTITY}")
    try:
        subprocess.check_output(
            [
                code_sign,
                "--force",
                "--sign",
                CODESIGN_IDENTITY,
                path,
            ]
        )
    except subprocess.CalledProcessError as error:
        fatal_error(
            f"Error signing {path}. stderr: {error.stderr}, stdout: {error.stdout}"
        )


def finalize_dir(full_path, name):
    if name.endswith(".framework"):
        finalize_framework(full_path)
    if name.endswith(".app"):
        finalize_app(full_path)


def finalize_bundle(output_path):
    unlock_keychain()
    for root, dirs, files in os.walk(output_path, topdown=False):
        for name in files:
            _, ext = os.path.splitext(name)
            if ext in (".dylib", ".so"):
                lib = os.path.join(root, name)
                remove_absolute_rpaths(lib)
                sign_path(lib)
        for name in dirs:
            full_path = os.path.join(root, name)
            finalize_dir(full_path, name)
    finalize_dir(output_path, os.path.basename(output_path))


def get_rpaths(binary_path):
    """
    Run otool -l on a binary and return list of rpaths.

    Args:
        binary_path: Path to the binary/framework/dylib

    Returns:
        List of rpath strings (e.g., ['@executable_path/../Frameworks'])
    """
    try:
        # Run otool -l
        result = subprocess.run(
            ["otool", "-l", binary_path], capture_output=True, text=True, check=True
        )

        rpaths = []
        lines = result.stdout.split("\n")

        # Look for LC_RPATH sections
        for i, line in enumerate(lines):
            if "cmd LC_RPATH" in line:
                # The path is typically 2 lines after LC_RPATH
                if i + 2 < len(lines):
                    path_line = lines[i + 2].strip()
                    # Extract path using regex
                    match = re.search(r"path\s+(.+?)\s+\(offset", path_line)
                    if match:
                        rpaths.append(match.group(1))

        return rpaths

    except subprocess.CalledProcessError as e:
        fatal_error(f"Error running otool: {e}")
    except FileNotFoundError:
        fatal_error("otool not found")


def delete_rpath(binary_path, rpath):
    """
    Delete an rpath from a binary using install_name_tool.

    Args:
        binary_path: Path to the binary/framework/dylib
        rpath: The rpath to delete (e.g., '@executable_path/../Frameworks')

    Raises:
        RuntimeError: If install_name_tool fails
    """
    try:
        subprocess.run(
            ["install_name_tool", "-delete_rpath", rpath, binary_path],
            capture_output=True,
            text=True,
            check=True,
        )
    except subprocess.CalledProcessError as e:
        fatal_error(f"install_name_tool failed: {e.stderr}")
    except FileNotFoundError:
        fatal_error("install_name_tool not found")


def remove_absolute_rpaths(binary_path):
    """
    Remove all absolute rpaths from a binary, keeping only relative ones (@-prefixed).

    Args:
        binary_path: Path to the binary/framework/dylib
    """
    rpaths = get_rpaths(binary_path)

    for rpath in rpaths:
        if not rpath.startswith("@"):
            logging.info(
                f"Deleting absolute rpath '{rpath}' from '{binary_path}'")
            delete_rpath(binary_path, rpath)


def main():
    parser = argparse.ArgumentParser(
        "finalize_macos_bundle",
        description="rpath fixer and codesign tool for MacOS build"
    )
    parser.add_argument("bundle")

    args = parser.parse_args()
    finalize_bundle(args.bundle)

if __name__ == "__main__":
    main()
