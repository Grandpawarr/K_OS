#include "test.h"

void test_printk()
{
    int aaa = 9527;
    char *name = "srhuang";
    char c = 'S';
    printk("test_printk:%d, 0x%x, %s, [%c]\n", aaa, aaa, name, c);

    char buf[1024] = {0};
    sprintf(buf, "test_sprintf:%d, 0x%x, %s, [%c]\n",
            aaa, aaa, name, c);

    // test sprintf return
    int buf02[1024] = {0};
    int nums = sprintf(buf02, "the character number is 28\n");
    printk("sprintf return: %d\n", nums);
    put_str(buf);
}

void thread_xxx(void *arg)
{
    uint32_t count = 0;

    while (1)
    {
        printk("Thread %s:%d ", arg, count++);
    }
}

void thread_yyy(void *arg)
{
    uint32_t count = 0;

    while (1)
    {
        printk("Thread %s:%d ", arg, count++);
    }
}

void thread_zzz(void *arg)
{
    uint32_t count = 0;

    while (1)
    {
        printk("Thread %s:%d ", arg, count++);
    }
}

void test_concurrency()
{
    struct task_struct *taskX = kthread_create(thread_xxx, "XXX", "kthreadX");
    struct task_struct *taskY = kthread_create(thread_yyy, "YYY", "kthreadY");
    struct task_struct *taskZ = kthread_create(thread_zzz, "ZZZ", "kthreadZ");

    kthread_run(taskX);
    kthread_run(taskY);
    kthread_run(taskZ);
}