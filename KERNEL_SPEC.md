# tinylisp -- Kernel Specification (Abstract)

> Version: 0.1 (draft)
> Date: 2026-07-06
>
> This document defines what a tinylisp kernel implementation must
> provide. It is the portable contract -- the document you rewrite
> when porting to a new host.
>
> This document contains ONLY abstract definitions. Concrete
> representation, encoding, and implementation details belong in
> the Reference Implementation document (KERNEL_REF.md).

---

## 1. VALUE DOMAIN

The kernel operates on a set of values V. V is the disjoint union
of the following types:

### 1.1 Fixnum

A fixnum is an integer in a finite range [F_min, F_max]. The
range is implementation-defined but MUST satisfy:
    F_max - F_min >= 2^31 - 1

The set of all fixnums is denoted Z_f.

### 1.2 Symbol

A symbol is an abstract identity with an associated name. The
name is a sequence of characters (definition of "character" is
implementation-defined).

Two symbols are the same if and only if they have the same
identity. Symbol identity is determined by interning: for any
given name, there exists at most one symbol with that name.

The set of all symbols is denoted S.

The following symbols are predefined and MUST exist in every
implementation:
    NIL   -- represents falsity and the empty list
    T     -- represents truth

Additionally, the following symbols MUST exist for the evaluator
to dispatch special forms (see Section 4):
    if, quote, lambda, let, setq, defmacro, function

### 1.3 Array

An array is a mutable sequence of values indexed by fixnums in
the range [0, n-1] for some fixnum n called the array's size.

An array of size n maps each index in [0, n-1] to a value in V.
All slots are initialized to NIL upon creation.

The set of all arrays is denoted A.

### 1.4 Function

A function is a callable entity. There are two kinds:

    Primitive:  provided by the kernel. Identified by a symbol.
    Closure:    created by the lambda special form. Consists of
                a parameter list, a body, and a captured
                environment (see Section 5).

The set of all functions is denoted F.

### 1.5 Type Predicates

For any value v in V, exactly one of the following holds:
    v is a fixnum
    v is a symbol
    v is an array
    v is a function

NIL is a symbol. T is a symbol. This means symbols serve double
duty as identifiers and as boolean values.

---

## 2. PRIMITIVE OPERATIONS

The kernel provides the following primitive operations. Each is
identified by a symbol and is callable as a function.

### 2.1 Array Operations

    make-array : Z_f -> A
        Given size n, allocate a new array of n slots, each
        initialized to NIL. n MUST be non-negative.

    aref : A x Z_f -> V
        Given array a and index i, return the value stored at
        position i. i MUST be in [0, size(a)-1].

    aset : A x Z_f x V -> V
        Given array a, index i, and value v, store v at position
        i and return v. i MUST be in [0, size(a)-1].

### 2.2 Arithmetic

    + : Z_f x Z_f -> Z_f
    - : Z_f x Z_f -> Z_f
    * : Z_f x Z_f -> Z_f
    / : Z_f x Z_f -> Z_f
        Standard integer arithmetic. Division truncates toward
        zero. Division by zero is an error.

    < : Z_f x Z_f -> {NIL, T}
    > : Z_f x Z_f -> {NIL, T}
    = : Z_f x Z_f -> {NIL, T}
        Comparison operations. Return T if the relation holds,
        NIL otherwise.

### 2.3 Character I/O

    read-char : -> Z_f | NIL
        Read one character from the input stream. Return its
        code as a fixnum. Return NIL at end of input. The
        mapping from characters to fixnum codes is
        implementation-defined.

    write-char : Z_f -> Z_f
        Write the character with the given code to the output
        stream. Return the code. The mapping from fixnum codes
        to characters is implementation-defined.

### 2.4 Error

    error : V -> (does not return)
        Signal an error. The argument may be any value (typically
        a string or symbol describing the error). Execution of
        the current evaluation stops. The behavior after an error
        (termination, restart, REPL re-entry) is
        implementation-defined.

---

## 3. ENVIRONMENT

An environment E is a mapping from symbols to values. Environments
form a tree structure:

    - There is a root environment (the global environment).
    - Each environment may have zero or more child environments.
    - Each child environment inherits all bindings from its
      parent and may add or shadow bindings.

Variable lookup: given symbol s in environment E, the value is
found by searching E, then E's parent, then its parent, and so
on up to the root. If s is not found in any environment in the
chain, an error is signaled ("unbound variable").

Variable assignment (setq): given symbol s and value v in
environment E, the nearest binding of s in the chain starting
at E is updated to v. If no binding exists, an error is
signaled.

Variable binding (let, lambda): a new child environment is
created with the specified bindings. The parent is the current
environment.

---

## 4. EVALUATION MODEL

The kernel provides an evaluator: a function eval that takes a
value (the form) and an environment, and produces a value (or
signals an error).

### 4.1 Evaluation Rules

Given form f and environment E:

    1. If f is NIL, return NIL.
    2. If f is T, return T.
    3. If f is a fixnum, return f.
    4. If f is a symbol other than NIL or T, look up f in E
       and return the bound value. If unbound, signal error.
    5. If f is an array, it represents a compound form (see 4.2).
    6. If f is a function, return f (functions are
       self-evaluating).

### 4.2 Compound Forms

If f is an array, it represents a list of sub-forms. The first
element (index 0) is the operator. The remaining elements are
the operands.

    operator = aref(f, 0)
    operands = aref(f, 1), aref(f, 2), ...

Evaluation proceeds as follows:

    a. If operator is a symbol naming a special form (Section 5),
       evaluate according to that special form's rules. Operands
       are NOT evaluated before dispatch (special forms control
       evaluation of their own operands).

    b. If operator is a symbol naming a macro (Section 6), expand
       the macro and evaluate the result.

    c. Otherwise, evaluate the operator and all operands, then
       apply the resulting function to the resulting arguments
       (Section 7).

### 4.3 Empty Compound Form

If f is an array of size 0 (equivalent to NIL), see rule 4.1.1.

If f is an array of size >= 1 but the operator position is NIL
or does not name a special form, macro, or function, an error is
signaled.

---

## 5. SPECIAL FORMS

Special forms are the kernel's built-in control structures. They
are recognized by symbol identity in the operator position of a
compound form. Each special form controls how (or whether) its
operands are evaluated.

### 5.1 if

    Form:   (if test then else)
    Eval:   Evaluate test in E. If the result is not NIL,
            evaluate then in E and return its value. Otherwise,
            evaluate else in E and return its value. If else is
            omitted, return NIL.

    Note:   Only one of then/else is evaluated. This is what
            distinguishes if from a function.

### 5.2 quote

    Form:   (quote form)
    Eval:   Return form unevaluated.

### 5.3 lambda

    Form:   (lambda (param...) body...)
    Eval:   Create a closure capturing the current environment E.
            The closure contains: the parameter list, the body
            forms, and E. Return the closure.

    When called with arguments arg1...argN:
            Create a child environment of E. Bind each param to
            the corresponding arg. Evaluate body forms in
            sequence. Return the value of the last body form.

### 5.4 let

    Form:   (let ((var val)...) body...)
    Eval:   Evaluate each val in E (the current environment).
            Create a child environment of E. Bind each var to
            its corresponding val. Evaluate body forms in
            sequence in the child environment. Return the value
            of the last body form.

    Note:   All vals are evaluated in E BEFORE any var is bound.
            This is parallel let, not sequential.

### 5.5 setq

    Form:   (setq var val)
    Eval:   Evaluate val in E. Find the nearest binding of var
            in the environment chain starting at E. Update it
            to the evaluated value. Return the value.

    Note:   setq does not create new bindings. If var is unbound,
            an error is signaled.

### 5.6 defmacro

    Form:   (defmacro name (param...) body...)
    Eval:   Create a macro expander: a closure-like object that
            captures E, with params and body. Associate name
            with this macro expander in the current environment.
            Return name.

    When a compound form (name arg...) is encountered and name
    is a macro:
            Call the macro expander with the UNEVALUATED
            arguments arg... The expander returns a form. This
            form is then evaluated in place of the original
            (name arg...) form.

### 5.7 function

    Form:   (function name)
    Eval:   If name is a symbol, return the function bound to
            name in the current environment. If name is a lambda
            form, evaluate it as if by the lambda special form
            and return the resulting closure.

    Note:   This distinguishes the function value of a symbol
            from its variable value. (function foo) returns the
            function, while foo (as a variable) returns the
            value bound to foo.

---

## 6. MACROS

Macros are user-defined code transformations. They are created
by defmacro and are stored in the environment alongside variable
bindings.

When the evaluator encounters a compound form whose operator is
a symbol with an associated macro, the macro is expanded before
evaluation. The expansion replaces the original form, and the
result is evaluated.

Macro expansion is NOT recursive at the kernel level: the
evaluator expands one level, then evaluates the result. If the
result is itself a macro call, it will be expanded during
evaluation of the result.

---

## 7. FUNCTION APPLICATION

When a compound form is neither a special form nor a macro, it
is a function call:

    (f arg1 arg2 ... argN)

1. Evaluate f in E to obtain a function value.
2. Evaluate each argi in E to obtain argument values.
3. If the function is a primitive, call it with the arguments.
4. If the function is a closure, create a child environment of
   the closure's captured environment, bind parameters to
   arguments, evaluate the body, return the last value.

The number of arguments MUST match the number of parameters.
Mismatch is an error.

---

## 8. TRUTHINESS

NIL is false. Every other value is true. There is no separate
boolean type. T is the conventional true value, but any non-NIL
value is accepted as true in boolean context (the test of if,
etc.).

---

## 9. WHAT THE KERNEL DOES NOT PROVIDE

The kernel does NOT provide:

    - cons, car, cdr        (build on aref/aset/make-array)
    - strings               (build on arrays + character codes)
    - the reader            (build on read-char)
    - the printer           (build on write-char)
    - numbers beyond fixnum (build on arrays of fixnums)
    - garbage collection    (build on array-based mark-sweep)
    - cond, and, or, when   (macros, build on if)
    - let*, labels, flet    (macros, build on let)
    - type predicates       (build on = and representation checks)
    - eq, eql, equal        (eq is = on fixnums/symbols; build rest)

This list is not exhaustive. The principle is: if it can be
built from the kernel primitives and special forms, it does not
belong in the kernel.