#!/usr/bin/env python3
"""
    Get the toolchains path
    https://proandroiddev.com/tutorial-compile-openssl-to-1-1-1-for-android-application-87137968fee
"""
import argparse
import atexit
import inspect
import os
import shutil
import stat
import sys
import textwrap

def get_host_tag_or_die():
    """Return the host tag for this platform. Die if not supported."""
    if sys.platform.startswith('linux'):
        return 'linux-x86_64'
    elif sys.platform == 'darwin':
        return 'darwin-x86_64'
    elif sys.platform == 'win32' or sys.platform == 'cygwin':
        host_tag = 'windows-x86_64'
        if not os.path.exists(os.path.join(NDK_DIR, 'prebuilt', host_tag)):
            host_tag = 'windows'
        return host_tag
    sys.exit('Unsupported platform: ' + sys.platform)


def get_toolchain_path_or_die(ndk, host_tag):
    """Return the toolchain path or die."""
    toolchain_path = os.path.join(ndk, 'toolchains/llvm/prebuilt',
                                  host_tag, 'bin')
    if not os.path.exists(toolchain_path):
        sys.exit('Could not find toolchain: {}'.format(toolchain_path))
    return toolchain_path

def main():
    """Program entry point."""
    parser = argparse.ArgumentParser(description='Optional app description')
    parser.add_argument('--ndk', required=True,
                    help='The NDK Home directory')
    args = parser.parse_args()

    host_tag = get_host_tag_or_die()
    toolchain_path = get_toolchain_path_or_die(args.ndk, host_tag)
    print(toolchain_path)

if __name__ == '__main__':
    main()
