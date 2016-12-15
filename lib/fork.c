// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	envid_t id = sys_getenvid();
        if(!(uvpt[PGNUM(addr)] & PTE_COW)) panic("fuck!");
        
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	// LAB 4: Your code here.
        addr = (void*)ROUNDDOWN((uintptr_t)addr, PGSIZE);
        if(sys_page_alloc(id, PFTEMP, PTE_W|PTE_U|PTE_P)) panic("alloc fail");
        memcpy(PFTEMP, addr, PGSIZE);
        sys_page_map(id, PFTEMP, id, addr, PTE_W|PTE_U|PTE_P);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	envid_t id = sys_getenvid();
	
        void* addr = (void *)(pn*PGSIZE);

        if(!(uvpt[PGNUM(addr)] & PTE_P)) return -1;
        if(uvpt[PGNUM(addr)] & PTE_SHARE) {
                sys_page_map(id, addr, envid, addr, PTE_P|PTE_W|PTE_U|PTE_SHARE);
                return 0;
        }
        if((uvpt[PGNUM(addr)] & PTE_W) || (uvpt[PGNUM(addr)] & PTE_COW)) {
                sys_page_map(id, addr, envid, addr, PTE_U|PTE_COW|PTE_P);
                sys_page_map(id, addr, id, addr, PTE_U|PTE_COW|PTE_P);
        } else {
                sys_page_map(id, addr, envid, addr, PTE_U|PTE_P);
        }
	
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	envid_t cp = sys_exofork();
	envid_t id = sys_getenvid();
	thisenv = envs + ENVX(sys_getenvid());
	if(!cp) return 0;
	if(sys_env_set_status(cp, ENV_NOT_RUNNABLE))
	        panic("set status fail");
	uintptr_t addr = 0;
	for(;addr < UTOP - PGSIZE; addr += PGSIZE) {
	        if(uvpd[PDX(addr)] && uvpt[PGNUM(addr)])
	                duppage(cp, addr/PGSIZE);
	}
	if(sys_page_alloc(cp, (void*)(UXSTACKTOP-PGSIZE), PTE_U|PTE_W|PTE_P))
	        panic("UXSTACKTOP alloc fail");
        if(sys_env_set_pgfault_upcall(cp, thisenv->env_pgfault_upcall))
                panic("set upcall fail");
	if(sys_env_set_status(cp, ENV_RUNNABLE))
	        panic("set status fail");
        return cp;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
