/**
 * @file include/x86_64/fcntl.h
 *
 * @author Hiroyuki Chishiro
 */
#ifndef __MCUBE_X86_64_FCNTL_H__
#define __MCUBE_X86_64_FCNTL_H__

/*
 * fcntl.h - File control options
 *
 * Taken from POSIX:2001
 */


/*
 * The following fields are used for open(,@oflags,).
 * NOTE!! "They shall be bitwise distinct" --SUSv3
 */

/*
 * File access modes: "Applications shall specify exactly one
 * of the file access modes values in open()." --SUSv3
 *
 * "Early UNIX implementations used the numbers 0, 1, and 2
 * instead of the POSIX names shown.  Most modern UNIX implem-
 * entations define these constants to have those values."
 * --M. Kerrisk.
 *
 * To let 'O_RDWR = O_RDONLY | O_WRONLY',we don't follow that
 * historical convention!
 */


/**
 * @def O_RDONLY
 * @brief Open for reading only.
 */
#define O_RDONLY 0x0001

/**
 * @def O_WRONLY
 * @brief Open for writing only.
 */
#define O_WRONLY 0x0002

/**
 * @def O_RDWR
 * @brief Open for reading and writing.
 */
#define O_RDWR 0x0003

/**
 * @def O_ACCMODE
 * @brief Mask for file access modes.
 */
#define O_ACCMODE 0x0003

/* File creation flags: */

/**
 * @def O_CREAT
 * @brief Create file if nonexistent.
 */
#define O_CREAT 0x0004

/**
 * @def O_TRUNC
 * @brief Truncate to zero length.
 */
#define O_TRUNC 0x0008

/**
 * @def O_NOCTTY
 * @brief Don't assign controlling terminal.
 */
#define O_NOCTTY 0x0010

/**
 * @def O_EXCL
 * @brief Exclusive use: Error if file exists.
 */
#define O_EXCL 0x0020

/* File status flags: */
/**
 * @def O_APPEND
 * @brief Set append mode.
 */
#define O_APPEND 0x0040

/**
 * @def O_DSYNC
 * @brief O_DSYNC like O_SYNC, except that there is no requirement to wait for any metadata
 * changes which are not necessary to read the just-written data.
 * In practice, O_DSYNC means that the application does not need to wait until
 * ancillary information (the file modification time, for example) has been
 * written to disk.
 * Using O_DSYNC instead of O_SYNC can often eliminate the need
 * to flush the file inode on a write.
 */
#define O_DSYNC 0x0080

/**
 * @def O_NONBLOCK
 * @brief No delay (for FIFOs, etc).
 */
#define O_NONBLOCK 0x0100

/**
 * @def O_RSYNC
 * @brief affect read operations, must be used in combination with
 * either O_SYNC or O_DSYNC.
 * It will cause a read() call to block until the data (and maybe metadata)
 * being read has been flushed to disk (if necessary).
 * This flag thus gives the kernel the option of delaying the flushing
 * of data to disk; any number of writes can happen, but data need not be flushed
 * until the application reads it back.
 */
#define O_RSYNC 0x0200

/**
 * @def O_SYNC
 * @brief O_SYNC requires that any write operations block until all data
 * and all metadata have been written to persistent storage.
 */
#define O_SYNC 0x0400

#endif /* __MCUBE_X86_64_FCNTL_H__ */
