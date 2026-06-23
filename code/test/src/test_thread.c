#include "test.h"

void thread_aaa(void *arg)
{
    uint32_t idx = 0;
    while (1)
    {
        msleep(1000);

        set_cursor(160);
        put_str(arg);
        put_str(":0x");
        put_int(idx++);
        put_str(" ");
    }
}

void thread_bbb(void *arg)
{
    uint32_t idx = 0;
    while (1)
    {
        msleep(100);

        set_cursor(240);
        put_str(arg);
        put_str(":0x");
        put_int(idx++);
        put_str(" ");
    }
}

void thread_ccc(void *arg)
{
    uint32_t idx = 0;
    while (1)
    {
        msleep(10);

        set_cursor(320);
        put_str(arg);
        put_str(":0x");
        put_int(idx++);
        put_str(" ");
    }
}

void thread_ddd(void *arg)
{
    uint32_t idx = 0;
    while (1)
    {
        usleep(1000);

        set_cursor(400);
        put_str(arg);
        put_str(":0x");
        put_int(idx++);
        put_str(" ");
    }
}

void thread_eee(void *arg)
{
    uint32_t idx = 0;
    while (1)
    {
        usleep(100);

        set_cursor(480);
        put_str(arg);
        put_str(":0x");
        put_int(idx++);
        put_str(" ");
    }
}

void thread_fff(void *arg)
{
    uint32_t idx = 0;
    while (1)
    {
        usleep(10);

        set_cursor(560);
        put_str(arg);
        put_str(":0x");
        put_int(idx++);
        put_str(" ");
    }
}

void thread_idle(void *arg)
{
    while (1)
    {
        msleep(1000);
        kthread_unblock(g_idle_task);
    }
}

void test_thread()
{
    struct task_struct *taskA = kthread_create(thread_aaa, "A", "kthreadA");
    struct task_struct *taskB = kthread_create(thread_bbb, "B", "kthreadB");
    struct task_struct *taskC = kthread_create(thread_ccc, "C", "kthreadC");
    struct task_struct *taskD = kthread_create(thread_ddd, "D", "kthreadD");
    struct task_struct *taskE = kthread_create(thread_eee, "E", "kthreadE");
    struct task_struct *taskF = kthread_create(thread_fff, "F", "kthreadF");

    // test block/unblock
    struct task_struct *task_idle =
        kthread_create(thread_idle, "IDLE", "kthreadI");

    cls_screen();
    kthread_run(taskA);
    kthread_run(taskB);
    kthread_run(taskC);
    kthread_run(taskD);
    kthread_run(taskE);
    kthread_run(taskF);
    kthread_run(task_idle);
}