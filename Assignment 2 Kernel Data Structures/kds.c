#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* KERN_INFO */
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>   /* Init and exit macros */
#include <linux/rbtree.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/radix-tree.h>
#include <linux/xarray.h>

static char *int_str = "11 44 22 33 5";
module_param(int_str, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(int_str, "Input parameter string");


struct myrbtree {
    struct rb_node node;
    int value;
};

struct mylinkedlist {
    struct list_head node;
    int value;
};

struct mydatanode {
    int value;
};

struct myhashnode {
    struct hlist_node hash;
    int value;
};

DEFINE_HASHTABLE(myhashtable, 14);

int insert_rb_tree(struct rb_root *root, struct myrbtree *new_node) {
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    /* Figure out where to put new node */
    while (*new) {
        struct myrbtree *this = container_of(*new, struct myrbtree, node);
        parent = *new;
        if (new_node->value < this->value)
            new = &((*new)->rb_left);
        else if (new_node->value > this->value)
            new = &((*new)->rb_right);
        else
            return 0;
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&new_node->node, parent, new);
    rb_insert_color(&new_node->node, root);
    printk(KERN_INFO "************ Inserted %d ************", new_node-> value);
    return 1;
}

int lookup_rb_tree(struct rb_root *root, int number) {
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    while (*new) {
        struct myrbtree *this = container_of(*new, struct myrbtree, node);
        parent = *new;
        if (number < this->value)
            new = &((*new)->rb_left);
        else if (number > this->value)
            new = &((*new)->rb_right);
        else {
            printk(KERN_INFO "Lookup found element : %d \n", number);
            return number;
        }
    }
    return -1;
}

void print_rb_tree(struct rb_root * mytree) {
    struct rb_node *node;
    if(!rb_first(mytree)) {
        printk(KERN_INFO "Root Node is null");
    }

    for (node = rb_first(mytree); node; node = rb_next(node)) {    
        printk(KERN_INFO "key = %d\n", rb_entry(node, struct myrbtree, node)->value);
    }

    printk(KERN_INFO "************ Printing completed ************");
}


void remove_rb_tree(struct rb_root * mytree) {
    struct rb_node *node;
    for (node = rb_first(mytree); node; node = rb_next(node))
    {
      struct myrbtree *temp = rb_entry(node, struct myrbtree, node); 
      rb_erase(&(temp->node), mytree);
      kfree(temp);
  }
  printk(KERN_INFO "************ RB Tree Erased ************");
}

void run_redblack_trees(int * int_array, int count) {
    int i = 0, ret = 0, number;
    struct rb_root myroot = RB_ROOT;
    struct myrbtree * mynode;
    
    print_rb_tree(&myroot);

    while(i< count) {
        mynode = (struct myrbtree *)kmalloc(sizeof(struct myrbtree), GFP_KERNEL);
        mynode->value = int_array[i++];
        insert_rb_tree(&myroot, mynode);
        print_rb_tree(&myroot);
    }

    i=0;
    while(i< count) {
        number = int_array[i++];
        ret = lookup_rb_tree(&myroot, number);
        if(ret < 0 )
            printk(KERN_INFO "Element not found : %d ", number);
    }

    remove_rb_tree(&myroot);
    print_rb_tree(&myroot);
}

void print_linked_list(struct list_head *head) {
    struct mylinkedlist *curr;
    printk(KERN_INFO "**** Printing list elements ****");
    list_for_each_entry(curr, head, node) {
       printk(KERN_INFO "Value: %d\n", curr->value);
    }
}

void run_linked_list(int * int_array, int count) {
    int i = 0;
    struct mylinkedlist *mynode, *curr, *next;
    LIST_HEAD(my_list_head);

    while(i < count) {
        mynode = (struct mylinkedlist *)kmalloc(sizeof(struct mylinkedlist), GFP_KERNEL);
        mynode->value = int_array[i++];
        list_add(&mynode->node, &my_list_head);
        printk(KERN_INFO "Element inserted : %d \n", mynode->value);
    }

    print_linked_list(&my_list_head);

    printk(KERN_INFO "**** Removing list elements ****");
    list_for_each_entry_safe(curr, next, &my_list_head, node){
        list_del(&curr->node);
        kfree(curr);
    }

    print_linked_list(&my_list_head);
}

void print_hash_table(void) {
    struct myhashnode * curr;
    int bkt;
    printk(KERN_INFO "*** Iterating and Printing Hash Table *** \n");
    hash_for_each(myhashtable, bkt, curr ,hash) {
        printk(KERN_INFO "Value : %d \n", curr->value);
    }
}

void lookup_hash_table(int number) {
    struct myhashnode * curr;
    int key = number; /* TODO Check if we can use identical hash function */
    hash_for_each_possible(myhashtable, curr, hash, key)
    {
        if(curr->value == number)
            printk(KERN_INFO "Value looked up: %d \n",curr->value);
    }
}

void remove_hash_table(int number) {
    struct myhashnode * curr;
    int key = number;
    hash_for_each_possible(myhashtable, curr, hash, key)
    {
        if(curr->value == number) {
            printk(KERN_INFO "Deleting : %d \n", curr->value);
            hash_del(&curr->hash);
            kfree(curr);
        }
    }
}

void run_hash_table(int * int_array, int count) {
    struct myhashnode * mynode;
    int i = 0, key;
    hash_init(myhashtable);

    while(i < count) {
        mynode = (struct myhashnode *)kmalloc(sizeof(struct myhashnode), GFP_KERNEL);
        mynode->value = int_array[i++];
        key = mynode -> value; /* TODO Check if we can use identical hash function */
        hash_add(myhashtable, &mynode->hash, key);
        printk(KERN_INFO "Inserted : %d \n", mynode->value);
    }

    print_hash_table();
    i = 0;
    while(i < count) {
        lookup_hash_table(int_array[i++]);
    }
    i = 0;
    while(i < count) {
        remove_hash_table(int_array[i++]);
    }
    print_hash_table();
}

void run_bitmap(int * int_array, int count) {
        int number;
        unsigned long bit , i = 0;

        DECLARE_BITMAP(mybitmap, 10);
        bitmap_zero(mybitmap, 10);

        while (i < count) {
            number = int_array[i++];
            bit = 0;
            printk(KERN_INFO "Inserting integer %d into bitmap", number);
            while(number > 0) {
                if (number % 2 == 1) {   
                    printk(KERN_INFO "Setting bit %lu", bit);
                    set_bit(bit, mybitmap);
                }
                bit++; number = number/2;
            }
        }
        printk(KERN_INFO "****** Printing all the set bits ******\n");
        for_each_set_bit(bit, mybitmap, 10){
            printk(KERN_INFO "%lu \n", bit);
        }
        bitmap_zero(mybitmap, 10);
        printk(KERN_INFO "Cleared all the bits in bitmap\n");
}

void lookup_radix_tree(struct radix_tree_root * my_radix, int * int_array, int count) {
    int i=0;
    unsigned long number;
    struct mydatanode * radix_node;
    /* Lookup for each inserted elements */
    while(i<count) {
        number = int_array[i++];
        radix_node = radix_tree_lookup(my_radix, number);
        if (radix_node == NULL)
        {
            printk(KERN_INFO "Element not found: %lu \n", number);
        } else {
            printk(KERN_INFO "Element found: %d \n", radix_node->value);
        }
    }
}

void tag_radix_tree(struct radix_tree_root * my_radix, int * int_array, int count) {
    int i=0;
    unsigned long number;
    int tag = 1;
    /* Lookup for each inserted elements */
    while(i<count) {
        number = int_array[i++];
        if (number % 2 == 1)
        {
            radix_tree_tag_set(my_radix, number, tag);
            printk(KERN_INFO "Tagging element: %lu \n", number);
        }
    }
}

void lookup_tagged_tree(struct radix_tree_root * my_radix) {
    struct mydatanode * results[10];
    int i = 0;
    unsigned int count;

    count = radix_tree_gang_lookup_tag(my_radix, (void *)results, 0, 100, 1);
    printk(KERN_INFO "Count of tagged items gang looked up: %d ", count);
    
    for (i = 0; i< count; i++) {
        printk(KERN_INFO "Tagged item gang looked up: %d ", results[i]->value);
    }
}

void remove_radix_tree(struct radix_tree_root * my_radix, int * int_array, int count) {
    int i=0;
    unsigned long number;
    struct mydatanode * radix_node;
    /* Remove all inserted elements */
    while(i<count) {
        number = int_array[i++];
        radix_node = radix_tree_delete(my_radix, number);
        if (radix_node == NULL)
        {
            printk(KERN_INFO "Element to delete not found: %lu \n", number);
        } else {
            printk(KERN_INFO "Element deleted: %d \n", radix_node -> value);
            kfree(radix_node);
        }
    }
}


void run_radix_tree(int * int_array, int count) {
    int i = 0;
    struct mydatanode * radix_node;

    RADIX_TREE(my_radix, GFP_KERNEL);

    /* Inserting elements one by one*/
    while(i<count) {
        radix_node = (struct mydatanode *)kmalloc(sizeof(struct mydatanode), GFP_KERNEL);
        radix_node->value = int_array[i++];
        radix_tree_insert(&my_radix, radix_node->value, radix_node);  //index of insertion is the data itself ??
        printk(KERN_INFO "Inserted element: %d \n", radix_node-> value);
    }

    lookup_radix_tree(&my_radix, int_array, count);

    tag_radix_tree(&my_radix, int_array, count);

    lookup_tagged_tree(&my_radix);    

    remove_radix_tree(&my_radix, int_array, count);

    printk(KERN_INFO "*** Initiating re-lookup for all elements ***");

    lookup_radix_tree(&my_radix, int_array, count);

}

void lookup_xarray(struct xarray * my_xarray, int * int_array, int count) {
    int i=0;
    unsigned long number;
    struct mydatanode * radix_node;
    /* Lookup for each inserted elements */
    while(i<count) {
        number = int_array[i++];
        radix_node = (struct mydatanode * ) xa_load(my_xarray, number);
        if (radix_node == NULL)
        {
            printk(KERN_INFO "Element not found: %lu \n", number);
        } else {
            printk(KERN_INFO "Element found: %d \n", radix_node->value);
        }
    }
}

void tag_xarray(struct xarray * my_xarray, int * int_array, int count) {
    int i=0;
    unsigned long number;
    while(i<count) {
        number = int_array[i++];
        if (number % 2 == 1)
        {
            xa_set_mark(my_xarray, number, XA_MARK_1);
            printk(KERN_INFO "Tagged element: %lu \n", number);
        }
    }
}

void lookup_tagged(struct xarray * my_xarray) {
    struct mydatanode * radix_node;
    unsigned long index;
    xa_for_each_marked(my_xarray, index, radix_node, XA_MARK_1) {
        printk(KERN_INFO "Tag looked up element : %d ", radix_node -> value);
    }
}

void remove_xarray(struct xarray * my_xarray, int * int_array, int count) {
    int i=0;
    struct mydatanode * radix_node;
    unsigned long number;
    while(i<count) {
        number = int_array[i++];
        radix_node = (struct mydatanode *) xa_erase(my_xarray, number);
        if (radix_node == NULL)
        {
            printk(KERN_INFO "Element to delete not found: %lu \n", number);
        } else {
            printk(KERN_INFO "Element deleted: %d \n", radix_node -> value);
            kfree(radix_node);
        }
    }
}

void run_xarray(int * int_array, int count) {
    int i=0;
    struct mydatanode * radix_node;
    DEFINE_XARRAY(my_xarray);

    while(i<count) {
        radix_node = (struct mydatanode *)kmalloc(sizeof(struct mydatanode), GFP_KERNEL);
        radix_node->value = int_array[i++];
        xa_store(&my_xarray, radix_node-> value, radix_node, GFP_KERNEL);
        printk(KERN_INFO "Inserted element: %d \n", radix_node-> value);
    }

    lookup_xarray(&my_xarray, int_array, count);

    tag_xarray(&my_xarray, int_array, count);

    lookup_tagged(&my_xarray);

    remove_xarray(&my_xarray, int_array, count);
}

void convert_str_to_integers(int * int_array, int count) {
    int i = 0, j = 0, sum = 0;
    while (int_str[i] != '\0' && j < count) {
        if (int_str[i] == ' '){
            int_array[j] = sum; sum = 0; j++;   
        } else {
            sum = sum * 10 + (int_str[i] - '0');
        } 
        i++;
    }
    int_array[j] = sum;
}   

static int __init lkp_init(void)
{
    int i = 0, count  = 0;
    int * int_array;

    printk(KERN_INFO "Module loaded ...\n");
    printk(KERN_INFO "The init string is %s ...\n", int_str);

    /* Counting the number of integers */
    while(int_str[i]!= '\0') {
      if (int_str[i] == ' ') {          
          count++;
      }
      i++;
  }
  if (i>0 && int_str[i-1]!= ' ') count++;
  printk(KERN_INFO "Number of integers : %d \n", count);  


    /* Allocating space in memory to store integers */
  int_array = kmalloc(count * sizeof(int), GFP_KERNEL);


    /* Converting string to integers */
  convert_str_to_integers(int_array, count);

    /* Printing integers */
    i = 0;
    while(i< count) {
        printk(KERN_INFO "Input Integers: %d\n", int_array[i++]);
    }

    printk(KERN_INFO "\n**** Initiating Linked List Operations **** \n");
    run_linked_list(int_array, count);

    printk(KERN_INFO "\n**** Initiating Hash Table Operations **** \n");
    run_hash_table(int_array, count);

    printk(KERN_INFO "\n**** Initiating Red Black Trees Operations **** \n");
    run_redblack_trees(int_array, count);

    printk(KERN_INFO "\n**** Initiating Radix Tree Operations **** \n");
    run_radix_tree(int_array, count);

    printk(KERN_INFO "\n**** Initiating X Array Operations **** \n");
    run_xarray(int_array, count);

    printk(KERN_INFO "\n**** Initiating Bitmap Operations **** \n");
    run_bitmap(int_array, count);

    kfree(int_array);
    return 0; 
}

static void __exit lkp_exit(void)
{
    printk(KERN_INFO "Module exiting ...\n");
}
module_init(lkp_init); /* lkp_init() will be called at loading the module */
module_exit(lkp_exit); /* lkp_exit() will be called at unloading the module */

MODULE_AUTHOR("Pratik Nagelia <pnagelia@cs.stonybrook.edu");
MODULE_DESCRIPTION("Assignment kds module");
MODULE_LICENSE("GPL");
