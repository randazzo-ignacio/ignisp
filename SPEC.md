# tinylisp -- Language Specification

> Version: 0.1 (draft)
> Date: 2026-07-06
>
> This specification defines the tinylisp programming language in
> three parts:
>
>   Part 1 -- Kernel: what the host implementation must provide.
>            This is the portable contract. Rewrite this per host.
>
>   Part 2 -- Language: the reader, evaluator, types, and scoping
>            rules that define tinylisp source code and its meaning.
>            Built on top of the kernel. Frozen once stable.
>
>   Part 3 -- Standard Library: functions and macros implemented
>            in tinylisp itself. Grows unbounded over time.
>
> The specification is written in terms of mathematical structure,
> not concrete encoding. Character assignments are defined as
> implementation parameters with a reference assignment. This
> separates the structure of the language (permanent) from its
> encoding (disposable, may change per host or per era).

---

# PART 2: LANGUAGE

## 1. LEXICAL SYNTAX

### 1.1 Preliminaries

The reader consumes a sequence of characters. A character is an
atomic unit of input as provided by the host. The specific
character encoding (ASCII, UTF-8, EBCDIC, custom) is
implementation-defined.

The reader produces values (defined in Section 2). The mapping
from character sequences to values is defined by the grammar
below.

### 1.2 Character Classes

The following character classes are used in the grammar. Each
class is defined by its role, not by specific characters. The
reference character assignment is given in parentheses.

    class           role                         reference chars
    -----           ----                         ---------------
    digit           decimal digit                0 1 2 3 4 5 6 7 8 9
    whitespace      token separator              space, tab, newline, CR
    comment         begins a comment             ;
    string-delim    delimits a string            "
    group-open      begins a list                (
    group-close     ends a list                  )
    quote           quote reader macro           '
    dot             dotted-pair marker           . (when standalone)
    symbol-char     any character not in the     (all others)
                    above classes

An implementation MUST provide at least one character for each
class. An implementation MAY provide additional characters in
any class. The reference assignment is the default; alternative
assignments MUST be documented by the implementation.

### 1.3 Grammar

The reader operates as follows. At each position in the input
sequence, the next token is determined by the first character:

    char class        action
    ----------        ------
    whitespace        skip, continue to next char
    comment           skip to end of line (next newline or EOF),
                      continue
    string-delim      read a string (see 1.4)
    digit             read a number (see 1.5)
    group-open        read a list (see 1.6)
    quote             read a quoted form (see 1.8)
    dot               read a dotted-pair marker (see 1.7),
                      only valid inside a list
    group-close       signals end of current list
    symbol-char       read a symbol (see 1.5)

A special case applies to the minus sign: if the first character
of a potential symbol is the character designated as "minus" (ref: -)
and it is immediately followed by a digit, the token is read as
a number. Otherwise, it is read as a symbol. This resolves the
ambiguity between -42 (number) and - (symbol).

### 1.4 Strings

A string begins with string-delim. The reader consumes characters
until the next string-delim. The consumed characters (after escape
processing) form the string's content.

The following escape sequences are recognized:

    escape           meaning
    ------           -------
    <string-delim>   literal string-delim character
    <escape-char>    literal escape-char (ref: \)
    n                newline
    t                tab

The escape-char (ref: \) precedes an escape sequence. Any character
following the escape-char that is not in the table above is an error.

A literal newline (or end of input) inside a string before the
closing string-delim is an error.

### 1.5 Numbers and Symbols

A number is a sequence of one or more digits, optionally preceded
by the minus character. Numbers are read as fixnums (Section 2.1).

    number ::= [minus] digit+

A symbol is a maximal sequence of one or more symbol-chars. Symbols
are interned: two symbols with the same name are the same object.

    symbol ::= symbol-char+

Note: the minus character is a symbol-char. The disambiguation
between negative numbers and the minus symbol is handled by
lookahead: minus followed by a digit is a number; minus followed
by anything else (or end of input) is a symbol.

### 1.6 Lists

A list begins with group-open. The reader recursively reads forms
until group-close is encountered. The forms are collected into a
proper list (chain of cons cells, Section 2.3).

    list ::= group-open form* group-close
    form  ::= string | number | symbol | list | quoted-form

The empty list (group-open immediately followed by group-close)
is read as NIL (Section 2.7).

If end of input is reached before group-close, an error is signaled.

### 1.7 Dotted Pairs

Inside a list, a standalone dot token indicates a dotted pair.
The form preceding the dot becomes the car of the final cons cell,
and the form following the dot becomes its cdr. The cdr is not
required to be a list.

    dotted-list ::= group-open form+ dot form group-close

The dot is recognized as a standalone token only when it is
surrounded by whitespace or delimiters. A dot that is part of a
longer sequence of symbol-chars is read as part of a symbol.

A dot appearing outside a list, or not between two forms, is an
error.

### 1.8 Quote

A quote character followed by a form produces a two-element list:
the symbol quote, followed by the form.

    quoted-form ::= quote form

This is equivalent to (quote form) written explicitly.

A quote character not followed by a form (e.g., at end of input)
is an error.

---

## 2. DATA TYPES

[TO BE WRITTEN]

## 3. EVALUATION MODEL

[TO BE WRITTEN]

## 4. SPECIAL FORMS

[TO BE WRITTEN]

## 5. SCOPING

[TO BE WRITTEN]

## 6. BOOLEAN SEMANTICS

[TO BE WRITTEN]

---

# PART 1: KERNEL

## K1. PRIMITIVES

[TO BE WRITTEN]

## K2. SPECIAL FORMS

[TO BE WRITTEN]

---

# PART 3: STANDARD LIBRARY

[TO BE WRITTEN -- grows over time]