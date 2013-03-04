The Primitives Library

Introduction
------------
The purpose of the primitives library is to give the freerdp code easy
access to *run-time* optimization via SIMD operations.  When the library
is initialized, dynamic checks of processor features are run (such as
the support of SSE3 or Neon), and entrypoints are linked to through
function pointers to provide the fastest possible operations.  All
routines offer generic C alternatives as fallbacks.

Run-time optimization has the advantage of allowing a single executable
to run fast on multiple platforms with different SIMD capabilities.


Use In Code
-----------
A singleton pointing to a structure containing the function pointers
is accessed through primitives_get().   The function pointers can then
be used from that structure, e.g.

    primitives_t *prims = primitives_get();
    prims->shiftC_16s(buffer, shifts, buffer, 256);

Of course, there is some overhead in calling through the function pointer
and setting up the SIMD operations, so it would be counterproductive to
call the primitives library for very small operation, e.g. initializing an
array of eight values to a constant.  The primitives library is intended
for larger-scale operations, e.g. arrays of size 64 and larger.


Initialization and Cleanup
--------------------------
Library initialization is done the first time primitives_init() is called
or the first time primitives_get() is used.  Cleanup (if any) is done by
primitives_deinit().


Intel Integrated Performance Primitives (IPP)
---------------------------------------------
If freerdp is compiled with IPP support (-DWITH_IPP=ON), the IPP function
calls will be used (where available) to fill the function pointers.
Where possible, function names and parameter lists match IPP format so
that the IPP functions can be plugged into the function pointers without
a wrapper layer.  Use of IPP is completely optional, and in many cases
the SSE operations in the primitives library itself are faster or similar
in performance.


Coverage
--------
The primitives library is not meant to be comprehensive, offering
entrypoints for every operation and operand type.  Instead, the coverage
is focused on operations known to be performance bottlenecks in the code.
For instance, 16-bit signed operations are used widely in the RemoteFX
software, so you'll find 16s versions of several operations, but there
is no attempt to provide (unused) copies of the same code for 8u, 16u,
32s, etc.


New Optimizations
-----------------
As the need arises, new optimizations can be added to the library,
including NEON, AVX, and perhaps OpenCL or other SIMD implementations.
The CPU feature detection is done in winpr/sysinfo.


Adding Entrypoints
------------------
As the need for new operations or operands arises, new entrypoints can
be added.  
  1) Function prototypes and pointers are added to 
     include/freerdp/primitives.h
  2) New module initialization and cleanup function prototypes are added
     to prim_internal.h and called in primitives.c (primitives_init()
     and primitives_deinit()).
  3) Operation names and parameter lists should be compatible with the IPP.
     IPP manuals are available online at software.intel.com.
  4) A generic C entrypoint must be available as a fallback.
  5) prim_templates.h contains macro-based templates for simple operations,
     such as applying a single SSE operation to arrays of data.
     The template functions can frequently be used to extend the
     operations without writing a lot of new code.

Cache Management
----------------
I haven't found a lot of speed improvement by attempting prefetch, and
in fact it seems to have a negative impact in some cases.  Done correctly
perhaps the routines could be further accelerated by proper use of prefetch,
fences, etc.


Testing
-------
In the test subdirectory is an executable (prim_test) that tests both
functionality and speed of primitives library operations.   Any new
modules should be added to that test, following the conventions already
established in that directory.  The program can be executed on various
target hardware to compare generic C, optimized, and IPP performance
with various array sizes.

