#include <limits.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "mips/trapframe.h"
#include <synch.h>
#include "kern/wait.h"
#include "kern/fcntl.h"
#include <vfs.h>
#include <vm.h>
  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */


int execv(userptr_t program, userptr_t args){
	//count numbers in args, including null terminator
	
	char **cur = (char **)args;
	int i = 0;
	int count = 0;
	while (cur[i] != NULL){
		count++;
		i++;
	}
	//use copyin, len 4, increment args, check if null
	//copy in for program as well
	
//	copyin(program, dest, 4,
	//create a kernel array to store args
	//copy each string into kernArr

	//need to allocate it
	char **kernArr = kmalloc(ARG_MAX);
	for (int i = 0; i < count; i++){
		int len = strlen(cur[i]) + 1;
		kernArr[i] = kmalloc(len);
		copyinstr ((const_userptr_t)cur[i], kernArr[i], len, NULL);
//		kprintf("\n");
	}	

	kernArr[count]= NULL;
	//copy program into kernel
	
	int len = strlen((char *)program) + 1;	
	char * kernProgram = kmalloc(len);
	copyinstr((const_userptr_t)program, kernProgram, len, NULL);
	//runprogram code
	struct vnode *v;
	struct addrspace *as;
	int result;
	vaddr_t entrypoint, stackptr;
	
	struct addrspace* oldAs = curproc_getas();
	result = vfs_open((char *)kernProgram, O_RDONLY, 0, &v);
	
	if (result){
		return result;
	}
	as = as_create();
	if (as ==NULL){
		vfs_close(v);
		return ENOMEM;
	}

	curproc_setas(as);
	as_activate();
	result = load_elf(v, &entrypoint);
	if (result){
		vfs_close(v);
		return result;
	}
	vfs_close(v);

	
	result = as_define_stack(as, &stackptr);
	if (result){
		return result;
	}


	vaddr_t tmpStackPtr = stackptr;
	//copies each word into user stack, incrementing stack pointer by len each time
	for (int i = 0; i < count; i++){
		int len  = ROUNDUP(strlen(kernArr[i]) +1, 4);
		tmpStackPtr -= len;
		result = copyoutstr (kernArr[i], (userptr_t)tmpStackPtr, ARG_MAX, NULL);
		if (result) {
			panic("copyout fail");
		}
		//in the kernArr, each index is given the correct pointer in the user stack	
		kernArr[i] = (char *)tmpStackPtr;
	}

	//copy the kernArr into user stack at the stack pointer location
	int newLen = ROUNDUP((count+1) * 4, 8);
	tmpStackPtr -= newLen;	
	copyout(kernArr,(userptr_t)tmpStackPtr, newLen);
	
	as_destroy(oldAs);
	enter_new_process(count, (userptr_t)tmpStackPtr, tmpStackPtr, entrypoint);
	return 0;

}





int sys_fork(struct trapframe *tf, int32_t* retval){
	//create process
	struct proc *child = proc_create_runprogram(curproc->p_name);

	//copy address space
	struct addrspace* as_child = kmalloc(sizeof(struct addrspace));
	as_copy(curproc->p_addrspace, &as_child);
	spinlock_acquire(&child->p_lock);
	child->p_addrspace = as_child;
	spinlock_release(&child->p_lock);
	array_add(curproc->p_child, child->p_procEntry, NULL);
	child->parentPid = curproc->pid;	
	*retval = child->pid;
	//kprintf("child pid: %d\n", child->pid);
	struct trapframe* tempTF = kmalloc(sizeof(struct trapframe));
	memcpy(tempTF, tf, sizeof(struct trapframe));
	thread_fork(curproc->p_name, child, &enter_forked_process, (void*)tempTF, 5);

	return 0;

}
void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  lock_acquire(procLock);
  int length = array_num(curproc->p_child);
   
  for (int i = 0; i < length; i++){
	
	struct procEntry * curEntry = array_get(curproc->p_child, i);
	if (curEntry->exited){
		int curPid = curEntry->pid;
		reusePid(curPid);
	}else {
		curEntry->parentExit = true;
	}
  }
  cv_signal(curproc->p_procEntry->cv, procLock); 
  curproc->p_procEntry->exitCode = exitcode;
  curproc->p_procEntry->exited = true;
  if(curproc->p_procEntry->parentExit){
	delete_procEntry(curproc->p_procEntry);
	reusePid(curproc->pid);
  }

  lock_release(procLock);
//NEED to free shit u added in proc and procEntry
 
	//FREE SHIT
  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);
   /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;
  
  bool exited = pid_exited(pid);
  exitstatus = _MKWAIT_EXIT(pid_wait(pid, !exited));

	

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

