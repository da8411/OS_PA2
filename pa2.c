/**********************************************************************
 * Copyright (c) 2019-2022
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

/* THIS FILE IS ALL YOURS; DO WHATEVER YOU WANT TO DO IN THIS FILE */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"

/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;


/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;


/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];


/**
 * Monotonically increasing ticks
 */
extern unsigned int ticks;


/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;


/***********************************************************************
 * Default FCFS resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
bool fcfs_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

/***********************************************************************
 * Default FCFS resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}



#include "sched.h"

/***********************************************************************
 * FIFO scheduler
 ***********************************************************************/
static int fifo_initialize(void)
{
	return 0;
}

static void fifo_finalize(void)
{
	
}

static struct process *fifo_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();

	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan) {
		return current;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue)) {
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);

		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the next process to run */
	return next;
}

struct scheduler fifo_scheduler = {
	.name = "FIFO",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fifo_initialize,
	.finalize = fifo_finalize,
	.schedule = fifo_schedule,
};


/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(void)
{
	struct process *next = NULL;
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if (current->age < current->lifespan) {
		return current;
	}

pick_next:

	if (!list_empty(&readyqueue)) {
		//lifespan 비교
		struct process *List, *cursor = malloc(sizeof(struct process));
		int min = __INT_MAX__;
		
		list_for_each_entry_safe(List, cursor, &readyqueue, list){
			if(List->lifespan < min) {
				min = List->lifespan;
				next = List;
			}
		}
		list_del_init(&next->list);
	}
	return next;
}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = sjf_schedule,		 /* TODO: Assign sjf_schedule()
								to this function pointer to activate
								SJF in the system */
};


/***********************************************************************
 * SRTF scheduler
 ***********************************************************************/
void srtf_fork(struct process *List)
{
	if(current == NULL){
		return;
	}
	else {
		int current_remaining_time = current->lifespan - current->age;
		int next_remaining_time = List->lifespan - List->age;
		
		if(next_remaining_time < current_remaining_time){
			current->list = readyqueue;
			list_add_tail(&(current->list), &readyqueue);
			current = NULL;
		}
	}
}

static struct process *srtf_schedule(void)
{
	struct process *next = NULL;
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if (current->age < current->lifespan) {
		return current;
	}

pick_next:

	if (!list_empty(&readyqueue)) {
		struct process *List, *cursor = malloc(sizeof(struct process));
		int min = __INT_MAX__;
		
		list_for_each_entry_safe(List, cursor, &readyqueue, list){
		int remaining_time = List->lifespan - List->age;
			if(remaining_time < min) {
				min = remaining_time;
				next = List;
			}
		}
		list_del_init(&next->list);
	}
	return next;
}

// void srtf_exit(struct process *List)
// {
// 	if(List == NULL){
// 		exit(0);
// 	}
// }

struct scheduler srtf_scheduler = {
	.name = "Shortest Remaining Time First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
    .forked = srtf_fork,
	.schedule = srtf_schedule,
	// .exiting = srtf_exit,
	/* You need to check the newly created processes to implement SRTF.
	 * Use @forked() callback to mark newly created processes */
	/* Obviously, you should implement srtf_schedule() and attach it here */
};


/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
static struct process *rr_schedule(void)
{
	struct process *next = NULL;
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if (current->age < current->lifespan) {
		current->list = readyqueue;
		list_add_tail(&(current->list), &readyqueue);
		current = NULL;
	}

pick_next:

	if (!list_empty(&readyqueue)) {
		next = list_first_entry(&readyqueue, struct process, list);
		list_del_init(&next->list);
	}
	return next;
}

struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = rr_schedule, /* Obviously, you should implement rr_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler
 ***********************************************************************/
bool prio_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

void prio_release(int resource_id)
{
	struct resource *r = resources + resource_id;

   	/* Ensure that the owner process is releasing the resource */
   	assert(r->owner == current);

   	/* Un-own this resource */
   	r->owner = NULL;

   	/* Let's wake up ONE waiter (if exists) that came first */
   	if (!list_empty(&r->waitqueue)) {
    	struct process *waiter, *cursor = malloc(sizeof(struct process));
      	int max = 0;
      	list_for_each_entry_safe(waiter, cursor, &r->waitqueue, list){
    		if(waiter->prio >= max) {
            	max = waiter->prio;
         	}
      	}

      	list_for_each_entry_safe(waiter, cursor, &r->waitqueue, list){
         	if(waiter->prio == max) {
            	break;
         	}
      	}
      	assert(waiter->status == PROCESS_WAIT);
      	list_del_init(&waiter->list);
      	waiter->status = PROCESS_READY;
      	list_add_tail(&waiter->list, &readyqueue);
   	}
}

static struct process *prio_schedule(void)
{
	struct process *next = NULL;
	if (!current || current->status == PROCESS_WAIT) {
    	goto pick_next;
   	}

   	if (current->age < current->lifespan) {
      	list_add_tail(&current->list,&readyqueue);
   	}

pick_next:
   	if (!list_empty(&readyqueue)) {
      	struct process *List, *cursor = NULL;
      	int max = 0;
      	list_for_each_entry_safe(List, cursor, &readyqueue, list){
         	if(List->prio >= max) {
            	max = List->prio;
        	}
      	}
      	list_for_each_entry_safe(List, cursor, &readyqueue, list){
         	if(List->prio == max) {
            	next = List;
            	break;
         	}
      	}
      	list_del_init(&next->list);
   	}
   	return next;
}
struct scheduler prio_scheduler = {
	.name = "Priority",
	.acquire = prio_acquire,
	.release = prio_release,
	.schedule =prio_schedule,
	/**
	 * Implement your own acqure/release function to make priority
	 * scheduler correct.
	 */
	/* Implement your own prio_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler with aging
 ***********************************************************************/
static struct process *pa_schedule(void)
{
	struct process *next = NULL;
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if (current->age < current->lifespan) {
		list_add_tail(&current->list, &readyqueue);
	}

pick_next:
	if (!list_empty(&readyqueue)) {
		struct process *List, *cursor = NULL;
		int max = 0;
		list_for_each_entry_safe(List, cursor, &readyqueue, list) {
			if (List->prio >= max) {
				max = List->prio;
			}
		}

		list_for_each_entry_safe(List, cursor, &readyqueue, list) {
			if (List->prio == max) {
				next = List;
				next->prio = next->prio_orig;
				break;
			}
		}

		list_del_init(&next->list);

		list_for_each_entry_safe(List, cursor, &readyqueue, list) {
			List->prio +=1;
			if (List->prio == MAX_PRIO){
				List->prio = MAX_PRIO;
			}
		}
	}
   	return next;
}
struct scheduler pa_scheduler = {
	.name = "Priority + aging",
	.acquire = prio_acquire,
	.release = prio_release,
	.schedule =pa_schedule,
	/**
	 * Implement your own acqure/release function to make priority
	 * scheduler correct.
	 */
	/* Implement your own prio_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler with priority ceiling protocol
 ***********************************************************************/
bool pcp_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;
	current->prio = MAX_PRIO;
	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

void pcp_release(int resource_id)
{
	struct process *next = NULL;
	int maxPrio = 0; // wait Queue에 있는 process 제일 큰 prio
	struct resource *r = resources + resource_id;
	struct process *waiter = NULL;

	assert(r->owner == current);
	r->owner->prio = r->owner->prio_orig;

	r->owner = NULL;

	if (!list_empty(&r->waitqueue)) {
		struct process *List, *cursor = NULL;

		// waitQueue에 있는 process중에 prority 가장 큰 친구 찾기
		list_for_each_entry_safe(List, cursor, &r->waitqueue, list) { 
			if (List->prio >= maxPrio) {
				maxPrio = List->prio;
			}
		}
		// priority가 가장 큰 친구가 next다
		list_for_each_entry_safe(List,cursor, &r->waitqueue, list) { 
			if (List->prio == maxPrio) {
				next = List;
				break;
			}
		}

		waiter = next; // 다음 친구는 next다

		assert(waiter->status == PROCESS_WAIT);
		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}
}

struct scheduler pcp_scheduler = {
	.name = "Priority + PCP Protocol",
	.acquire = pcp_acquire,
	.release = pcp_release,
	.schedule =prio_schedule,
	/**
	 * Implement your own acqure/release function too to make priority
	 * scheduler correct.
	 */
};


/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
bool pip_acquire(int resource_id)
{
	struct resource *r = resources + resource_id; 
	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}
	
	/* OK, this resource is taken by @r->owner. */
	if (r->owner->prio < current->prio) {
		r->owner->prio = current->prio + 1;
	}

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

void pip_release(int resource_id)
{
	struct process *next = NULL;
	int maxPrio = 0;      // wait Queue에 있는 process 제일 큰 prio

	struct resource *r = resources + resource_id;
	struct process *waiter = NULL;
	assert(r->owner == current);

    struct process *List, *cursor = NULL;   
	if (!list_empty(&r->waitqueue)) {
		// waitQueue에 있는 process중에 prority 가장 큰 친구 찾기
		list_for_each_entry_safe(List, cursor, &r->waitqueue, list) { 
			if (List->prio >= maxPrio) {
				maxPrio = List->prio;
			}
		}

		// priority가 가장 큰 친구가 next다
		list_for_each_entry_safe(List, cursor, &r->waitqueue, list) { 
			if (List->prio == maxPrio) {
				next = List;
				break;
        	}
      	}
		waiter = next; // 다음 친구는 next다
		assert(waiter->status == PROCESS_WAIT);

		maxPrio = 0;
		list_del_init(&waiter->list);
		// priority 복원시키기
		if (!list_empty(&r->waitqueue)) {
			// waitQueue에 있는 process중에 prority 가장 큰 친구 찾기
			list_for_each_entry_safe(List, cursor, &r->waitqueue, list) { 
				if (List->prio >= maxPrio) {
					maxPrio = List->prio;
				}
			}
        // prio 복원시키기
        r->owner->prio = (maxPrio > r->owner->prio_orig) ? maxPrio : r->owner->prio_orig;
    	}
		else {
			// prio 복원시키기
			r->owner->prio = r->owner->prio_orig;
		}

		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
   	}
   	r->owner = NULL;
}
struct scheduler pip_scheduler = {
	.name = "Priority + PIP Protocol",
	.acquire = pip_acquire,
   .release = pip_release,
   .schedule = prio_schedule,
	/**
	 * Ditto
	 */
};
