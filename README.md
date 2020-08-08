# InputPropagateAnalysis
A simple LLVM Pass for tracing hardware input propagation within Linux drivers.

To evaluate whether Linux driver can be fuzzing from a hardware input perspective for coverage improvement, a simple pass was built to perform static analysis on Linux kernel.

The insight is that if a hardware input can affect the branch/jmp operation during driver code, it may help for increasing fuzzing coverage.

# Requirements

- [LLVM](http://llvm.org/docs/GettingStarted.html#overview) (The pass was implemented based on llvm-9.0.0)
- CMake
- Linux driver bitcode

# Build
1. [Download](https://releases.llvm.org/download.html#9.0.0) and build llvm-9.0.0, and one may build clang as well.
        
        // After downloading and extracting llvm-9.0.0 & clang-9.0.0 (cfe-9.0.0)
        // Build with clang
        mv cfe-9.0.0.src llvm-9.0.0.src/tools/clang
        mkdir build
        cd build
        cmake ../llvm-9.0.0.src -DLLVM_TARGETS_TO_BUILD=X86 -DCMAKE_BUILD_TYPE=Release
        make -j4
    
    When finished, one can find the executable llvm & clang under `build/bin`.
    
    See more details from the [official documentation](https://llvm.org/docs/GettingStarted.html#getting-the-source-code-and-building-llvm).

2. Copy the `ValuePropagate` into llvm source code directory (`llvm-9.0.0.src/lib/Transforms`), as the following structure:

        llvm_9.0.0
        ├── build                               (3)
        ├── llvm-9.0.0.src/lib/Transforms
        │   ├── ValuePropagate
        │   │   ├── ValuePropagate.cpp
        │   │   └── CMakeLists.txt              (1)
        │   ├── Arm64ValuePropagate
        │   │   ├── Arm64ValuePropagate.cpp
        │   │   └── CMakeLists.txt
        │   ├── CMakeLists.txt                  (2)
        ...

3. Add the following contents in the `CMakeLists.txt` (2):

        add_subdirectory(ValuePropagate)
        add_subdirectory(Arm64ValuePropagate)

4. Back to your llvm compile directory (For me which is `llvm_9.0.0/build` (3)), it should be in the same catalogue as `llvm-9.0.0.src`)

5. Compile the pass with llvm.

        make -j4


# How it works
The `ValuePropagate` Pass takes the Linux driver llvm bitcode as input, it could used by `opt` to perform static analysis, and finally gives a simple tree structure output.

- `ValuePropagate` for x86 arch Linux Kernel.

- `Arm64ValuePropagate` for AArch64 arch Linux Kernel.

## Input
One can obtain the input bitcode file from the following methods:

- To perform analysis on single hardware driver source file:
    - Complie the driver source file using `clang-9` with `-emit-llvm` flag to get the bitcode file.
    - `example/rtc.ll` is the rtc driver bitcode file in x86 Linux Kernel.
    - One may find nothing with only a single driver file cause the hardware input not happened here.
- To perform analysis on each driver module:
    - One may use [difuze](https://github.com/ucsb-seclab/difuze) to obtain a series of driver modules. (difuze use `llvm-link` to link each driver module into a single bitcode file.)
- To perform analysis on the whole Linux kernel：
    - Compile the Linux kernel into a single bitcode file, one may use [wllvm](https://github.com/SRI-CSL/whole-program-llvm) to compile it.
    - The analysis process on the whole kernel may be very slow.

## Run

Use opt to perform the static analysis on Linux driver bitcode file.

        opt -load ./llvm_9.0.0/build/lib/LLVMValuePropagate.so -ValuePropagate -analyze rtc.ll | tee output.log

## Output

Check the dummy output file.

Each hardware input start from a function which use an input assembly instruction (`in/mov` in X86, `ldr` in ARM64) to obtain a hardware input from an external device (via driver code), then follow with a variable use-chain where the input value propagate on, and finally end at a `br/ret` instruction.

# Result
There are still some unhandled situation during static analysis, but it seems that the hardware input does not affect the branch/jmp operation during driver code too much.. so yes, this may just a failed exploration.
<!-- , but at least it make me know a little about llvm :). -->