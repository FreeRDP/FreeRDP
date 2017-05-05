/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-thread.h - Preliminary portable thread support */
/*
 * Copyright 2004,2005,2006,2007,2008 by the Massachusetts Institute of Technology.
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

#ifndef K5_THREAD_H
#define K5_THREAD_H

#include "autoconf.h"
#ifndef KRB5_CALLCONV
# define KRB5_CALLCONV
#endif
#ifndef KRB5_CALLCONV_C
# define KRB5_CALLCONV_C
#endif

/* Interface (tentative):

     Mutex support:

     // Between these two, we should be able to do pure compile-time
     // and pure run-time initialization.
     //   POSIX:   partial initializer is PTHREAD_MUTEX_INITIALIZER,
     //            finish does nothing
     //   Windows: partial initializer is an invalid handle,
     //            finish does the real initialization work
     k5_mutex_t foo_mutex = K5_MUTEX_PARTIAL_INITIALIZER;
     int k5_mutex_finish_init(k5_mutex_t *);
     // for dynamic allocation
     int k5_mutex_init(k5_mutex_t *);
     // Must work for both kinds of alloc, even if it means adding flags.
     int k5_mutex_destroy(k5_mutex_t *);

     // As before.
     int k5_mutex_lock(k5_mutex_t *);
     int k5_mutex_unlock(k5_mutex_t *);

     In each library, one new function to finish the static mutex init,
     and any other library-wide initialization that might be desired.
     On POSIX, this function would be called via the second support
     function (see below).  On Windows, it would be called at library
     load time.  These functions, or functions they calls, should be the
     only places that k5_mutex_finish_init gets called.

     A second function or macro called at various possible "first" entry
     points which either calls pthread_once on the first function
     (POSIX), or checks some flag set by the first function (Windows),
     and possibly returns an error.  (In the non-threaded case, a simple
     flag can be used to avoid multiple invocations, and the mutexes
     don't need run-time initialization anyways.)

     A third function for library termination calls mutex_destroy on
     each mutex for the library.  This function would be called
     automatically at library unload time.  If it turns out to be needed
     at exit time for libraries that don't get unloaded, perhaps we
     should also use atexit().  Any static mutexes should be cleaned up
     with k5_mutex_destroy here.

     How does that second support function invoke the first support
     function only once?  Through something modelled on pthread_once
     that I haven't written up yet.  Probably:

     k5_once_t foo_once = K5_ONCE_INIT;
     k5_once(k5_once_t *, void (*)(void));

     For POSIX: Map onto pthread_once facility.
     For non-threaded case: A simple flag.
     For Windows: Not needed; library init code takes care of it.

     XXX: A general k5_once mechanism isn't possible for Windows,
     without faking it through named mutexes or mutexes initialized at
     startup.  I was only using it in one place outside these headers,
     so I'm dropping the general scheme.  Eventually the existing uses
     in k5-thread.h and k5-platform.h will be converted to pthread_once
     or static variables.


     Thread-specific data:

     // TSD keys are limited in number in gssapi/krb5/com_err; enumerate
     // them all.  This allows support code init to allocate the
     // necessary storage for pointers all at once, and avoids any
     // possible error in key creation.
     enum { ... } k5_key_t;
     // Register destructor function.  Called in library init code.
     int k5_key_register(k5_key_t, void (*destructor)(void *));
     // Returns NULL or data.
     void *k5_getspecific(k5_key_t);
     // Returns error if key out of bounds, or the pointer table can't
     // be allocated.  A call to k5_key_register must have happened first.
     // This may trigger the calling of pthread_setspecific on POSIX.
     int k5_setspecific(k5_key_t, void *);
     // Called in library termination code.
     // Trashes data in all threads, calling the registered destructor
     // (but calling it from the current thread).
     int k5_key_delete(k5_key_t);

     For the non-threaded version, the support code will have a static
     array indexed by k5_key_t values, and get/setspecific simply access
     the array elements.

     The TSD destructor table is global state, protected by a mutex if
     threads are enabled.


     Any actual external symbols will use the krb5int_ prefix.  The k5_
     names will be simple macros or inline functions to rename the
     external symbols, or slightly more complex ones to expand the
     implementation inline (e.g., map to POSIX versions and/or debug
     code using __FILE__ and the like).


     More to be added, perhaps.  */

#include <assert.h>


/* The mutex structure we use, k5_mutex_t, is defined to some
   OS-specific bits.  The use of multiple layers of typedefs are an
   artifact resulting from debugging code we once used, implemented as
   wrappers around the OS mutex scheme.

   The OS specific bits, in k5_os_mutex, break down into three primary
   implementations, POSIX threads, Windows threads, and no thread
   support.  However, the POSIX thread version is further subdivided:
   In one case, we can determine at run time whether the thread
   library is linked into the application, and use it only if it is
   present; in the other case, we cannot, and the thread library must
   be linked in always, but can be used unconditionally.  In the
   former case, the k5_os_mutex structure needs to hold both the POSIX
   and the non-threaded versions.

   The various k5_os_mutex_* operations are the OS-specific versions,
   applied to the OS-specific data, and k5_mutex_* uses k5_os_mutex_*
   to do the OS-specific parts of the work.  */

/* Define the OS mutex bit.  */

typedef char k5_os_nothread_mutex;
# define K5_OS_NOTHREAD_MUTEX_PARTIAL_INITIALIZER       0
/* Empty inline functions avoid the "statement with no effect"
   warnings, and do better type-checking than functions that don't use
   their arguments.  */
static inline int k5_os_nothread_mutex_finish_init(k5_os_nothread_mutex *m) {
    return 0;
}
static inline int k5_os_nothread_mutex_init(k5_os_nothread_mutex *m) {
    return 0;
}
static inline int k5_os_nothread_mutex_destroy(k5_os_nothread_mutex *m) {
    return 0;
}
static inline int k5_os_nothread_mutex_lock(k5_os_nothread_mutex *m) {
    return 0;
}
static inline int k5_os_nothread_mutex_unlock(k5_os_nothread_mutex *m) {
    return 0;
}

/* Values:
   2 - function has not been run
   3 - function has been run
   4 - function is being run -- deadlock detected */
typedef unsigned char k5_os_nothread_once_t;
# define K5_OS_NOTHREAD_ONCE_INIT       2
# define k5_os_nothread_once(O,F)                               \
    (*(O) == 3 ? 0                                              \
     : *(O) == 2 ? (*(O) = 4, (F)(), *(O) = 3, 0)               \
     : (assert(*(O) != 4), assert(*(O) == 2 || *(O) == 3), 0))



#ifndef ENABLE_THREADS

typedef k5_os_nothread_mutex k5_os_mutex;
# define K5_OS_MUTEX_PARTIAL_INITIALIZER        \
    K5_OS_NOTHREAD_MUTEX_PARTIAL_INITIALIZER
# define k5_os_mutex_finish_init        k5_os_nothread_mutex_finish_init
# define k5_os_mutex_init               k5_os_nothread_mutex_init
# define k5_os_mutex_destroy            k5_os_nothread_mutex_destroy
# define k5_os_mutex_lock               k5_os_nothread_mutex_lock
# define k5_os_mutex_unlock             k5_os_nothread_mutex_unlock

# define k5_once_t                      k5_os_nothread_once_t
# define K5_ONCE_INIT                   K5_OS_NOTHREAD_ONCE_INIT
# define k5_once                        k5_os_nothread_once

#elif HAVE_PTHREAD

# include <pthread.h>

/* Weak reference support, etc.

   Linux: Stub mutex routines exist, but pthread_once does not.

   Solaris <10: In libc there's a pthread_once that doesn't seem to do
   anything.  Bleah.  But pthread_mutexattr_setrobust_np is defined
   only in libpthread.  However, some version of GNU libc (Red Hat's
   Fedora Core 5, reportedly) seems to have that function, but no
   declaration, so we'd have to declare it in order to test for its
   address.  We now have tests to see if pthread_once actually works,
   so stick with that for now.

   Solaris 10: The real thread support now lives in libc, and
   libpthread is just a filter object.  So we might as well use the
   real functions unconditionally.  Since we haven't got a test for
   this property yet, we use NO_WEAK_PTHREADS defined in aclocal.m4
   depending on the OS type.

   IRIX 6.5 stub pthread support in libc is really annoying.  The
   pthread_mutex_lock function returns ENOSYS for a program not linked
   against -lpthread.  No link-time failure, no weak symbols, etc.
   The C library doesn't provide pthread_once; we can use weak
   reference support for that.

   If weak references are not available, then for now, we assume that
   the pthread support routines will always be available -- either the
   real thing, or functional stubs that merely prohibit creating
   threads.

   If we find a platform with non-functional stubs and no weak
   references, we may have to resort to some hack like dlsym on the
   symbol tables of the current process.  */
extern int krb5int_pthread_loaded(void)
#ifdef __GNUC__
/* We should always get the same answer for the life of the process.  */
    __attribute__((const))
#endif
    ;
#if defined(HAVE_PRAGMA_WEAK_REF) && !defined(NO_WEAK_PTHREADS)
# pragma weak pthread_once
# pragma weak pthread_mutex_lock
# pragma weak pthread_mutex_unlock
# pragma weak pthread_mutex_destroy
# pragma weak pthread_mutex_init
# pragma weak pthread_self
# pragma weak pthread_equal
# define K5_PTHREADS_LOADED     (krb5int_pthread_loaded())
# define USE_PTHREAD_LOCK_ONLY_IF_LOADED

/* Can't rely on useful stubs -- see above regarding Solaris.  */
typedef struct {
    pthread_once_t o;
    k5_os_nothread_once_t n;
} k5_once_t;
# define K5_ONCE_INIT   { PTHREAD_ONCE_INIT, K5_OS_NOTHREAD_ONCE_INIT }
# define k5_once(O,F)   (K5_PTHREADS_LOADED                     \
                         ? pthread_once(&(O)->o,F)              \
                         : k5_os_nothread_once(&(O)->n,F))

#else

/* no pragma weak support */
# define K5_PTHREADS_LOADED     (1)

typedef pthread_once_t k5_once_t;
# define K5_ONCE_INIT   PTHREAD_ONCE_INIT
# define k5_once        pthread_once

#endif

#if defined(__mips) && defined(__sgi) && (defined(_SYSTYPE_SVR4) || defined(__SYSTYPE_SVR4__))
# ifndef HAVE_PRAGMA_WEAK_REF
#  if defined(__GNUC__) && __GNUC__ < 3
#   error "Please update to a newer gcc with weak symbol support, or switch to native cc, reconfigure and recompile."
#  else
#   error "Weak reference support is required"
#  endif
# endif
#endif

typedef pthread_mutex_t k5_os_mutex;
# define K5_OS_MUTEX_PARTIAL_INITIALIZER        \
    PTHREAD_MUTEX_INITIALIZER

#ifdef USE_PTHREAD_LOCK_ONLY_IF_LOADED

# define k5_os_mutex_finish_init(M)             (0)
# define k5_os_mutex_init(M)                                    \
    (K5_PTHREADS_LOADED ? pthread_mutex_init((M), 0) : 0)
# define k5_os_mutex_destroy(M)                                 \
    (K5_PTHREADS_LOADED ? pthread_mutex_destroy((M)) : 0)
# define k5_os_mutex_lock(M)                            \
    (K5_PTHREADS_LOADED ? pthread_mutex_lock(M) : 0)
# define k5_os_mutex_unlock(M)                          \
    (K5_PTHREADS_LOADED ? pthread_mutex_unlock(M) : 0)

#else

static inline int k5_os_mutex_finish_init(k5_os_mutex *m) { return 0; }
# define k5_os_mutex_init(M)            pthread_mutex_init((M), 0)
# define k5_os_mutex_destroy(M)         pthread_mutex_destroy((M))
# define k5_os_mutex_lock(M)            pthread_mutex_lock(M)
# define k5_os_mutex_unlock(M)          pthread_mutex_unlock(M)

#endif /* is pthreads always available? */

#elif defined _WIN32

typedef struct {
    HANDLE h;
    int is_locked;
} k5_os_mutex;

# define K5_OS_MUTEX_PARTIAL_INITIALIZER { INVALID_HANDLE_VALUE, 0 }

# define k5_os_mutex_finish_init(M)                                     \
    (assert((M)->h == INVALID_HANDLE_VALUE),                            \
     ((M)->h = CreateMutex(NULL, FALSE, NULL)) ? 0 : GetLastError())
# define k5_os_mutex_init(M)                                            \
    ((M)->is_locked = 0,                                                \
     ((M)->h = CreateMutex(NULL, FALSE, NULL)) ? 0 : GetLastError())
# define k5_os_mutex_destroy(M)                                 \
    (CloseHandle((M)->h) ? ((M)->h = 0, 0) : GetLastError())

static inline int k5_os_mutex_lock(k5_os_mutex *m)
{
    DWORD res;
    res = WaitForSingleObject(m->h, INFINITE);
    if (res == WAIT_FAILED)
        return GetLastError();
    /* Eventually these should be turned into some reasonable error
       code.  */
    assert(res != WAIT_TIMEOUT);
    assert(res != WAIT_ABANDONED);
    assert(res == WAIT_OBJECT_0);
    /* Avoid locking twice.  */
    assert(m->is_locked == 0);
    m->is_locked = 1;
    return 0;
}

# define k5_os_mutex_unlock(M)                  \
    (assert((M)->is_locked == 1),               \
     (M)->is_locked = 0,                        \
     ReleaseMutex((M)->h) ? 0 : GetLastError())

#else

# error "Thread support enabled, but thread system unknown"

#endif




typedef k5_os_mutex k5_mutex_t;
#define K5_MUTEX_PARTIAL_INITIALIZER    K5_OS_MUTEX_PARTIAL_INITIALIZER
static inline int k5_mutex_init(k5_mutex_t *m)
{
    return k5_os_mutex_init(m);
}
static inline int k5_mutex_finish_init(k5_mutex_t *m)
{
    return k5_os_mutex_finish_init(m);
}
#define k5_mutex_destroy(M)                     \
    (k5_os_mutex_destroy(M))

static inline void k5_mutex_lock(k5_mutex_t *m)
{
    int r = k5_os_mutex_lock(m);
    assert(r == 0);
}

static inline void k5_mutex_unlock(k5_mutex_t *m)
{
    int r = k5_os_mutex_unlock(m);
    assert(r == 0);
}

#define k5_mutex_assert_locked(M)       ((void)(M))
#define k5_mutex_assert_unlocked(M)     ((void)(M))
#define k5_assert_locked        k5_mutex_assert_locked
#define k5_assert_unlocked      k5_mutex_assert_unlocked


/* Thread-specific data; implemented in a support file, because we'll
   need to keep track of some global data for cleanup purposes.

   Note that the callback function type is such that the C library
   routine free() is a valid callback.  */
typedef enum {
    K5_KEY_COM_ERR,
    K5_KEY_GSS_KRB5_SET_CCACHE_OLD_NAME,
    K5_KEY_GSS_KRB5_CCACHE_NAME,
    K5_KEY_GSS_KRB5_ERROR_MESSAGE,
#if defined(__MACH__) && defined(__APPLE__)
    K5_KEY_IPC_CONNECTION_INFO,
#endif
    K5_KEY_MAX
} k5_key_t;
/* rename shorthand symbols for export */
#define k5_key_register krb5int_key_register
#define k5_getspecific  krb5int_getspecific
#define k5_setspecific  krb5int_setspecific
#define k5_key_delete   krb5int_key_delete
extern int k5_key_register(k5_key_t, void (*)(void *));
extern void *k5_getspecific(k5_key_t);
extern int k5_setspecific(k5_key_t, void *);
extern int k5_key_delete(k5_key_t);

extern int  KRB5_CALLCONV krb5int_mutex_alloc  (k5_mutex_t **);
extern void KRB5_CALLCONV krb5int_mutex_free   (k5_mutex_t *);
extern void KRB5_CALLCONV krb5int_mutex_lock   (k5_mutex_t *);
extern void KRB5_CALLCONV krb5int_mutex_unlock (k5_mutex_t *);

/* In time, many of the definitions above should move into the support
   library, and this file should be greatly simplified.  For type
   definitions, that'll take some work, since other data structures
   incorporate mutexes directly, and our mutex type is dependent on
   configuration options and system attributes.  For most functions,
   though, it should be relatively easy.

   For now, plugins should use the exported functions, and not the
   above macros, and use krb5int_mutex_alloc for allocations.  */
#if defined(PLUGIN) || (defined(CONFIG_SMALL) && !defined(THREAD_SUPPORT_IMPL))
#undef k5_mutex_lock
#define k5_mutex_lock krb5int_mutex_lock
#undef k5_mutex_unlock
#define k5_mutex_unlock krb5int_mutex_unlock
#endif

#endif /* multiple inclusion? */
