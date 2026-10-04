# 🚀 FOS (FCIS Operating System)

An educational, monolithic x86 32-bit operating system kernel designed to explore low-level systems programming, x86 protected mode memory management, preemptive scheduling, IPC, and concurrency primitives.

---

## 📌 Table of Contents

- [Overview](#-overview)
- [Key Features](#-key-features)
- [Architecture & Repository Structure](#-architecture--repository-structure)
- [Prerequisites & Toolchain](#-prerequisites--toolchain)
- [Getting Started](#-getting-started)
  - [Building the Kernel](#building-the-kernel)
  - [Running with Bochs](#running-with-bochs)
  - [Debugging with GDB](#debugging-with-gdb)
- [Testing Suite](#-testing-suite)
- [License & Acknowledgments](#-license--acknowledgments)

---

## 📖 Overview

**FOS** is an educational operating system modeled around x86 architecture principles. It features a modular kernel architecture covering hardware initialization, virtual memory management with paging, user environments (processes), dynamic heap allocators, synchronization primitives, and custom user programs.

---

## ✨ Key Features

- **Bootloader & Protected Mode**: 2-stage loading mechanism switching the CPU from 16-bit real mode into 32-bit protected mode.
- **Memory Management**:
  - Two-tier multi-level paging with page table mapping and allocation helpers.
  - Kernel and user dynamic memory allocation (`kheap`, `uheap`) with block management and BST tracking.
  - Page fault handling and disk-backed pagefile swapping (`fault_handler`, `pagefile_manager`).
  - Working set management and page replacement policies (Clock, Modified Clock, LRU, Optimal).
- **Concurrency & Synchronization**:
  - Kernel and user-level spinlocks (`kspinlock`, `uspinlock`).
  - Counting semaphores, sleeplocks, and conditional wake channels (`channel`, `ksemaphore`).
  - Inter-Process Communication (IPC) using shared memory pages.
- **Process & CPU Management**:
  - Process environment lifecycle management (`user_environment`).
  - Preemptive and priority-based round-robin scheduling (`sched`, `picirq`, `kclock`).
  - Trap-frame-based exception handling, hardware interrupts, and system call dispatching.
- **Interactive Shell & Diagnostics**:
  - Built-in kernel command-line monitor and readline utility.
  - Rich suite of user programs and benchmarks for concurrency, sorting, and memory leak detection.

---

## 📂 Architecture & Repository Structure

```text
├── boot/           # Boot sector assembly (boot.S), loader (main.c), and sign tool
├── inc/            # Core system headers (mmu.h, trap.h, memlayout.h, syscall.h)
├── kern/           # Kernel space source tree
│   ├── cmd/        # Built-in kernel shell prompt, readline, and command definitions
│   ├── conc/       # Synchronization (spinlocks, semaphores, sleeplocks, channels)
│   ├── cons/       # VGA console driver and kernel printf
│   ├── cpu/        # Context switching, timer (kclock), PIC, and scheduler
│   ├── disk/       # IDE disk interface and pagefile manager
│   ├── mem/        # Boot memory manager, paging, kernel heap, and working set
│   ├── proc/       # Process environments and program loader
│   ├── tests/      # Kernel-level unit and integration test routines
│   └── trap/       # IDT initialization, fault handlers, and system calls
├── lib/            # Shared runtime C library routines (string, printf, uheap)
├── user/           # User space applications, benchmarks, and regression tests
├── conf/           # Make and toolchain configuration files
├── .bochsrc        # Configuration file for Bochs x86 PC emulation
└── GNUmakefile     # Top-level GNU build system
```

---

## 🛠 Prerequisites & Toolchain

```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y build-essential gcc-multilib g++-multilib gdb bochs bochs-x perl

# Fedora
sudo dnf install -y gcc glibc-devel.i686 libgcc.i686 gdb bochs perl make
```

---

## 🚀 Getting Started

### Building the Kernel

```bash
make clean
make
```

### Running with Bochs

```bash
# On Linux / macOS
bochs -f .bochsrc -q

# On Windows
bochscon.bat
```

### Debugging with GDB

```bash
# Terminal 1: Start Bochs with debug stub
bochs -f .bochsrc-debug

# Terminal 2: Attach GDB
gdb -x .gdbinit
```

---

## 🧪 Testing Suite

| Test Category | Target Files | Description |
| --- | --- | --- |
| Virtual Memory | `tst_page_replacement_*`, `tst_placement.c` | Verifies page fault handling and page replacement algorithms (Clock, LRU, Optimal). |
| Dynamic Memory | `tst_malloc_*`, `tst_free_*`, `ef_mergesort_leakage.c` | Checks user and kernel heap behavior, leak detection, and fragmentation handling. |
| Memory Sharing | `ef_tst_sharing_*`, `tst_sharing_*` | Tests cross-process shared memory regions and write protection flags. |
| Synchronization | `tst_ksemaphore_*`, `tst_sleeplock_*`, `tst_air.c` | Evaluates mutual exclusion, semaphores, and concurrency hazards. |
| Scheduling | `test_scheduler.c`, `priRR_fib*` | Validates preemptive priority round-robin execution and process switches. |

---

## 📄 License & Acknowledgments

This project is derived from the MIT JOS educational operating system framework and adapted for university operating system laboratory coursework and system programming instruction.
