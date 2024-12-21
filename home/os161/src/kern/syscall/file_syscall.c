/**
 * @file file_syscall.c
 * @author G. Cabodi
 * @brief Simple implementation of sys_read and sys_write system calls.
 * 
 * @details These implementations handle basic read and write operations 
 *          for file descriptors. The current implementation is limited 
 *          to standard input (stdin) for `sys_read` and standard output 
 *          (stdout) or standard error (stderr) for `sys_write`.
 * 
 * @note The functions currently do not support other file descriptors 
 *       or interact with a file system.
 * 
 * @warning Attempting to use these functions with unsupported file 
 *          descriptors will result in an error.
 * 
 * @todo Extend support to handle other file descriptors and enable 
 *       full interaction with a file system.
 */

#include <types.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>

/**
 * @brief Writes data to the specified file descriptor.
 * 
 * @details Writes `size` bytes from the buffer pointed by `buf_ptr` 
 *          to the specified file descriptor `fd`. Currently, only 
 *          `STDOUT_FILENO` and `STDERR_FILENO` are supported. 
 *          Characters are written to the console using `putch`.
 * 
 * @param fd The file descriptor to write to. Must be either `STDOUT_FILENO`
 *           (standard output) or `STDERR_FILENO` (standard error).
 * @param buf_ptr A pointer to the buffer containing the data to write.
 * @param size The number of bytes to write from the buffer.
 * 
 * @return The number of bytes written on success.
 * @retval -1 If an unsupported file descriptor is provided.
 * 
 * @warning Writing to unsupported file descriptors will result in an error.
 */
int
sys_write(int fd, userptr_t buf_ptr, size_t size)
{
    int i;
    char *p = (char *)buf_ptr;

    /* Check if the file descriptor is supported */
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        kprintf("sys_write supported only to stdout or stderr\n");
        return -1;
    }

    /* Write each character from the buffer to the console */
    for (i = 0; i < (int)size; i++) {
        putch(p[i]);
    }

    /* Return the total number of bytes written */
    return (int)size;
}

/**
 * @brief Reads data from the specified file descriptor.
 * 
 * @details Reads `size` bytes from the specified file descriptor `fd` 
 *          into the buffer pointed to by `buf_ptr`. Currently, only 
 *          `STDIN_FILENO` is supported. Characters are read from the 
 *          console using `getch`.
 * 
 * @param fd The file descriptor to read from. Must be `STDIN_FILENO` (standard input).
 * @param buf_ptr A pointer to the buffer where the read data will be stored.
 * @param size The maximum number of bytes to read into the buffer.
 * 
 * @return The number of bytes successfully read.
 * @retval -1 If an unsupported file descriptor is provided.
 * 
 * @warning Reading from unsupported file descriptors will result in an error.
 */
int
sys_read(int fd, userptr_t buf_ptr, size_t size)
{
    int i;
    char *p = (char *)buf_ptr;

    /* Check if the file descriptor is supported */
    if (fd != STDIN_FILENO) {
        kprintf("sys_read supported only from stdin\n");
        return -1;
    }

    /* Read each character from the console into the buffer */
    for (i = 0; i < (int)size; i++) {
        p[i] = getch();

        /* Stop reading if an invalid character is encountered */
        if (p[i] < 0) {
            return i;
        }
    }

    /* Return the total number of bytes read */
    return (int)size;
}
