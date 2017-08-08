# About 
nolo-vr is an OSVR plugin for LYRobotix Nolo VR tracker system.

# Requirements
* [OSVR Core](https://github.com/OSVR/OSVR-Core) - The core libraries for the OSVR software.
* [hidapi](https://github.com/signal11/hidapi) - A USB Library.

The following are tools for creating an automated development environment:
* Arch - [Arch wiki](https://wiki.archlinux.org/index.php/Virtual_reality#OSVR)
* Ubuntu - [osvr-core-ubuntu-build-script](https://bitbucket.org/monkygames/osvr-core-ubuntu-build-script)

# Building

## Linux

Create a build directory.

```
mkdir build
```

### Compile
The following command has a variable called INSTALL_DIR.  Change this to the location where the other OSVR components are installed.

```
cd build
cmake ..  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"
```

### Setup
Run the following:

``` make install ```

### udev rules
The udev rules need to be added to /etc/udev/rules.d

``` sudo cp 99-nolo.rules /etc/udev/rules.d/.  ```

Run the following command to update udev.

``` sudo udevadm trigger ```

### nolo configuration

TODO

## Windows (with VS 2013 and CMAKE gui)

### Build HIDAPI

Clone [hidapi](https://github.com/signal11/hidapi) into a directory of choice. Then navigate to Windows and open hidapi.sln to open the project.

Visual Studio will probably ask to update the project files due to VS version incompatibility - let it do so.

In the Solution Explorer, right-click on `hidapi` and then select `Configuration Manager`. Change the `Active solution configuration` to Release, then change the `Active solution platform` to x64.

If x64 is not an option, select the drop-down and select `<New`>. Choose x64 as the new platform and copy settings from Win32.

Now build `hidapi` (from the Build menu in the toolbar or by right-clicking `hidapi` in the Solution Explorer).

### Build Nolo-OSVR

Open the CMAKE gui and point the source line at top to your checked out repo. Point the build directory where you want the VS project files to go.

Press `Configure`.

You may get errors that say certain dependencies cannot be found. If CMAKE can't find them automatically, here's a list of where to point them to as they appear:

Key | Directory
------------ | -------------
osvr_DIR | Find your OSVR SDK, then point to x64\lib\cmake\osvr
libfunctionality_DIR | Find your OSVR SDK, then point to x64\lib\cmake\libfunctionality
HIDAPI_INCLUDE_DIR | Find your HIDAPI install directory, then point to the hidapi subfolder (contains hidapi.h)
HIDAPI_LIBRARY | Find your HIDAPI install directory, then point to windows\x64\Release\hidapi.lib


Press `Configure` again. Hopefully all errors are gone. Press `Generate` then `Open Project`

Build as you normally would in VS. Remember if Releasing to right-click the `INSTALL` project, select properties, open Configuration Manager, and change active solution configuration to `RelWithDebInfo`.

**DO NOT RELEASE WITH THE CONFIGURATION SET TO "RELEASE"**. For some reason, it most versions of VS this will cause the plugin to silently fail to load, even though it will build correctly.

# Installation

## Linux

To-do

## Windows

Copy `hidapi.dll` from `your-hidapi-directory\windows\x64\Release` into `your-OSVR-Core-directory\bin`

Copy `com_osvr_Nolo.dll` into your-OSVR-Core-director\bin\osvr-plugins-0`

