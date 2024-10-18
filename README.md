# OS161 notes

[TOC]

## Intro (docker image)

> For the installation instructions using Docker you can refer to [this](https://github.com/marcopalena/polito-os161-docker).

The image is based on Ubuntu 20.04 and contains the following components:

- OS161 sources
- System/161
- Build toolchain (gcc, gdb, etc.)

The sudo password for os161user is `os161`. Note that the installed packages will not be stored in the volume, therefore they will not be persisted if you destroy and recreate the container. They will however still be available if you stop and restart the container.

### OS161 Environment

The OS161 operating system is already pre-installed in the `os161/src` directory, while the required software packages (binutils, gcc compiler, gdb debugger, MIPS emulator, etc...) are installed in the `os161/tools` directory. 

> BE CAREFUL: ALL DIRECTORIES ARE WRITTEN STARTING FROM `$HOME`, which is located in `/home/os161user`. 

Remember that OS161 is executed by a MIPS processor emulator, *SYS161*. In order to re-compile OS161, to debug it, or to perform other operations (such as executable file visualization), you must use software intended for the MIPS platform: these tools are re-named with the prefix “*mips-harvard-os161-*”: for example, you must use:

- *mips-harvard-os161-gcc*, instead of *gcc*;
- *mips-harvard-os161-gdb* instead of *gdb*, and so on. 

Executables of these tools are located in `os161/tools/bin`.
The reference web site is http://www.os161.org/.
Another really interesting website is the one of the “[CS350](http://www.student.cs.uwaterloo.ca/~cs350/common/OS161main.html)” class at the Waterloo University (Canada).

#### Main working directories

There are a few main work directories:

- `os161/src`: it holds source codes, configuration files, object and executable files, of OS161; this is the folder to edit and re-compile OS161.
- `os161/tools`: this is the folder of all tools used for compilation/link, debug and other tasks (customized for the MIPS processor).
- `os161/root`: it is the folder used to boot the operating system and to execute user processes; this is the folder to run and test OS161.

### OS161 Bootstrap

There are two main ways to bootstrap OS161 in the sys161 emulated environment:

- **normal execution**

  ```bash
  cd $HOME/os161/root
  sys161 kernel	# Run sys161 which is a program acting as a VM with
  				# a MIPS processor. "kernel" is an executable file,
  				# loaded and executed
  ```

  After printing some lines, you can execute commands (menu with `?`). Some commands (for example those with option `?o`) are NOT fully available, since OS161 is NOT a complete operating system (students will complete some of the missing parts).

- **debugger**

  ```bash
  # Both commands need to be run in the same directory!!! 
  cd $HOME/os161/root
  
  # In one terminal...
  sys161 -w kernel	# Wait for debugger before starting the kernel
  
  # In another terminal...
  mips-harvard-os161-gdb kernel	# Execute gdb (properdebugger for 									# MIPS architecture) that
  								# communicate by means of a socket
  								
  # Output:
  (gdb) dir ../../os161/os161-base-2.0.3/kern/compile/DUMBVM
  (gdb) target remote unix:.sockets/gdb
  ```

  The previous output means:

  1. The (compiling) directory, needed to start locating source files: only if interested (but highly recommended) to a debug session showing the C source code under analysis. In this situation, it is enough to locate object files, since they hold a link to the corresponding source files.
  2. The socket connection needed for the communication between sys161 and gdb.

  > For simplicity, the two previous commands `dir` and `target` are added into an initialization command file used by gdb (located in `os161/root/.gdbinit`): a new commands is defined, `dbos161`, and also used by gdb itself (making useless their explicit execution). 
  >
  > THEREFORE, IN THE PROVIDED VERSION OF OS161, THE TWO COMMANDS ARE NOT NEEDED IF YOU ARE WORKING WITH DUMBV. IF YOU CHANGE THE
  > KERNEL VERSION, IT IS RECOMMENDED TO CAREFULLY EDIT `os161/root/.gdbinit`, in order to expect other versions. In fact you can edit this file, adding other commands for any further version.

  Debugger commands:

  - `list`: show the assembly part of the kernel where the debug is currently stopped.
  - `b`: add a breakpoint in a certain position (es: `b kmain`, where kmain is a function).
  - `continue`: jump to next breakpoint.
  - `dbos161`: it executes the previous two commands when gdb starts. Useful if the kernel was stopped and restarted and we need to reattach the debugger to it.
  - `q`: quit the debugger.

### VS Code Tasks

There are five task you can run to automatize some basic operation. You can run them from the command
palette: `CTRL+SHIFT+P > Run Task > <choose_task_to_run>`:

- **Copy Config**: Copy the DUMBVM config creating a new named configuration
- **Run Config**: Start the setup for a given configuration
- **Make Depend**: Add dependencies to the appropriate folder
- **Build and Install**: Compile the new version and copy the new executable in the appropriate folder
- **Run OS161**: Start the system in debug mode from within VS Code

To enter debug mode, press `F5` (or use the command from the contextual menu). Make sure to start sys161 in debug mode before launching your debug session.

## Lab 1

The goal of this lab is to learn how to create a new version of the kernel that contains some optional functionalities which can be enabled or disabled during the build process. The second part, instead, is focused on concurrent programming is os161 using threads.

### Modify the kernel

In order to modify the kernel with new functionalities it's recommended to create a new version of it:

- Create a `.c` and `.h` (`kern > include`) file that will contain the new features of the kernel (es: `hello.c` and `hello.h`);

- Open `src > kern > conf > conf.kern` file and add a new option to indicate that a file has been added to the compilation step:

  ```
  defoption   hello
  optfile     hello   main/hello.c
  ```

- Create a new kernel configuration: `Run task > Copy config > [VERSION_NAME]`;

- Open `VERSION_NAME` file and add an entry at the end to enable the previously defined option for this specific kernel version: `options hello`;

- Execute `Run task > Run config > [VERSION_NAME]` to generate the files for the new version under `kern > compile` and check the presence of a `opt-[version_name].h` file that holds the definition of an option `OPT-[VERSION_NAME]` that can be used in the codebase to optionally execute some code related to this specific version.

- Define the prototypes of the new functions inside the header file and make them optional using the `OPT-[VERSION_NAME]` symbol:

  ```c
  #ifndef _HELLO_H_
  #define _HELLO_H_
  
  #include "types.h"
  #include "lib.h"
  #include "opt-hello.h" // Import the symbol based on current config
  
  #if OPT_HELLO
      void hello(void);
  #endif
  
  #endif
  ```

- When calling a function from another file remember to include the header file and to surround it with the previous if block:

  ```c
  #include "hello.h"
  
  void main() {
      #if OPT_HELLO
      	void hello(void);
  	#endif
  }
  ```

  



## References

* [The OS/161 Instructional Operating System](http://www.os161.org/)
* [Developing inside a Container](https://code.visualstudio.com/docs/remote/containers)
* [VSCode configuration](https://github.com/thomascristofaro/os161vscode)
* [Dockerfile](https://github.com/marcopalena/polito-os161-docker)
