# x86-kernel
A 32-bit x86 kernel written from scratch in C.

## What is it?
This project was meant to be a learning exercise about kernel and OS
development. It started as a simple "Hello world" kernel, using the [Bare Bones
osdev tutorial](https://wiki.osdev.org/Bare_Bones) and naturally grew from
there.

Here are the most notable features of this kernel:

* Multiboot compliant and bootable by GRUB
* Runs on physical machines (YMMV, only tested on a Thinkpad t440s with an Intel
  CPU)
* Paging support with user and kernel address space isolation
* Multicore support, up to 255 cores
* Capable of running processes in user-space
* Preemptible round-robin scheduling using the LAPIC timer
* LAPIC and IO-APIC support
* Inter-Processor-Interrupts for synchronization between cores and TLB shootdown
* ELF loader (statically compiled binaries only)
* Syscall interface using software interrupts
* Rudimentary file-system interface to open, read and write files
* Basic block device support
* Serial and VGA outputs

I've mostly focused on the kernel side of things, that is hardware interaction,
process & memory management, ... there is no user-space per se (i.e. no console
or tty) although the kernel does support running user programs and has a couple
of syscalls to play with. While there is a file-system interface, it is pretty
rudimentary and for now it is only able to mount a TAR archive as a file-system
and read and write the files it contains. It does not yet support appending
data to files, only overwriting them (mostly because the TAR format was never
meant to be used in this way).

The decision to use C was mostly for simplicity, kernel programming is already
hard enough by itself and the programming language you use may make things more
complicated. Not all programming languages were meant to be used in a
baremetal/kernel environment, in this regard C is perfectly suited for this kind
of task. Less fighting against the language to make it work in a kernel
environment meant more focus on the actual project.

Limiting myself to 32-bit x86 was also intentional and again in the name of
simplicity. x86_64 is much more complex than 32-bit x86 and I wanted to first
have some experience with the latter before eventually looking into 64-bit mode.

## Design
There is not much to be said here. Monolithic kernels are the simplest to
understand and write and so I naturally chose that route. Most of the
architecture and interfaces are my own creation, although sometimes I took
inspiration from the Linux kernel (the `struct sched` being a good example of
this).

At the time of this project, I only had access to physical machines with Intel
CPUs therefore the kernel was developed with the assumption that it would run on
an Intel CPU (or on a VM running on an Intel host). That being said I have not,
if I recall correctly, used any Intel-specific features/MSRs and running the
kernel on a VM on an AMD host seems to be working just fine.

The code is heavily commented, to the point that someone may actually use this
to learn a bit more about kernel/OS development. Additionally a lot of effort
has been put into unit testing. All tests are run within the kernel upon booting
up, as a matter of fact this is pretty much the only thing the kernel does at
this point since there is no console / tty to login into.

## State of the project
I am no longer working on this project for a few different reasons.

First, most of the "easy" stuff (i.e. well documented on osdev and / or Intel
manuals) has been implemented, what I would like to have now is support for
actual hardware devices such as hard-drives and/or network cards. However
supporting those requires supporting AHCI and/or PCIe first both of which
require substantial effort to implement and could be projects of their own.
There is a branch attempting to add PCIe support, however I quickly realised
that there is no good way to properly test PCIe support other than trying to
communicate with a device.

The second reason had more to do with 32-bit limitations. I reached a point
where I learned all that I wanted about the 32-bit x86 architecture and started
to get interested in 64-bit mode. However, adding support for 64-bit mode would
mean pretty much starting over, as this kernel was always developed with 32-bit
mode in mind.

Lastly, as the scope of the project grew, I started to feel somewhat limited by
C. Sure you can get very far with C (the Linux kernel is a good example of this)
but at some point I started to miss the niceties of higher-level language.
Macros and their weird tricks can only get you so far ...

These days, I am working on another kernel targeting x86_64 and written in C++.
It is currently in a private repository but I plan to make it public at some
point.

## Building and Running
There are few dependencies required to build and run this kernel, your machine
needs `make`, `docker` and `qemu` (more specifically `qemu-system-x86`) to be
installed.

### Docker container
The first thing [osdev](https://wiki.osdev.org/Main_Page) teaches you is that
kernel development requires using a cross-compiler. Rather than polluting my
machine (and yours!) with the cross-compiling toolchain, the toolchain lives
within a docker image. The Makefile takes care of creating this image for you if
it is not available and then uses it to build the kernel.

Using a docker container to build the kernel has one draw-back however: it
requires root privileges. Therefore you might need to execute `make` using sudo,
or be ready to enter your password during the building process (which isn't long
anyway).

### Building
To build the kernel simply execute `make` in the top directory. If this is the
first time building the kernel, the Makefile will create the docker image
containing the cross-compiler first. Note that this may take a while as it needs
to build the entire toolchain from sources, this only need to be done once
however.

Once the docker image is created, or if it is already available, the Makefile
uses the docker image to build the kernel from sources, it then creates an image
(e.g. an ELF) for the kernel (`build_dir/kernel.bin` by default) which can be
used by `qemu`.

### Running
To run the kernel, execute `make run`. If the kernel was not already built, the
recipe builds it first. `make run` then starts `qemu` passing it the kernel
image `build_dir/kernel.bin`.

Qemu is run with a configurable number of cpus and amount of memory. The
Makefile has variables for both of those that you can modify if needed.

By default the kernel outputs its log messages in the serial console and `qemu`
is started in _nographics_ mode in which it shows the serial console (i.e. no
GUI).  This behaviour can be changed by setting the `OUTPUT` variable in the
Makefile accordingly, available values are `SERIAL` and `VGA`. Note that if you
want to use `VGA` you will need to modify the `qemu` command line to run in GUI
mode.

#### Debugging
You can also debug the kernel using `gdb`. If the kernel is already running
under `qemu` simply execute:

```
gdb -ex "source kernel.gdb" -ex "target remote localhost:1234" -ex "add-symbol-file build_dir/kernel.bin"
```

This will drop you into a gdb session debugging the kernel running within the
qemu VM.

Additionally you can tell qemu to _wait_ for a GDB session before starting
execution by running `make runs`. In this target, qemu will only start execution
once you've connected to it with GDB and typed `continue`. This can be useful as
it gives you time to set up breakpoint(s) before starting the kernel.

### Other Makefile Targets
A few other targets are available in the Makefile:

* `all` (default): Build the kernel in debug mode.
* `run`: Build the kernel in debug mode and start it using qemu.
* `runs`: Build the kernel in debug mode and start it using qemu. Qemu is
  started with the -S flag, e.g. waiting for gdb to start the VM's execution.
* `debug`: Build the kernel in debug mode with -O0 and -g.
* `release`: Build the kernel in release mode with -O2
* `baremetal_debug`: Build the kernel in debug mode and create an .iso file that
  is meant to be run on a physical machine.
* `baremetal_release`: Build the kernel in release mode and create an .iso file
  that is meant to be run on a physical machine.
* `clean`: Remove all compilation artifacts.

### Running on a physical machine
Executing `make baremetal_debug` or `make baremetal_release` create a bootable
.iso file that can be `dd`'ed onto a USB stick in order to run on a physical
machine.

Building the baremetal .iso requires a few more dependencies: `xorriso` and
`mformat` (from the `mtools` package on Ubuntu).

I was only able to test on a Thinkpad t440s with an Intel CPU, hence cannot
guarantee that it will run on all machines.
