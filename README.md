# Direct Meet-in-the-Middle Attack

**Student IDs:**  
28602563    Assia Mastor    
21417723    Nguyen Kieu Nhung  

## 1. Introduction
This repository contains a `makefile` to compile and run the `mitm.c` program with support for **MPI (Message Passing Interface)** and **OpenMP**. The `mitm.c` program is intended to solve a cryptographic problem, utilizing parallel processing for better performance.

## 2. Prerequisites
- **MPI Compiler (mpicc)**: Required for compiling with MPI support.
- **OpenMP**: For parallelizing sections of the code.

## 3. Makefile Configuration Details

- **CC**: The compiler to use (`mpicc` for MPI support).
- **CFLAGS**: Compiler flags that optimize the code and enable OpenMP and vectorization.
- **LDFLAGS**: Linker flags to link math library (`-lm`).
- **TARGET**: The name of the final executable (`mitm`).
- **SRCS**: The source files to compile (`mitm.c`).

## 4. Usage

### Compile the Program

To compile the program, simply run:

```
make
```

This will create the `mitm` executable.

### Run the Program

To run the program with MPI, you need to specify the following arguments:

- **NPROC**: Number of processes
- **N**: The value for `N` 
- **C0**: The value for `C0` 
- **C1**: The value for `C1` 

Example command to run the program with 8 processes, `N=30`, `C0=b35de13b51802ec6`, and `C1=1fae4ee8767747ef`:

```
make NPROC=8 N=30 C0=b35de13b51802ec6 C1=1fae4ee8767747ef run
```

### Clean the Build

To clean up the compiled object files and the executable, run:

```
make clean
```

This will remove any generated `.o` files and the executable (`mitm`).

## 5. Troubleshooting

- If you encounter an error about missing arguments, ensure that all the required variables (`NPROC`, `N`, `C0`, `C1`) are provided when running the `make run` command.
- If you run the executable with **n ≥ 34**, you need to include `-machinefile $OAR_NODEFILE` tag in your `make run` command