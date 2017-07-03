# About 
nolo-vr is an OSVR plugin for LYRobotix Nolo VR tracker system.

# Requirements
A development environment and the [OSVR Core](https://github.com/OSVR/OSVR-Core) application compiled.

The following are tools for creating an automated development environment:
* Arch - [Arch wiki](https://wiki.archlinux.org/index.php/Virtual_reality#OSVR)
* Ubuntu - [osvr-core-ubuntu-build-script](https://bitbucket.org/monkygames/osvr-core-ubuntu-build-script)


# Building

Create a build directory.

```
mkdir build
```

## Compile
The following command has a variable called INSTALL_DIR.  Change this to the location where the other OSVR components are installed.

```
cd build
cmake ..  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"
```

# Setup
Run the following:

``` make install ```

## udev rules
The udev rules need to be added to /etc/udev/rules.d

```
sudo cp 99-nolo.rules /etc/udev/rules.d/.
```

Run the following command to update udev.

```
sudo udevadm trigger

```

## nolo configuration
The nolo configuration file should be copied to the osvr directory.

TODO
