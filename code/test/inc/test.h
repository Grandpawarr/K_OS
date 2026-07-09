#ifndef __TEST_INC_TEST_H
#define __TEST_INC_TEST_H

#include "assert.h"
#include "fs.h"
#include "ide.h"
#include "interrupt.h"
#include "list.h"
#include "lock.h"
#include "memory.h"
#include "print.h"
#include "printk.h"
#include "stddef.h"
#include "stdio.h"
#include "string.h"
#include "thread.h"
#include "timer.h"

//=========================
// test_print.c
//=========================
void test_put_char(void);
void test_put_str(void);
void test_put_int(void);
void test_set_cursor(void);
void test_cls_screen(void);

//=========================
// test_string.c
//=========================
void test_string(void);

//=========================
// test_bitmap.c
//=========================
void test_bitmap(void);

//=========================
// test_list.c
//=========================
void test_list(void);

//=========================
// test_memory.c
//=========================
void test_memory(bool test_free, bool test_list_flag);

//=========================
// test_interrupt.c
//=========================
void test_intr(void);

//=========================
// test_interrupt.c
//=========================
void test_timer(void);

//=========================
// test_thread.c
//=========================
void test_thread(void);

//=========================
// test_printf.c
//=========================
void test_printk();
void test_concurrency();

//=========================
// test_ide.c
//=========================
void test_ide();

//=========================
// test_fs.c
//=========================
void test_fs();
//=========================
// test_all.c
//=========================
void test_all(void);

#endif
