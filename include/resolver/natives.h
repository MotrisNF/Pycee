#ifndef NATIVES_H
# define NATIVES_H

/*
 * The global native functions, in slot order: the resolver reserves
 * their names (no shadowing) and checks their arity, and the runtime
 * implements each one as native_<name> (src/natives/). Being one list,
 * the slots of both can never disagree. Adding a native = one line here
 * plus its function. X(name, min arguments, max arguments; -1 = any,
 * repl: 1 if it only exists in the REPL, where a file does not see it).
 * The rest of the standard library goes in modules (import math...).
 * Each error type that a program can raise has its constructor here,
 * named as the type: ValueError("bad input").
 */
# define NATIVE_LIST(X) \
    X(print,    0, -1,  0) \
    X(input,    0, 1,   0) \
    X(str,      1, 1,   0) \
    X(num,      1, 1,   0) \
    X(int,      1, 1,   0) \
    X(float,    1, 1,   0) \
    X(range,    1, 3,   0) \
    X(tuple,    0, -1,  0) \
    X(set,      0, -1,  0) \
    X(dict,     0, -1,  0) \
    X(any,      1, 1,   0) \
    X(all,      1, 1,   0) \
    X(sum,      1, 1,   0) \
    X(min,      1, 1,   0) \
    X(max,      1, 1,   0) \
    X(exit,     0, 1,   1) \
    X(Error,                0, 1, 0) \
    X(TypeError,            0, 1, 0) \
    X(ValueError,           0, 1, 0) \
    X(ZeroDivisionError,    0, 1, 0) \
    X(OverflowError,        0, 1, 0) \
    X(IndexError,           0, 1, 0) \
    X(KeyError,             0, 1, 0) \
    X(AttributeError,       0, 1, 0) \
    X(AssignmentError,      0, 1, 0) \
    X(RuntimeError,         0, 1, 0) \
    X(RecursionError,       0, 1, 0) \
    X(NotImplementedError,  0, 1, 0) \
    X(EOFError,             0, 1, 0) \
    X(AssertionError,       0, 1, 0)

#endif
