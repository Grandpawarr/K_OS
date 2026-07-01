#include "init.h"
#include "ide.h"
#include "interrupt.h"
#include "memory.h"
#include "printk.h"
#include "syscall_sys.h"
#include "thread.h"
#include "timer.h"

void kernel_init() {
    mem_init();
    intr_init();
    syscall_init();
    timer_init();
    kthread_init();
    printk_init();

    // enable interrupt
    intr_set_status(true);

    ide_init();
}