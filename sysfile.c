//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "memlayout.h"

// Lottery scheduling
#include "processesinfo.h"

// Count the write
int write_count = 0;

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
	int fd;
	struct file *f;

	if(argint(n, &fd) < 0)
		return -1;
	if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
		return -1;
	if(pfd)
		*pfd = fd;
	if(pf)
		*pf = f;
	return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
	int fd;
	struct proc *curproc = myproc();

	for(fd = 0; fd < NOFILE; fd++){
		if(curproc->ofile[fd] == 0){
		curproc->ofile[fd] = f;
		return fd;
		}
	}
	return -1;
}

int
sys_dup(void)
{
	struct file *f;
	int fd;

	if(argfd(0, 0, &f) < 0)
		return -1;
	if((fd=fdalloc(f)) < 0)
		return -1;
	filedup(f);
	return fd;
}

int
sys_read(void)
{
	struct file *f;
	int n;
	char *p;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
		return -1;
	return fileread(f, p, n);
}

int
sys_write(void)
{
	struct file *f;
	int n;
	char *p;

	write_count++;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
		return -1;
	return filewrite(f, p, n);
}

int
sys_setwritecount(void)
{
	int arg = 0;
	if (argint(0, &arg) != 0)
		return -1;
	else
		write_count = arg;
	return (write_count == arg)? 0: -1;
}

int
sys_writecount(void)
{
	return write_count;
}

int
sys_close(void)
{
	int fd;
	struct file *f;

	if(argfd(0, &fd, &f) < 0)
		return -1;
	myproc()->ofile[fd] = 0;
	fileclose(f);
	return 0;
}

int
sys_fstat(void)
{
	struct file *f;
	struct stat *st;

	if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
		return -1;
	return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
	char name[DIRSIZ], *new, *old;
	struct inode *dp, *ip;

	if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
		return -1;

	begin_op();
	if((ip = namei(old)) == 0){
		end_op();
		return -1;
	}

	ilock(ip);
	if(ip->type == T_DIR){
		iunlockput(ip);
		end_op();
		return -1;
	}

	ip->nlink++;
	iupdate(ip);
	iunlock(ip);

	if((dp = nameiparent(new, name)) == 0)
		goto bad;
	ilock(dp);
	if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
		iunlockput(dp);
		goto bad;
	}
	iunlockput(dp);
	iput(ip);

	end_op();

	return 0;

	bad:
	ilock(ip);
	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);
	end_op();
	return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
	int off;
	struct dirent de;

	for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
		if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
		panic("isdirempty: readi");
		if(de.inum != 0)
		return 0;
	}
	return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
	struct inode *ip, *dp;
	struct dirent de;
	char name[DIRSIZ], *path;
	uint off;

	if(argstr(0, &path) < 0)
		return -1;

	begin_op();
	if((dp = nameiparent(path, name)) == 0){
		end_op();
		return -1;
	}

	ilock(dp);

	// Cannot unlink "." or "..".
	if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
		goto bad;

	if((ip = dirlookup(dp, name, &off)) == 0)
		goto bad;
	ilock(ip);

	if(ip->nlink < 1)
		panic("unlink: nlink < 1");
	if(ip->type == T_DIR && !isdirempty(ip)){
		iunlockput(ip);
		goto bad;
	}

	memset(&de, 0, sizeof(de));
	if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
		panic("unlink: writei");
	if(ip->type == T_DIR){
		dp->nlink--;
		iupdate(dp);
	}
	iunlockput(dp);

	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);

	end_op();

	return 0;

	bad:
	iunlockput(dp);
	end_op();
	return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
	struct inode *ip, *dp;
	char name[DIRSIZ];

	if((dp = nameiparent(path, name)) == 0)
		return 0;
	ilock(dp);

	if((ip = dirlookup(dp, name, 0)) != 0){
		iunlockput(dp);
		ilock(ip);
		if(type == T_FILE && ip->type == T_FILE)
		return ip;
		iunlockput(ip);
		return 0;
	}

	if((ip = ialloc(dp->dev, type)) == 0)
		panic("create: ialloc");

	ilock(ip);
	ip->major = major;
	ip->minor = minor;
	ip->nlink = 1;
	iupdate(ip);

	if(type == T_DIR){  // Create . and .. entries.
		dp->nlink++;  // for ".."
		iupdate(dp);
		// No ip->nlink++ for ".": avoid cyclic ref count.
		if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
		panic("create dots");
	}

	if(dirlink(dp, name, ip->inum) < 0)
		panic("create: dirlink");

	iunlockput(dp);

	return ip;
}

int
sys_open(void)
{
	char *path;
	int fd, omode;
	struct file *f;
	struct inode *ip;

	if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
		return -1;

	begin_op();

	if(omode & O_CREATE){
		ip = create(path, T_FILE, 0, 0);
		if(ip == 0){
		end_op();
		return -1;
		}
	} else {
		if((ip = namei(path)) == 0){
		end_op();
		return -1;
		}
		ilock(ip);
		if(ip->type == T_DIR && omode != O_RDONLY){
		iunlockput(ip);
		end_op();
		return -1;
		}
	}

	if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
		if(f)
		fileclose(f);
		iunlockput(ip);
		end_op();
		return -1;
	}
	iunlock(ip);
	end_op();

	f->type = FD_INODE;
	f->ip = ip;
	f->off = 0;
	f->readable = !(omode & O_WRONLY);
	f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
	return fd;
}

int
sys_mkdir(void)
{
	char *path;
	struct inode *ip;

	begin_op();
	if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
		end_op();
		return -1;
	}
	iunlockput(ip);
	end_op();
	return 0;
}

int
sys_mknod(void)
{
	struct inode *ip;
	char *path;
	int major, minor;

	begin_op();
	if((argstr(0, &path)) < 0 ||
		argint(1, &major) < 0 ||
		argint(2, &minor) < 0 ||
		(ip = create(path, T_DEV, major, minor)) == 0){
		end_op();
		return -1;
	}
	iunlockput(ip);
	end_op();
	return 0;
}

int
sys_chdir(void)
{
	char *path;
	struct inode *ip;
	struct proc *curproc = myproc();
	
	begin_op();
	if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
		end_op();
		return -1;
	}
	ilock(ip);
	if(ip->type != T_DIR){
		iunlockput(ip);
		end_op();
		return -1;
	}
	iunlock(ip);
	iput(curproc->cwd);
	end_op();
	curproc->cwd = ip;
	return 0;
}

int
sys_exec(void)
{
	char *path, *argv[MAXARG];
	int i;
	uint uargv, uarg;

	if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
		return -1;
	}
	memset(argv, 0, sizeof(argv));
	for(i=0;; i++){
		if(i >= NELEM(argv))
		return -1;
		if(fetchint(uargv+4*i, (int*)&uarg) < 0)
		return -1;
		if(uarg == 0){
		argv[i] = 0;
		break;
		}
		if(fetchstr(uarg, &argv[i]) < 0)
		return -1;
	}
	return exec(path, argv);
}

int
sys_pipe(void)
{
	int *fd;
	struct file *rf, *wf;
	int fd0, fd1;

	if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
		return -1;
	if(pipealloc(&rf, &wf) < 0)
		return -1;
	fd0 = -1;
	if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
		if(fd0 >= 0)
			myproc()->ofile[fd0] = 0;
		fileclose(rf);
		fileclose(wf);
		return -1;
	}
	fd[0] = fd0;
	fd[1] = fd1;
	return 0;
}

// Lottery Scheduling System Calls
int sys_settickets(void)
{
	int number = 0;
	if(argint(0, &number) < 0)
		return -1;
	// Get the current process
	struct proc *p = myproc();
	if((number < 1) || (number > 100000))
		// The number is too big or too small
		return -1;
	// Set the number of tickets
	p->tickets = number;
	// Return success
	return 0;
}

extern struct {
	struct spinlock lock;
	struct proc proc[NPROC];
} ptable;

int sys_getprocessesinfo(void)
{
	struct processes_info *p;
	// Get the argument from userspace
	if(argptr(0, (char **)&p, sizeof(struct processes_info)) < 0)
		return -1;
	// Read the process table and fill the struct p with information
	struct proc *curproc;
	// Check if the pointer is valid
	if(p == 0)
		return -1;
	// Lock the process table
	acquire(&ptable.lock);
	// Zero memory for the number of processes
	p->num_processes = 0;
	// Loop through the process table
	int i;
	for(i = 0; i < NPROC; i++)
	{
		// Get the ith process
		curproc = &ptable.proc[i];
		// Skip if unused; don't know if skipping over ZOMBIE is necessary or even a good idea
		if((curproc->state == UNUSED)) {
			p->pids[i] = -1;
			continue;
		}
		// Copy the information to the struct
		p->pids[i] = curproc->pid;
		p->tickets[i] = curproc->tickets;
		p->times_scheduled[i] = curproc->times_scheduled;
		p->num_processes++;
	}
	// Release the lock
	release(&ptable.lock);
	// Return success
	return 0;
}

// Walk the page table to find the physical address of the virtual address
extern pte_t * walkpgdir_wrap(pde_t *pgdir, const void *va, int alloc);

// Paging system calls
int sys_getpagetableentry(void) {
	// Get the arguments from userspace
	int pid = 0;
	uint address = 0;
	if(argint(0, &pid) < 0 || argint(1, (int *)&address) < 0) {
		return -1;
	}
	struct proc *p;
	pte_t *pte;
	// cprintf("pid: %d, address: %d\r\n", pid, address);
	// Acquire the ptable lock
	acquire(&ptable.lock);
	// cprintf("acquired lock\r\n");
	// Loop through the process table
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		// Check if the process is the one we're looking for
		if((p->pid == pid) && (p->state != UNUSED)) {
			// Release the lock
			release(&ptable.lock);
			// Get the page table entry
			pte = walkpgdir_wrap(p->pgdir, (void *)address, 0);
			// cprintf("pte: %d", *pte);
			return *pte;
		}
	}
	// Release the lock
	release(&ptable.lock);
	// Failure; pid doesn't exist
	return 0;
}

extern struct {
	struct spinlock lock;
	int use_lock;
	struct run *freelist;
} kmem;

int sys_isphysicalpagefree(void) {
	// Get the argument from userspace
	int page = 0;
	if(argint(0, &page) < 0) {
		return -1;
	}
	// Lock if necessary
	if(kmem.use_lock)
		acquire(&kmem.lock);
	// Check if the page is free by checking the free list in kalloc.c
	struct run *r = kmem.freelist;
	while(r) {
		if(V2P(r) == (page << 12)) {
			// Release the lock
			if(kmem.use_lock)
				release(&kmem.lock);
			// Page is free
			return 1;
		}
		r = r->next;
	}
	// Release the lock
	if(kmem.use_lock)
		release(&kmem.lock);
	// Page is not free
	return 0;
}

int sys_dumppagetable(void) {
	// Get the argument from userspace
	int *pid = NULL;
	if(argint(0, pid) < 0) {
		return -1;
	}
	struct proc *p;
	pte_t *pte;
	// Acquire the ptable lock
	acquire(&ptable.lock);
	// Loop through the process table
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		// Check if the process is the one we're looking for
		if(p->pid == *pid) {
			// Release the lock
			release(&ptable.lock);
			// Start the printing
			cprintf("START PAGE TABLE; PID %d:\r\n", *pid);
			cprintf("===================================================================================================\r\n");
			cprintf("Page Table Entry\t\tPhysical Address\t\tWritable\t\tUser Mode\r\n");
			// Loop through the page table
			uint i;
			for(i = 0; i < p->sz; i += PGSIZE) {
				// Get the page table entry
				if((pte = walkpgdir_wrap(p->pgdir, (void*)i, 0)) == 0) {
					// The walk failed for a valid virtual address
					// Quitting with -1 should be fine, but I'm not doing it
					cprintf("%d !!! WALKPAGE FAILED !!!\r", (int)i/PGSIZE);
					continue;
				} else if (!(*pte & PTE_P)) {
					// If the page isn't present, skip it
					continue;
				} else {
					// At this point, we have a valid page table entry
					// Print the virtual address
					cprintf("   %x\t\t", *pte);
					// Print the physical address
					cprintf("\t   %x\t\t", PTE_ADDR(*pte));
					// Print the writable bit
					if (PTE_FLAGS(*pte) & PTE_W) {
						cprintf("\tYes\t\t\t");
					} else {
						cprintf("\tNo\t\t\t");
					}
					// Print the user mode bit
					if (PTE_FLAGS(*pte) & PTE_U) {
						cprintf("Yes\r\n");
					} else {
						cprintf("No\r\n");
					}
				}
			}
			cprintf("===================================================================================================\r\n");
			cprintf("END PAGE TABLE; PID %d:\r\n", *pid);
			// Return success
			return 0;
		}
	}
	// Release the lock
	release(&ptable.lock);
	// Print an error message
	cprintf("Error: PID %d does not exist\r\n", *pid);
	// Failure; pid doesn't exist
	return -1;
}