/* -*- mode: c; indent-tabs-mode: nil -*- */
/* include/k5-platform.h */
/*
 * Copyright 2003, 2004, 2005, 2007, 2008, 2009 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * Some platform-dependent definitions to sync up the C support level.
 * Some to a C99-ish level, some related utility code.
 *
 * Currently:
 * + [u]int{8,16,32}_t types
 * + 64-bit types and load/store code
 * + SIZE_MAX
 * + shared library init/fini hooks
 * + consistent getpwnam/getpwuid interfaces
 * + va_copy fudged if not provided
 * + strlcpy/strlcat
 * + fnmatch
 * + [v]asprintf
 * + mkstemp
 * + zap (support function; macro is in k5-int.h)
 * + constant time memory comparison
 * + path manipulation
 * + _, N_, dgettext, bindtextdomain (for localization)
 */

#ifndef K5_PLATFORM_H
#define K5_PLATFORM_H

#include "autoconf.h"
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

#ifdef _WIN32
#define CAN_COPY_VA_LIST
#endif

#if defined(macintosh) || (defined(__MACH__) && defined(__APPLE__))
#include <TargetConditionals.h>
#endif

/* Initialization and finalization function support for libraries.

   At top level, before the functions are defined or even declared:
   MAKE_INIT_FUNCTION(init_fn);
   MAKE_FINI_FUNCTION(fini_fn);
   Then:
   int init_fn(void) { ... }
   void fini_fn(void) { if (INITIALIZER_RAN(init_fn)) ... }
   In code, in the same file:
   err = CALL_INIT_FUNCTION(init_fn);

   To trigger or verify the initializer invocation from another file,
   a helper function must be created.

   This model handles both the load-time execution (Windows) and
   delayed execution (pthread_once) approaches, and should be able to
   guarantee in both cases that the init function is run once, in one
   thread, before other stuff in the library is done; furthermore, the
   finalization code should only run if the initialization code did.
   (Maybe I could've made the "if INITIALIZER_RAN" test implicit, via
   another function hidden in macros, but this is hairy enough
   already.)

   The init_fn and fini_fn names should be chosen such that any
   exported names staring with those names, and optionally followed by
   additional characters, fits in with any namespace constraints on
   the library in question.


   There's also PROGRAM_EXITING() currently always defined as zero.
   If there's some trivial way to find out if the fini function is
   being called because the program that the library is linked into is
   exiting, we can just skip all the work because the resources are
   about to be freed up anyways.  Generally this is likely to be the
   same as distinguishing whether the library was loaded dynamically
   while the program was running, or loaded as part of program
   startup.  On most platforms, I don't think we can distinguish these
   cases easily, and it's probably not worth expending any significant
   effort.  (Note in particular that atexit() won't do, because if the
   library is explicitly loaded and unloaded, it would have to be able
   to deregister the atexit callback function.  Also, the system limit
   on atexit callbacks may be small.)


   Implementation outline:

   Windows: MAKE_FINI_FUNCTION creates a symbol with a magic name that
   is sought at library build time, and code is added to invoke the
   function when the library is unloaded.  MAKE_INIT_FUNCTION does
   likewise, but the function is invoked when the library is loaded,
   and an extra variable is declared to hold an error code and a "yes
   the initializer ran" flag.  CALL_INIT_FUNCTION blows up if the flag
   isn't set, otherwise returns the error code.

   UNIX: MAKE_INIT_FUNCTION creates and initializes a variable with a
   name derived from the function name, containing a k5_once_t
   (pthread_once_t or int), an error code, and a pointer to the
   function.  The function itself is declared static, but the
   associated variable has external linkage.  CALL_INIT_FUNCTION
   ensures thath the function is called exactly once (pthread_once or
   just check the flag) and returns the stored error code (or the
   pthread_once error).

   (That's the basic idea.  With some debugging assert() calls and
   such, it's a bit more complicated.  And we also need to handle
   doing the pthread test at run time on systems where that works, so
   we use the k5_once_t stuff instead.)

   UNIX, with compiler support: MAKE_FINI_FUNCTION declares the
   function as a destructor, and the run time linker support or
   whatever will cause it to be invoked when the library is unloaded,
   the program ends, etc.

   UNIX, with linker support: MAKE_FINI_FUNCTION creates a symbol with
   a magic name that is sought at library build time, and linker
   options are used to mark it as a finalization function for the
   library.  The symbol must be exported.

   UNIX, no library finalization support: The finalization function
   never runs, and we leak memory.  Tough.

   DELAY_INITIALIZER will be defined by the configure script if we
   want to use k5_once instead of load-time initialization.  That'll
   be the preferred method on most systems except Windows, where we
   have to initialize some mutexes.




   For maximum flexibility in defining the macros, the function name
   parameter should be a simple name, not even a macro defined as
   another name.  The function should have a unique name, and should
   conform to whatever namespace is used by the library in question.
   (We do have export lists, but (1) they're not used for all
   platforms, and (2) they're not used for static libraries.)

   If the macro expansion needs the function to have been declared, it
   must include a declaration.  If it is not necessary for the symbol
   name to be exported from the object file, the macro should declare
   it as "static".  Hence the signature must exactly match "void
   foo(void)".  (ANSI C allows a static declaration followed by a
   non-static one; the result is internal linkage.)  The macro
   expansion has to come before the function, because gcc apparently
   won't act on "__attribute__((constructor))" if it comes after the
   function definition.

   This is going to be compiler- and environment-specific, and may
   require some support at library build time, and/or "asm"
   statements.  But through macro expansion and auxiliary functions,
   we should be able to handle most things except #pragma.

   It's okay for this code to require that the library be built
   with the same compiler and compiler options throughout, but
   we shouldn't require that the library and application use the
   same compiler.

   For static libraries, we don't really care about cleanup too much,
   since it's all memory handling and mutex allocation which will all
   be cleaned up when the program exits.  Thus, it's okay if gcc-built
   static libraries don't play nicely with cc-built executables when
   it comes to static constructors, just as long as it doesn't cause
   linking to fail.

   For dynamic libraries on UNIX, we'll use pthread_once-type support
   to do delayed initialization, so if finalization can't be made to
   work, we'll only have memory leaks in a load/use/unload cycle.  If
   anyone (like, say, the OS vendor) complains about this, they can
   tell us how to get a shared library finalization function invoked
   automatically.

   Currently there's --disable-delayed-initialization for preventing
   the initialization from being delayed on UNIX, but that's mainly
   just for testing the linker options for initialization, and will
   probably be removed at some point.  */

/* Helper macros.  */

# define JOIN__2_2(A,B) A ## _ ## _ ## B
# define JOIN__2(A,B) JOIN__2_2(A,B)

/* XXX Should test USE_LINKER_INIT_OPTION early, and if it's set,
   always provide a function by the expected name, even if we're
   delaying initialization.  */

#if defined(DELAY_INITIALIZER)

/* Run the initialization code during program execution, at the latest
   possible moment.  This means multiple threads may be active.  */
# include "k5-thread.h"
typedef struct { k5_once_t once; int error, did_run; void (*fn)(void); } k5_init_t;
# ifdef USE_LINKER_INIT_OPTION
#  define MAYBE_DUMMY_INIT(NAME)                \
        void JOIN__2(NAME, auxinit) () { }
# else
#  define MAYBE_DUMMY_INIT(NAME)
# endif
# ifdef __GNUC__
/* Do it in macro form so we get the file/line of the invocation if
   the assertion fails.  */
#  define k5_call_init_function(I)                                      \
        (__extension__ ({                                               \
                k5_init_t *k5int_i = (I);                               \
                int k5int_err = k5_once(&k5int_i->once, k5int_i->fn);   \
                (k5int_err                                              \
                 ? k5int_err                                            \
                 : (assert(k5int_i->did_run != 0), k5int_i->error));    \
            }))
#  define MAYBE_DEFINE_CALLINIT_FUNCTION
# else
#  define MAYBE_DEFINE_CALLINIT_FUNCTION                        \
        static inline int k5_call_init_function(k5_init_t *i)   \
        {                                                       \
            int err;                                            \
            err = k5_once(&i->once, i->fn);                     \
            if (err)                                            \
                return err;                                     \
            assert (i->did_run != 0);                           \
            return i->error;                                    \
        }
# endif
# define MAKE_INIT_FUNCTION(NAME)                               \
        static int NAME(void);                                  \
        MAYBE_DUMMY_INIT(NAME)                                  \
        /* forward declaration for use in initializer */        \
        static void JOIN__2(NAME, aux) (void);                  \
        static k5_init_t JOIN__2(NAME, once) =                  \
                { K5_ONCE_INIT, 0, 0, JOIN__2(NAME, aux) };     \
        MAYBE_DEFINE_CALLINIT_FUNCTION                          \
        static void JOIN__2(NAME, aux) (void)                   \
        {                                                       \
            JOIN__2(NAME, once).did_run = 1;                    \
            JOIN__2(NAME, once).error = NAME();                 \
        }                                                       \
        /* so ';' following macro use won't get error */        \
        static int NAME(void)
# define CALL_INIT_FUNCTION(NAME)       \
        k5_call_init_function(& JOIN__2(NAME, once))
/* This should be called in finalization only, so we shouldn't have
   multiple active threads mucking around in our library at this
   point.  So ignore the once_t object and just look at the flag.

   XXX Could we have problems with memory coherence between processors
   if we don't invoke mutex/once routines?  Probably not, the
   application code should already be coordinating things such that
   the library code is not in use by this point, and memory
   synchronization will be needed there.  */
# define INITIALIZER_RAN(NAME)  \
        (JOIN__2(NAME, once).did_run && JOIN__2(NAME, once).error == 0)

# define PROGRAM_EXITING()              (0)

#elif defined(__GNUC__) && !defined(_WIN32) && defined(CONSTRUCTOR_ATTR_WORKS)

/* Run initializer at load time, via GCC/C++ hook magic.  */

# ifdef USE_LINKER_INIT_OPTION
     /* Both gcc and linker option??  Favor gcc.  */
#  define MAYBE_DUMMY_INIT(NAME)                \
        void JOIN__2(NAME, auxinit) () { }
# else
#  define MAYBE_DUMMY_INIT(NAME)
# endif

typedef struct { int error; unsigned char did_run; } k5_init_t;
# define MAKE_INIT_FUNCTION(NAME)               \
        MAYBE_DUMMY_INIT(NAME)                  \
        static k5_init_t JOIN__2(NAME, ran)     \
                = { 0, 2 };                     \
        static void JOIN__2(NAME, aux)(void)    \
            __attribute__((constructor));       \
        static int NAME(void);                  \
        static void JOIN__2(NAME, aux)(void)    \
        {                                       \
            JOIN__2(NAME, ran).error = NAME();  \
            JOIN__2(NAME, ran).did_run = 3;     \
        }                                       \
        static int NAME(void)
# define CALL_INIT_FUNCTION(NAME)               \
        (JOIN__2(NAME, ran).did_run == 3        \
         ? JOIN__2(NAME, ran).error             \
         : (abort(),0))
# define INITIALIZER_RAN(NAME)  (JOIN__2(NAME,ran).did_run == 3 && JOIN__2(NAME, ran).error == 0)

# define PROGRAM_EXITING()              (0)

#elif defined(USE_LINKER_INIT_OPTION) || defined(_WIN32)

/* Run initializer at load time, via linker magic, or in the
   case of WIN32, win_glue.c hard-coded knowledge.  */
typedef struct { int error; unsigned char did_run; } k5_init_t;
# define MAKE_INIT_FUNCTION(NAME)               \
        static k5_init_t JOIN__2(NAME, ran)     \
                = { 0, 2 };                     \
        static int NAME(void);                  \
        void JOIN__2(NAME, auxinit)()           \
        {                                       \
            JOIN__2(NAME, ran).error = NAME();  \
            JOIN__2(NAME, ran).did_run = 3;     \
        }                                       \
        static int NAME(void)
# define CALL_INIT_FUNCTION(NAME)               \
        (JOIN__2(NAME, ran).did_run == 3        \
         ? JOIN__2(NAME, ran).error             \
         : (abort(),0))
# define INITIALIZER_RAN(NAME)  \
        (JOIN__2(NAME, ran).error == 0)

# define PROGRAM_EXITING()              (0)

#else

# error "Don't know how to do load-time initializers for this configuration."

# define PROGRAM_EXITING()              (0)

#endif



#if !defined(SHARED) && !defined(_WIN32)

/*
 * In this case, we just don't care about finalization.
 *
 * The code will still define the function, but we won't do anything
 * with it.  Annoying: This may generate unused-function warnings.
 */

# define MAKE_FINI_FUNCTION(NAME)               \
        static void NAME(void)

#elif defined(USE_LINKER_FINI_OPTION) || defined(_WIN32)
/* If we're told the linker option will be used, it doesn't really
   matter what compiler we're using.  Do it the same way
   regardless.  */

# ifdef __hpux

     /* On HP-UX, we need this auxiliary function.  At dynamic load or
        unload time (but *not* program startup and termination for
        link-time specified libraries), the linker-indicated function
        is called with a handle on the library and a flag indicating
        whether it's being loaded or unloaded.

        The "real" fini function doesn't need to be exported, so
        declare it static.

        As usual, the final declaration is just for syntactic
        convenience, so the top-level invocation of this macro can be
        followed by a semicolon.  */

#  include <dl.h>
#  define MAKE_FINI_FUNCTION(NAME)                                          \
        static void NAME(void);                                             \
        void JOIN__2(NAME, auxfini)(shl_t, int); /* silence gcc warnings */ \
        void JOIN__2(NAME, auxfini)(shl_t h, int l) { if (!l) NAME(); }     \
        static void NAME(void)

# else /* not hpux */

#  define MAKE_FINI_FUNCTION(NAME)      \
        void NAME(void)

# endif

#elif defined(__GNUC__) && defined(DESTRUCTOR_ATTR_WORKS)
/* If we're using gcc, if the C++ support works, the compiler should
   build executables and shared libraries that support the use of
   static constructors and destructors.  The C compiler supports a
   function attribute that makes use of the same facility as C++.

   XXX How do we know if the C++ support actually works?  */
# define MAKE_FINI_FUNCTION(NAME)       \
        static void NAME(void) __attribute__((destructor))

#else

# error "Don't know how to do unload-time finalization for this configuration."

#endif


/* 64-bit support: krb5_ui_8 and krb5_int64.

   This should move to krb5.h eventually, but without the namespace
   pollution from the autoconf macros.  */
#if defined(HAVE_STDINT_H) || defined(HAVE_INTTYPES_H)
# ifdef HAVE_STDINT_H
#  include <stdint.h>
# endif
# ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
# endif
# define INT64_TYPE int64_t
# define UINT64_TYPE uint64_t
# define INT64_FMT PRId64
# define UINT64_FMT PRIu64
#elif defined(_WIN32)
# define INT64_TYPE signed __int64
# define UINT64_TYPE unsigned __int64
# define INT64_FMT "I64d"
# define UINT64_FMT "I64u"
#else /* not Windows, and neither stdint.h nor inttypes.h */
# define INT64_TYPE signed long long
# define UINT64_TYPE unsigned long long
# define INT64_FMT "lld"
# define UINT64_FMT "llu"
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)((size_t)0 - 1))
#endif

#ifndef UINT64_MAX
# define UINT64_MAX ((UINT64_TYPE)((UINT64_TYPE)0 - 1))
#endif

#ifdef _WIN32
# define SSIZE_MAX ((ssize_t)(SIZE_MAX/2))
#endif

/* Read and write integer values as (unaligned) octet strings in
   specific byte orders.  Add per-platform optimizations as
   needed.  */

#if HAVE_ENDIAN_H
# include <endian.h>
#elif HAVE_MACHINE_ENDIAN_H
# include <machine/endian.h>
#endif
/* Check for BIG/LITTLE_ENDIAN macros.  If exactly one is defined, use
   it.  If both are defined, then BYTE_ORDER should be defined and
   match one of them.  Try those symbols, then try again with an
   underscore prefix.  */
#if defined(BIG_ENDIAN) && defined(LITTLE_ENDIAN)
# if BYTE_ORDER == BIG_ENDIAN
#  define K5_BE
# endif
# if BYTE_ORDER == LITTLE_ENDIAN
#  define K5_LE
# endif
#elif defined(BIG_ENDIAN)
# define K5_BE
#elif defined(LITTLE_ENDIAN)
# define K5_LE
#elif defined(_BIG_ENDIAN) && defined(_LITTLE_ENDIAN)
# if _BYTE_ORDER == _BIG_ENDIAN
#  define K5_BE
# endif
# if _BYTE_ORDER == _LITTLE_ENDIAN
#  define K5_LE
# endif
#elif defined(_BIG_ENDIAN)
# define K5_BE
#elif defined(_LITTLE_ENDIAN)
# define K5_LE
#elif defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
# define K5_BE
#elif defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
# define K5_LE
#endif
#if !defined(K5_BE) && !defined(K5_LE)
/* Look for some architectures we know about.

   MIPS can use either byte order, but the preprocessor tells us which
   mode we're compiling for.  The GCC config files indicate that
   variants of Alpha and IA64 might be out there with both byte
   orders, but until we encounter the "wrong" ones in the real world,
   just go with the default (unless there are cpp predefines to help
   us there too).

   As far as I know, only PDP11 and ARM (which we don't handle here)
   have strange byte orders where an 8-byte value isn't laid out as
   either 12345678 or 87654321.  */
# if defined(__i386__) || defined(_MIPSEL) || defined(__alpha__) || (defined(__ia64__) && !defined(__hpux))
#  define K5_LE
# endif
# if defined(__hppa__) || defined(__rs6000__) || defined(__sparc__) || defined(_MIPSEB) || defined(__m68k__) || defined(__sparc64__) || defined(__ppc__) || defined(__ppc64__) || (defined(__hpux) && defined(__ia64__))
#  define K5_BE
# endif
#endif
#if defined(K5_BE) && defined(K5_LE)
# error "oops, check the byte order macros"
#endif

/* Optimize for GCC on platforms with known byte orders.

   GCC's packed structures can be written to with any alignment; the
   compiler will use byte operations, unaligned-word operations, or
   normal memory ops as appropriate for the architecture.

   This assumes the availability of uint##_t types, which should work
   on most of our platforms except Windows, where we're not using
   GCC.  */
#ifdef __GNUC__
# define PUT(SIZE,PTR,VAL)      (((struct { uint##SIZE##_t i; } __attribute__((packed)) *)(PTR))->i = (VAL))
# define GET(SIZE,PTR)          (((const struct { uint##SIZE##_t i; } __attribute__((packed)) *)(PTR))->i)
# define PUTSWAPPED(SIZE,PTR,VAL)       PUT(SIZE,PTR,SWAP##SIZE(VAL))
# define GETSWAPPED(SIZE,PTR)           SWAP##SIZE(GET(SIZE,PTR))
#endif
/* To do: Define SWAP16, SWAP32, SWAP64 macros to byte-swap values
   with the indicated numbers of bits.

   Linux: byteswap.h, bswap_16 etc.
   Solaris 10: none
   Mac OS X: machine/endian.h or byte_order.h, NXSwap{Short,Int,LongLong}
   NetBSD: sys/bswap.h, bswap16 etc.  */

#if defined(HAVE_BYTESWAP_H) && defined(HAVE_BSWAP_16)
# include <byteswap.h>
# define SWAP16                 bswap_16
# define SWAP32                 bswap_32
# ifdef HAVE_BSWAP_64
#  define SWAP64                bswap_64
# endif
#elif TARGET_OS_MAC
# include <architecture/byte_order.h>
# if 0 /* This causes compiler warnings.  */
#  define SWAP16                OSSwapInt16
# else
#  define SWAP16                k5_swap16
static inline unsigned int k5_swap16 (unsigned int x) {
    x &= 0xffff;
    return (x >> 8) | ((x & 0xff) << 8);
}
# endif
# define SWAP32                 OSSwapInt32
# define SWAP64                 OSSwapInt64
#elif defined(HAVE_SYS_BSWAP_H)
/* XXX NetBSD/x86 5.0.1 defines bswap16 and bswap32 as inline
   functions only, so autoconf doesn't pick up on their existence.
   So, no feature macro test for them here.  The 64-bit version isn't
   inline at all, though, for whatever reason.  */
# include <sys/bswap.h>
# define SWAP16                 bswap16
# define SWAP32                 bswap32
/* However, bswap64 causes lots of warnings about 'long long'
   constants; probably only on 32-bit platforms.  */
# if LONG_MAX > 0x7fffffffL
#  define SWAP64                bswap64
# endif
#endif

/* Note that on Windows at least this file can be included from C++
   source, so casts *from* void* are required.  */
static inline void
store_16_be (unsigned int val, void *vp)
{
    unsigned char *p = (unsigned char *) vp;
#if defined(__GNUC__) && defined(K5_BE) && !defined(__cplusplus)
    PUT(16,p,val);
#elif defined(__GNUC__) && defined(K5_LE) && defined(SWAP16) && !defined(__cplusplus)
    PUTSWAPPED(16,p,val);
#else
    p[0] = (val >>  8) & 0xff;
    p[1] = (val      ) & 0xff;
#endif
}
static inline void
store_32_be (unsigned int val, void *vp)
{
    unsigned char *p = (unsigned char *) vp;
#if defined(__GNUC__) && defined(K5_BE) && !defined(__cplusplus)
    PUT(32,p,val);
#elif defined(__GNUC__) && defined(K5_LE) && defined(SWAP32) && !defined(__cplusplus)
    PUTSWAPPED(32,p,val);
#else
    p[0] = (val >> 24) & 0xff;
    p[1] = (val >> 16) & 0xff;
    p[2] = (val >>  8) & 0xff;
    p[3] = (val      ) & 0xff;
#endif
}
static inline void
store_64_be (UINT64_TYPE val, void *vp)
{
    unsigned char *p = (unsigned char *) vp;
#if defined(__GNUC__) && defined(K5_BE) && !defined(__cplusplus)
    PUT(64,p,val);
#elif defined(__GNUC__) && defined(K5_LE) && defined(SWAP64) && !defined(__cplusplus)
    PUTSWAPPED(64,p,val);
#else
    p[0] = (unsigned char)((val >> 56) & 0xff);
    p[1] = (unsigned char)((val >> 48) & 0xff);
    p[2] = (unsigned char)((val >> 40) & 0xff);
    p[3] = (unsigned char)((val >> 32) & 0xff);
    p[4] = (unsigned char)((val >> 24) & 0xff);
    p[5] = (unsigned char)((val >> 16) & 0xff);
    p[6] = (unsigned char)((val >>  8) & 0xff);
    p[7] = (unsigned char)((val      ) & 0xff);
#endif
}
static inline unsigned short
load_16_be (const void *cvp)
{
    const unsigned char *p = (const unsigned char *) cvp;
#if defined(__GNUC__) && defined(K5_BE) && !defined(__cplusplus)
    return GET(16,p);
#elif defined(__GNUC__) && defined(K5_LE) && defined(SWAP16) && !defined(__cplusplus)
    return GETSWAPPED(16,p);
#else
    return (p[1] | (p[0] << 8));
#endif
}
static inline unsigned int
load_32_be (const void *cvp)
{
    const unsigned char *p = (const unsigned char *) cvp;
#if defined(__GNUC__) && defined(K5_BE) && !defined(__cplusplus)
    return GET(32,p);
#elif defined(__GNUC__) && defined(K5_LE) && defined(SWAP32) && !defined(__cplusplus)
    return GETSWAPPED(32,p);
#else
    return (p[3] | (p[2] << 8)
            | ((uint32_t) p[1] << 16)
            | ((uint32_t) p[0] << 24));
#endif
}
static inline UINT64_TYPE
load_64_be (const void *cvp)
{
    const unsigned char *p = (const unsigned char *) cvp;
#if defined(__GNUC__) && defined(K5_BE) && !defined(__cplusplus)
    return GET(64,p);
#elif defined(__GNUC__) && defined(K5_LE) && defined(SWAP64) && !defined(__cplusplus)
    return GETSWAPPED(64,p);
#else
    return ((UINT64_TYPE)load_32_be(p) << 32) | load_32_be(p+4);
#endif
}
static inline void
store_16_le (unsigned int val, void *vp)
{
    unsigned char *p = (unsigned char *) vp;
#if defined(__GNUC__) && defined(K5_LE) && !defined(__cplusplus)
    PUT(16,p,val);
#elif defined(__GNUC__) && defined(K5_BE) && defined(SWAP16) && !defined(__cplusplus)
    PUTSWAPPED(16,p,val);
#else
    p[1] = (val >>  8) & 0xff;
    p[0] = (val      ) & 0xff;
#endif
}
static inline void
store_32_le (unsigned int val, void *vp)
{
    unsigned char *p = (unsigned char *) vp;
#if defined(__GNUC__) && defined(K5_LE) && !defined(__cplusplus)
    PUT(32,p,val);
#elif defined(__GNUC__) && defined(K5_BE) && defined(SWAP32) && !defined(__cplusplus)
    PUTSWAPPED(32,p,val);
#else
    p[3] = (val >> 24) & 0xff;
    p[2] = (val >> 16) & 0xff;
    p[1] = (val >>  8) & 0xff;
    p[0] = (val      ) & 0xff;
#endif
}
static inline void
store_64_le (UINT64_TYPE val, void *vp)
{
    unsigned char *p = (unsigned char *) vp;
#if defined(__GNUC__) && defined(K5_LE) && !defined(__cplusplus)
    PUT(64,p,val);
#elif defined(__GNUC__) && defined(K5_BE) && defined(SWAP64) && !defined(__cplusplus)
    PUTSWAPPED(64,p,val);
#else
    p[7] = (unsigned char)((val >> 56) & 0xff);
    p[6] = (unsigned char)((val >> 48) & 0xff);
    p[5] = (unsigned char)((val >> 40) & 0xff);
    p[4] = (unsigned char)((val >> 32) & 0xff);
    p[3] = (unsigned char)((val >> 24) & 0xff);
    p[2] = (unsigned char)((val >> 16) & 0xff);
    p[1] = (unsigned char)((val >>  8) & 0xff);
    p[0] = (unsigned char)((val      ) & 0xff);
#endif
}
static inline unsigned short
load_16_le (const void *cvp)
{
    const unsigned char *p = (const unsigned char *) cvp;
#if defined(__GNUC__) && defined(K5_LE) && !defined(__cplusplus)
    return GET(16,p);
#elif defined(__GNUC__) && defined(K5_BE) && defined(SWAP16) && !defined(__cplusplus)
    return GETSWAPPED(16,p);
#else
    return (p[0] | (p[1] << 8));
#endif
}
static inline unsigned int
load_32_le (const void *cvp)
{
    const unsigned char *p = (const unsigned char *) cvp;
#if defined(__GNUC__) && defined(K5_LE) && !defined(__cplusplus)
    return GET(32,p);
#elif defined(__GNUC__) && defined(K5_BE) && defined(SWAP32) && !defined(__cplusplus)
    return GETSWAPPED(32,p);
#else
    return (p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
#endif
}
static inline UINT64_TYPE
load_64_le (const void *cvp)
{
    const unsigned char *p = (const unsigned char *) cvp;
#if defined(__GNUC__) && defined(K5_LE) && !defined(__cplusplus)
    return GET(64,p);
#elif defined(__GNUC__) && defined(K5_BE) && defined(SWAP64) && !defined(__cplusplus)
    return GETSWAPPED(64,p);
#else
    return ((UINT64_TYPE)load_32_le(p+4) << 32) | load_32_le(p);
#endif
}

#ifdef _WIN32
#define UINT16_TYPE unsigned __int16
#define UINT32_TYPE unsigned __int32
#else
#define UINT16_TYPE uint16_t
#define UINT32_TYPE uint32_t
#endif

static inline void
store_16_n (unsigned int val, void *vp)
{
    UINT16_TYPE n = val;
    memcpy(vp, &n, 2);
}
static inline void
store_32_n (unsigned int val, void *vp)
{
    UINT32_TYPE n = val;
    memcpy(vp, &n, 4);
}
static inline void
store_64_n (UINT64_TYPE val, void *vp)
{
    UINT64_TYPE n = val;
    memcpy(vp, &n, 8);
}
static inline unsigned short
load_16_n (const void *p)
{
    UINT16_TYPE n;
    memcpy(&n, p, 2);
    return n;
}
static inline unsigned int
load_32_n (const void *p)
{
    UINT32_TYPE n;
    memcpy(&n, p, 4);
    return n;
}
static inline UINT64_TYPE
load_64_n (const void *p)
{
    UINT64_TYPE n;
    memcpy(&n, p, 8);
    return n;
}
#undef UINT16_TYPE
#undef UINT32_TYPE

/* Assume for simplicity that these swaps are identical.  */
static inline UINT64_TYPE
k5_htonll (UINT64_TYPE val)
{
#ifdef K5_BE
    return val;
#elif defined K5_LE && defined SWAP64
    return SWAP64 (val);
#else
    return load_64_be ((unsigned char *)&val);
#endif
}
static inline UINT64_TYPE
k5_ntohll (UINT64_TYPE val)
{
    return k5_htonll (val);
}

/* Make the interfaces to getpwnam and getpwuid consistent.
   Model the wrappers on the POSIX thread-safe versions, but
   use the unsafe system versions if the safe ones don't exist
   or we can't figure out their interfaces.  */

/* int k5_getpwnam_r(const char *, blah blah) */
#ifdef HAVE_GETPWNAM_R
# ifndef GETPWNAM_R_4_ARGS
/* POSIX */
#  define k5_getpwnam_r(NAME, REC, BUF, BUFSIZE, OUT)   \
        (getpwnam_r(NAME,REC,BUF,BUFSIZE,OUT) == 0      \
         ? (*(OUT) == NULL ? -1 : 0) : -1)
# else
/* POSIX drafts? */
#  ifdef GETPWNAM_R_RETURNS_INT
#   define k5_getpwnam_r(NAME, REC, BUF, BUFSIZE, OUT)  \
        (getpwnam_r(NAME,REC,BUF,BUFSIZE) == 0          \
         ? (*(OUT) = REC, 0)                            \
         : (*(OUT) = NULL, -1))
#  else
#   define k5_getpwnam_r(NAME, REC, BUF, BUFSIZE, OUT)  \
        (*(OUT) = getpwnam_r(NAME,REC,BUF,BUFSIZE), *(OUT) == NULL ? -1 : 0)
#  endif
# endif
#else /* no getpwnam_r, or can't figure out #args or return type */
/* Will get warnings about unused variables.  */
# define k5_getpwnam_r(NAME, REC, BUF, BUFSIZE, OUT) \
        (*(OUT) = getpwnam(NAME), *(OUT) == NULL ? -1 : 0)
#endif

/* int k5_getpwuid_r(uid_t, blah blah) */
#ifdef HAVE_GETPWUID_R
# ifndef GETPWUID_R_4_ARGS
/* POSIX */
#  define k5_getpwuid_r(UID, REC, BUF, BUFSIZE, OUT)    \
        (getpwuid_r(UID,REC,BUF,BUFSIZE,OUT) == 0       \
         ? (*(OUT) == NULL ? -1 : 0) : -1)
# else
/* POSIX drafts?  Yes, I mean to test GETPWNAM... here.  Less junk to
   do at configure time.  */
#  ifdef GETPWNAM_R_RETURNS_INT
#   define k5_getpwuid_r(UID, REC, BUF, BUFSIZE, OUT)   \
        (getpwuid_r(UID,REC,BUF,BUFSIZE) == 0           \
         ? (*(OUT) = REC, 0)                            \
         : (*(OUT) = NULL, -1))
#  else
#   define k5_getpwuid_r(UID, REC, BUF, BUFSIZE, OUT)  \
        (*(OUT) = getpwuid_r(UID,REC,BUF,BUFSIZE), *(OUT) == NULL ? -1 : 0)
#  endif
# endif
#else /* no getpwuid_r, or can't figure out #args or return type */
/* Will get warnings about unused variables.  */
# define k5_getpwuid_r(UID, REC, BUF, BUFSIZE, OUT) \
        (*(OUT) = getpwuid(UID), *(OUT) == NULL ? -1 : 0)
#endif

/* Ensure, if possible, that the indicated file descriptor won't be
   kept open if we exec another process (e.g., launching a ccapi
   server).  If we don't know how to do it... well, just go about our
   business.  Probably most callers won't check the return status
   anyways.  */

#if 0
static inline int
set_cloexec_fd(int fd)
{
#if defined(F_SETFD)
# ifdef FD_CLOEXEC
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
        return errno;
# else
    if (fcntl(fd, F_SETFD, 1) != 0)
        return errno;
# endif
#endif
    return 0;
}

static inline int
set_cloexec_file(FILE *f)
{
    return set_cloexec_fd(fileno(f));
}
#else
/* Macros make the Sun compiler happier, and all variants of this do a
   single evaluation of the argument, and fcntl and fileno should
   produce reasonable error messages on type mismatches, on any system
   with F_SETFD.  */
#ifdef F_SETFD
# ifdef FD_CLOEXEC
#  define set_cloexec_fd(FD)    (fcntl((FD), F_SETFD, FD_CLOEXEC) ? errno : 0)
# else
#  define set_cloexec_fd(FD)    (fcntl((FD), F_SETFD, 1) ? errno : 0)
# endif
#else
# define set_cloexec_fd(FD)     ((FD),0)
#endif
#define set_cloexec_file(F)     set_cloexec_fd(fileno(F))
#endif



/* Since the original ANSI C spec left it undefined whether or
   how you could copy around a va_list, C 99 added va_copy.
   For old implementations, let's do our best to fake it.

   XXX Doesn't yet handle implementations with __va_copy (early draft)
   or GCC's __builtin_va_copy.  */
#if defined(HAS_VA_COPY) || defined(va_copy)
/* Do nothing.  */
#elif defined(CAN_COPY_VA_LIST)
#define va_copy(dest, src)      ((dest) = (src))
#else
/* Assume array type, but still simply copyable.

   There is, theoretically, the possibility that va_start will
   allocate some storage pointed to by the va_list, and in that case
   we'll just lose.  If anyone cares, we could try to devise a test
   for that case.  */
#define va_copy(dest, src)      memcpy(dest, src, sizeof(va_list))
#endif

/* Provide strlcpy/strlcat interfaces. */
#ifndef HAVE_STRLCPY
#define strlcpy krb5int_strlcpy
#define strlcat krb5int_strlcat
extern size_t krb5int_strlcpy(char *dst, const char *src, size_t siz);
extern size_t krb5int_strlcat(char *dst, const char *src, size_t siz);
#endif

/* Provide fnmatch interface. */
#ifndef HAVE_FNMATCH
#define fnmatch k5_fnmatch
int k5_fnmatch(const char *pattern, const char *string, int flags);
#define FNM_NOMATCH     1       /* Match failed. */
#define FNM_NOSYS       2       /* Function not implemented. */
#define FNM_NORES       3       /* Out of resources */
#define FNM_NOESCAPE    0x01    /* Disable backslash escaping. */
#define FNM_PATHNAME    0x02    /* Slash must be matched by slash. */
#define FNM_PERIOD      0x04    /* Period must be matched by period. */
#define FNM_CASEFOLD    0x08    /* Pattern is matched case-insensitive */
#define FNM_LEADING_DIR 0x10    /* Ignore /<tail> after Imatch. */
#endif

/* Provide [v]asprintf interfaces.  */
#ifndef HAVE_VSNPRINTF
#ifdef _WIN32
static inline int
vsnprintf(char *str, size_t size, const char *format, va_list args)
{
    va_list args_copy;
    int length;

    va_copy(args_copy, args);
    length = _vscprintf(format, args_copy);
    va_end(args_copy);
    if (size > 0) {
        _vsnprintf(str, size, format, args);
        str[size - 1] = '\0';
    }
    return length;
}
static inline int
snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    int n;

    va_start(args, format);
    n = vsnprintf(str, size, format, args);
    va_end(args);
    return n;
}
#else /* not win32 */
#error We need an implementation of vsnprintf.
#endif /* win32? */
#endif /* no vsnprintf */

#ifndef HAVE_VASPRINTF

extern int krb5int_vasprintf(char **, const char *, va_list)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 2, 0)))
#endif
    ;
extern int krb5int_asprintf(char **, const char *, ...)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;

#define vasprintf krb5int_vasprintf
/* Assume HAVE_ASPRINTF iff HAVE_VASPRINTF.  */
#define asprintf krb5int_asprintf

#elif defined(NEED_VASPRINTF_PROTO)

extern int vasprintf(char **, const char *, va_list)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 2, 0)))
#endif
    ;
extern int asprintf(char **, const char *, ...)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;

#endif /* have vasprintf and prototype? */

/* Return true if the snprintf return value RESULT reflects a buffer
   overflow for the buffer size SIZE.

   We cast the result to unsigned int for two reasons.  First, old
   implementations of snprintf (such as the one in Solaris 9 and
   prior) return -1 on a buffer overflow.  Casting the result to -1
   will convert that value to UINT_MAX, which should compare larger
   than any reasonable buffer size.  Second, comparing signed and
   unsigned integers will generate warnings with some compilers, and
   can have unpredictable results, particularly when the relative
   widths of the types is not known (size_t may be the same width as
   int or larger).
*/
#define SNPRINTF_OVERFLOW(result, size) \
    ((unsigned int)(result) >= (size_t)(size))

#ifndef HAVE_MKSTEMP
extern int krb5int_mkstemp(char *);
#define mkstemp krb5int_mkstemp
#endif

#ifndef HAVE_GETTIMEOFDAY
extern int krb5int_gettimeofday(struct timeval *tp, void *ignore);
#define gettimeofday krb5int_gettimeofday
#endif

extern void krb5int_zap(void *ptr, size_t len);

/*
 * Return 0 if the n-byte memory regions p1 and p2 are equal, and nonzero if
 * they are not.  The function is intended to take the same amount of time
 * regardless of how many bytes of p1 and p2 are equal.
 */
int k5_bcmp(const void *p1, const void *p2, size_t n);

/*
 * Split a path into parent directory and basename.  Either output parameter
 * may be NULL if the caller doesn't need it.  parent_out will be empty if path
 * has no basename.  basename_out will be empty if path ends with a path
 * separator.  Returns 0 on success or ENOMEM on allocation failure.
 */
long k5_path_split(const char *path, char **parent_out, char **basename_out);

/*
 * Compose two path components, inserting the platform-appropriate path
 * separator if needed.  If path2 is an absolute path, path1 will be discarded
 * and path_out will be a copy of path2.  Returns 0 on success or ENOMEM on
 * allocation failure.
 */
long k5_path_join(const char *path1, const char *path2, char **path_out);

/* Return 1 if path is absolute, 0 if it is relative. */
int k5_path_isabs(const char *path);

/*
 * Localization macros.  If we have gettext, define _ appropriately for
 * translating a string.  If we do not have gettext, define _ and
 * bindtextdomain as no-ops.  N_ is always a no-op; it marks a string for
 * extraction to pot files but does not translate it.
 */
#ifdef ENABLE_NLS
#include <libintl.h>
#define KRB5_TEXTDOMAIN "mit-krb5"
#define _(s) dgettext(KRB5_TEXTDOMAIN, s)
#else
#define _(s) s
#define dgettext(d, m) m
#define ngettext(m1, m2, n) (((n) == 1) ? m1 : m2)
#define bindtextdomain(p, d)
#endif
#define N_(s) s

#endif /* K5_PLATFORM_H */
