#!/usr/bin/env python3
"""
This script builds OpenCV into an xcframework compatible with the platforms
of your choice. Just run it and grab a snack; you'll be waiting a while.
"""

import sys, os, argparse, pathlib, traceback
from cv_build_utils import execute, print_error, print_header, get_xcode_version, get_cmake_version

def get_framework_build_command_for_platform(platform, destination, framework_name, only_64_bit, other_args):
    """
    Generates the build command that creates a framework supporting the given platform.
    This command can be handed off to the command line for execution.

    Parameters
    ----------
    platform : str
        The name of the platform you want to build for. Options are
        "ios", "ios-simulator", "ios-maccatalyst", "macos"
    destination : str
        The directory you want to build the framework into.
    framework_name : str
        The name of the generated framework.
    only_64_bit : bool
        Build only 64-bit archs
    other_args : [str]
        Arguments that will be passed through to the ios/osx build_framework scripts.
    """
    osx_script_path = os.path.abspath(os.path.abspath(os.path.dirname(__file__))+'/../osx/build_framework.py')
    ios_script_path = os.path.abspath(os.path.abspath(os.path.dirname(__file__))+'/../ios/build_framework.py')
    args = [
        "python3"
    ]
    if platform == 'macos':
        args += [osx_script_path, "--archs", "x86_64,arm64", "--framework_name", framework_name, "--build_only_specified_archs"]
    elif platform == 'ios-maccatalyst':
        # This is not a mistake. For Catalyst, we use the osx toolchain.
        # TODO: This is building with objc turned off due to an issue with CMake. See here for discussion: https://gitlab.kitware.com/cmake/cmake/-/issues/21436
        args += [osx_script_path, "--catalyst_archs", "x86_64,arm64", "--framework_name", framework_name, "--without=objc", "--build_only_specified_archs"]
    elif platform == 'ios':
        archs = "arm64" if only_64_bit else "arm64,armv7,armv7s"
        args += [ios_script_path, "--iphoneos_archs", archs, "--framework_name", framework_name, "--build_only_specified_archs"]
    elif platform == 'ios-simulator':
        archs = "x86_64,arm64" if only_64_bit else "x86_64,arm64,i386"
        args += [ios_script_path, "--iphonesimulator_archs", archs, "--framework_name", framework_name, "--build_only_specified_archs"]
    else:
        raise Exception(f"Platform {platform} has no associated build commands.")

    args += other_args
    args.append(destination.replace(" ", "\\ "))  # Escape spaces in destination path
    return args

if __name__ == "__main__":

    # Check for dependencies
    assert sys.version_info >= (3, 6), f"Python 3.6 or later is required! Current version is {sys.version_info}"
    # Need CMake 3.18.5/3.19 or later for a Silicon-related fix to building for the iOS Simulator.
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/21425 for context.
    assert get_cmake_version() >= (3, 18, 5), f"CMake 3.18.5 or later is required. Current version is {get_cmake_version()}"
    # Need Xcode 12.2 for Apple Silicon support
    assert get_xcode_version() >= (12, 2), f"Xcode 12.2 command line tools or later are required! Current version is {get_xcode_version()}. \
    Run xcode-select to switch if you have multiple Xcode installs."

    # Parse arguments
    parser = argparse.ArgumentParser(description='This script builds OpenCV into an xcframework supporting the Apple platforms of your choice.')
    parser.add_argument('out', metavar='OUTDIR', help='The directory where the xcframework will be created')
    parser.add_argument('--platform', default='ios,ios-simulator,ios-maccatalyst,macos', help='Platforms to build for (default: ios,ios-simulator,ios-maccatalyst,macos)')
    parser.add_argument('--framework_name', default='opencv2', help='Name of OpenCV xcframework (default: opencv2, will change to OpenCV in future version)')
    parser.add_argument('--only_64_bit', default=False, action='store_true', help='Build for 64-bit archs only')
    parser.add_argument('passthrough_args', nargs=argparse.REMAINDER, help='Any flags not captured by this script will be passed through to the ios/osx build_framework.py scripts')

    args, unknown_args = parser.parse_known_args()
    print(f"The following args will be passed through to the ios/osx build_framework.py scripts: {args.passthrough_args}")

    platforms = args.platform.split(',')

    try:
        # Build .frameworks for each platform
        build_folders = []
        for platform in platforms:
            folder = f"./{args.out}/{platform}"
            pathlib.Path(folder).mkdir(parents=True, exist_ok=True)
            build_folders.append(folder)
            framework_build_command = get_framework_build_command_for_platform(platform, folder, args.framework_name, args.only_64_bit, args.passthrough_args)

            print("")
            print_header(f"Building frameworks for {platform}")
            execute(framework_build_command, cwd=os.getcwd())

        # Put all the built .frameworks together into a .xcframework
        xcframework_build_command = [
            "xcodebuild",
            "-create-xcframework",
            "-output",
            f"{args.out}/{args.framework_name}.xcframework",
        ]
        for folder in build_folders:
            xcframework_build_command += ["-framework", f"{folder}/{args.framework_name}.framework"]
        execute(xcframework_build_command, cwd=os.getcwd())

        print("")
        print_header(f"Finished building {args.out}/{args.framework_name}.xcframework")
    except Exception as e:
        print_error(e)
        traceback.print_exc(file=sys.stderr)
        sys.exit(1)