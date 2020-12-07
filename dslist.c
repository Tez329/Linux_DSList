#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/poison.h>
#include <linux/list.h>

#define UP 0
#define DOWN 1

/** DECLARE DSLIST
 * @param name - a new name of structure
 * @param structure - param like "struct mynode"
 */

#define DECLARE_DSLIST(name, structure) typedef struct { \
	struct list_head list; \
	unsigned char toggle; \
	structure up; \
	structure down; \
} __attribute__((packed)) name;

/** INIT_DSLIST
 * @param dslist - declared structure defined by DECLARE_DSLIST
 */

#define INIT_DSLIST(dslist) memset(&dslist, 0, sizeof(dslist));

/** dslist_istoggled
 * @param pos - the position of toggle, UP or DOWN
 * @param cluster - structure defined by DECLARE_DSLIST
 */
#define dslist_istoggled(pos, cluster) pos == UP \
	? ((cluster->toggle & 0x01) == 0x01) \
	: ((cluster->toggle & 0x02) == 0x02)

/** dslist_toggle
 * @param pos - the position of toggle, UP or DOWN
 * @param cluster - structure defined by DECLARE_DSLIST
 */
#define dslist_toggle(pos, cluster) cluster->toggle = (pos == UP \
	? (cluster->toggle ^ 0x01) \
	: (cluster->toggle ^ 0x02))

/** dslist_cluster
 * @param ptr: the &struct list_head pointer
 * @param type: the type of cluster
 */
#define dslist_cluster(ptr, type)\
	list_entry(ptr, type, list)

/** dslist_up
 * @param cluster
 */
#define dslist_up(cluster)\
	cluster->up

/** dslist_down
 * @param clust
 */
#define dslist_down(cluster)\
	cluster->down

/** dslist_next_cluster
 * @param pos - cursor of the cluster points next cluster
 * @param head - the head of dslist
 */
#define dslist_next_cluster(pos, head)\
	list_next_entry(pos, head)

/** dslist_for_each_cluster
 * @param pos - cursor of the cluster used in traverse
 * @param head - the head of dslist
 */
#define dslist_for_each_cluster(pos, head)\
	list_for_each_entry(pos, head, list)

#define dslist_add(head, structure, data) do {	\
	if(head->next == head) {	\
		structure *new = kmalloc(sizeof(structure), GFP_KERNEL);	\
		INIT_DSLIST(*new);	\
		dslist_toggle(UP, new);	\
		memcpy(&(new->up), &data, sizeof(data)); \
		list_add(&(new->list), head);	\
	}	\
	else {	\
		structure *nextCluster = list_entry(head->next, structure, list);	\
		if(dslist_istoggled(DOWN, nextCluster) == 0) {	\
			memcpy(&(nextCluster->down), &(nextCluster->up), sizeof(nextCluster->up)); \
			memcpy(&(nextCluster->up), &data, sizeof(data)); \
			dslist_toggle(DOWN, nextCluster);	\
		}	\
		else {	\
			structure *new = kmalloc(sizeof(structure), GFP_KERNEL);	\
			INIT_DSLIST(*new);	\
			dslist_toggle(UP, new);	\
			memcpy(&(new->up), &data, sizeof(data)); \
			list_add(&(new->list), head);	\
		}	\
	}	\
}while(0);

/** dslist_del
 * @param pos - the position of toggle, UP or DOWN
 * @param cluster - structure defined by DECLARE_DSLIST
 */
#define dslist_del(head, structure, cluster, pos) do { \
	if (pos == UP) { \
		if (cluster->list.prev != head) { \
			structure *prevCluster = list_entry(cluster->list.prev, structure, list); \
			if (dslist_istoggled(DOWN, cluster)) { \
				if (dslist_istoggled(DOWN, prevCluster)) { \
					memcpy(&(cluster->up), &(cluster->down), sizeof(cluster->down)); \
					dslist_toggle(DOWN, cluster); \
				} \
				else { \
					memcpy(&(prevCluster->down), &(cluster->down), sizeof(cluster->down)); \
					dslist_toggle(DOWN, prevCluster); \
					list_del(&(cluster->list)); \
					kfree(cluster); \
				} \
			} else { \
				list_del(&(cluster->list)); \
				kfree(cluster); \
			} \
		}\
		else { \
			if (dslist_istoggled(DOWN, cluster)) {\
				memcpy(&(cluster->up), &(cluster->down), sizeof(cluster->down)); \
				dslist_toggle(DOWN, cluster); \
			} else { \
				list_del(&(cluster->list)); \
				kfree(cluster); \
			} \
		} \
	} \
	else if (pos == DOWN) { \
		if (cluster->list.prev != head) { \
			structure *prevCluster = list_entry(cluster->list.prev, structure, list); \
			if (dslist_istoggled(DOWN, prevCluster)) dslist_toggle(DOWN, cluster); \
			else { \
				memcpy(&(prevCluster->down), &(cluster->up), sizeof(cluster->up)); \
				dslist_toggle(DOWN, prevCluster); \
				list_del(&(cluster->list)); \
				kfree(cluster); \
			} \
		} \
		else dslist_toggle(DOWN, cluster); \
		 \
	} \
} while(0)


#define NUM_OF_NODES 100000
#define BILLION 1000000000

struct total_time {
	unsigned long long total_time;
	unsigned long long total_count;
};

struct total_time time;

void calclock_interval(struct timespec *spec) {
	unsigned long long timedelay;
	long long interval_sec = spec[1].tv_sec - spec[0].tv_sec;
	long long interval_nsec = spec[1].tv_nsec - spec[0].tv_nsec;
	if (interval_nsec <= 0) {
		interval_sec -= 1;
		interval_nsec += BILLION;
	}
	timedelay = BILLION * interval_sec + interval_nsec;
	__sync_fetch_and_add(&time.total_time, timedelay);
	__sync_fetch_and_add(&time.total_count, 1);
}

void calclock_init(struct total_time *time) {
	time->total_time = 0;
	time->total_count = 0;
}

// --

struct my_node {
	int a;
};

DECLARE_DSLIST(dslist, struct my_node);

struct list_head *my_list, *my_dslist;

void start(void) {
	int i;
	struct timespec spec[2];
	dslist *cluster;


	my_list = (struct list_head *) kmalloc(sizeof(struct list_head), GFP_KERNEL);
	my_dslist = (struct list_head *) kmalloc(sizeof(struct list_head), GFP_KERNEL);

//	INIT_LIST_HEAD(my_list);
	INIT_LIST_HEAD(my_dslist);

	// insert
	calclock_init(&time);
	for(i=0; i<NUM_OF_NODES; i++) {
		getnstimeofday(&spec[0]);
		struct my_node data;
		data.a = i;
		dslist_add(my_dslist, dslist, data);
		getnstimeofday(&spec[1]);
		calclock_interval(spec);
	}

	printk("time: %lldns, count: %lld\n", time.total_time, time.total_count);

	int sum = 0;
	list_for_each_entry(cluster, my_dslist, list) {
		sum += sizeof(*cluster);
	}

	// traerse
	calclock_init(&time);
	for(i=0; i<NUM_OF_NODES; i++) {
		struct my_node data = {i};
		getnstimeofday(&spec[0]);
		list_for_each_entry(cluster, my_dslist, list) {
			if (dslist_istoggled(UP, cluster) && cluster->up.a == i);
			else if(dslist_istoggled(DOWN, cluster) && cluster->down.a == i);
			else continue;
			break;
		}
		getnstimeofday(&spec[1]);
		calclock_interval(spec);
	}
	printk("time: %lldns, count: %lld\n", time.total_time, time.total_count);

	// delete
	calclock_init(&time);
	for(i=0; i<NUM_OF_NODES; i++) {
		struct my_node data = {i};
		getnstimeofday(&spec[0]);
		list_for_each_entry(cluster, my_dslist, list) {
			if (dslist_istoggled(UP, cluster) && cluster->up.a == i)
				dslist_del(my_dslist, dslist, cluster, UP);
			else if(dslist_istoggled(DOWN, cluster) && cluster->down.a == i)
				dslist_del(my_dslist, dslist, cluster, DOWN);
			else continue;
			break;
		}
	
		getnstimeofday(&spec[1]);
		calclock_interval(spec);
	}
	printk("time: %lldns, count: %lld\n", time.total_time, time.total_count);

	printk("size of dslist: %d", sum);
}

static int __init hello_module_init(void) {
	printk("*************** Init Module ****************\n");
	start();
	
	return 0;
}

static void __exit hello_module_cleanup(void) {
	printk("**************** Exit Module ****************\n");
}

module_init(hello_module_init);
module_exit(hello_module_cleanup);
MODULE_LICENSE("GPL");
