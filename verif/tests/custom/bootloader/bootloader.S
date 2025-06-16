#include "encoding.h"

#if __riscv_xlen == 64
# define LREG ld
# define SREG sd
# define REGBYTES 8
#else
# define LREG lw
# define SREG sw
# define REGBYTES 4
#endif

  .section ".text.init"
  .globl _start
_start:

    # Set stack pointer
    la sp, _stack_top

    //Lines Added
    # Enable UART RX interrupt (IER at 0x1001_0001)
    li t0, 0x10010001
    li t1, 0x01
    sb t1, 0(t0)
   //Lines Ended

    #Set mtvec to our trap handler
    la t0, handle_trap
    csrw mtvec, t0

    # Enable External interrupts
    li t0, (1 << 11) 
    csrs mie, t0
    
    # Enable Timer interrupts
    li t0, (1 << 7)           
    csrs mie, t0
    
    # Enable Global M-node interrupts
    li t0, (1 << 3)
    csrs mstatus, t0

    # Jump to main
    call main

    # Infinite loop if main ever returns
hang:
    j hang
 .size    _start, . - _start
 .align 2   
  //////  
    .data               # Data section for global/static variables
    .global count       # Make the symbol visible to other files
    .align 8            # Align on 8-byte boundary (optional but good)
count:
    .quad 0             # Define a 64-bit global variable initialized to 0
 
  .align 2

 .type   handle_trap, @function
handle_trap:
    /* b) Move mcause→a0, mepc→a1 for the C handler */
    csrr a0, mcause
    csrr a1, mepc

    //uart_send
    //la   t0, count
    //lw   t1, 0(t0)
    //li   t2, 0x10000000
    //sb   t1,0(t2)

    li t0, 0x80000007       # Load expected mcause value for timer interrupt
    li t1, 0x8000000b       # Load expected mcause value for external interrupt
    beq a0, t0, time_isr    # If interrupt is time
    beq a0, t1, extern_isr  # If interrupt is external
    mret
    
time_isr:
    la t1, count            # Load address of global count
    lw t2, 0(t1)            # Load count
    addi t2, t2, 1          # Increment count
    li t3,0x10000000        # UART_ADDRESS
    sb t2,0(t3)             # Write count in UART
    sw t2, 0(t1)            # Store back incremented count
    call write_mtimecmp
skip: 
     li t3, 5
     bge t2, t3, disable_timer       # If count >= 5, exit

    mret
    
disable_timer:
    
    # Disable global machine-mode external interrupt (MTIE bit in MIE)
    li t0, (1<<11)
    csrc mie, t0

    # Disable global machine-mode timer interrupt (MTIE bit in MIE)
    li t0, (1<<7)
    csrc mie, t0

    # Disable global machine-mode interrupt (MIE bit in mstatus)
    li t0, (1<<3)
    csrc mstatus, t0
    
    j _exit

extern_isr:
    

    li a0, 0x80000009
    # Read interrupt ID from PLIC
    li t0, 0x0C201004   # PLIC_CLAIM
    lw t1, 0(t0)
    
    li t2, 7
    
    bne t1,t2,e_skip


    
    li t3,0x10000000        # UART_ADDRESS

    li t4,9    
    sb t4,0(t3)             # Write 'E' (External Interrupt) in UART
    
    //li t5, 0x10012000   # GPIO base
    //lw t6, 0(t5)        # Read switch state

   // sb t6,0(t3)
   // sb t4,0(t3)

    # Clear interrupt by writing back
    sw t1, 0(t0)
e_skip:    

    mret



  .weak   _init
  .type   _init, @function
_init:
  jal     main
  j       _exit
  .size   _init, . - _init

  // Minimal low-level exit function.
  .global  _exit
_exit:
  li  a0, 1      # Set up exit code: shift a0 left by 1
  la  t5,tohost  # and set bit 0.
  sw   a0, 0(t5) # Store bits [31:0] of a0 into variable tohost
  nop            # Leave one cycle of slack before entering endless loop
1:
  j        1b;        # Wait indefinitely (until the env terminates the simulation)
  .size    _exit, . - _exit

.section ".tohost","aw",@progbits
.align 6
.globl tohost
tohost: .dword 0
.align 6
.globl fromhost
fromhost: .dword 0

.section ".uvmif","aw",@progbits
# Alignment is 2**6 == 64 bytes
.align 6
.globl int_ack
int_ack: .dword 0
