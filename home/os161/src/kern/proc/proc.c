/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <vfs.h>
#include <synch.h>
#include "opt-waitpid.h"

/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

#if OPT_WAITPID
/*
 * Initialize waitpid-specific fields for a process.
 * @param proc - Pointer to the process structure.
 * @param name - Name of the semaphore to be created.
 */
static void
proc_init_waitpid(struct proc *proc, const char *name) {
  // Create a semaphore for waitpid
  proc->p_sem = sem_create(name, 0);
}


/*
 * Destroy waitpid-specific fields of a process.
 * @param proc - Pointer to the process structure.
 */
static void
proc_end_waitpid(struct proc *proc) {
  // Destroy the waitpid semaphore
  sem_destroy(proc->p_sem);
}
#endif

/*
 * Create a proc structure.
 * @param name - Name of the process to be created.
 * @return Pointer to the created process structure or NULL on failure.
 */
static
struct proc *
proc_create(const char *name)
{
	struct proc *proc;

	// Allocate memory for the process structure
	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		// Return NULL if memory allocation fails
		return NULL;
	}
	// Duplicate the process name
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		// Free allocated memory if name duplication fails
		kfree(proc);
		return NULL;
	}

	// Initialize thread count to zero
	proc->p_numthreads = 0;
	// Initialize the spinlock for process structure
	spinlock_init(&proc->p_lock);

	/* VM fields */
	// Initialize address space to NULL
	proc->p_addrspace = NULL;

	/* VFS fields */
	// Initialize current working directory to NULL
	proc->p_cwd = NULL;

#if OPT_WAITPID
    // Initialize waitpid-related fields if enabled
	proc_init_waitpid(proc,name);
#endif

 	// Return the created process structure
	return proc;
}

/*
 * Destroy a proc structure.
 *
 * Note: nothing currently calls this. Your wait/exit code will
 * probably want to do so.
 */
void
proc_destroy(struct proc *proc)
{

#if OPT_SMARTVM
	// Close the vnode if SMARTVM option is enabled
	vfs_close(proc->p_vnode);
#endif

	/*
	 * You probably want to destroy and null out much of the
	 * process (particularly the address space) at exit time if
	 * your wait/exit design calls for the process structure to
	 * hang around beyond process exit. Some wait/exit designs
	 * do, some don't.
	 */

	// Ensure the process is valid and is not the kernel process
	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd) {
		// Decrement the reference count for the directory
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}

	/* VM fields */
	if (proc->p_addrspace) {
		/*
		 * If p is the current process, remove it safely from
		 * p_addrspace before destroying it. This makes sure
		 * we don't try to activate the address space while
		 * it's being destroyed.
		 *
		 * Also explicitly deactivate, because setting the
		 * address space to NULL won't necessarily do that.
		 *
		 * (When the address space is NULL, it means the
		 * process is kernel-only; in that case it is normally
		 * ok if the MMU and MMU- related data structures
		 * still refer to the address space of the last
		 * process that had one. Then you save work if that
		 * process is the next one to run, which isn't
		 * uncommon. However, here we're going to destroy the
		 * address space, so we need to make sure that nothing
		 * in the VM system still refers to it.)
		 *
		 * The call to as_deactivate() must come after we
		 * clear the address space, or a timer interrupt might
		 * reactivate the old address space again behind our
		 * back.
		 *
		 * If p is not the current process, still remove it
		 * from p_addrspace before destroying it as a
		 * precaution. Note that if p is not the current
		 * process, in order to be here p must either have
		 * never run (e.g. cleaning up after fork failed) or
		 * have finished running and exited. It is quite
		 * incorrect to destroy the proc structure of some
		 * random other process while it's still running...
		 */
		struct addrspace *as;

		if (proc == curproc) {
			// Set the current process's address space to NULL
			as = proc_setas(NULL);
			// Deactivate the address space
			as_deactivate();
		}
		else {
			// Clear the process's address space
			as = proc->p_addrspace;
			proc->p_addrspace = NULL;
		}
		// Destroy the address space
		as_destroy(as);
	}

	// Ensure no threads are associated with the process
	KASSERT(proc->p_numthreads == 0);
	// Clean up the spinlock
	spinlock_cleanup(&proc->p_lock);

#if OPT_WAITPID
	// Clean up waitpid-related fields if enabled
	proc_end_waitpid(proc);
#endif

	// Free the process name and structure
	kfree(proc->p_name);
	kfree(proc);
}

/*
 * Create the process structure for the kernel.
 */
void
proc_bootstrap(void)
{
	// Create the kernel process
	kproc = proc_create("[kernel]");
	if (kproc == NULL) {
		// Panic if kernel process creation fails
		panic("proc_create for kproc failed\n");
	}
}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 * @param name - Name of the process to be created.
 * @return Pointer to the created process structure or NULL on failure
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *newproc;

	// Create a new process
	newproc = proc_create(name);
	if (newproc == NULL) {
		return NULL;
	}

	/* VM fields */

	// Initialize the address space to NULL
	newproc->p_addrspace = NULL;

	/* VFS fields */

	/*
	 * Lock the current process to copy its current directory.
	 * (We don't need to lock the new process, though, as we have
	 * the only reference to it.)
	 */
	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		// Increment reference count for the current directory
		VOP_INCREF(curproc->p_cwd);
		// Set the new process's directory
		newproc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);

	return newproc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
	int spl;

	// Ensure the thread is not already associated with a process
	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	// Increment the thread count for the process
	proc->p_numthreads++;
	spinlock_release(&proc->p_lock);

	// Disable interrupts
	spl = splhigh();
	// Associate the thread with the process
	t->t_proc = proc;
	// Restore interrupts
	splx(spl);

	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
void
proc_remthread(struct thread *t)
{
	struct proc *proc;
	int spl;

	// Get the process associated with the thread
	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	// Ensure the process has at least one thread
	KASSERT(proc->p_numthreads > 0);
	// Decrement the thread count
	proc->p_numthreads--;
	spinlock_release(&proc->p_lock);

	// Disable interrupts
	spl = splhigh();
	// Remove the thread's association with the process
	t->t_proc = NULL;
	// Restore interrupts
	splx(spl);
}

/*
 * Fetch the address space of (the current) process.
 *
 * Caution: address spaces aren't refcounted. If you implement
 * multithreaded processes, make sure to set up a refcount scheme or
 * some other method to make this safe. Otherwise the returned address
 * space might disappear under you.
 */
struct addrspace *
proc_getas(void)
{
	// Declare a pointer to hold the address space of the process
	struct addrspace *as;
	// Fetch the current process
	struct proc *proc = curproc;

	if (proc == NULL) {
		return NULL;
	}

	// Acquire the spinlock for the process to ensure thread safety
	spinlock_acquire(&proc->p_lock);
	// Retrieve the address space of the current process
	as = proc->p_addrspace;
	// Release the spinlock after accessing the address space
	spinlock_release(&proc->p_lock);
	// Return the retrieved address space
	return as;
}

/*
 * Change the address space of (the current) process. Return the old
 * one for later restoration or disposal.
 */
struct addrspace *
proc_setas(struct addrspace *newas)
{
	// Declare a pointer to store the old address space
	struct addrspace *oldas;
	// Fetch the current process
	struct proc *proc = curproc;

	KASSERT(proc != NULL);

	// Acquire the spinlock for the process to ensure thread safety
	spinlock_acquire(&proc->p_lock);
	// Store the current address space
	oldas = proc->p_addrspace;
	// Set the new address space for the current process
	proc->p_addrspace = newas;
	// Release the spinlock after modifying the address space
	spinlock_release(&proc->p_lock);
	return oldas;
}


#if OPT_WAITPID


int proc_wait(struct proc *proc){

	// Declare a variable to store the return status
	int return_status;
	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	// Wait for the semaphore associated with the process (block until signaled)
	P(proc -> p_sem);
	// Retrieve the exit status of the process
	return_status = proc->status;
	/* 
	 * destroy the address space of the 
	 * process after getting the exit status
	 */
	proc_destroy(proc);		
	return return_status;

}
#endif