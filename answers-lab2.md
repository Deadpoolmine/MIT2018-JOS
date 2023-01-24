1. It should be `uintptr_t` since it is a pointer to an address. 

2. What entries (rows) in the page directory have been filled in at this point? What addresses do they map and where do they point? In other words, fill out this table as much as possible:

Entry	        Base Virtual Address	        Points to (logically):
1023	        0xFFC00000                      Page table for top 4MB of phys memory
1022	        0xFF800000                      Kernel code
.	            ?	                            ?
.	            ?	                            ?
.	            ?	                            ?
2	            0x00800000	                    
1	            0x00400000	                    
0	            0x00000000	                    

The solution in https://www.cnblogs.com/gatsby123/p/9832223.html can be used to answer this question. Except for the arrow from ULIM to end should be UVPT to end.

3. We have placed the kernel and user environment in the same address space. Why will user programs not be able to read or write the kernel's memory? What specific mechanisms protect the kernel memory?
(1) Permission bits.
(2) The permission bits are not set to PTE_U, so the user program cannot access the kernel memory.

4. What is the maximum amount of physical memory that this operating system can support? Why?
(1) 32093 + 159 physical memory pages can be used.
(2) In total we have 32768 physical memory pages, and we have already used 516 pages for memory management, page tables for mapping kernel space (256MB) to 0, page tables for mapping KSTACK to bootstack, and page tables for mapping pages to UPAGES. Thus, only 32093 + 159 physical memory pages are left for user programs.

5. How much space overhead is there for managing memory, if we actually had the maximum amount of physical memory? How is this overhead broken down?
To manage memory, (1) we should allocate `pages` structure, each PageInfo is 16B, thus there are 1MB phsical memory should be reaserved. (2) we need at most 1 page directory and 32 page tables for addressing 128MiB memory. Thus, 33 * 4 = 132 KB memory is needed.

6. Revisit the page table setup in kern/entry.S and kern/entrypgdir.c. Immediately after we turn on paging, EIP is still a low number (a little over 1MB). At what point do we transition to running at an EIP above KERNBASE? What makes it possible for us to continue executing at a low EIP between when we enable paging and when we begin running at an EIP above KERNBASE? Why is this transition necessary?

(1) After running:
```asm
	mov	$relocated, %eax
	jmp	*%eax
```

(2) The reason is that we have already set up the page table for the kernel, so we can still access the kernel memory. See the code below:
```c
__attribute__((__aligned__(PGSIZE)))
pde_t entry_pgdir[NPDENTRIES] = {
	// Map VA's [0, 4MB) to PA's [0, 4MB)
	[0]
		= ((uintptr_t)entry_pgtable - KERNBASE) + PTE_P,
	// Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
	[KERNBASE>>PDXSHIFT]
		= ((uintptr_t)entry_pgtable - KERNBASE) + PTE_P + PTE_W
};
```

(3) This transition is necessary because we need to set up the page table for the kernel. And if we do not transition, we would touch the code that does not belong to kernel.