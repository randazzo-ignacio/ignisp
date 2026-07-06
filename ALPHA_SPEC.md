# ignisp v0.1 -- Alpha Specification

> Date: 2026-07-06
> Status: FROZEN -- documents the pilot implementation as-is.
> This spec describes what ignisp v0.1 IS, not what it should be.
> Future versions may change any of this.
>
> The implementation is in ignisp.c (~500 lines of C).
> Build: gcc -o ignisp ignisp.c
> Run:   ./ignisp

---

## 1. LEXICAL SYNTAX

### 1.1 Whitespace

Space, tab, newline, and carriage return are whitespace. Whitespace
separates tokens and is otherwise ignored.

### 1.2 Comments

A semicolon (;) begins a line comment. The comment extends to the
next newline or end of input. Comments are ignored.

There are no block comments.

### 1.3 Strings

A string is delimited by double quotes ("). The following escape
sequences are recognized inside a string:

    \"    literal double quote
    \\    literal backslash
    \n    newline
    \t    tab

Any other character after a backslash is an error. A string may not
contain a literal newline (this is an error). Strings have a maximum
length of 1023 characters.

### 1.4 Numbers

A number is a sequence of decimal digits, optionally preceded by a
minus sign (-). Numbers are read as signed 64-bit integers (C long).

Examples: 0, 42, -17, 1000000

A minus sign not followed by a digit is read as the symbol "-".

### 1.5 Symbols

A symbol is a maximal sequence of characters that are not whitespace,
not (, not ), not ;, not ", and not '. There are no other
restrictions on symbol names.

Symbols are interned: two symbols with the same name are the same
object (identical by pointer equality).

The tokens "nil" and "t" are read as the predefined symbols NIL and
T (see Section 2), not as ordinary symbols.

Symbol names have a maximum length of 255 characters.

### 1.6 Lists

An open parenthesis ( begins a list. Forms are read sequentially
until a close parenthesis ). The forms are collected into a cons
cell chain (a proper list). The empty list () is read as NIL.

If end of input is reached before ), an error is signaled.

### 1.7 Dotted Pairs

A dot (.) appearing as a standalone token inside a list (surrounded
by whitespace or delimiters) is a dotted-pair marker. The form
before the dot is the car, the form after the dot is the cdr. A
close parenthesis must follow the cdr form.

Examples:
    (a . b)     cons with car=a, cdr=b
    (a b . c)   cons with car=a, cdr=(b . c)

A dot that is not standalone (part of a longer token) is read as
part of a symbol. Example: foo.bar is the symbol "foo.bar".

### 1.8 Quote

A single quote (') followed by a form is read as (quote form).

Examples:
    'x       -> (quote x)
    '(a b)   -> (quote (a b))

A quote at end of input is an error.

### 1.9 Token Dispatch Summary

The reader dispatches on the first character:

    Character    Token type
    ----------   ----------
    whitespace   skipped
    ;            comment (skipped to newline)
    "            string
    digit or -   number (if digits follow) or symbol
    (            list begin
    )            list end (or error if not in a list)
    '            quote macro
    . (standalone in list)  dotted pair marker
    other        symbol

---

## 2. DATA TYPES

There are seven types. Every value is exactly one type.

### 2.1 Fixnum

A signed 64-bit integer. Self-evaluating. Literal syntax: decimal
digits, optionally preceded by -.

### 2.2 Symbol

An interned name. Symbols serve as both identifiers (variable names)
and as values. Self-evaluating EXCEPT when used as a variable
reference (see Evaluation Model).

Two predefined symbols exist:
    NIL -- the empty list and the false value
    T   -- the canonical true value

NIL and T are symbols. They are not separate types.

### 2.3 Cons

A pair of two values: car and cdr. Cons cells are the building block
of lists. Literal syntax: parenthesized lists and dotted pairs.

### 2.4 String

A sequence of characters. Self-evaluating. Literal syntax:
double-quoted with escapes. Maximum 1023 characters.

### 2.5 Array

A mutable, fixed-size sequence of values indexed by fixnums from 0
to size-1. All slots initialize to NIL. No literal syntax (created
by make-array). Self-evaluating.

### 2.6 Closure

A function created by lambda or define. Contains a parameter list,
a body (list of forms), and a captured environment. Self-evaluating.
Printed as #<closure>.

### 2.7 Primitive

A built-in function provided by the kernel. Self-evaluating. Printed
as #<primitive>.

---

## 3. EVALUATION MODEL

The evaluator takes a form and an environment and produces a value
or signals an error.

### 3.1 Self-Evaluating Forms

The following forms evaluate to themselves:
    - NIL
    - T
    - Fixnums
    - Strings
    - Arrays
    - Closures
    - Primitives

### 3.2 Symbol Evaluation

A symbol (other than NIL and T) is evaluated as a variable reference.
The evaluator searches the environment chain for a binding of that
symbol. If found, the bound value is returned. If not found, an
error is signaled ("unbound variable: <name>") and the REPL recovers.

### 3.3 Cons Evaluation

A cons is evaluated as a compound form. The car is the operator,
the cdr is the argument list.

    1. If the operator is a symbol naming a special form, the
       special form is evaluated (operands are NOT pre-evaluated).

    2. If the operator is a symbol naming a macro (in the macro
       table), the macro is expanded: the macro function is called
       with the UNEVALUATED arguments, and the result is then
       evaluated in place of the original form.

    3. Otherwise, the operator and all arguments are evaluated
       left-to-right, and the resulting function is applied to
       the resulting arguments.

If the operator is not a symbol, it is evaluated as a regular form
and used as a function.

### 3.4 Function Application

When a function is called:
    - If it is a primitive, the C function is called with the
      argument list (a cons chain).
    - If it is a closure, a new environment is created as a child
      of the closure's captured environment. Parameters are bound
      to arguments. The body forms are evaluated in sequence. The
      last value is returned.

If the value in operator position is not a function (not a closure
or primitive), an error is signaled ("not callable").

### 3.5 Argument Evaluation

Arguments are evaluated left-to-right in the caller's environment
before the function is called. The evaluated arguments are collected
into a new list. This list is passed to primitives as-is and used
to bind closure parameters.

### 3.6 Parameter Binding

Closures bind parameters positionally. Each parameter symbol is
bound to the corresponding argument in a new child environment.
Extra arguments are ignored. Missing arguments leave the parameter
unbound (accessing it will signal an error).

NOTE: Closures do NOT support rest parameters (dotted params).
Only macros support rest parameters (see Section 5.6).

---

## 4. SPECIAL FORMS

### 4.1 if

    (if test then [else])

Evaluate test. If the result is not NIL, evaluate then and return
its value. Otherwise, evaluate else (if present) and return its
value, or return NIL if else is omitted.

Only one of then/else is evaluated.

### 4.2 quote

    (quote form)

Return form unevaluated.

### 4.3 lambda

    (lambda (params...) body...)

Create a closure capturing the current environment. When called,
bind params to arguments in a child environment, evaluate body
forms in sequence, return the last value.

### 4.4 let

    (let ((var val)...) body...)

Evaluate each val in the CURRENT environment (not the new one).
Create a child environment binding each var to its val. Evaluate
body forms in sequence in the new environment. Return the last
value.

This is PARALLEL let: all vals are evaluated before any var is
bound. A var cannot reference other vars in the same let.

### 4.5 setq

    (setq var val)

Evaluate val. Search the environment chain for an existing binding
of var. If found, update it. If NOT found, create a new binding in
the GLOBAL environment (this is a convenience behavior -- setq on
an unbound variable does not error, it creates a global).

Returns val.

### 4.6 define

    (define name value)
    (define (name params...) body...)

First form: evaluate value in the current environment and bind name
to the result in the GLOBAL environment. Returns name.

Second form: create a closure with params and body capturing the
global environment. Bind name to this closure in the global
environment. The name is pre-bound to NIL before the closure is
created, so the closure can reference itself (enabling recursion).
Returns name.

define ALWAYS binds in the global environment, regardless of the
current environment.

### 4.7 defmacro

    (defmacro name (params...) body...)
    (defmacro (name . params) body...)

Define a macro. The macro function is a closure that captures the
current environment. It is stored in the macro table (separate from
the function/variable environment).

When a form (name arg...) is evaluated and name is a macro, the
macro function is called with the UNEVALUATED arguments. The result
of the macro function is then evaluated in place of the original
form.

Returns name.

### 4.8 begin

    (begin expr...)

Evaluate each expr in sequence in the current environment. Return
the last value. If no expressions, return NIL.

---

## 5. MACROS

### 5.1 Macro Definition

Macros are defined with defmacro and stored in a separate macro
table. A macro name can coexist with a function of the same name;
the macro table is checked before function lookup.

### 5.2 Macro Expansion

When the evaluator encounters a compound form whose operator is a
symbol with a macro binding:
    1. The macro function is called with the unevaluated arguments.
    2. The result (a form) is evaluated in the caller's environment.
    3. This result replaces the original form.

Macro expansion is one level: the evaluator expands the macro, then
evaluates the result. If the result is itself a macro call, it will
be expanded during that evaluation.

### 5.3 Macro Parameters

Macro parameters are bound positionally, like closures. Unevaluated
argument forms are bound to parameter symbols.

### 5.4 Rest Parameters in Macros

Macros support rest parameters via dotted notation:

    (defmacro (name a b . rest) ...)

Here, `a` and `b` are bound to the first two unevaluated arguments,
and `rest` is bound to a list of the remaining unevaluated arguments.

If fewer arguments are provided than required parameters, the
missing parameters are bound to NIL. If the rest parameter exists
but no extra arguments are provided, it is bound to NIL.

### 5.5 Macros vs Closures

Macros and closures are both stored as closure objects, but they
behave differently:
    - Closures receive EVALUATED arguments.
    - Macros receive UNEVALUATED arguments.
    - Closures do NOT support rest parameters.
    - Macros DO support rest parameters.
    - The macro result is evaluated; the closure result is returned
      directly.

### 5.6 Quoting in Macros

Since macro arguments are unevaluated forms, the macro body must
use quote to prevent premature evaluation when constructing the
output form. The typical pattern is:

    (defmacro (when c . body)
      (list (quote if) c (cons (quote begin) body)))

Here, c and body are forms (unevaluated). The list function
constructs a new form that will be evaluated after expansion.

---

## 6. PRIMITIVES

All primitives take their arguments as a single linked list (the
cons chain of evaluated arguments).

### 6.1 List Operations

    (car x) -> value
        Return the car of x. If x is NIL, return NIL.
        If x is not a cons and not NIL, signal error.

    (cdr x) -> value
        Return the cdr of x. If x is NIL, return NIL.
        If x is not a cons and not NIL, signal error.

    (cons a b) -> cons
        Return a new cons cell with car=a, cdr=b.

    (list a b c ...) -> list
        Return a cons chain of the arguments. (list) returns NIL.
        This is an identity function on its argument list -- it
        returns the raw argument list as-is.

### 6.2 Arithmetic

All arithmetic operates on fixnums. Non-fixnum arguments cause
undefined behavior (no type checking in v0.1).

    (+ a b c ...) -> fixnum
        Sum of all arguments. (+) returns 0.

    (- a b c ...) -> fixnum
        If one argument: return its negation.
        If multiple: a - b - c - ...
        (-) returns 0.

    (* a b c ...) -> fixnum
        Product of all arguments. (*) returns 1.

    (/ a b c ...) -> fixnum
        Integer division, truncating toward zero.
        a / b / c / ... Left to right.
        Division by zero signals an error.
        (/) signals an error.

### 6.3 Comparison

All comparisons take exactly two fixnum arguments.

    (< a b) -> T or NIL
    (> a b) -> T or NIL
    (= a b) -> T or NIL

### 6.4 Equality

    (eq a b) -> T or NIL
        Return T if a and b are the same object (pointer equality),
        OR if both are fixnums with the same value. Otherwise NIL.

        Note: This means eq on fixnums is value equality, not
        identity. This is a convenience for the tagged representation
        where equal fixnums may or may not be the same pointer.

### 6.5 Predicates

    (null x) -> T or NIL
        Return T if x is NIL, otherwise NIL.

### 6.6 Array Operations

    (make-array size) -> array
        Allocate a new array of size elements, each initialized to
        NIL. size must be a fixnum.

    (aref array index) -> value
        Return the value at position index. Signals error if index
        is out of bounds [0, size-1].

    (aset array index value) -> value
        Store value at position index. Return value. Signals error
        if index is out of bounds.

### 6.7 Character I/O

    (read-char) -> fixnum or NIL
        Read one character from standard input. Return its ASCII
        code as a fixnum. Return NIL at end of input.

    (write-char code) -> fixnum
        Write the character with the given ASCII code to standard
        output. Return the code.

### 6.8 Utility

    (print x) -> x
        Print x to standard output (using the printer, Section 7).
        Return x. No newline is printed.

    (eval form) -> value
        Evaluate form in the global environment. Return the result.

    (error x) -> (does not return)
        Print "error: " followed by x to standard output, then
        signal an error. In the REPL, this returns to the prompt.
        In script mode, this would terminate (v0.1 only has REPL
        mode).

---

## 7. PRINTER

The printer produces a textual representation of any value.

    Type        Output
    ----        ------
    NIL         nil
    T           t
    Fixnum      decimal digits (e.g. 42, -17)
    Symbol      the symbol name (e.g. foo, +, setq)
    String      "content" (with double quotes, no escape on output)
    Cons        (a b c) or (a . b) for dotted pairs
    Array       #<array:N> where N is the size
    Closure     #<closure>
    Primitive   #<primitive>

Lists are printed as space-separated elements in parentheses. If
the cdr of the last cons is not NIL and not a cons, it is printed
after a dot: (a b . c).

Strings are printed WITH surrounding double quotes but WITHOUT
escaping internal characters. This means a string containing a
double quote will not round-trip correctly through read/print.

---

## 8. ENVIRONMENT AND SCOPING

### 8.1 Environment Structure

Environments are association lists (linked lists of (symbol . value)
pairs). Each environment has a parent environment, forming a chain
ending at the global environment.

### 8.2 Variable Lookup

When a symbol is evaluated, the environment chain is searched from
the current environment outward. The first binding found is used.
If no binding is found, an error is signaled.

### 8.3 Variable Binding

New bindings are created by:
    - let (creates a child environment with the specified bindings)
    - lambda/define function calls (parameter bindings in child env)
    - define (always binds in the global environment)

### 8.4 Variable Assignment

setq searches the environment chain for an existing binding and
updates it. If no binding is found, it creates one in the global
environment (this is a v0.1 convenience -- it does not signal an
error).

### 8.5 Closure Capture

Closures capture the environment by reference. This means:
    - Mutations to the captured environment (via setq) are visible
      to the closure.
    - Multiple closures sharing the same environment share state.
    - This enables mutable counters and shared state patterns.

Example:
    (define (counter)
      (let ((n 0))
        (lambda () (setq n (+ n 1)) n)))
    (define c (counter))
    (c)  -> 1
    (c)  -> 2
    (c)  -> 3

---

## 9. BOOLEAN SEMANTICS

NIL is false. Every other value is true. There is no boolean type.

T is the conventional true value, but any non-NIL value is accepted
as true in the test position of if.

---

## 10. THE REPL

### 10.1 Startup

On startup, ignisp initializes the global environment with all
primitives and predefined symbols, then prints "ignisp 0.1 (pilot)"
and enters the read-eval-print loop.

### 10.2 Loop

    1. Print "> " as the prompt.
    2. Read a form.
    3. If EOF, exit.
    4. Evaluate the form in the global environment.
    5. Print the result.
    6. Print a newline.
    7. Go to 1.

### 10.3 Error Recovery

If an error occurs during read or eval, the error message is printed
to stderr (or stdout for the error primitive), and the REPL returns
to the prompt. Any remaining input on the current line is consumed
before the next prompt.

---

## 11. KNOWN LIMITATIONS (v0.1)

These are NOT bugs to fix -- they are the defined behavior of v0.1.

1. NO GARBAGE COLLECTION. All allocations leak. Long-running programs
   will exhaust memory.

2. NO TAIL CALL OPTIMIZATION. Deep recursion will overflow the C
   stack. The limit depends on the platform but is typically a few
   thousand frames.

3. CLOSURES DO NOT SUPPORT REST PARAMETERS. Only macros support
   dotted-parameter (rest) syntax. A closure with (a b . rest) will
   bind a and b but ignore the rest parameter.

4. NO TYPE CHECKING ON ARITHMETIC. Passing non-fixnums to +, -, *, /
   causes undefined behavior (likely garbage results or crash).

5. NO TYPE PREDICATES. There is no way to check the type of a value
   at runtime (no symbolp, consp, fixnump, etc.).

6. STRINGS DO NOT ROUND-TRIP. The printer does not escape special
   characters in strings. A string containing " will not read back
   correctly.

7. NO CHARACTER TYPE. Characters are represented as fixnum ASCII
   codes. There is no character literal syntax.

8. NO MULTIPLE VALUES. Functions return exactly one value.

9. NO ERROR RECOVERY IN SCRIPTS. The error primitive and unbound
   variable errors use setjmp/longjmp to return to the REPL. There
   is no try/catch mechanism.

10. SETQ ON UNBOUND VARIABLES CREATES GLOBALS. This is intentional
    convenience for v0.1 but may surprise users expecting an error.

11. DEFINE ALWAYS BINDS GLOBALLY. Even inside a let or lambda body,
    define creates a global binding, not a local one.

12. NO MODULES OR PACKAGES. All symbols share a single namespace.

13. FIXED BUFFER SIZES. Symbols are limited to 255 characters,
    strings to 1023 characters. Longer tokens are silently truncated.

14. NO FILE I/O. Only stdin/stdout via read-char/write-char.

15. EQ ON FIXNUMS IS VALUE EQUALITY, NOT IDENTITY. (eq 1 1) returns
    T. This is because the implementation may or may not intern
    fixnums, so eq was made to compare fixnum values directly.

---

## 12. REFERENCE CARD

### Special Forms

    (if test then [else])          conditional
    (quote form)                   prevent evaluation
    (lambda (params...) body...)   create closure
    (let ((var val)...) body...)   parallel binding
    (setq var val)                 assignment (creates global if unbound)
    (define name value)            global definition
    (define (name params...) body...)  function definition (recursive)
    (defmacro name (params...) body...)  macro definition
    (defmacro (name . params) body...)  macro with rest params
    (begin expr...)                sequential evaluation

### Primitives

    (car x)          (cdr x)          (cons a b)      (list ...)
    (+ ...)          (- ...)          (* ...)         (/ ...)
    (< a b)          (> a b)          (= a b)
    (eq a b)         (null x)
    (make-array n)   (aref a i)       (aset a i v)
    (read-char)      (write-char c)
    (print x)        (eval form)      (error x)

### Reader Syntax

    (...)     list             ()        NIL
    'x        (quote x)        "..."     string
    ;...      comment          (a . b)   dotted pair
    42        fixnum           -17       negative fixnum
    nil       NIL              t         T
