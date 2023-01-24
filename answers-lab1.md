## Part 2: The Boot Loader
1. At what point does the processor start executing 32-bit code? What exactly causes the switch from 16- to 32-bit mode?
(1) `ljmp    $PROT_MODE_CSEG, $protcseg`
(2) See the code below
```asm
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE_ON, %eax
  movl    %eax, %cr0
```

2. What is the last instruction of the boot loader executed, and what is the first instruction of the kernel it just loaded?
(1) `((void (*)(void)) (ELFHDR->e_entry))();`, i.e., call *0x10018. *0x10018 is the entry point of the kernel (at 0x10000c). 
(2) `movw   $0x1234,0x472`

3. Where is the first instruction of the kernel?
At 0x10000c

4. How does the boot loader decide how many sectors it must read in order to fetch the entire kernel from disk? Where does it find this information?
The information in ELFHDR

5. Reset the machine (exit QEMU/GDB and start them again). Examine the 8 words of memory at 0x00100000 at the point the BIOS enters the boot loader, and then again at the point the boot loader enters the kernel. Why are they different? What is there at the second breakpoint? (You do not really need to use QEMU to answer this question. Just think.)
Kernel code is loaded at 0x00100000 before entering the entry.

6. What is the first instruction after the new mapping is established that would fail to work properly if the mapping weren't in place? Comment out the movl %eax, %cr0 in kern/entry.S, trace into it, and see if you were right.
`=> 0x10002a:    jmp    *%eax`. Since *%eax is not mapped correctly.

## Part 3: Kernel
1. Explain the interface between printf.c and console.c. Specifically, what function does console.c export? How is this function used by printf.c?
console.c exports the following functions:

```c
void cputchar(int c);
int	getchar(void);
int	iscons(int fd);
```

printf.c uses these functions as basic I/O functinons.

2. Explain the following from console.c:
```c
1      if (crt_pos >= CRT_SIZE) {
2              int i;
3              memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
4              for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
5                      crt_buf[i] = 0x0700 | ' ';
6              crt_pos -= CRT_COLS;
7      }
```
scroll up the screen by one line when the cursor is at the bottom of the screen.

3. For the following questions you might wish to consult the notes for Lecture 2. These notes cover GCC's calling convention on the x86.
Trace the execution of the following code step-by-step:

```c
int x = 1, y = 3, z = 4;
cprintf("x %d, y %x, z %d\n", x, y, z);
```

- In the call to `cprintf()`, to what does `fmt` point? To what does ap point?
(1) "x %d, y %x, z %d\n"
(2) ap points to the first argument of `cprintf`, i.e., x.

- List (in order of execution) each call to `cons_putc`, `va_arg`, and `vcprintf`. For `cons_putc`, list its argument as well. For `va_arg`, list what `ap` points to before and after the call. For `vcprintf` list the values of its two arguments.

4. Run the following code.

```c
    unsigned int i = 0x00646c72;
    cprintf("H%x Wo%s", 57616, &i);
```

What is the output? Explain how this output is arrived at in the step-by-step manner of the previous exercise. Here's an ASCII table that maps bytes to characters.

(1) He110 World (2) The hex value of 57616 is 0xE110. While i is stored in some address, &i points to the address of i, thus %s will print the string starting from the address of i, i.e., 0x0064(d)6c(l)72(r)

The output depends on that fact that the x86 is little-endian. If the x86 were instead big-endian what would you set i to in order to yield the same output? Would you need to change 57616 to a different value?

(1) 0x726c6400 (2) No, 57616 is still 0xE110.

5. In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?
```c
    cprintf("x=%d y=%d", 3);
```

6. Let's say that GCC changed its calling convention so that it pushed arguments on the stack in declaration order, so that the last argument is pushed last. How would you have to change cprintf or its interface so that it would still be possible to pass it a variable number of arguments?

7. Determine where the kernel initializes its stack, and exactly where in memory its stack is located. How does the kernel reserve space for its stack? And at which "end" of this reserved area is the stack pointer initialized to point to?

(1) In the entry.S
```asm
movl	$(bootstacktop),%esp
```

(2) 0xf0117000

(3) See the code below
```asm
bootstack:
	.space		KSTKSIZE
	.globl		bootstacktop   
```

(4) The stack pointer is initialized to point to the bottom (or top, since the stack is grown down) of the stack.