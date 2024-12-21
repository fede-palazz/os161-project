/**
 * @file proc_syscall.c
 * @author G. Cabodi
 * @brief A very simple implementation of sys__exit. 
 *        It avoids crash/panic during process exit. Full process 
 *        exit handling is still a TODO.
 * 
 * @details This implementation performs basic cleanup of the process's 
 *          resources by releasing the address space and terminating 
 *          the associated thread. The exit status is not yet handled.
 * 
 * @note Address space is released, and the thread is terminated. 
 *       thread_exit() does not return.
 */

#include <types.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>

/**
 * @brief Handles the termination of a process.
 * 
 * @details This function performs the cleanup necessary when a process
 *          terminates. It releases the process's address space, terminates 
 *          the thread associated with the process, and ensures that no 
 *          resources are left dangling. Currently, exit status handling 
 *          is not implemented.
 * 
 * @param status The exit status of the process. (Currently unused.)
 * 
 * @note Address space destruction is handled by `as_destroy`. 
 *       Thread termination is handled by `thread_exit`, which 
 *       does not return.
 * 
 * @warning If `thread_exit` fails to terminate the thread (which should
 *          never happen), the kernel will panic.
 */
void
sys__exit(int status)
{
    /* Retrieve the current process's address space */
    struct addrspace *as = proc_getas();

    /*
     * Clean up the address space:
     * 
     * The address space holds all virtual memory mappings and 
     * structures specific to the process. Destroying it releases 
     * the resources associated with this process's memory.
     */
    as_destroy(as);

    /*
     * Terminate the thread:
     * 
     * thread_exit() stops the execution of the current thread and
     * removes it from the system. Since there is no returning 
     * from this function, any code following it will not be executed.
     */
    thread_exit();

    /*
     * If we ever reach this point, it indicates a severe error.
     * thread_exit() should never return. We call panic() here
     * to notify the kernel that something has gone wrong.
     */
    panic("thread_exit returned (should not happen)\n");

    /*
     * Note: The `status` argument is currently unused.
     * It should eventually be stored in the process structure 
     * to allow the parent process to retrieve the exit status 
     * via the `waitpid` system call.
     */
    (void) status; // Prevent unused variable warning (TODO: implement status handling)
}
