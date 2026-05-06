# README Universal Turing Machine Emulator (C + Assembly)

## Overview

This program implements a Universal Turing Machine emulator following the ZHAW encoding scheme.

* Input/Output: implemented in C
* Core simulation: implemented in x86-64 assembly
* Stack: uses a dedicated TM stack region, switched via `rsp` save/restore

## Assembly Details

* Architecture: x86-64 (AMD64)
* Syntax: Intel (`.intel_syntax noprefix`)
* ABI: System V AMD64

## Registers Used

* `rsp` -> TM private stack during simulation, C stack otherwise
* `r12` -> `TMState *` pointer (persists across entire function)
* `eax` -> current TM state (q_i)
* `ebx` -> head position on tape
* `edx` -> current tape symbol under head
* `r13d` -> transition count (loop bound)
* `r14` -> transition array scan pointer
* `r15d` -> search loop index (`tm_step`)
* `r8` -> cached tape pointer (`tm_run` only)
* `r9d` -> cached tape length (`tm_run` only)
* `rcx` -> transition scan pointer (`tm_run` only)
* `esi`/`edi` -> scratch for write symbol / new state

## Stack Layout

```
High addresses
┌──────────────────┐
│  C stack         │  normal call frames
│  callee-saved    │  rbp, rbx, r12–r15
├──────────────────┤ <- saved_c_rsp
│                  │
│  (gap)           │
│                  │
├──────────────────┤ <- tm_stack_top (rsp during simulation)
│  TM stack        │  64 KiB heap-allocated
└──────────────────┘
Low addresses
```

## Program Flow

1. Read TM encoding (C) from binary string, file, or decimal Gödel number
2. Parse encoding into transition table + optional tape input (C)
3. Initialise tape (128 KiB, blank-filled), place input at centre (C)
4. Call assembly function `tm_step` or `tm_run`
5. For each simulation step:
    * save C stack pointer, switch `rsp` to TM stack
    * read tape symbol at head position
    * linear scan transitions for match on `(state, symbol)`
    * match found -> write new symbol, update state, move head, increment step counter
    * no match -> halt (reject)
    * state = q2 -> halt (accept)
6. Step mode (`tm_step`):
    * execute one transition, switch stack back, return to C
    * C prints state, tape, head position
    * repeat until halt or user quits
7. Run mode (`tm_run`):
    * loop entirely on TM stack until halt
    * single stack switch back to C on completion
8. Print final result: state, tape content, unary count (C)

## Encoding Format

* States: `q_i` -> `0` repeated _i_ times (q1 = start, q2 = accept)
* Symbols: `X_j` -> `0` repeated _j_ times (X1 = `0`, X2 = `1`, X3 = blank)
* Directions: `D_m` -> `0` repeated _m_ times (D1 = L, D2 = R)
* Transition `δ(q_i, X_j) = (q_k, X_l, D_m)` -> `0^i 1 0^j 1 0^k 1 0^l 1 0^m`
* Transitions separated by `11`
* TM code and tape input separated by `111`
* Gödel number: prepend `1` to binary, interpret as decimal integer

## Input Modes

* `--binary <str>` -> binary encoding string directly
* `--file <path>` -> read binary encoding from file
* `--decimal <num>` -> Gödel number (arbitrary precision)
* `--input <str>` -> tape input as `0`/`1` characters
* `--input-unary <n>` -> tape input as _n_ ones (for unary-coded problems)

## Execution Modes

* `--step` -> interactive single-step (Enter = step, `r` = run, `q` = quit)
* `--run` -> execute all steps without pause (default)
* `--max-steps <n>` -> safety limit to prevent infinite loops (default 100000)

## Build

```bash
make          # produces ./utm
make clean    # remove build artifacts
make test     # run smoke tests with T1/T2 from lecture
```

Requires: `gcc`, `make` on x86-64 Linux.

## Test Machines (from lecture)

* T1: `010010001010011000101010010110001001001010011000100010001010`
  (Gödel: `1480103890654955658`)
* T2: `1010010100100110101000101001100010010100100110001010010100`
  (Gödel: `185943403774763668`)

## Notes

* The TM is deterministic (first matching transition wins)
* The simulation stack is physically separate from the C call stack
* Step mode switches `rsp` back to the C stack before each print call
* Tape bounds are clamped (no dynamic growth)