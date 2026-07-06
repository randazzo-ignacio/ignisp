# tinylisp -- Kernel Reference Implementation Notes

> Version: 0.1 (draft)
> Date: 2026-07-06
>
> This document defines the concrete reference assignment for the
> abstract kernel specification (KERNEL_SPEC.md). It fixes the
> implementation parameters that the abstract spec leaves open.
>
> The abstract spec defines the STRUCTURE. This document defines
> the ENCODING. When porting to a new host, rewrite this document.
> The abstract spec stays unchanged.

---

## 1. CHARACTER ENCODING

Characters are encoded as ASCII. The read-char primitive returns
ASCII codes (0-127) as fixnums. The write-char primitive accepts
fixnums in [0, 127] and writes the corresponding ASCII character.

Non-ASCII input is implementation-defined (may error, may pass
through as raw byte values > 127).

## 2. SYMBOL NAMING

Symbol names are sequences of ASCII characters. The mapping from
character sequences to symbols (interning) uses string comparison
on the name.

## 3. FIXNUM RANGE

Fixnums are signed 64-bit integers: [-2^63, 2^63 - 1].

This exceeds the minimum required by the spec (2^31 - 1 range).

## 4. ARRAY REPRESENTATION

Arrays are contiguous blocks of memory. Each slot stores a
tagged value (see Section 5). Array size is a fixnum.

## 5. VALUE REPRESENTATION (TAGGED)

Values are represented as tagged 64-bit words:

    Tag bit 0 (low bit):
        0 -> fixnum (value = word >> 1, signed)
        1 -> pointer (to heap object)

    Heap object header (first word of object):
        Bits 0-7:  type tag
            0x01 -> symbol
            0x02 -> array
            0x03 -> closure
        Bits 8-63: object-specific data or pointer to data

This representation makes eq equivalent to = on the raw word
value: two fixnums with the same value have the same bit
pattern, and two references to the same object have the same
pointer.

## 6. MEMORY MANAGEMENT

Phase 1 (initial): bump allocator. malloc is called for each
new array or object. No free. Memory leaks are accepted.

Phase 2 (planned): mark-sweep garbage collector operating on
the heap. Root set: global environment array, register file
array, call stack array. ~100-150 lines of C.

## 7. PREDEFINED SYMBOLS

The following symbols are interned at startup:

    NIL, T, if, quote, lambda, let, setq, defmacro, function,
    make-array, aref, aset, +, -, *, /, <, >, =, read-char,
    write-char, error

## 8. INPUT/OUTPUT STREAMS

read-char reads from standard input (fd 0).
write-char writes to standard output (fd 1).

No buffered I/O at the kernel level. The standard library may
implement buffering on top.

## 9. ERROR BEHAVIOR

On error, the kernel prints the error value to standard error
(fd 2) and enters the REPL (if running interactively) or exits
with non-zero status (if running a script).