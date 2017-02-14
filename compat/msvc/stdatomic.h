#if !defined(__cplusplus) && defined(_MSC_VER)

#include <windows.h>

#define  memory_order_relaxed 0

/* Define macros in order to dispatch to the correct function depending on the type.
   Several ranges are need because some operations are not implemented for all types. */
#  define atomic_type_dispatch_32_64(operation, object, ...) \
    (sizeof(*object) == 4 ? operation((LONG *)object, __VA_ARGS__) : \
    sizeof(*object) == 8 ? operation##64((LONGLONG *)object, __VA_ARGS__) : \
    (abort(), 0))

#  define atomic_exchange(object,desired) \
    atomic_type_dispatch_32_64(InterlockedExchange, object, desired)
#  define atomic_exchange_explicit(object,desired,order) \
    atomic_exchange(object,desired)

#  define atomic_load(object) \
    atomic_type_dispatch_32_64(InterlockedCompareExchange, object, 0, 0)
#  define atomic_load_explicit(object,order) \
    atomic_load(object)

#  define atomic_store(object,desired) \
    atomic_type_dispatch_32_64(InterlockedExchange, object, desired)
#  define atomic_store_explicit(object,desired,order) \
    atomic_store(object, desired)

static __forceinline bool msvc_std_exchange_32(LONG *obj, LONG *expected, LONG desired)
{
    LONG was = InterlockedCompareExchange(obj, desired, *expected);
    if (was == desired)
        return true;
    *expected = was;
    return false;
}

static __forceinline bool msvc_std_exchange_64(LONGLONG *obj, LONGLONG *expected, LONGLONG desired)
{
    LONGLONG was = InterlockedCompareExchange64(obj, desired, *expected);
    if (was == desired)
        return true;
    *expected = was;
    return false;
}

#  define atomic_compare_exchange_strong(object,expected,desired) \
    (sizeof(*object) == 4 ? msvc_std_exchange_32(object,expected,desired) : \
     sizeof(*object) == 8 ? msvc_std_exchange_64(object,expected,desired) : \
     (abort(), 0))

#  define atomic_compare_exchange_weak(object,expected,desired) \
    atomic_compare_exchange_strong(object, expected, desired)

#  define atomic_compare_exchange_weak_explicit(object,expected,desired,order1,order2) \
    atomic_compare_exchange_strong(object, expected, desired)

#  define atomic_fetch_add(object,operand) \
    atomic_type_dispatch_32_64(InterlockedExchangeAdd, object, operand)
#  define atomic_fetch_add_explicit(object,operand,order) \
    atomic_fetch_add(object, operand)

#  define atomic_fetch_sub(object,operand) \
    atomic_type_dispatch_32_64(InterlockedExchangeAdd, object, -(LONGLONG)operand)

#  define atomic_fetch_or(object,operand) \
    atomic_type_dispatch_32_64(InterlockedOr, object, operand)
#  define atomic_fetch_or_explicit(object,operand,order) \
    atomic_fetch_or(object, operand)

#  define atomic_fetch_and(object,operand) \
    atomic_type_dispatch_32_64(InterlockedAnd, object, operand)
#  define atomic_fetch_and_explicit(object,operand,order) \
    atomic_fetch_and(object, operand)

/* atomic types */
/* Some atomic operations of the Interlocked API are only
   available for desktop apps. Thus we define the atomic types to
   be at least 32 bits wide. */
typedef      int_least32_t atomic_flag;
typedef      int_least32_t atomic_bool;
typedef      int_least32_t atomic_char;
typedef      int_least32_t atomic_schar;
typedef     uint_least32_t atomic_uchar;
typedef      int_least32_t atomic_short;
typedef     uint_least32_t atomic_ushort;

typedef          int       atomic_int;
typedef unsigned int       atomic_uint;
typedef         uintptr_t  atomic_uintptr_t;
typedef          long long atomic_llong;

typedef     uint_fast32_t atomic_uint_fast32_t;

typedef     uint_least32_t atomic_uint_least32_t;

#  define ATOMIC_VAR_INIT(value) (value)

#  define atomic_init(obj, value) \
    do { *(obj) = (value); } while(0)

#endif /* __cplusplus */
