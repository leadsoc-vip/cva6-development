

#include <stdint.h>
#include <stdio.h>

// CLINT base address (physical) for CV32A60X (RV32)
#define CLINT_BASE          0x02000000UL

// Offsets from CLINT_BASE for the low and high halves of mtime/mtimecmp
#define MTIME_LO_OFFSET     0x0000BFF8  // mtime[31:0]
#define MTIME_HI_OFFSET     0x0000BFFC  // mtime[63:32]
#define MTIMECMP_LO_OFFSET  0x00004000  // mtimecmp[31:0]
#define MTIMECMP_HI_OFFSET  0x00004004  // mtimecmp[63:32]

// Base address for the PLIC — platform-dependent!
#define PLIC_BASE           0x0C000000UL
#define GPIO_BASE           0x10012000UL        

#define PLIC_PRIORITY       (PLIC_BASE + 0x0000)
#define PLIC_PENDING        (PLIC_BASE + 0x1000)
#define PLIC_ENABLE         (PLIC_BASE + 0X2000)
#define PLIC_THRESHOLD      (PLIC_BASE + 0x200000)
#define PLIC_CLAIM          (PLIC_BASE + 0x200004)

//#define GPIO_INPUT          (*(volatile uint32_t *)(GPIO_BASE + 0x00))
//#define GPIO_OUTPUT         (*(volatile uint32_t *)(GPIO_BASE + 0x0C))
//#define GPIO_DIRECTION      (*(volatile uint32_t *)(GPIO_BASE + 0x04))

#define EXTERNAL_IRQ_NUM    7 

#define REG32(p) (*(volatile uint32_t *)(p))

// Pointers to each 32-bit half (RV32 pointers are 32-bit)
static volatile uint32_t* const MTIME_LO    = (uint32_t*)(CLINT_BASE + MTIME_LO_OFFSET);
static volatile uint32_t* const MTIME_HI    = (uint32_t*)(CLINT_BASE + MTIME_HI_OFFSET);
static volatile uint32_t* const MTIMECMP_LO = (uint32_t*)(CLINT_BASE + MTIMECMP_LO_OFFSET);
static volatile uint32_t* const MTIMECMP_HI = (uint32_t*)(CLINT_BASE + MTIMECMP_HI_OFFSET);

extern void _exit(int);         // _exit is implemented in syscalls.c
extern void handle_trap(void); 


// Base address for the PLIC — platform-dependent!
void enable_gpio_interrupt() {
    // 1. Set GPIO interrupt priority to 1 (non-zero)
    REG32(PLIC_PRIORITY + 4 * EXTERNAL_IRQ_NUM) = 1;

    // 2. Enable GPIO interrupt for hart 0 (machine mode)
    REG32(PLIC_ENABLE + 0) = (1 << EXTERNAL_IRQ_NUM);

    // 3. Set threshold to 0 (allow all priorities)
    REG32(PLIC_THRESHOLD) = 0;

    /*
    for (int i = 0; i<100000;i++);
    uint32_t temp = GPIO_OUTPUT | 0x1;
    GPIO_OUTPUT = temp;
    for (int i = 0; i<100000;i++);
    temp = GPIO_OUTPUT & ~0x1;
    GPIO_OUTPUT = temp;
    for (int i = 0; i<10000;i++);
*/
    }

void write_mtimecmp(void){

   uint32_t hi, lo, hi2;
   
   do {
        hi  = *MTIME_HI;   // read upper 32 bits
        lo  = *MTIME_LO;   // read lower 32 bits
        hi2 = *MTIME_HI;   // re-read upper 32 bits
    } while (hi != hi2);
    const uint64_t now = ((uint64_t)hi << 32) | (uint64_t)lo;

    // 2) Choose a tick interval (in cycles)
    const uint64_t TICK_INTERVAL = 100000ULL;
    
    const uint64_t value = now + TICK_INTERVAL;
   

    lo = (uint32_t)( value        & 0xFFFFFFFFUL );
    hi = (uint32_t)((value >> 32) & 0xFFFFFFFFUL );
    
    *MTIMECMP_LO = lo;
    *MTIMECMP_HI = hi;
}


int main(void) {
    enable_gpio_interrupt();
    // 3) Program the first timer interrupt at (now + interval)
    write_mtimecmp();
    
   
    // 4) Enter an infinite loop, waiting for interrupts
    while (1) {
        asm volatile("wfi");  // wait-for-interrupt (low-power stall)
    }
    return 0;
    }
