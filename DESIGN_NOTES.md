# tinylisp -- Design Notes

> Created: 2026-07-06
> Status: Pre-spec design notes, conversation synthesis
> Purpose: Capture all architectural decisions and ideas before writing the formal spec.
> This document is NOT the spec. It is the reasoning that leads to the spec.

---

## 1. PROJECT VISION

### What is tinylisp?

A minimal Lisp derivative designed for **maximum portability and longevity**.
The goal is a personal computational environment that survives decades --
a language to grow with, modify at will, and rebuild on any platform that
exists or will exist.

### Core Philosophy

- **The spec is permanent. The implementation is disposable.**
  In 50 years, the kernel may have been rewritten 20 times in 20 host
  languages. The spec is what stays constant. The value is in the spec
  and the library, not in the implementation.

- **Minimal kernel, maximal self-implementation.**
  The host provides the smallest possible set of primitives. Everything
  else -- cons cells, reader, eval, GC, macros, object system -- is
  implemented in tinylisp itself.

- **Absolute portability over performance.**
  100x slowdown is acceptable. The kernel must be small enough to rewrite
  in a day in any host language that has arrays and basic arithmetic.

- **KISS and DRY.** Solo project principles that have paid dividends.
  Every added complexity must justify itself.

- **No hard dependencies.** If a dependency is small enough to rewrite
  in a day, it's acceptable. Otherwise, eliminate it.

### The 50-Year Horizon

This is not a product or a learning exercise. This is a life project.
The language must be:
- Simple enough to fully understand decades later
- Documented well enough to reconstruct from the spec alone
- Portable enough to run on any future platform
- Expressive enough to build everything in itself

### Hardware Vision

The architecture is designed so that the kernel can be implemented in
Verilog/VHDL, enabling a true Lisp machine on FPGA or eventually ASIC.
The kernel primitives map directly to FPGA resources:
- make-array -> BRAM
- aref/aset -> BRAM read/write ports
- Arithmetic -> DSP slices
- read-char/write-char -> UART/I/O
- error -> halt and signal

This is three projects sharing one spec: language, VM, hardware.
Build them sequentially, not simultaneously.

---

## 2. ARCHITECTURE: LAYERED DESIGN

```
Layer 3: User programs, CLOS-like system, LOOP, FORMAT, etc.
         (All written in tinylisp, portable)

Layer 2: Standard library
         (cons, car, cdr, reader, printer, eval, defmacro, macros,
          GC, string ops, list utilities -- all in tinylisp, portable)

Layer 1.5: VM + Compiler (optional, for performance)
         (Register-based VM, instruction dispatch, register allocator
          -- all in tinylisp, portable)

Layer 1: KERNEL -- implemented in host language
         (C today, Python tomorrow, Verilog in 10 years)
         ~200-400 lines. Provides ~15 primitives + 7 special forms.

Layer 0: HOST (C, Python, CL, Verilog, whatever the future brings)
```

### Key Architectural Decisions

1. **GC is Layer 0/1 concern, NOT a macro.**
   GC cannot be a Lisp macro -- it needs runtime root set access.
   In the array-based VM, GC is pure Lisp (~30-50 lines) because
   all state is in explicit arrays. The host provides allocation;
   GC is implemented in Layer 2 using only array operations.

2. **The compiler is Layer 2, not Layer 1.**
   The compiler is written in tinylisp. It is automatically portable.
   Multiple backends possible (bytecode, C, x86). The interpreter
   is all you need to bootstrap -- run the compiler through the
   interpreter to produce compiled code.

3. **Object representation: tagged values in array slots.**
   - Fixnums: low bit = 0, shift right to get value
   - Heap pointers (indices): low bit = 1, clear bit to get index
   - eq/eql become = on raw slot values
   - This is how real Lisps do tagged representation, just with
     array indices instead of machine pointers.

4. **Two-phase bootstrap:**
   - Phase 1: Tree-walking interpreter (get language working, iterate on design)
   - Phase 2: Register-based VM (performance, structure, hardware-readiness)
   - Layer 2 code runs unchanged on either execution engine.

---

## 3. THE KERNEL (Layer 1)

The kernel is the only host-dependent code. It must be small enough
to rewrite in a day. Target: ~200-400 lines.

### Primitives (host-provided, ~15)

| Primitive | Purpose | C implementation |
|-----------|---------|-----------------|
| make-array | Allocate memory | malloc |
| aref | Read array slot | pointer dereference |
| aset | Write array slot | pointer assignment |
| + | Addition | native arithmetic |
| - | Subtraction | native arithmetic |
| * | Multiplication | native arithmetic |
| / | Division | native arithmetic |
| < | Less than | native comparison |
| > | Greater than | native comparison |
| = | Equality | native comparison |
| read-char | Read one character | getchar/fread |
| write-char | Write one character | putchar/fwrite |
| error | Abort with message | fprintf + exit/longjmp |

Note: `eq` can be implemented as `=` since everything is an integer
(fixnum or array index) in the tagged representation.

### Special Forms (7)

| Form | Purpose |
|------|---------|
| if | Conditional branching |
| lambda | Function/closure creation |
| let | Lexical binding |
| quote | Prevent evaluation |
| setq | Variable assignment |
| defmacro | Macro definition |
| function | Function value (vs variable value) |

### Kernel Parameters (target-dependent)

- Register count (8 on microcontroller, 16-32 on laptop, 256 on server)
- Heap size
- Stack size

These are compile-time or startup parameters. They do not change
the instruction set or the VM code -- only the register allocator's
behavior and the array sizes.

---

## 4. MEMORY MANAGEMENT

### Why Not Rust-Style Ownership?

Rust's borrow checker works because:
- Statically typed (all types known at compile time)
- No eval (all code known at compile time)
- No runtime macros (codegen before borrow check)
- Restricted aliasing (no two mutable refs)

Lisp fundamentally contradicts all of these:
- Dynamic typing
- eval exists
- Macros expand at runtime
- Arbitrary aliasing (closures, shared data)

Ownership analysis for a dynamic Lisp requires a compiler more
complex than the language itself. The result wouldn't be Lisp anymore.

### Why Not Reference Counting?

- Cycles leak (need separate cycle collector = partial mark-sweep)
- Every assignment needs inc/dec (hidden overhead everywhere)
- More code than mark-sweep
- More complexity for less benefit

### Chosen Approach: Mark-Sweep GC (in Lisp)

**Phase 1 -- Bump allocator, no GC (~10 lines of Lisp):**
```
heap-ptr starts at 0
alloc(n) = advance heap-ptr by n, return old ptr
cons(x,y) = alloc 2 slots, store x and y, return ptr
Never free. Leak. Fine for short programs and REPL.
```

**Phase 2 -- Mark-sweep GC (~30-50 lines of Lisp):**
```
1. MARK: Walk from roots (registers, stack, global env).
   Mark every reachable object.
2. SWEEP: Walk entire heap. Unmarked objects -> free list.
   Clear all marks.
3. Resume.
```

Root set is explicit and small:
- Global environment (known data structure)
- VM call stack (we control it)
- Temporary roots (temp root stack array)

GC handles closures, cycles, eval, dynamic dispatch -- everything.
No language restrictions. No compile-time analysis needed.

### Memory Model

All VM state lives in host-provided arrays:
- THE HEAP: cons cells, symbols, strings, everything
- REGISTERS: small array (R0-Rn), n is target-dependent
- THE STACK: call frames
- FREE LIST: available heap slots

No hidden state in C local variables. Everything explicit and visible.
This means:
- GC rooting is trivial (roots = registers + stack, both are arrays)
- Serialization is free (dump arrays = save state, load = resume)
  This is image-based persistence, like SBCL core files or Smalltalk images
- Debugging is transparent (inspect every register, frame, heap slot)
- Portability is absolute (host only provides arrays)

---

## 5. THE VM (Layer 1.5, Phase 2)

### Register-Based Architecture

The VM is register-based (like Lua 5.x, Dalvik), not stack-based.
Instructions reference registers by index.

### Instruction Set ("TLASM" -- Tinylisp Assembly)

~20-25 instructions. Each is a simple operation on registers and heap.

```
;; Memory operations
LOAD r0 constant        ; load fixnum/nil/true into r0
GETCAR r0 r1            ; r0 = car of cons at r1
GETCDR r0 r1            ; r0 = cdr of cons at r1
SETCAR r0 r1            ; car of cons at r0 = r1
SETCDR r0 r1            ; cdr of cons at r0 = r1
CONS r0 r1 r2           ; r0 = new cons (car=r1, cdr=r2)

;; Arithmetic
ADD r0 r1 r2            ; r0 = r1 + r2
SUB r0 r1 r2            ; r0 = r1 - r2
;; etc.

;; Control flow
JUMP label              ; unconditional jump
JUMP-IF r0 label        ; jump if r0 is not nil
CALL r0 func args...    ; call function, result in r0
RET r0                  ; return r0 to caller

;; Environment
LOOKUP r0 sym           ; r0 = value of symbol in current env
BIND sym r0             ; bind symbol to r0 in current env
SET sym r0              ; set symbol to r0

;; Functions
MAKE-CLOSURE r0 lambda env  ; r0 = closure
```

### VM Dispatch Loop (in Lisp)

The VM is a Lisp function with TCO. It reads instructions from an
array and dispatches. The host's call stack is irrelevant -- the VM
is a state machine with state in arrays.

```lisp
(defun vm-run (pc)
  (let ((instr (aref code-array pc)))
    (let ((op (aref instr 0)))
      (cond
        ((= op OP-ADD)
         (aset regs (aref instr 1)
               (+ (aref regs (aref instr 2))
                  (aref regs (aref instr 3))))
         (vm-run (+ pc 1)))
        ((= op OP-JUMP)
         (vm-run (aref instr 1)))
        ((= op OP-RET)
         (aref regs (aref instr 1)))
        (t (error "unknown opcode"))))))
```

### Variable Register Count

The register count is a target parameter, not a structural change:
- Microcontroller: 8 registers
- Laptop: 16-32 registers
- Server: 256 registers

Same instruction set, same VM code. Only the compiler's register
allocator changes behavior. Compiled bytecode is target-specific
(different register usage), but source code is portable. Recompile
per target, same as C.

---

## 6. BOOTSTRAPPING PLAN

### Phase 0: Spec (1 week)
Write the formal specification. Types, special forms, primitives,
evaluation rules, scoping, TCO policy. One page if possible.
This is the permanent asset. Write it like it's the only thing
that will survive.

### Phase 1: C Kernel (1-2 weeks)
Implement ~15 primitives in C. ~200-400 lines.
No GC. No reader. No eval. Just primitives.

### Phase 2: tinylisp Core Library (2-4 weeks)
- Bump allocator (cons, car, cdr)
- Tagged value representation
- Reader (tokenizer + parser using read-char and arrays)
- Printer
- eval (special forms + function application)
- defmacro and macroexpansion
- Basic list utilities (list, append, reverse, mapcar)
- Working Lisp with no GC. Leaks memory. That's fine.

### Phase 3: GC (1 week)
- Mark-sweep, ~30-50 lines of Lisp
- Root set: global env + VM call stack
- Call gc when heap fills up
- Long-lived programs now work

### Phase 4: Standard Library (ongoing)
- cond, case, when, unless, and, or (macros)
- let*, labels, flet (macros)
- String operations
- loop (macro, if wanted)
- Simple CLOS-like object system (closures + hash tables)
- Format-like string interpolation (macro)
- This is where the language becomes YOURS.

### Phase 5: Register VM + Compiler (2-4 months, optional)
- Compiler: Lisp AST -> instruction array
- VM: register file, stack, instruction dispatch
- Register allocator parameterized by target
- Standard library runs unchanged on VM

### Phase 6: Self-Host (optional, the holy grail)
- Compile the compiler with itself
- Kernel only needed for bootstrapping new hosts

### Phase 7: Port Kernel (to prove the bet)
- Rewrite 200-400 line C kernel in Python (or other host)
- Same tinylisp library runs on top
- Validate the portability claim

### Phase 8: Hardware (long-term dream)
- Implement kernel in Verilog
- Run on FPGA as a true Lisp machine
- Eventually: custom ASIC

---

## 7. TYPES (DRAFT -- to be finalized in spec)

Proposed initial type set:
- Fixnum (tagged, immediate)
- Symbol (interned, index into symbol table)
- Cons (two heap slots)
- String (array of characters, or cons list initially)
- Array (host-provided, primitive)
- Function/Closure (lambda + captured environment)
- NIL (empty/false)
- TRUE (truthy -- could just be a specific fixnum)

Deferred types (implement in Layer 2 if needed):
- Bignum (Lisp-implemented on top of fixnum arrays)
- Ratio (Lisp-implemented)
- Complex (Lisp-implemented)
- Vector (Lisp-implemented on top of arrays)
- Hash table (Lisp-implemented)
- Character (could be fixnum initially)

---

## 8. SCOPING (DRAFT -- to be finalized in spec)

Proposed: lexical scoping by default.
Dynamic/special variables can be added later as a macro or
special form if needed.

TCO: Yes. The VM controls the call stack (it's an array),
so tail calls are just "replace current frame and jump."
This is natural in the register VM architecture.

---

## 9. WHAT WAS REJECTED AND WHY

| Idea | Why Rejected |
|------|-------------|
| Full ANSI CL | Too complex. 1100 pages. Decades of work. |
| CLOS in kernel | Not irreducible. Can be macros + closures. |
| LOOP in kernel | It's a macro. Build it in Layer 2. |
| FORMAT in kernel | It's a macro. Build it in Layer 2. |
| GC as a macro | Impossible. GC needs runtime root access. |
| Rust-style ownership | Requires static analysis, restricts dynamic features. Not Lisp. |
| Reference counting | Cycles leak. More code than GC. Hidden overhead. |
| Compile to native (SBCL-style) | Decades of work. Not needed for this project. |
| Stack-based VM | Register-based is more natural for this architecture. |

---

## 10. KEY INSIGHTS FROM THE DESIGN CONVERSATION

1. **The spec is the language.** The implementation is disposable.
   In 50 years, the kernel may be rewritten 20 times. The spec persists.

2. **The kernel is shockingly small.** ~15 primitives, 7 special forms,
   200-400 lines. This is what makes "rewrite in a day" real.

3. **GC in the array-based VM is pure Lisp.** ~30-50 lines. Handles
   everything. No host dependency. No language restrictions.

4. **The VM state is entirely in arrays.** No hidden C stack state.
   This enables: trivial GC rooting, image-based persistence,
   transparent debugging, absolute portability.

5. **Two-phase bootstrap avoids the chicken-and-egg problem.**
   Interpreter first (get language working), compiler second
   (get performance), self-host third (the holy grail).

6. **Variable register count per target** is a parameter, not a
   structural change. Same instruction set, same VM, different
   register file size. Source portable, bytecode target-specific.

7. **The FPGA/ASIC path is real.** The kernel maps to standard FPGA
   primitives (BRAM, DSP, UART). A 20-instruction Lisp machine is
   small enough to fit on a modest FPGA.

8. **Mark-sweep is simpler than ownership for dynamic languages.**
   Rust's complexity is in the compiler. GC's complexity is in the
   runtime. For a one-person project, runtime complexity is better
   because you can test it.

---

## 11. OPEN QUESTIONS (to resolve in spec)

- [ ] Exact type set and representation details
- [ ] Exact special form semantics (especially defmacro expansion model)
- [ ] Error/condition handling model (start with basic error, expand later?)
- [ ] Package/namespace system (needed? or flat symbol table initially?)
- [ ] Multiple return values (needed? or defer?)
- [ ] String representation (array of chars vs cons list vs host string)
- [ ] Character type (fixnum alias vs distinct type)
- [ ] Exact instruction set for TLASM (finalize when building VM)
- [ ] Calling convention (how are arguments passed -- in registers? on stack?)
- [ ] Closure representation (how is the captured environment stored?)

---

## NEXT STEP

Write the formal spec. One page. The permanent document.
Types, special forms, primitives, evaluation rules, scoping, TCO.

Everything above is the reasoning. The spec is the conclusion.