#ifndef KATOMIC_H
#define KATOMIC_H

// The Win32 API provides "InterlockedIncrement" and
// "InterlockedDecrement" functions, but there's no common Linux API
// (even in Posix, it seems) for doing an increment or decrement
// that's guaranteed to be "atomic".  What I hear is that many CPU
// architectures just don't have the ability to do it.  Well, ours
// does, and I'm going to present an API for it.

// "Atomic" means that there's no chance that two "threads" would go
// to increment a 1 and it *not* become 3.  Imagine it takes three
// instructions to perform an increment: fetch to register, increment
// register, store to memory.  If one thread fetches, then gets
// suspended and another thread fetches, then the result would be 2,
// not 3.  Oh, and one of those threads might be another CPU core (so
// one might increment it's copy in local cache while another does the
// same thing), or it might be a signal handler, or some such.  So
// being "atomic" is extra effort.

// x86 has a user-level instruction that takes care of it.  However,
// "taking care of it" is so expensive (many clock cycles, CPU
// pipeline stalls, cache synchronization bus transactions, etc) that
// compilers won't issue it.

// Furthermore, many other CPU architectures *don't* have a simple
// instruction that takes care of it.  Sometimes it can be done with
// some combination of instructions they do have.  Some actually can't
// do it.

// Since Linux wants to run on platofrms that can't do it, Linux
// doesn't offer an API.  Neither does Posix 1003.1.

// Wimps.

// Well, it turns out that GCC, as of 4.1, provides atomic increment
// and decrement builtin operators.  However, my current build
// environment is GCC 4.0.  Sigh.

// So - inline asm code.

// Happily, it turns out that the asm sequences for both x86 and
// x86-64 are identical.  These the only platforms we're likely to
// target in the near future.  When we start looking into hosting ARM
// (possible) or Power (less likely) platforms, we'll have to extend
// this.

#if defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ > 1

typedef volatile int katomic_t;	// The data type of our atomic counter
// (note: "volatile" probably doesn't do anything)

// Note: It's a plain scalar, so it can be initialized simply.

// I need to be compatable with ARM targets as well.
// So, use the GCC compiler intrinsics.

static inline katomic_t katomic_add(katomic_t *c, unsigned int incr)
{
	return __sync_fetch_and_add(c, incr);
}

static inline katomic_t katomic_inc(katomic_t *c)
{
	return __sync_fetch_and_add(c, 1);
}

static inline int katomic_dec_test(katomic_t *c)
{
	return __sync_sub_and_fetch(c, 1) == 0;
}

#else
#if defined(WIN32) || defined(WIN64)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
typedef volatile LONG katomic_t;
#else
typedef volatile int katomic_t;	// The data type of our atomic counter
// (note: "volatile" probably doesn't do anything)

// Note: It's a plain scalar, so it can be initialized simply.
#endif
// sadly the old code only generates x86 and x64 assembly language.

static inline katomic_t katomic_add(katomic_t *c, unsigned int incr)
{
   unsigned int result;
#if defined(WIN32) || defined(WIN64)
   result = InterlockedAdd(c, incr);
#else
    __asm__ __volatile__ ("lock; xadd %0, %1"
            :"=r"(result), "=m"(*c)
            :"0"(incr), "m"(*c)
            :"memory");
#endif
    return result;
}

static inline katomic_t katomic_inc(katomic_t *c)
{
	return katomic_add(c, 1);
}

static inline int katomic_dec_test(katomic_t *c)
{
#if defined(WIN32) || defined(WIN64)
	return InterlockedDecrement(c) == 0;
#else
	unsigned char zero;
	__asm__ __volatile__ ( "lock; decl %0; setz %1"
						   : "=m" (*c), "=qm" (zero)
						   : "m" (*c));
	return zero;				// Returns 1 if the decrement produced
								// zero; 0 otherwise.
#endif
}

#endif


#endif // include guard KATOMIC_H
