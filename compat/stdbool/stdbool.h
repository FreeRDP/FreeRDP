/*===---- stdbool.h - Standard header for booleans -------------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __STDBOOL_H
#define __STDBOOL_H

/* Don't define bool, true, and false in C++, except as a GNU extension. */
#ifndef __cplusplus
typedef int bool;
#define true 1
#define false 0
#endif

#define __bool_true_false_are_defined 1

#endif /* __STDBOOL_H */
