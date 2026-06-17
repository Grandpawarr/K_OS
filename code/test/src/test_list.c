#include "stdbool.h"
#include "list.h"
#include "stddef.h"
#include "print.h"

struct test_item
{
    int id;
    struct list_elem tag;
};

static bool check_id(struct list_elem *elem, int arg)
{
    struct test_item *item = elem2entry(struct test_item, tag, elem);
    return item->id == arg;
}

void test_list(void)
{
    struct list all_tag;
    struct test_item node1, node2, node3, node4, node5;
    struct list_elem *get_tag;
    struct test_item *get_item;

    node1.id = 0x111;
    node2.id = 0x222;
    node3.id = 0x333;
    node4.id = 0x444;
    node5.id = 0x555;

    put_str("test list_init\n");
    list_init(&all_tag);

    //*
    put_str("test list_insert\n");
    list_insert(&all_tag.tail, &node1.tag);
    //*/

    /*
    put_str("test list_remove\n");
    list_remove(&node1.tag);
    //*/

    /*
    put_str("test list_append\n");
    list_append(&all_tag, &node2.tag);
    //*/

    /*
    put_str("test list_push\n");
    list_push(&all_tag, &node3.tag);
    list_push(&all_tag, &node4.tag);
    //*/

    /*
    put_str("test list_pop\n");
    get_tag = list_pop(&all_tag);
    put_str("pop tag: ");
    if (NULL == get_tag) {
        put_str("NULL");
    } else {
        get_item = elem2entry(struct test_item, tag, get_tag);
        put_int(get_item->id);
    }
    put_str("\n");
    //*/

    /*
    put_str("test list_empty\n");
    if(list_empty(&all_tag)) {
        put_str("list is empty.\n");
    } else {
        put_str("list is NOT empty.\n");
    }
    //*/

    /*
    put_str("test list_len\n");
    int len = list_len(&all_tag);
    put_str("list length: ");
    put_int(len);
    put_str("\n");
    //*/

    /*
    put_str("test list_find\n");
    if (list_find(&all_tag, &node3.tag)) {
        put_str("Found.\n");
    } else {
        put_str("NOT Found.\n");
    }
    //*/

    /*
    put_str("test list_traversal\n");
    get_tag = list_traversal(&all_tag, check_id, 0x333);
    if (NULL == get_tag) {
        put_str("NOT Found\n");
    } else {
        get_item = elem2entry(struct test_item, tag, get_tag);
        put_str("Found: ");
        put_int(get_item->id);
        put_str("\n");
    }
    //*/

    /* Print all list elements */
    struct list_elem *cur_elem = all_tag.head.next;
    struct test_item *aaa;
    put_str("list: head->");
    while (cur_elem != &all_tag.tail)
    {
        aaa = elem2entry(struct test_item, tag, cur_elem);
        put_int(aaa->id);
        put_str("->");
        cur_elem = cur_elem->next;
    }
    put_str("tail\n");
}
