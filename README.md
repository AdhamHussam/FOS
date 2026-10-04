# 🚀 FOS (Faculty Operating System)

An educational, monolithic x86 32-bit operating system kernel built to explore low-level systems programming, x86 protected mode memory management, preemptive scheduling, IPC, and concurrency primitives.

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

**FOS** is an educational operating system modeled around x86 architecture principles. It features a modular kernel design covering hardware initialization, virtual memory management with paging, user environments (processes), dynamic heap allocators, synchronization primitives, and custom user programs.

---

## ✨ Key Features

- **Bootloader & Protected Mode**: 2-stage loading mechanism switching the CPU from 16-bit real mode into 32-bit protected mode.
- **Memory Management**:
  - Multi-level x86 two-tier paging (`boot_memory_manager`, `paging_helpers`).
  - Kernel and user dynamic memory allocation (`kheap`, `uheap`) with block tracking and BST helpers.
  - Page file swapping and page fault handling (`fault_handler`, `pagefile_manager`).
  - Advanced page replacement policies (Clock, Modified Clock, LRU, Optimal).
- **Concurrency & Synchronization**:
  - Kernel and user spinlocks (`kspinlock`, `uspinlock`).
  - Counting semaphores and sleeping locks (`sleeplock`, `channel`).
  - Inter-Process Communication (IPC) via shared memory pages.
- **Process & CPU Management**:
  - Process environment abstraction (`user_environment`).
  - Preemptive and priority-based round-robin scheduling (`sched`, `picirq`, `kclock`).
  - Syscall interface and trap-frame-based exception/interrupt dispatching.
- **Interactive Shell & Utilities**:
  - Integrated kernel monitor / command-line prompt[cite: 1].
  - Rich userland test suite for memory leaks, sorting algorithms, and concurrency validation[cite: 1].

---

## 📂 Architecture & Repository Structure

```text
├── boot/           # Boot sector code (boot.S, main.c) and disk signing scripts[cite: 1]
├── inc/            # Core header definitions (mmu.h, trap.h, memlayout.h, syscall.h)[cite: 1]
├── kern/           # Kernel implementation[cite: 1]
│   ├── cmd/        # Built-in kernel monitor, readline, and command handlers[cite: 1]
│   ├── conc/       # Synchronization primitives (spinlocks, semaphores, sleeplocks)[cite: 1]
│   ├── cons/       # VGA console driver and formatted kernel output[cite: 1]
│   ├── cpu/        # Context switching, timer (kclock), PIC, and scheduler[cite: 1]
│   ├── disk/       # IDE/Disk driver and pagefile manager for virtual memory swapping[cite: 1]
│   ├── mem/        # Boot allocator, paging, kernel heap, and working set management[cite: 1]
│   ├── proc/       # User environments, process control, and program loading[cite: 1]
│   ├── tests/      # Kernel-level unit and integration tests[cite: 1]
│   └── trap/       # Interrupt Descriptor Table (IDT), traps, faults, and syscalls[cite: 1]
├── lib/            # Shared runtime libraries for kernel and userland (string, printf, uheap)[cite: 1]
├── user/           # User space applications, benchmarks, and regression tests[cite: 1]
├── conf/           # Make and environment configuration[cite: 1]
├── .bochsrc        # Configuration files for Bochs x86 PC emulator[cite: 1]
└── GNUmakefile     # Main build configuration[cite: 1]
```
