/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2015-2020 Micron Technology, Inc.  All rights reserved.
 */

#ifndef MPOOL_MERR_H
#define MPOOL_MERR_H

/*
 * The merr_t typedef is designed to enable developers to identify the line
 * of code that first throws an error without having debug printfs up and down
 * the call stack.  If a stack of functions use merr_t as a return type,
 * and each layer propagates merr_t values up the stack, then the caller at
 * the top can log an error with information to help identify the code that
 * first threw the error.
 *
 * For convenience and simplicity a merr_t is a 64 bit integer that uniquely
 * identifies the file, line, and errno of the call site where the merr_t
 * was generated.  If we use a full 32 bits for the errno and 14 bits for
 * the line number we are left with only 18 bits into which to save the file
 * name.  We cannot easily store the pointer to the file name within 18 bits,
 * but we can store the difference between the file name and a well-known
 * symbol that is located nearby.
 *
 * To that end, each file has a private symbol named "_merr_file" and there
 * exists one global symbol named "merr_base" from which the difference is
 * obtained.  All these symbols are placed in the same section by the
 * linker, so the relative difference between any two of these symbols
 * is small enough to easly fit within 18 bits.
 *
 * Note that merr_t generated from within the kernel can be correctly decoded
 * in user space by leveraging merr_base[] as a copyout buffer for kernel
 * file names.  Hence, the user space needs no knowledge of the files used
 * in the kernel.  See merr_to_user() for details.
 *
 * The only way to construct a valid merr_t is either by assignment to zero,
 * generation via merr() (e.g., err = merr(EINVAL)), or by direct assignment
 * from a merr_t generated by one of the first two  methods.  All other
 * constructions are invalid and will cause 'merr_file(err)' to generate a
 * string of the form "merr_bug[0-3][uk]', where the final character denotes
 * whether the call to merr_file() was performed in user space or the kernel.
 * Invalid merr_t can be extremely difficult to trace to their point of origin
 * due to the fact that they likely contain no valid call * site details.  If
 * you're lucky, the line number is valid and you can use the following find
 * command to locate the origin of the error:
 *
 *     find . -name \*.[ch] -print | xargs grep -n 'merr' | grep :120:
 *
 *     foo.c:120:  return merr(ENOSPC);
 */

#define EBUG                (666)

/* MERR_ALIGN      Alignment of _merr_file in section ".merr"
 * MERR_INFO_SZ    Max size of struct merr_info message buffer
 * MERR_BASE_SZ    Size of struct merr_base[] user buffer for module file names
 */
#define MERR_ALIGN          (1 << 6)
#define MERR_BASE_SZ        (MERR_ALIGN * 64 * 2)

#define _merr_section       __attribute__((__section__("mpool_merr")))
#define _merr_attributes    _merr_section __aligned(MERR_ALIGN) __maybe_unused

static char _merr_file[]    _merr_attributes = KBUILD_BASENAME;

/* Layout of merr_t:
 *
 *   Field   #bits  Description
 *   ------  -----  ----------
 *   63..48   16    signed offset of (_merr_file - merr_base) / MERR_ALIGN
 *   47..32   16    line number
 *   31..31    1    reserved bits
 *   30..0    31    positive errno value
 */
#define MERR_FILE_SHIFT     (48)
#define MERR_LINE_SHIFT     (32)
#define MERR_RSVD_SHIFT     (31)

#define MERR_FILE_MASK      (0xffff000000000000ul)
#define MERR_LINE_MASK      (0x0000ffff00000000ul)
#define MERR_RSVD_MASK      (0x0000000080000000ul)
#define MERR_ERRNO_MASK     (0x000000007ffffffful)

typedef s64                 merr_t;

/**
 * merr() - Pack given errno and call-site info into a merr_t
 */
#define merr(_errnum)       merr_pack((_errnum), _merr_file, __LINE__)
#define merr_once(_errnum)  merr_pack((_errnum), _merr_file, __LINE__)

/* Not a public API, called only via the merr() macro.
 */
merr_t merr_pack(int error, const char *file, const int line);

/**
 * merr_strerror() - Format errno description from merr_t
 */
char *merr_strerror(merr_t err, char *buf, size_t bufsz);

/**
 * merr_strinfo() - Format file, line, and errno from merr_t
 */
char *merr_strinfo(merr_t err, char *buf, size_t bufsz);

/**
 * merr_file() - Return file name ptr from merr_t
 */
const char *merr_file(merr_t err);

/**
 * merr_to_user() - Convert a merr_t from kernel to user space
 */
merr_t merr_to_user(merr_t err, char __user *ubuf);

/**
 * merr_errno() - Return the errno from given merr_t
 */
static inline int merr_errno(merr_t merr)
{
	return merr & MERR_ERRNO_MASK;
}

/**
 * merr_lineno() - Return the line number from given merr_t
 */
static inline int merr_lineno(merr_t err)
{
	return (err & MERR_LINE_MASK) >> MERR_LINE_SHIFT;
}

#endif /* MPOOL_MERR_H */
