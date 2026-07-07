# ignisp -- Design Notes

> Created: 2026-07-06
> Revised: 2026-07-10
> Status: Pre-spec design notes, conversation synthesis
> Purpose: Capture all architectural decisions and ideas before writing the formal spec.
> This document is NOT the spec. It is the reasoning that leads to the spec.

---

## 1. PROJECT VISION

### What is ignisp?

A minimal Lisp derivative designed for **maximum portability and longevity**.
The goal is a personal computational environment that survives decades --
a language to grow with, modify at will, and rebuild on any platform that
exists or will exist.

### What ignisp is NOT

- Not a production language. Not for building web servers or running
  image classification.
- Not a language that hides performance behind optimizations.
- Not a language that requires a team to maintain or port.

### What ignisp IS

- A personal computing language for everyday tasks: replacing bash
  scripts, engineering computing Python programs, automating tasks.
- A sort of computable pseudocode -- expressive enough to think in,
  simple enough to fully understand.
- A language that forces you to write performant code by not hiding
  complexity behind runtime optimizations. It won't be fast, but it
  will be honest about what it's doing.
- A language portable to any computational substrate: CPU, GPU,
  distributed systems, FPGA, and whatever exists in 50 years.
- In the 50-year horizon: a language that can be transpiled to
  optimized languages for specific use cases.

### Core Philosophy

- **The spec is permanent. The implementation is disposable.**
  In 50 years, Layer 1 may have been rewritten 20 times in 20 host
  languages. The spec is what stays constant. The value is in the
  spec and the library, not in the implementation.

- **Three layers, three concerns.**
  Hardware abstraction (disposable), computational abstraction
  (permanent), and the language (permanent). Each layer has a
  clear job and a clear boundary.

- **Absolute portability over performance.**
  100x slowdown is acceptable. The Layer 1 reducer must be small
  enough to rewrite in a day in any host language.

- **KISS and DRY.** Solo project principles that have paid dividends.
  Every added complexity must justify itself.

- **No hard dependencies.** If a dependency is small enough to rewrite
  in a day, it's acceptable. Otherwise, eliminate it.

- **Force performance honesty.** The language should not hide
  computational cost behind lazy evaluation or runtime optimizations.
  If something is O(n²), the programmer should know.

### The 50-Year Horizon

This is not a weekend project. It is a language to grow with, modify
at will, and rebuild on any future platform. When Nacho is 70, he
wants to still be using this language.

The architecture is designed so that paradigm shifts (CPU → GPU,
single-node → distributed, silicon → something else) only require
rewriting Layer 1. Layers 2 and 3 survive unchanged.

---

## 2. ARCHITECTURE: THREE-LAYER DESIGN

```
Layer 3: IGNISP -- THE LANGUAGE (permanent)
         A normal Lisp. Built on Layer 2.
         Reader, eval, macros, cons cells, stdlib, object system.
         This is where the art lives. This is where Nacho spends time.
         ↓ compiled to
Layer 2: COMPUTATIONAL ABSTRACTION (permanent, the "assembly")
         Lambda calculus + native integers + I/O.
         Not human-writable. A compiler target.
         This is the permanent kernel of ignisp. Spec frozen once stable.
         ↓ executed on
Layer 1: HARDWARE ABSTRACTION (disposable, rewrite per platform)
         A lambda calculus reducer. Implemented in C today,
         Python tomorrow, FPGA someday.
         Rewriting this is the ONLY work needed to port ignisp.
```

### Why Three Layers?

The original design was two layers: a minimal C kernel (arrays,
arithmetic, I/O) with everything else implemented in Lisp on top
(metacircular). This had two problems:

1. **The metacircular bootstrap paradox.** To write the reader in
   Lisp, you need cons cells. To build cons cells, you need the
   reader to load the code that defines cons cells. This is solvable
   but painful and leads to fragile bootstraps.

2. **No separation between computational core and language.** If
   the kernel and the language are the same thing, there's no
   boundary to optimize against. "Optimize cons" becomes an
   abstract task with no home. Knowing himself, Nacho would pour
   all effort into Layer 3 features and leave the core unoptimized.

Three layers solves both:

- **Layer 2 is the boundary.** It has its own spec, its own identity.
  "Optimize the cons implementation" is a concrete task about Layer 2.
- **The bootstrap is clean.** Layer 2 is simple enough to generate
  from a script. No chicken-and-egg.
- **Each layer has a clear job.** Layer 1 is about hardware. Layer 2
  is about computation. Layer 3 is about the language experience.

### Why Lambda Calculus for Layer 2?

1. **It is the mathematical foundation of computation.** Lambda
   calculus has been studied for 80+ years and will be studied for
   50 more. It is permanent in a way that no bytecode format will be.

2. **It is minimal.** Three constructs: variables, abstraction,
   application. Plus the pragmatic additions (integers, I/O). Few
   things to implement, few things to port, few things to get wrong.

3. **It separates computation from hardware.** Layer 2 says "here is
   how computation works." Layer 1 says "here is how to do that on
   this specific hardware." A paradigm shift (CPU → GPU) changes
   Layer 1 but not Layer 2.

4. **It maps to any computational substrate.** Lambda calculus
   reducers have been implemented on CPUs, GPUs, FPGAs, and in
   distributed systems. The computational model doesn't change.

5. **It is the honest answer to "what is the minimal computational
   substrate?"** McCarthy's insight was that eval is all you need.
   Lambda calculus is what eval is built on.

### Layer 2 Design Decisions

**Eager evaluation (applicative order).**

Arguments are evaluated before the function is applied. This is
how Lisps work. It is simpler to implement than lazy evaluation.
It avoids space leaks. And it forces performance honesty: if you
write an infinite list, it actually tries to evaluate it, instead
of silently deferring until you touch it.

Lazy evaluation lets you get away with non-performant code by
deferring computation. Eager evaluation doesn't. This aligns with
the goal of forcing the programmer to be aware of computational cost.

**Z combinator for recursion (no `define` in Layer 2).**

In pure lambda calculus, functions can't refer to themselves by
name. The Y combinator solves this in lazy evaluation, but with
eager evaluation it expands infinitely. The Z combinator works
with eager evaluation:

```
Z = λf. (λx. f (λv. x x v)) (λx. f (λv. x x v))
```

Every recursive function in Layer 2 is wrapped in Z. This is ugly
for humans but Layer 2 is not human-writable -- the compiler
handles it automatically.

The alternative would be adding `define` or `letrec` as a special
form in Layer 2. This was rejected because:
- It breaks the purity of Layer 2 (lambda + application + primitives).
- Every special form is something Layer 1 must understand.
- The Z combinator is a one-time compiler transformation, not an
  ongoing complexity.
- Layer 3 (ignisp) has `define`. The compiler translates it to Z.

**Layer 2 is not human-writable.**

Layer 2 is a compiler target, not a language for humans. It is
generated by the ignisp compiler (Layer 3 → Layer 2). Humans write
ignisp; the compiler produces lambda terms.

This means Layer 2 can be as verbose and mechanical as needed.
Church-encoded booleans, Z-combinator-wrapped recursion, thunked
conditionals -- the compiler handles all of it. The human never
sees Layer 2 code.

**Conditionals via Church encoding with thunks.**

Layer 2 has no `if` special form. Conditionals are Church-encoded:

```
true  = λx.λy.x
false = λx.λy.y
if    = λc.λt.λf. c t f
```

With eager evaluation, branches must be wrapped in thunks to
prevent both from being evaluated:

```
(if condition (λd. then-expr) (λd. else-expr))
```

The compiler handles this automatically. The programmer writes
`(if c then else)` in ignisp; the compiler emits the thunked version.

**Data representation via Church encoding.**

Cons cells, lists, booleans -- all Church-encoded in Layer 2:

```
pair = λa.λb.λf. f a b
fst  = λp. p (λx.λy.x)    (car)
snd  = λp. p (λx.λy.y)    (cdr)
```

With eager evaluation, `pair a b` captures already-evaluated `a`
and `b` in a closure. `fst` and `snd` extract them. This works
correctly and naturally with eager evaluation.

### What Each Layer Contains

**Layer 1 (Hardware Abstraction) provides:**
- Representation of lambda terms (variables, abstractions, applications)
- Native integer values and operations (+, -, *, /, <, >, =)
- Character I/O (read-char, write-char)
- Beta reduction engine (eager/applicative order)
- Memory allocation for terms (Phase 1: malloc, no GC)
- Garbage collection (Phase 2: mark-sweep or similar)
- NO mutation. NO arrays. NO mutable cells. Pure functional.

**Layer 2 (Computational Abstraction) provides:**
- Lambda abstraction: λx.M
- Application: M N
- Native integer operations (from Layer 1)
- I/O primitives (from Layer 1)
- Church-encoded data: booleans, pairs, lists, numbers
- Z combinator for recursion
- Thunked conditionals
- The ignisp compiler (compiles Layer 3 to Layer 2)
- The ignisp reader (reads S-expressions, produces Layer 2 terms)
- The ignisp eval (evaluates Layer 3 forms)
- NO mutation. NO arrays. NO setq. Pure functional.

**Layer 3 (Ignisp) provides:**
- S-expression syntax (what the programmer writes)
- Special forms: if, quote, lambda, let, define, defmacro, begin
  (NO setq -- immutable language)
- Macros and macroexpansion
- Cons cells, strings, symbols, closures (as Lisp types, Church-encoded in Layer 2)
- Standard library: list ops, string ops, math, I/O
- Ergonomic macros for immutable programming:
  `->` (thread-first), `loop`/`for` (comprehensions), `with-state` (state threading)
- Object system (CLOS-like, built on closures)
- LOOP, FORMAT, and other conveniences
- Everything the programmer interacts with
- Fully immutable. No mutation anywhere. State is threaded through
  recursion. Macros provide ergonomic syntax that expands to immutable code.

---

## 3. LAYER 1: THE REDUCER

Layer 1 is the only host-dependent code. It must be small enough
to rewrite in a day. Target: ~300-500 lines.

### What Layer 1 Implements

A lambda calculus reducer with native integers and I/O.

**Core:**
- Term representation: variables, abstractions (λx.M), applications (M N),
  integer literals
- Beta reduction: (λx.M) N → M[x := N] with capture-avoiding substitution
- Evaluation strategy: eager (applicative order) -- evaluate arguments
  before applying

**Primitives (non-lambda values the reducer recognizes):**
- Native integers (machine-word, signed)
- Arithmetic: +, -, *, / (integer division, truncating)
- Comparison: <, >, = (return Church-encoded booleans? or native?
  -- see Open Questions)
- I/O: read-char (returns integer or EOF), write-char (takes integer)

**Memory:**
- Phase 1: Allocate terms with malloc, never free. Acceptable for
  bootstrap and short programs.
- Phase 2: Garbage collection. The reducer owns all term memory, so
  the root set is explicit: the current term being reduced + any
  referenced terms. GC is a Layer 1 concern because the reducer
  controls memory layout.

### Why GC is Layer 1, Not Layer 2

In the old architecture, GC was Layer 2 (Lisp code operating on
arrays). In the new architecture, Layer 2 is lambda calculus -- it
has no concept of memory management. The reducer (Layer 1) allocates
terms during beta reduction (substitution creates new terms). The
reducer must also collect them.

This is actually simpler: the reducer knows exactly what terms exist
and which are reachable (it's holding them). Mark-sweep over the term
graph is straightforward.

### Porting Layer 1

To port ignisp to a new platform:
1. Implement term representation on the new platform
2. Implement beta reduction
3. Implement integer arithmetic
4. Implement read-char / write-char
5. Implement memory allocation (and eventually GC)

That's it. No reader, no printer, no eval, no macros, no cons cells.
All of that is Layer 2/3 and runs unchanged.

The porting effort depends on the platform:
- **C → Python:** Straightforward. Python has objects, GC, and I/O.
  Maybe 200 lines.
- **C → FPGA:** Harder but well-studied. Graph reduction on FPGA is
  a known technique. Terms in BRAM, reduction logic in LUTs.
- **C → GPU:** Research territory but possible. Lambda calculus on
  GPUs has been explored. The parallel reduction model maps to
  GPU architecture.
- **C → distributed (Erlang-style):** Harder. Term reduction across
  nodes requires serialization and message passing. But the Layer 2
  spec doesn't change -- only the Layer 1 implementation.

---

## 4. LAYER 2: THE COMPUTATIONAL ABSTRACTION

### The Language

Layer 2 is lambda calculus + native integers + I/O. It has:

- **Variables:** x, y, z, ...
- **Abstraction:** λx.M (function creation)
- **Application:** M N (function call)
- **Integer literals:** 0, 1, 42, -17, ...
- **Integer operations:** +, -, *, /, <, >, = (primitives)
- **I/O:** read-char, write-char (primitives)

That's it. No `define`. No `if`. No `let`. No `quote`. No `defmacro`.
No strings. No cons cells. No symbols. No arrays.

Everything else is built from these:

| Concept | Layer 2 encoding |
|---------|-----------------|
| true/false | λx.λy.x / λx.λy.y |
| if | λc.λt.λf. c t f (with thunks for branches) |
| pair (cons) | λa.λb.λf. f a b |
| fst (car) | λp. p (λx.λy.x) |
| snd (cdr) | λp. p (λx.λy.y) |
| nil | λf.λx.x (Church-encoded empty list) |
| list | chains of pairs ending in nil |
| recursion | Z combinator wrapping |
| numbers | native integers (not Church numerals) |

### What Lives in Layer 2

The ignisp core is implemented in Layer 2:
- **The compiler:** translates ignisp S-expressions (Layer 3) into
  Layer 2 lambda terms. This is the most complex piece of Layer 2.
- **The reader:** reads characters from input (via read-char) and
  produces ignisp S-expressions (represented as Church-encoded lists
  of symbols and values).
- **The eval:** takes an ignisp form and evaluates it. In the
  tree-walking phase, this is direct. In the compiled phase, this
  is the compiler itself.
- **The printer:** takes an ignisp value and writes its textual
  representation (via write-char).

### No Arrays in Layer 2

Everything is Church-encoded. Cons cells are lambda closures.
Strings are lists of integers. The reader, eval, and compiler all
work with Church-encoded data. This is the purest approach.

Performance will be poor (O(n) random access for everything, heavy
closure allocation). This is an accepted trade-off: simplicity and
purity over performance. If performance becomes a real problem,
arrays can be added as a Layer 1 primitive later. The spec starts
pure.

---

## 5. BOOTSTRAPPING

### The Bootstrap Problem

Layer 2 is not human-writable. The ignisp compiler (which translates
Layer 3 to Layer 2) is itself written in... what? If it's written in
ignisp, it needs to be compiled to Layer 2. But to compile it, you
need the compiler. Chicken-and-egg.

### The Bootstrap Solution

Use a Python script as the bootstrap compiler. The script:

1. **Implements a minimal lambda calculus reducer** (Layer 1 in
   Python -- quick and dirty, for testing only).

2. **Generates Layer 2 code** that implements the ignisp core
   (reader, eval, basic stdlib). This generation is done by the
   Python script -- it writes out lambda terms that, when reduced,
   constitute a working ignisp interpreter.

3. **Tests the generated Layer 2 code** on the Python reducer.

Once the Layer 2 code works on the Python reducer:

4. **Write Layer 1 in C** (the real reducer).

5. **Run the same Layer 2 code** on the C reducer.

6. **ignisp is now running.** From this point, all development
   happens in ignisp (Layer 3). The Python script and Python reducer
   are discarded.

The bootstrap is a one-time cost. Once ignisp runs on the C reducer,
the Python script is thrown away. Future changes to ignisp happen
in ignisp, compiled by the ignisp compiler (which runs on the C
reducer, which executes Layer 2).

### Bootstrap Phases

**Phase 0: Spec (1-2 weeks)**
- Write the Layer 2 spec (lambda + ints + IO, evaluation rules)
- Write the Layer 3 spec (ignisp: types, special forms, primitives)
- Write the Layer 1 spec (reducer interface, memory model)
- These are the permanent documents.

**Phase 1: Bootstrap in Python (2-4 weeks)**
- Python lambda calculus reducer (~200 lines)
- Generate Layer 2 code for ignisp core (reader, eval, printer)
- Test: can it read and evaluate simple ignisp programs?
- This is throwaway code. Don't over-engineer it.

**Phase 2: C Reducer (1-2 weeks)**
- Implement Layer 1 in C (~300-500 lines)
- Run the Phase 1 Layer 2 code on it
- Verify: same behavior as Python reducer
- Now ignisp runs on C. Python bootstrap discarded.

**Phase 3: ignisp Core in ignisp (2-4 weeks)**
- Rewrite the reader, eval, and printer in ignisp itself
- Compile to Layer 2 using the bootstrap compiler
- Verify: the self-hosted version produces the same Layer 2 code
- Now ignisp is self-hosting. The bootstrap compiler can be retired.

**Phase 4: Standard Library (ongoing)**
- cond, case, when, unless, and, or (macros)
- let*, labels, flet (macros)
- String operations
- List utilities (map, filter, reduce, append, reverse)
- File I/O
- Error handling / conditions
- This is where the language becomes YOURS.

**Phase 5: GC (1-2 weeks)**
- Mark-sweep in the C reducer
- Root set: current term + referenced terms
- Long-lived programs now work

**Phase 6: Transpiler (long-term, future)**
- Compile ignisp to C, Python, or other target languages
- The 50-year horizon: transpile to whatever exists
- Not needed for the language to be useful, but needed for
  the "computable pseudocode" vision.

**Phase 7: Hardware (long-term dream)**
- Implement Layer 1 (reducer) in Verilog
- Run on FPGA as a lambda calculus machine
- Eventually: custom ASIC

---

## 6. MEMORY MANAGEMENT

### Phase 1: Bump Allocator, No GC

The reducer allocates terms with malloc and never frees. Acceptable
for bootstrap and short REPL sessions. Memory leaks are expected
and documented.

### Phase 2: Mark-Sweep GC

The reducer owns all term memory. The root set is explicit: the
current term being reduced, plus any terms referenced by it (which
the reducer is tracking during reduction). Mark-sweep walks the
term graph, marks reachable terms, frees the rest.

Why mark-sweep:
- Handles cycles (closures can reference themselves via Z combinator)
- No compile-time analysis needed
- Simple to implement in the reducer (~50-100 lines of C)
- No language restrictions (eval, runtime macros, arbitrary aliasing
  all work)

### Why Not Other Approaches?

| Approach | Why Rejected |
|----------|-------------|
| Rust-style ownership | Requires static analysis. Restricts dynamic features (eval, runtime macros). Not Lisp. |
| Reference counting | Cycles leak. Every term reference needs inc/dec. More code than mark-sweep. |
| Copying GC | Requires moving terms, which means updating all references. Harder with lambda terms (closures capture references). |
| Region-based | Requires compile-time analysis. Too complex for a solo project. |

---

## 7. WHAT WAS REJECTED AND WHY

| Idea | Why Rejected |
|------|-------------|
| Metacircular (X = Y) | No layer boundary to optimize against. Bootstrap paradox. Knowing himself, Nacho would never optimize the core. |
| Pure lambda calculus (no ints) | Church numerals are impractical. O(n) addition. Can't compute 1000+1. |
| Pure SKI combinators | Not human-readable at all. Even compiler-generated code is unmanageable. |
| Lazy evaluation | Hides performance cost. Space leaks. Harder to implement. Doesn't force performance honesty. |
| `define` in Layer 2 | Breaks purity. Every special form is something Layer 1 must understand. Z combinator is a compiler transformation, not a language feature. |
| Full ANSI CL | Too complex. 1100 pages. Decades of work. |
| CLOS in kernel | Not irreducible. Can be macros + closures in Layer 3. |
| Stack-based VM | Replaced by lambda calculus reducer. The computational model is lambda calculus, not a register/stack machine. |
| Register-based VM (old IGASM) | Replaced by lambda calculus reducer. Same reason. The VM concept is now Layer 1 (the reducer), not a separate layer. |
| Arrays as the core data structure | Replaced by Church encoding. No arrays in any layer. Simplicity and purity over performance. |
| Mutation (setq, boxes, mutable cells) | Rejected. Fully immutable language. State threaded through recursion. Macros provide ergonomic syntax. If shared mutable state is needed in the future, boxes can be added then. YAGNI. |

---

## 8. KEY INSIGHTS

1. **Three layers, not two.** The metacircular approach (X = Y)
   collapses the computational core and the language into one thing.
   Three layers gives each concern its own home and its own spec.

2. **Layer 2 is lambda calculus, not bytecode.** Bytecode formats
   are ephemeral -- they change with hardware. Lambda calculus is
   permanent -- it has outlived every hardware architecture of the
   last 80 years and will outlive the next 80.

3. **Layer 2 is not human-writable.** This is a feature, not a bug.
   It means Layer 2 can be mechanically generated, mechanically
   optimized, and mechanically verified. Humans write ignisp;
   the compiler handles the rest.

4. **Eager evaluation forces honesty.** Lazy evaluation lets you
   write O(n²) code that looks like O(1). Eager evaluation doesn't
   hide the cost. This aligns with the goal of a language that
   teaches computing.

5. **The Z combinator is the price of purity.** No `define` in
   Layer 2 means recursion goes through Z. This is ugly for humans
   but invisible (the compiler handles it). The payoff: Layer 2
   has no special forms at all. Pure lambda + ints + IO.

6. **The bootstrap is a Python script, not a fat C kernel.** The
   old approach was to build a fat C kernel with cons/car/cdr/reader,
   use it to bootstrap, then throw it away. The new approach is to
   generate Layer 2 code from Python, test it on a Python reducer,
   then move to C. Cleaner, more testable, and the Python script
   is truly throwaway.

7. **Porting = rewriting Layer 1.** Only the reducer needs to be
   rewritten per platform. Everything else (Layer 2 and 3) is
   platform-independent. This is the portability promise.

8. **The FPGA path is a lambda calculus machine.** The reducer
   on FPGA is a graph reducer -- a well-studied hardware architecture.
   Terms in BRAM, reduction logic in LUTs, I/O via UART. This is
   not science fiction; it's been done.

---

## 9. OPEN QUESTIONS

- [x] **Arrays in Layer 2?** RESOLVED: No arrays. Everything is
      Church-encoded. Simplicity over performance. If performance
      becomes a problem, this can be revisited, but the spec starts
      pure.
- [x] **Mutation in ignisp?** RESOLVED: Fully immutable. No setq,
      no boxes, no mutable references anywhere. State is threaded
      through recursion. Macros (`->`, `loop`, `with-state`) provide
      ergonomic syntax that expands to immutable code. If shared
      mutable state is needed in the future (e.g., for concurrency),
      boxes can be added then. YAGNI.
- [ ] **Comparison return type.** Do <, >, = return Church-encoded
      booleans or native integers (0/1)? Church booleans are more
      pure but require the reducer to understand lambda terms in
      conditional positions.
- [ ] **How are symbols represented in Layer 2?** Symbols are needed
      for the reader and eval. Options: unique integers (symbol table
      in Layer 2), Church-encoded strings, or something else.
- [ ] **How are strings represented?** Church-encoded lists of
      integers? Something else?
- [ ] **Error handling in Layer 2.** Pure lambda calculus has no
      concept of errors. The reducer needs some error mechanism
      (halt? exception? Church-encoded error values?).
- [ ] **GC algorithm details.** Mark-sweep is chosen, but the
      specifics depend on term representation. To be finalized when
      Layer 1 is implemented.
- [ ] **Compilation model.** How does the Layer 3 → Layer 2 compiler
      work? Tree-walking? ANF (A-normal form)? CPS (continuation-passing
      style)? To be decided during bootstrap.
- [ ] **Layer 3 special forms.** Which special forms does ignisp
      have? The old design had 8 (if, quote, lambda, let, setq,
      defmacro, define, begin). setq is now removed (immutable).
      The remaining set needs to be finalized.
- [ ] **Exact Layer 2 term representation on the tape/in memory.**
      How are variables, abstractions, and applications represented
      in C? In Python? On FPGA? This is a Layer 1 implementation detail.

---

## NEXT STEPS

1. **Resolve remaining open questions.** Comparison return type,
   symbol representation, string representation, error handling.
2. **Write the Layer 2 spec.** Lambda + ints + IO. Evaluation rules,
   primitives, encoding of basic data (booleans, pairs, lists).
3. **Write the Layer 3 spec.** Ignisp: types, special forms, reader
   syntax, evaluation model. Immutable, no setq.
4. **Write the Layer 1 spec.** Reducer interface, term representation,
   memory model, I/O.
5. **Start the Python bootstrap.** Reducer + Layer 2 code generation.