/*
    (c) 2016 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
    WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

    MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
    TERMS.
*/

#ifdef __XC
#include <xc.h>
#endif
#include <stdio.h>
#include "../mcc.h"
#include "timeout.h"

#define SCHEDULER_BASE_PERIOD 8    // ms

timerStruct_t *listHead          = NULL;
timerStruct_t * volatile dueHead = NULL;

volatile ticks  currTime = 0;

// callback prototype
void timeout_isr(void);

// compare two timestamps and return true if a >= thenb
// timestamps are unsigned, using Z math (Z = 16-bit or 32-bit)
// so their difference must be < half Z period
// to simplify the check we compute the difference and cast it to signed
inline bool greaterOrEqual(ticks a, ticks thenb)
{
    return ((int16_t)(a - thenb) >= 0);
}

void timeout_initialize(void)
{
    RTC_SetPITIsrCallback(timeout_isr);

	while (RTC.PITSTATUS > 0)
        ;      /* Wait for all register to be synchronized */
	RTC.CLKSEL = RTC_CLKSEL_INT1K_gc;   /* Clock Select: Internal 1kHz OSC */
	while (RTC.PITSTATUS > 0)
        ;      /* Wait for all register to be synchronized */
    RTC_EnablePITInterrupt();
    RTC.PITCTRLA = RTC_PERIOD_CYC8_gc;  // 8 msec match SCHEDULER_BASE_PERIOD
	while (RTC.PITSTATUS > 0)
        ;      /* Wait for all register to be synchronized */
    RTC.PITCTRLA |= RTC_PI_bm;            // enable PIT function
}

//void timeout_print_list(void)
//{
//	timer_struct_t *pTask = tasks_head;
//
//    printf("@%d tasks_head -> ", curr_time);
//	while (pTask != NULL) {
//		printf("%s:%ld -> ", pTask->name, (uint32_t)pTask->due);
//		pTask = pTask->next;
//	}
//	printf("NULL\n");
//}

// Returns true if the insert was at the head, false if not
static void sortedInsert(timerStruct_t *timer)
{
	uint8_t   at_head       = true;
	timerStruct_t *insert_point = listHead;
	timerStruct_t *prev_point   = NULL;

    timer->next = NULL;
	while (insert_point != NULL) {
		if (greaterOrEqual(insert_point->due, timer->due)) {
			break; // found the spot
		}
		prev_point   = insert_point;
		insert_point = insert_point->next;
		at_head      = false;
	}

	if (at_head) { // the front of the list.
		timer->next = listHead;
		listHead = timer;
		return;
	}
    else { // middle of the list
		timer->next = prev_point->next;
	}
	prev_point->next = timer;
	return;
}

// Cancel and remove all active timers
void timeout_flush(void)
{
    while (listHead != NULL)
        timeout_delete(listHead);

    while (dueHead != NULL)
        timeout_delete(dueHead);
}


// Deletes a timer from a list and returns true if the timer was found and
//     removed from the list specified
static bool timeout_dequeue(timerStruct_t * volatile *list, timerStruct_t *timer)
{
    bool retVal = false;
    if (*list == NULL)
        return retVal;

    // Guard in case we get interrupted, we cannot safely compare/search and get interrupted
    RTC_DisablePITInterrupt();

    // Special case, the head is the one we are deleting
    if (timer == *list)
    {
        *list = (*list)->next;       // Delete the head
        retVal = true;
    } else
    {                      // compare from the second task (if present) down
		timerStruct_t *delete_point = (*list)->next;
		timerStruct_t *prev_task = *list;   // start from the second element
		while (delete_point != NULL) {
			if (delete_point == timer) {
				prev_task->next = delete_point->next; // delete it from list
				retVal = true;
				break;
			}
			prev_task = delete_point; // advance down the list
			delete_point = delete_point->next;
        }
    }
    RTC_EnablePITInterrupt();

    return retVal;
}

// This will cancel/remove a running timer. If the timer is already expired it will
//     also remove it from the callback queue
void timeout_delete(timerStruct_t *timer)
{
    if (!timeout_dequeue(&listHead, timer))
    {
        timeout_dequeue(&dueHead, timer);
    }

    timer->next = NULL;
}

// This function checks the list of due tasks and calls the first one in the
//    list if the list is not empty. It also reschedules the task if on repeat
// It is recommended this is called from the main superloop (while(1)) in your code
//    but by design this can also be called from the task ISR. If you wish callbacks
//    to happen from the ISR context you can call this as the last action in timeout_isr
//    instead.
inline void timeout_next(void)
{
    if (dueHead == NULL)
        return;

    RTC_DisablePITInterrupt();

    timerStruct_t *pTimer = dueHead;

 	dueHead = dueHead->next;      // and remove it from the list
    sortedInsert(pTimer);       // re-enter it immediately in the task queue
//    timeout_print_list();

    RTC_EnablePITInterrupt();

	bool reschedule = pTimer->callback(pTimer->payload); // execute the task

    // Do we have to reschedule it? If yes then add delta to absolute for reschedule
    if(!reschedule)
    {
        timeout_delete(pTimer);
    }
}

// This function queues a task with a given period/duration
// If the task was already active/running it will be replaced by this and the
//    old (active) task will be removed/cancelled first
// inputs:
//   ms         time expressed in ms
//   return     true if successful, false if period < SCHEDULER_BASE_PERIOD
bool timeout_create(timerStruct_t *timer, uint32_t ms)
{
    // If this timer is already active, replace it
    timeout_delete(timer);
    if ((ms == 0) || (ms > MAX_BASE_PERIOD)){
        return false;
    }

    RTC_DisablePITInterrupt();

    timer->period = (ticks)ms;               // store period scaled
    timer->due = currTime + timer->period;   // compute due time
    sortedInsert(timer);
    RTC_EnablePITInterrupt();
    return true;    // successful creation
}

// NOTE: assumes the callback completes before the next timer tick
void timeout_isr(void)
{
    currTime += SCHEDULER_BASE_PERIOD;    // forever advancing and wrapping around
    // activate timers that are due (move to due list))
    while( (listHead)  &&
            greaterOrEqual(currTime, listHead->due) ) {
        listHead->due += listHead->period;    // update immediately the due time
        timerStruct_t * pNext = listHead->next;     // save next temporarily
        listHead->next = dueHead;         // insert at head of due
        dueHead = listHead;
        listHead = pNext;               // remove task from scheduler queue
    }
 }
