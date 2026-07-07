;;; ignisp 0.2 -- Standard Library (Layer 2)
;;;
;;; This file is loaded by the kernel at startup. It implements
;;; everything that is NOT in the kernel: type predicates, list
;;; utilities, control-flow macros, the printer, string operations,
;;; and more.
;;;
;;; The kernel provides: cons, car, cdr, eq, +, -, *, /, <, >, =,
;;; make-array, aref, aset, array-length, type-of, symbol-name,
;;; read-char, write-char, error, eval
;;; Plus 8 special forms: if, quote, lambda, let, setq, defmacro,
;;; define, begin
;;;
;;; Everything below is built from those primitives.
;;;
;;; NOTE: Rest parameters use dotted notation: (lambda (a . rest) ...)
;;;       NOT &rest. This is the kernel convention.

;;; ============================================================
;;; TYPE PREDICATES
;;; ============================================================

(define (null x) (eq x nil))

(define (not x) (if x nil t))

(define (fixnump x) (eq (type-of x) (quote fixnum)))

(define (symbolp x) (eq (type-of x) (quote symbol)))

(define (consp x) (eq (type-of x) (quote cons)))

(define (stringp x) (eq (type-of x) (quote string)))

(define (arrayp x) (eq (type-of x) (quote array)))

(define (functionp x)
  (let ((ty (type-of x)))
    (if (eq ty (quote closure)) t
      (if (eq ty (quote primitive)) t nil))))

(define (listp x)
  (if (null x) t
    (if (consp x) t
      nil)))

;;; ============================================================
;;; LIST OPERATIONS
;;; ============================================================

(define (list . xs) xs)

(define (length lst)
  (if (null lst) 0
    (+ 1 (length (cdr lst)))))

(define (append a b)
  (if (null a) b
    (cons (car a) (append (cdr a) b))))

(define (reverse lst)
  (if (null lst) nil
    (append (reverse (cdr lst))
            (cons (car lst) nil))))

(define (nth n lst)
  (if (null lst) nil
    (if (= n 0) (car lst)
      (nth (- n 1) (cdr lst)))))

(define (member x lst)
  (if (null lst) nil
    (if (eq x (car lst)) lst
      (member x (cdr lst)))))

(define (assoc key alist)
  (if (null alist) nil
    (if (eq key (car (car alist)))
        (car alist)
      (assoc key (cdr alist)))))

(define (map f lst)
  (if (null lst) nil
    (cons (f (car lst))
          (map f (cdr lst)))))

(define (filter pred lst)
  (if (null lst) nil
    (if (pred (car lst))
        (cons (car lst) (filter pred (cdr lst)))
      (filter pred (cdr lst)))))

(define (reduce f init lst)
  (if (null lst) init
    (reduce f (f init (car lst)) (cdr lst))))

;;; ============================================================
;;; c*R COMBINATORS
;;; ============================================================

(define (caar x) (car (car x)))
(define (cadr x) (car (cdr x)))
(define (cdar x) (cdr (car x)))
(define (cddr x) (cdr (cdr x)))
(define (caaar x) (car (car (car x))))
(define (caadr x) (car (car (cdr x))))
(define (cadar x) (car (cdr (car x))))
(define (caddr x) (car (cdr (cdr x))))
(define (cdaar x) (cdr (car (car x))))
(define (cdadr x) (cdr (car (cdr x))))
(define (cddar x) (cdr (cdr (car x))))
(define (cdddr x) (cdr (cdr (cdr x))))

;;; ============================================================
;;; CONTROL FLOW MACROS
;;; ============================================================

(defmacro cond (. clauses)
  (if (null clauses) nil
    (let ((clause (car clauses)))
      (list (quote if) (car clause)
            (if (null (cdr clause))
                (car clause)
              (cons (quote begin) (cdr clause)))
            (if (null (cdr clauses))
                nil
              (cons (quote cond) (cdr clauses)))))))

(defmacro and (. args)
  (if (null args) t
    (if (null (cdr args)) (car args)
      (list (quote if) (car args)
            (cons (quote and) (cdr args))
            nil))))

(defmacro or (. args)
  (if (null args) nil
    (if (null (cdr args)) (car args)
      (list (quote if) (car args)
            (car args)
            (cons (quote or) (cdr args))))))

(defmacro when (test . body)
  (list (quote if) test
        (cons (quote begin) body)
        nil))

(defmacro unless (test . body)
  (list (quote if) test
        nil
        (cons (quote begin) body)))

(defmacro let* (bindings . body)
  (if (null bindings)
      (cons (quote begin) body)
    (list (quote let) (list (car bindings))
          (cons (quote let*) (cons (cdr bindings) body)))))

(defmacro labels (bindings . body)
  (if (null bindings)
      (cons (quote begin) body)
    (let ((binding (car bindings)))
      (list (quote begin)
            (list (quote define)
                  (cons (car binding) (cadr binding))
                  (caddr binding))
            (cons (quote labels) (cons (cdr bindings) body))))))

(defmacro while (test . body)
  (list (quote if) test
        (cons (quote begin)
              (append body
                       (list (cons (quote while)
                                   (cons test body)))))
        nil))

;;; ============================================================
;;; PRINTING
;;; ============================================================

(define (digit-to-char n)
  (if (= n 0) 48
    (if (= n 1) 49
      (if (= n 2) 50
        (if (= n 3) 51
          (if (= n 4) 52
            (if (= n 5) 53
              (if (= n 6) 54
                (if (= n 7) 55
                  (if (= n 8) 56
                    (if (= n 9) 57
                      63)))))))))))  ; ? for invalid

(define (write-fixnum n)
  (if (< n 0)
      (begin (write-char 45)  ; '-'
             (write-fixnum (- 0 n)))
    (if (< n 10)
        (write-char (digit-to-char n))
      (begin
        (write-fixnum (/ n 10))
        (write-char (digit-to-char (- n (* 10 (/ n 10)))))))))

(define (write-string-raw s)
  (let ((len (array-length s)))
    (write-string-raw-loop s 0 len)))

(define (write-string-raw-loop s i len)
  (if (= i len) nil
    (begin
      (write-char (aref s i))
      (write-string-raw-loop s (+ i 1) len))))

(define (write-string-escaped s)
  (let ((len (array-length s)))
    (write-string-escaped-loop s 0 len)))

(define (write-string-escaped-loop s i len)
  (if (= i len) nil
    (let ((c (aref s i)))
      (begin
        (if (= c 34)        ; "
            (begin (write-char 92) (write-char 34))
          (if (= c 92)      ; backslash
              (begin (write-char 92) (write-char 92))
            (if (= c 10)    ; newline
                (begin (write-char 92) (write-char 110))
              (if (= c 9)    ; tab
                  (begin (write-char 92) (write-char 116))
                (write-char c)))))
        (write-string-escaped-loop s (+ i 1) len)))))

(define (prin1-list lst)
  (write-char 40)  ; (
  (prin1 (car lst))
  (prin1-list-loop (cdr lst)))

(define (prin1-list-loop lst)
  (if (null lst)
      (write-char 41)  ; )
    (if (consp lst)
        (begin
          (write-char 32)  ; space
          (prin1 (car lst))
          (prin1-list-loop (cdr lst)))
      (begin
        (write-char 32)  ; space
        (write-char 46)  ; .
        (write-char 32)  ; space
        (prin1 lst)
        (write-char 41)))))  ; )

(define (prin1 obj)
  (let ((ty (type-of obj)))
    (if (eq ty (quote fixnum))
        (write-fixnum obj)
      (if (eq ty (quote symbol))
          (write-string-raw (symbol-name obj))
        (if (eq ty (quote string))
            (begin
              (write-char 34)  ; "
              (write-string-escaped obj)
              (write-char 34))  ; "
          (if (eq ty (quote cons))
              (prin1-list obj)
            (if (eq ty (quote array))
                (begin
                  (write-char 35)  ; #
                  (write-char 60)  ; <
                  (write-string-raw (symbol-name (quote array)))
                  (write-char 58)  ; :
                  (write-fixnum (array-length obj))
                  (write-char 62))  ; >
              (if (eq ty (quote closure))
                  (begin
                    (write-char 35)  ; #
                    (write-char 60)  ; <
                    (write-string-raw (symbol-name (quote closure)))
                    (write-char 62))  ; >
                (if (eq ty (quote primitive))
                    (begin
                      (write-char 35)  ; #
                      (write-char 60)  ; <
                      (write-string-raw (symbol-name (quote primitive)))
                      (write-char 62))  ; >
                  (begin
                    (write-char 35)  ; #
                    (write-char 60)  ; <
                    (write-string-raw (symbol-name (quote unknown)))
                    (write-char 62)))))))))))  ; >

(define (print obj)
  (prin1 obj)
  (write-char 10)  ; newline
  obj)

(define (terpri) (write-char 10))

;;; ============================================================
;;; STRING OPERATIONS
;;; ============================================================

(define (string-equal a b)
  (if (not (= (array-length a) (array-length b))) nil
    (string-equal-loop a b 0 (array-length a))))

(define (string-equal-loop a b i len)
  (if (= i len) t
    (if (= (aref a i) (aref b i))
        (string-equal-loop a b (+ i 1) len)
      nil)))

(define (string-append a b)
  (let ((la (array-length a))
        (lb (array-length b)))
    (let ((result (make-array (+ la lb))))
      (string-append-loop a b result 0 la lb 0))))

(define (string-append-loop a b result i la lb j)
  (if (= i la)
      (if (= j lb) result
        (begin
          (aset result (+ i j) (aref b j))
          (string-append-loop a b result i la lb (+ j 1))))
    (begin
      (aset result i (aref a i))
      (string-append-loop a b result (+ i 1) la lb j))))

;;; ============================================================
;;; UTILITY FUNCTIONS
;;; ============================================================

(define (mod a b)
  (- a (* b (/ a b))))

(define (abs n)
  (if (< n 0) (- 0 n) n))

(define (max a b)
  (if (> a b) a b))

(define (min a b)
  (if (< a b) a b))

(define (zerop n) (= n 0))

(define (onep n) (= n 1))

(define (1+ n) (+ n 1))

(define (1- n) (- n 1))

(define (apply f args)
  (eval (cons f args)))

(define (funcall f . args)
  (apply f args))

;;; ============================================================
;;; EQUAL -- structural equality
;;; ============================================================

(define (equal a b)
  (if (eq a b) t
    (if (and (consp a) (consp b))
        (if (equal (car a) (car b))
            (equal (cdr a) (cdr b))
          nil)
      (if (and (stringp a) (stringp b))
          (string-equal a b)
        nil))))

;;; ============================================================
;;; SETF-like macros (minimal)
;;; ============================================================

(defmacro incf-var (var)
  (list (quote setq) var (list (quote +) var 1)))

(defmacro decf-var (var)
  (list (quote setq) var (list (quote -) var 1)))

;;; ============================================================
;;; TEST SUITE
;;; ============================================================

(define (factorial-test n)
  (if (= n 0) 1
    (* n (factorial-test (- n 1)))))

(defmacro test (expr expected)
  (list (quote if)
        (list (quote equal) expr expected)
        (quote t)
        (list (quote begin)
              (list (quote write-string-raw) "FAIL: ")
              (list (quote prin1) (list (quote quote) expr))
              (list (quote terpri))
              (list (quote error) (list (quote quote) (quote test-failure))))))

(define (run-tests)
  (begin
    ;; Type predicates
    (test (null nil) t)
    (test (null (quote (1))) nil)
    (test (not nil) t)
    (test (not 5) nil)
    (test (fixnump 5) t)
    (test (fixnump nil) nil)
    (test (symbolp (quote a)) t)
    (test (symbolp 5) nil)
    (test (consp (quote (1 2))) t)
    (test (consp nil) nil)
    (test (stringp "hello") t)
    (test (stringp 5) nil)
    (test (listp nil) t)
    (test (listp (quote (1 2 3))) t)
    (test (listp 5) nil)

    ;; List operations
    (test (length nil) 0)
    (test (length (quote (1 2 3))) 3)
    (test (append nil nil) nil)
    (test (append (quote (1 2)) (quote (3 4))) (quote (1 2 3 4)))
    (test (reverse (quote (1 2 3))) (quote (3 2 1)))
    (test (nth 0 (quote (a b c))) (quote a))
    (test (nth 1 (quote (a b c))) (quote b))
    (test (nth 5 (quote (a b c))) nil)
    (test (member (quote b) (quote (a b c))) (quote (b c)))
    (test (member (quote d) (quote (a b c))) nil)
    (test (map (lambda (x) (+ x 1)) (quote (1 2 3))) (quote (2 3 4)))
    (test (filter (lambda (x) (< x 3)) (quote (1 2 3 4))) (quote (1 2)))
    (test (reduce + 0 (quote (1 2 3 4))) 10)

    ;; c*r combinators
    (test (caar (quote ((1 2) 3))) 1)
    (test (cadr (quote (1 2 3))) 2)
    (test (caddr (quote (1 2 3))) 3)
    (test (cdar (quote ((1 2) 3))) (quote (2)))
    (test (cddr (quote (1 2 3))) (quote (3)))

    ;; Control flow macros
    (test (cond (nil 1) (t 2)) 2)
    (test (cond (nil 1) (nil 2) (t 3)) 3)
    (test (and t t) t)
    (test (and t nil) nil)
    (test (and 1 2 3) 3)
    (test (or nil nil 3) 3)
    (test (or nil nil) nil)
    (test (when t 1 2 3) 3)
    (test (when nil 1 2 3) nil)
    (test (unless nil 1 2 3) 3)
    (test (unless t 1 2 3) nil)
    (test (let* ((x 1) (y (+ x 1))) (+ x y)) 3)

    ;; Arithmetic
    (test (mod 10 3) 1)
    (test (mod 7 3) 1)
    (test (abs (- 0 5)) 5)
    (test (abs 5) 5)
    (test (max 3 7) 7)
    (test (min 3 7) 3)
    (test (1+ 5) 6)
    (test (1- 5) 4)

    ;; Equal
    (test (equal (quote (1 2 3)) (quote (1 2 3))) t)
    (test (equal (quote (1 2)) (quote (1 2 3))) nil)
    (test (equal "hello" "hello") t)
    (test (equal "hello" "world") nil)
    (test (equal 5 5) t)

    ;; Strings
    (test (string-equal "abc" "abc") t)
    (test (string-equal "abc" "abd") nil)
    (test (array-length "hello") 5)

    ;; Labels
    (test (labels ((fact (n) (if (= n 0) 1 (* n (fact (- n 1))))))
                   (fact 5))
          120)

    ;; Recursion via define
    (test (factorial-test 5) 120)

    ;; Printer (prin1 writes to stdout, returns nil for fixnums)
    (prin1 42)
    (terpri)
    (prin1 (quote (1 2 3)))
    (terpri)
    (prin1 "hello")
    (terpri)

    (write-string-raw "All tests passed!")
    (terpri)))

(write-string-raw "stdlib.lisp loaded.")
(terpri)