// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

extern pde_t *kern_pgdir;

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a stack backtrace", mon_backtrace },
	{ "showmappings", "Display the page mappings", mon_showmappings },
	{ "setperm", "Set the permissions of a page", mon_setperm },
	{ "dump", "Dump the contents of the given memeory range (either PA or VA)", mon_dump },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

/*********************
 * Run gdb you can figure out the stack frame layout.
 * 
 * 1. when entering the caller function, CPU minuses 20 bytes from esp. These space is used to store the arguments.
 * 2. `call` will automatically push the return address to the stack.
 * 3. when entering the callee function, CPU pushes caller's %ebp to the stack.
 * 4. Now, %ebp is updated by assigning current %esp. 
 * 
 * Thus, the stack frame of a function looks like this:
 * 
 * +-----------------+
 * | 4 bytes (arg 5) |
 * +-----------------+
 * | 4 bytes (arg 4) |
 * +-----------------+
 * | 4 bytes (arg 3) |
 * +-----------------+
 * | 4 bytes (arg 2) |
 * +-----------------+
 * | 4 bytes (arg 1) |
 * +-----------------+
 * | 4 bytes (EIP)   |
 * +-----------------+
 * | 4 bytes (EBP)   | (Caller's)
 * +-----------------+ <---------- %esp
 * 
 * We trace from %esp back to the caller's %ebp, and then trace from the caller's %ebp to the caller's caller's %ebp, and so on, until 0 is reached (which is defined in entry.S).
 */

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	struct Eipdebuginfo info;
	uint32_t ebp = read_ebp();
	int i, ret;

	cprintf("Stack backtrace:\n");
	while (ebp != 0) {
		uint32_t eip = *((uint32_t *)ebp + 1);
		cprintf("  ebp %08x  eip %08x  args", ebp, eip);
		for (i = 2; i < 7; i++) {
			cprintf(" %08x", *((uint32_t *)ebp + i));
		}
		cprintf("\n");

		ret = debuginfo_eip(eip, &info);
		if (ret) {
			return ret;
		}
		cprintf("         %s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, eip - info.eip_fn_addr);

		ebp = *((uint32_t *)ebp);
	}

	return 0;
}

static pde_t *
get_cur_pgdir(int print)
{
	uint32_t cr3 = rcr3();
	physaddr_t pgdir_pa = cr3;
	pde_t *pgdir = (pde_t *)KADDR(pgdir_pa);
	char hints[100] = {0};
	memset(hints, 0, sizeof(hints));
	if (print) {
		if (pgdir_pa == PADDR(kern_pgdir))
			strcat(hints, "kernel");
		else
			strcat(hints, "unknown");
		cprintf("Current Page Directory: %08p (%s)\n", pgdir_pa, hints);
	}
	return pgdir;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	pde_t *pgdir = get_cur_pgdir(1);
	void *va, *va_start, *va_end;
	
	if (argc != 3)
		return -1;
	
	va_start = (void *) strtol(argv[1], NULL, 16);
	va_end = (void *) strtol(argv[2], NULL, 16);
	
	for (va = va_start; va <= va_end; va += PGSIZE) {
		pte_t *pte = pgdir_walk(pgdir, va, 0);
		if (pte == NULL) {
			cprintf("    va %08p: not mapped \n", va);
		} else {
			cprintf("    va %08x: pte %08p, pa %08p\n", va,* pte, PTE_ADDR(*pte));
		}
	}
	return 0;	
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
	pde_t *pgdir = get_cur_pgdir(1);
	void *va;
	int perm;
	if (argc != 3)
		return -1;
	
	va = (void *)strtol(argv[1], NULL, 16);
	perm = strtol(argv[2], NULL, 16);
	
	pte_t *pte = pgdir_walk(pgdir, va, 0);
	if (pte == NULL) {
		cprintf("    va %08p: not mapped \n", va);
	} else {
		cprintf("    before set: va %08p: pte %08p, pa %08p\n", va, *pte, PTE_ADDR(*pte));
		*pte = PTE_ADDR(*pte) | perm;
		cprintf("    after  set: va %08p: pte %08p, pa %08p\n", va, *pte, PTE_ADDR(*pte));
	}
	return 0;
}

/* dump va 0xF0000000 0xF0000000 */
int
mon_dump(int argc, char **argv, struct Trapframe *tf)
{
	pde_t *pgdir = get_cur_pgdir(1);
	char *type;
	void *addr, *addr_start, *addr_end;
	
	if (argc != 4)
		return -1;
	
	type = argv[1];
	addr_start = (void *) strtol(argv[2], NULL, 16);
	addr_end = (void *) strtol(argv[3], NULL, 16);

	if (memcmp(type, "va", 2) == 0) {
		for (addr = addr_start; addr <= addr_end; addr += 4) {
			cprintf("    va %08p: %02x %02x %02x %02x\n", addr, *((uint8_t *)addr), *((uint8_t *)addr + 1), *((uint8_t *)addr + 2), *((uint8_t *)addr + 3));
		}
	} else if (memcmp(type, "pa", 2) == 0) {
		for (addr = addr_start; addr <= addr_end; addr += 4) {
			cprintf("    pa %08p: %02x %02x %02x %02x\n", addr, *((uint8_t *)KADDR((physaddr_t)addr)), *((uint8_t *)KADDR((physaddr_t)addr) + 1), *((uint8_t *)KADDR((physaddr_t)addr) + 2), *((uint8_t *)KADDR((physaddr_t)addr) + 3));
		}
	} else {
		return -1;
	}

	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
