# thinkfan [![Total alerts](https://img.shields.io/lgtm/alerts/g/vmatare/thinkfan.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/vmatare/thinkfan/alerts/)[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/vmatare/thinkfan.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/vmatare/thinkfan/context:cpp)
Thinkfan is a simple, lightweight fan control program. 

# WARNING
There's only very basic sanity checking on the configuration (semantic
plausibility). You can set the temperature limits as insane as you like.

Any change to fan behaviour that results in higher temperatures in some parts
of the system will shorten your system's lifetime and/or cause weird hardware
bugs that'll make you pull out your hair.

   **No warranties whatsoever**

If this program steals your car, kills your horse, smokes your dope or pees
on your carpet... too bad, you're on your own.


# Building and installing
To compile thinkfan, you will need to have the following things installed:
- A recent C++ compiler (GCC >= 4.8 or clang)
- pkgconfig or an equivalent (pkgconf or pkg-config)
- cmake (and optionally a cmake GUI if you want to configure interactively)
- optional: libyaml-cpp for YAML support (the -dev or -devel package)

E.g. on a debian-based system that usually boils down to:
```bash
sudo apt install -y cmake g++ libyaml-cpp-dev pkgconfig libsensors-dev
```

on EL/Fedora based system, usually :
```bash
sudo dnf install -y cmake g++ pkgconfig yaml-cpp-devel lm_sensors-devel
```

1. In the thinkfan main directory, do
   ```bash
   mkdir build && cd build
   ```

2. Then configure your build, either interactively:
   ```bash
   ccmake ..
   ```
   Or set your build options from the command line. E.g. to configure a build
   with full debugging support:
   ```bash
    cmake -D CMAKE_BUILD_TYPE:STRING=Debug ..
    ```

   `CMAKE_BUILD_TYPE:STRING` can also be `Release`, which produces a fully
   optimized binary, or `RelWithDebInfo`, which is also optimized but can
   still be debugged with gdb.
   
   Other options are:

   `USE_NVML:BOOL` (default: `ON`)
       Allows thinkfan to read GPU temperatures from the proprietary nVidia
       driver. The interface library is loaded dynamically, so it does not
       need to be installed when compiling.

   `USE_ATASMART:BOOL` (default: `OFF`)
       Enable libatasmart to read temperatures directly from hard disks. Use
       this only when you really need it, since libatasmart is unreasonably
       CPU-intensive.

   `USE_LM_SENSORS:BOOL` (default: `ON`)
       Use LM sensors to read temperatures directly from Linux drivers.
       The `libsensors` library needs to be installed for this feature, probably
       with required headers and development files (e.g., `libsensors-dev`).

   `USE_YAML:BOOL` (default: `ON`)
       Support config file in the new, more flexible YAML format. The old
       config format will be deprecated after the thinkfan 1.0 release. New
       features will be supported in YAML configs only. See
       examples/thinkfan.conf.yaml.  Requires libyaml-cpp.


3. To compile simply run:
   ```bash
   make
   ```

4. If you did not change `CMAKE_INSTALL_PREFIX`, thinkfan will be installed
   under `/usr/local` by doing:
   ```bash
   sudo make install
   ```
   
   CMake will detect whether you use OpenRC or systemd and install some
   appropriate service files. With systemd, you can edit the commandline
   arguments of the thinkfan service with `systemctl edit thinkfan`.
   With OpenRC, we install only a plain initscript (edit `/etc/init.d/thinkfan`
   to change options).



# Documentation
- Run `thinkfan -h`
- Manpages: `thinkfan(1)`, `thinkfan.conf(5)`
- Example configs: https://github.com/vmatare/thinkfan/tree/master/examples
- Linux kernel hwmon doc: https://www.kernel.org/doc/html/latest/hwmon/sysfs-interface.html
- Linux kernel thinkpad_acpi doc: https://www.kernel.org/doc/html/latest/admin-guide/laptops/thinkpad-acpi.html
- If all fails: https://github.com/vmatare/thinkfan/issues
