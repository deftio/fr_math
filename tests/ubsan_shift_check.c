/* ubsan_shift_check.c - shift-overflow checker for the test suites.
 *
 * FR_Math deliberately left-shifts negative values (e.g. I2FR(-3, 16)):
 * every supported compiler treats that as multiplication by 2^n, and
 * C++20 defines it that way. The stock UBSan shift-base check reports
 * those shifts, and it reports only the first problem at each source
 * location, so a negative shift there would hide a real overflow on the
 * same line later. This file replaces the UBSan runtime handler for
 * shifts: a shift is accepted when x * 2^n is representable in the
 * result type (negative x included) and the exponent is in range, and
 * anything else aborts with the location and operands, every time.
 *
 * Build (see `make test-shift`):
 *   -fsanitize=shift -fsanitize-recover=shift -fno-sanitize-link-runtime
 * and link this object. Only the shift handlers are provided, so no other
 * sanitizer may be enabled in the same build.
 *
 * The data layout follows compiler-rt's ubsan_handlers.h /
 * ubsan_value.h (stable since clang 3.x; GCC uses the same ABI).
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct { const char *file; uint32_t line, column; } ub_loc;
typedef struct { uint16_t kind, info; char name[1]; } ub_type;
typedef struct { ub_loc loc; const ub_type *lhs_type, *rhs_type; } ub_shift_data;

/* Integer type: kind 0, info = log2(bit width) << 1 | signed. */
static unsigned width_of(const ub_type *t) { return 1u << (t->info >> 1); }
static int is_signed(const ub_type *t)     { return t->info & 1; }

/* A value no wider than a pointer is passed inline, otherwise by address. */
static int64_t value_of(const ub_type *t, uintptr_t h)
{
    unsigned w = width_of(t);
    uint64_t u = (w <= sizeof(uintptr_t) * 8) ? (uint64_t)h : *(const uint64_t *)h;
    if (w < 64) {
        u &= (1ull << w) - 1u;
        if (is_signed(t) && (u >> (w - 1)))   /* sign-extend */
            u |= ~0ull << w;
    }
    return (int64_t)u;
}

static void check(const ub_shift_data *d, uintptr_t lhs, uintptr_t rhs)
{
    unsigned w = width_of(d->lhs_type);
    int64_t x = value_of(d->lhs_type, lhs);
    int64_t n = value_of(d->rhs_type, rhs);
    const char *what = NULL;

    if (d->rhs_type->kind == 0 && is_signed(d->rhs_type) && n < 0)
        what = "negative shift exponent";
    else if ((uint64_t)n >= w)
        what = "shift exponent too large for the type";
    else if (is_signed(d->lhs_type)) {
        /* Only left shifts reach here with a valid exponent: x * 2^n
         * must fit in [-2^(w-1), 2^(w-1) - 1]. */
        int64_t lo = (w >= 64) ? INT64_MIN : -((int64_t)1 << (w - 1));
        int64_t hi = (w >= 64) ? INT64_MAX : ((int64_t)1 << (w - 1)) - 1;
        if (x > (hi >> n) || x < (lo >> n))
            what = "left shift overflows the type";
    }
    if (!what)
        return;                                 /* in range: accepted */
    fprintf(stderr, "%s:%u:%u: shift check: %s: %lld << %lld (%s, %u bits)\n",
            d->loc.file ? d->loc.file : "?", d->loc.line, d->loc.column,
            what, (long long)x, (long long)n, d->lhs_type->name, w);
    abort();
}

void __ubsan_handle_shift_out_of_bounds(void *d, uintptr_t lhs, uintptr_t rhs);
void __ubsan_handle_shift_out_of_bounds_abort(void *d, uintptr_t lhs, uintptr_t rhs);

void __ubsan_handle_shift_out_of_bounds(void *d, uintptr_t lhs, uintptr_t rhs)
{
    check((const ub_shift_data *)d, lhs, rhs);
}

void __ubsan_handle_shift_out_of_bounds_abort(void *d, uintptr_t lhs, uintptr_t rhs)
{
    check((const ub_shift_data *)d, lhs, rhs);
}
