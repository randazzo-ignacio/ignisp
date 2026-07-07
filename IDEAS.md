# ignisp -- Ideas & Notes

> Created: 2026-07-07 (late night session, 2026-07-10 in local time)
> Status: Exploratory notes from design conversations
> Purpose: Capture ideas, insights, and discussion points that don't
> belong in the formal design notes but are worth preserving.

---

## 1. THE ORIGIN OF THE THREE-LAYER DESIGN

The three-layer design emerged from a conversation about the bootstrap
problem. The original design was metacircular: one Lisp implementing
itself on top of a minimal C kernel. Two problems surfaced:

1. **The bootstrap paradox.** To write the reader in Lisp, you need
   cons cells. To build cons cells, you need the reader to load the
   code that defines cons cells. Solvable but fragile.

2. **No optimization boundary.** If the kernel and the language are
   the same thing, "optimize cons" has no home. Knowing himself,
   Nacho would pour all effort into language features and leave the
   core unoptimized forever.

The breakthrough was realizing that Layer 2 (the computational core)
and Layer 3 (the language) don't have to be the same language. They
share a relationship (Layer 2 is what Layer 3 compiles to), but they
have different jobs, different specs, and different optimization
concerns.

This is how real systems work: C → LLVM IR → x86, Java → bytecode →
JVM, Haskell → Core → STG → machine. The three-layer design is the
standard compiler pipeline, with lambda calculus as the IR.

---

## 2. WHY LAMBDA CALCULUS AS THE IR

Most languages invent a custom bytecode IR. ignisp uses lambda calculus.
The bet: lambda calculus is a better IR than a custom bytecode because
it's permanent. In 50 years, a custom bytecode designed for today's
hardware will be irrelevant. Lambda calculus will still be lambda
calculus.

GHC uses lambda calculus (Core) as its IR, but then compiles *away*
from it to efficient machine code. ignisp chooses to *stop at* lambda
calculus and run it directly. This is the unusual choice. It trades
performance for portability and simplicity.

The escape hatch: if performance is ever needed, ignisp can be
transpiled to whatever target language exists in the future. Lambda
calculus as IR doesn't prevent that — it enables it, because lambda
calculus is the most studied compilation target in computer science.

---

## 3. THE "WHY HAS NO ONE BUILT THIS" QUESTION

Nacho's instinct to ask "why has no one built this yet" is a quality
filter, not a hesitation. The answer for the three-layer design:
people have built it (GHC, Turner's combinator reduction, the
Reduceron FPGA), but they all kept going past lambda calculus to
something faster. Nacho is choosing to stop there.

The reason no mainstream language stops at lambda calculus is
performance. For a production language, that's the right call. For
a personal language with a 50-year horizon, the permanence of the
computational model matters more than current execution efficiency.

---

## 4. PERFORMANCE ESTIMATES

### The key insight

ignisp's reducer is an interpreter. Python is also an interpreter.
The per-operation overhead is similar. The difference is how many
operations each language needs to express the same computation.

ignisp is N× slower than Python, where N is the ratio of ignisp
operations to Python operations for the same program. The complexity
class is the same (O(n) in Python is O(n) in ignisp), just with
bigger constants.

### Estimated performance vs Python (unoptimized)

| Program type | vs Python | Usable? |
|---|---|---|
| Arithmetic (fib, factorial) | 3-5x slower | Yes |
| List processing (map, filter, reduce) | 5-10x slower | Yes |
| Sequential file I/O + processing | 10-20x slower | Yes |
| String/text processing | 20-50x slower | Borderline |
| Random access into lists | 100-500x slower | Painful |
| REPL responsiveness | 5-10x slower | Yes |
| Bootstrap (reader+eval on reducer) | 100-1000x slower | One-time pain |

### With Layer 3 compiler optimizations

| Program type | vs Python (optimized) |
|---|---|
| Arithmetic | 2-3x slower |
| List processing | 3-5x slower |
| File I/O + processing | 5-10x slower |
| String processing | 10-30x slower |
| REPL | 2-5x slower |

### Why there are no exponential operations

Every operation is O(1), O(n), or at worst O(n²). The only way to get
exponential behavior in lambda calculus is naive substitution
(copying N for every occurrence of x during beta reduction). No
practical reducer does this — they all use environments (deferred
substitution), making beta reduction O(1).

A 100MB file is 100 million O(1) operations. Maybe 10-30 seconds.
Memory-heavy (100 million cons cells ≈ several GB), but linear,
not exponential.

### The Achilles heel: strings

Strings as linked lists of integers are fundamentally expensive. A
1MB string is 1 million closures in memory. This is the one place
where the no-arrays decision hurts most.

Possible future mitigation: add a string type as a Layer 1 primitive
(a contiguous buffer), even if everything else stays Church-encoded.
This doesn't violate the architecture — it's the same as having
native integers instead of Church numerals. Not needed now, but
worth keeping in mind.

---

## 5. IMMUTABILITY AND HOW MACROS REPLACE MUTATION

### The three patterns that replace mutation

1. **Accumulator loops → recursion.** Instead of `total = 0; for
   item in list: total += item`, you write a tail-recursive function
   that threads the accumulator forward.

2. **Building up data → cons + recursion.** Instead of appending to
   a mutable list, you cons elements onto the front as you recurse,
   building the result list on the way back up.

3. **State machines → closures + recursion.** Instead of mutable
   state variables, you thread state through recursive calls. Each
   "mutation" is a new binding that shadows the old one.

### The macros that make this ergonomic

| Macro | Replaces | How |
|---|---|---|
| `->` (thread-first) | Sequential mutation (x = f(x)) | Threads result through as first argument of next expression |
| `loop` / `for` | For loops with accumulators | Expands to tail-recursive accumulator functions |
| `with-state` | Multi-variable state machines | Threads state variables through recursion, `set` becomes shadowing |

`->` alone covers 80% of script-writing use cases. `loop` covers
most of the rest. `with-state` is for complex parsers and state
machines.

### What you CAN'T do without mutation

Shared mutable state across concurrent contexts. If two functions
need to see updates to the same variable and they're not in a
caller-callee relationship, you need some form of mutable reference.

For sequential scripts (read, transform, write) — the stated use
case — you never need this. The state is always threaded top-to-bottom.

If concurrency is ever needed (10+ years), boxes (mutable cells) can
be added as a Layer 1 primitive. The architecture doesn't prevent it
— it just doesn't include it yet. YAGNI.

---

## 6. DISTRIBUTED LAMBDA REDUCTION (FUTURE EXPLORATION)

### The difficulties, ranked by severity

1. **Substitution across nodes (hard, solvable).** Beta reduction
   replaces x with N everywhere in M. If M is spread across nodes,
   you need remote references (graph reduction) instead of copying.

2. **Network latency (fundamental, not solvable).** A single beta
   reduction on CPU: nanoseconds. Network round-trip: milliseconds.
   1,000,000x difference. Only parallelism helps — sequential
   computations are millions of times slower distributed.

3. **Distributed GC (hard, well-studied).** Terms live across nodes.
   When is a term safe to collect? Local GC per node + reference
   counting for remote references. Solvable but adds complexity.

4. **Load balancing (moderate).** Lambda terms grow and shrink
   unpredictably. Work-stealing (like Cilk, parallel Haskell) is
   the standard approach.

5. **Ordering/consistency (moderate).** Eager evaluation has a
   defined order. In distributed systems, argument nodes must
   synchronize before function application. Standard barrier
   synchronization.

### Why it's research, not engineering

Lambda calculus reduction is graph rewriting. The graph structure
is dynamic and data-dependent — you don't know the shape of the
computation graph until you're computing it. You can't pre-partition
it efficiently. Compare to matrix multiplication (GPU's bread and
butter): known data layout, regular computation, trivial parallelism.

### Why it's still encouraging for ignisp

Layer 2 doesn't change. Lambda calculus is lambda calculus whether
it runs on one CPU, a GPU, or a cluster. The distributed problem is
entirely a Layer 1 implementation concern. The three-layer design
specifically insulates ignisp from this.

### What a distributed Layer 1 would look like (someday)

- Combinator graph reduction (S, K, I) instead of direct lambda terms
- Work-stealing for load balancing
- Remote references for substitution
- Local GC per node + reference counting for cross-node references
- Maybe 2,000-3,000 lines of code. Hard but not impossible for one
  person with 10 years.

For ignisp's use case (personal computing, scripts), most programs
are sequential. A distributed reducer would be slower, not faster.
But it would *work*, and "works on distributed systems" might matter
more than "is fast on distributed systems" in the 50-year horizon.

---

## 7. THE BOOTSTRAP RISK

The architecture is a 50-year decision. The bootstrap is a 3-month
sprint. They have different risk profiles.

The real risk is not "is the architecture sound" (it is) but "can
you bootstrap it before you lose interest?" Getting from "lambda
calculus reducer in C" to "a Lisp you can write programs in" is a
lot of work. Church-encoded cons cells are slow. The reader on a
naive reducer might take seconds to parse a small file. The first
REPL might take 1-5 seconds to evaluate a simple expression.

The Python bootstrap must be quick and dirty. Get ignisp running,
even if it takes 10 seconds to evaluate (+ 1 2). Then optimize the
reducer. Then rewrite the reader in ignisp. Then the eval. One step
at a time.

---

## 8. CONCEPTS AND REFERENCES MENTIONED

Terms that came up during the conversation, for future reference:

- **McCarthy's metacircular evaluator** (1960): eval written in Lisp
  itself. "Recursive Functions of Symbolic Expressions and Their
  Computation by Machine." The insight that eval is just a Lisp
  function. Paul Graham's "The Roots of Lisp" is the modern retelling.

- **Y combinator**: λf. (λx. f(x x))(λx. f(x x)). Enables recursion
  in lazy lambda calculus. Expands infinitely under eager evaluation.

- **Z combinator**: λf. (λx. f(λv. x x v))(λx. f(λv. x x v)). The
  eager-evaluation-compatible version of Y. What ignisp uses for
  recursion in Layer 2.

- **Omega combinator**: (λx. x x)(λx. x x). Expands to itself
  forever. The simplest non-terminating lambda term.

- **Church encoding**: representing data as functions. Booleans,
  pairs, lists, numbers — all expressible as lambda terms without
  any primitives. The foundation of Layer 2's data representation.

- **SKI combinators**: S, K, I. Three combinators that are Turing-
  complete. S = λx.λy.λz.(x z)(y z), K = λx.λy.x, I = λx.x. The
  theoretical floor of computation. Not practical for programming
  but relevant for the distributed reduction path (combinator graph
  reduction).

- **GHC Core**: Haskell's intermediate representation, essentially
  typed lambda calculus. The closest mainstream example of lambda
  calculus as IR. GHC then compiles Core through STG → Cmm → assembly.

- **The Reduceron**: FPGA-based graph reducer for a functional
  language, by Tomasz Radko. Proves that lambda calculus on hardware
  is feasible. Relevant for the hardware path.

- **Turner's combinator reduction**: 1970s-80s work on compiling
  functional languages to SKI combinators and reducing them on graph
  reduction machines. The historical basis for the Layer 2 → Layer 1
  approach.

- **Clojure's approach to immutability**: immutable data by default,
   with explicit mutable references (atoms) for state. ignisp is
   more radical — no mutable references at all, just macros. But
   Clojure's pattern is the reference point if boxes are ever added.

- **SICP metacircular evaluator**: Structure and Interpretation of
  Computer Programs. The REPL without mutation pattern (environment
  passed forward, not mutated) is literally how the SICP evaluator
  works.

---

## 9. THOUGHTS ON THE EXPLORATORY PHASE

Nacho is still in the exploratory phase. These conversations are for
documentation gathering — pouring ideas out of his head into files.
He will rewrite things by hand in a few days. The conversations and
files are raw material, not final products.

This is the right approach. The design notes capture decisions and
rationale. This file captures the thinking process, the alternatives
considered, and the reasoning behind the decisions. Both are needed:
the design notes for "what we decided," this file for "why we decided
it and what we considered along the way."

The next step after exploration: write the formal specs (Layer 1,
Layer 2, Layer 3) and start the Python bootstrap. But not yet. The
exploration needs to finish first.