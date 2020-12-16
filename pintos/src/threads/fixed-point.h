#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <debug.h>

/* Convert a value to fixed-point value. */
#define FP_CONST(A) ((fixed_t)(A << FP_SHIFT_AMOUNT))
#define FIX_BITS 32        /* Total bits per fixed-point number. */
#define FIX_P 16           /* Number of integer bits. */
#define FIX_Q 16           /* Number of fractional bits. */
#define FIX_F (1 << FIX_Q) /* pow(2, FIX_Q). */

#define FIX_MIN_INT (-FIX_MAX_INT)     /* Smallest representable integer. */
#define FIX_MAX_INT ((1 << FIX_P) - 1) /* Largest representable integer. */

/* Basic definitions of fixed point. */
typedef int fixed_t;
/* 16 LSB used for fractional part. */
#define FP_SHIFT_AMOUNT 16
/* Convert a value to fixed-point value. */
#define FP_CONST(A) ((fixed_t)(A << FP_SHIFT_AMOUNT))
/* Add two fixed-point value. */
#define FP_ADD(A, B) (A + B)
/* Add a fixed-point value A and an int value B. */
#define FP_ADD_MIX(A, B) (A + (B << FP_SHIFT_AMOUNT))
/* Substract two fixed-point value. */
#define FP_SUB(A, B) (A - B)
/* Substract an int value B from a fixed-point value A */
#define FP_SUB_MIX(A, B) (A - (B << FP_SHIFT_AMOUNT))
/* Multiply a fixed-point value A by an int value B. */
#define FP_MULT_MIX(A, B) (A * B)
/* Divide a fixed-point value A by an int value B. */
#define FP_DIV_MIX(A, B) (A / B)
/* Multiply two fixed-point value. */
#define FP_MULT(A, B) ((fixed_t)(((int64_t)A) * B >> FP_SHIFT_AMOUNT))
/* Divide two fixed-point value. */
#define FP_DIV(A, B) ((fixed_t)((((int64_t)A) << FP_SHIFT_AMOUNT) / B))
/* Get integer part of a fixed-point value. */
#define FP_INT_PART(A) (A >> FP_SHIFT_AMOUNT)
/* Get rounded integer of a fixed-point value. */
#define FP_ROUND(A)                                                                                \
  (A >= 0 ? ((A + (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT)                                \
          : ((A - (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT))

/* A fixed-point number. */
typedef struct {
  int f;
} fixed_point_t;

/* Returns a fixed-point number with F as its internal value. */
static inline fixed_point_t __mk_fix(int f) {
  fixed_point_t x;
  x.f = f;
  return x;
}

/* Returns fixed-point number corresponding to integer N. */
static inline fixed_point_t fix_int(int n) {
  ASSERT(n >= FIX_MIN_INT && n <= FIX_MAX_INT);
  return __mk_fix(n * FIX_F);
}

/* Returns fixed-point number corresponding to N divided by D. */
static inline fixed_point_t fix_frac(int n, int d) {
  ASSERT(d != 0);
  ASSERT(n / d >= FIX_MIN_INT && n / d <= FIX_MAX_INT);
  return __mk_fix((long long)n * FIX_F / d);
}

/* Returns X rounded to the nearest integer. */
static inline int fix_round(fixed_point_t x) { return (x.f + FIX_F / 2) / FIX_F; }

/* Returns X truncated down to the nearest integer. */
static inline int fix_trunc(fixed_point_t x) { return x.f / FIX_F; }

/* Returns X + Y. */
static inline fixed_point_t fix_add(fixed_point_t x, fixed_point_t y) {
  return __mk_fix(x.f + y.f);
}

/* Returns X - Y. */
static inline fixed_point_t fix_sub(fixed_point_t x, fixed_point_t y) {
  return __mk_fix(x.f - y.f);
}

/* Returns X * Y. */
static inline fixed_point_t fix_mul(fixed_point_t x, fixed_point_t y) {
  return __mk_fix((long long)x.f * y.f / FIX_F);
}

/* Returns X * N. */
static inline fixed_point_t fix_scale(fixed_point_t x, int n) {
  ASSERT(n >= 0);
  return __mk_fix(x.f * n);
}

/* Returns X / Y. */
static inline fixed_point_t fix_div(fixed_point_t x, fixed_point_t y) {
  return __mk_fix((long long)x.f * FIX_F / y.f);
}

/* Returns X / N. */
static inline fixed_point_t fix_unscale(fixed_point_t x, int n) {
  ASSERT(n > 0);
  return __mk_fix(x.f / n);
}

/* Returns 1 / X. */
static inline fixed_point_t fix_inv(fixed_point_t x) { return fix_div(fix_int(1), x); }

/* Returns -1 if X < Y, 0 if X == Y, 1 if X > Y. */
static inline int fix_compare(fixed_point_t x, fixed_point_t y) {
  return x.f < y.f ? -1 : x.f > y.f;
}

#endif /* threads/fixed-point.h */