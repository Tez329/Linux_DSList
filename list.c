#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/random.h>
#include <linux/slab.h>

#define BILLION 1000000000

struct my_node {
	struct list_head list;
	unsigned int data;
};

unsigned long long add_list_time;
unsigned long long add_list_count;

void
calclock_interval(struct timespec *spclock, unsigned long long *total_time, unsigned long long *total_count) {
	long long interval_sec = spclock[1].tv_sec - spclock[0].tv_sec;
	long long interval_nsec = spclock[1].tv_nsec - spclock[0].tv_nsec;
	if (interval_nsec <= 0) {
		interval_sec -= 1;
		interval_nsec += BILLION;
	}

	unsigned long long timedelay = BILLION * interval_sec + interval_nsec;
	__sync_fetch_and_add(total_time, timedelay);
	__sync_fetch_and_add(total_count, 1);
}

// Calculate insert, search delete time of length n array
void calculate(int n) {
	struct list_head *ptr, *ptrn, *my_list;
	struct timespec spclock[2];
	unsigned long long list_time, list_count;
	struct my_node *current_node;
	int i;
	int sum = 0;

	my_list = (struct list_head *) kmalloc(sizeof(struct list_head), GFP_KERNEL);
	INIT_LIST_HEAD(my_list);

	// Insert (with random number)
	list_time = list_count = 0;
	for(i=0; i<n; i++) {
		struct my_node *new = kmalloc(sizeof(struct my_node), GFP_KERNEL);
		new->data = get_random_int();

		getnstimeofday(&spclock[0]);
		list_add(&new->list, my_list);
		getnstimeofday(&spclock[1]);
		calclock_interval(spclock, &list_time, &list_count);
	}
	printk("insert time: %lld, count: %lld\n", list_time, list_count);

	// Search
	list_time = list_count = 0;
	ptr = my_list->next;
	while(ptr != my_list) {
		getnstimeofday(&spclock[0]);
		ptr = ptr->next;		
		getnstimeofday(&spclock[1]);
		calclock_interval(spclock, &list_time, &list_count);
		sum += sizeof(*ptr);
	}
	printk("Search time: %lld, count: %lld\n", list_time, list_count);

	// Delete
	list_time = list_count = 0;
	list_for_each_safe(ptr, ptrn, my_list) {
		getnstimeofday(&spclock[0]);
		current_node = list_entry(ptr, struct my_node, list);
		list_del(ptr);
		kfree(current_node);
		getnstimeofday(&spclock[1]);
		calclock_interval(spclock, &list_time, &list_count);
	}
	printk("delete time: %lld, count: %lld\n", list_time, list_count);

	printk("size of list: %d\n", sum);
}


void calclock(void) {
	printk("Case 100000\n");
	calculate(100000);
}



static int __init hello_module_init(void) {
	printk("Calclock module inserted!!!\n");
	calclock();	
	return 0;
}
static void __exit hello_module_cleanup(void) {
	printk("Calclock module deleted\n");
}

module_init(hello_module_init);
module_exit(hello_module_cleanup);
