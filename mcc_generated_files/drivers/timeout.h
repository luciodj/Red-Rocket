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

#ifndef __TIMEOUTDRIVER_H
#define __TIMEOUTDRIVER_H

#include <stdint.h>
#include <stdbool.h>

/*
*   Please note that the timer tick is different from the timer period.
*   A tick occurs each time the peripheral timer increases its count
*   The timer period is when the number of ticks reaches its specified maximum and
*      the timer overflow interrupt occurs.
*   This library sets the timer period internally as needed
*/
#define INLINE

/** Datatype used to hold the number of ticks until a timer expires */
typedef uint16_t ticks;
#define MAX_BASE_PERIOD     32767   // related to ticks definition (16 or 32-bit)

/** Typedef for the function pointer for the timeout callback function */
typedef uint32_t (*timercallback_ptr_t)(void *payload);

/** Data structure completely describing one timer */
typedef struct timerStruct_s {
	timercallback_ptr_t    callback; ///< Pointer to a callback function that is called when this timer expires
	void *                 payload; ///< Pointer to data that user would like to pass along to the callback function
	struct timerStruct_s *next;    ///< Pointer to a linked list of all timers that have expired and whose callback
	                                ///functions are due to be called
	ticks period;   ///< The number of ticks the timer will count before it expires
    ticks due;
} timerStruct_t;

//********************************************************
// The following functions form the API for scheduler mode.
//********************************************************

/**
 * \brief Initialize the timer used and isr call back
 *
 * \return True if successful, False if the
 */
void timeout_initialize(void);

/**
 * \brief Schedule the specified timer task to execute at the specified time
 *
 * \param[in] timer Pointer to struct describing the task to execute
 * \param[in] ms    Number of ms to wait before executing the task
 *
 * \return    True if successful, False if duration (ms) exceed max allowed
 *            Hint: if failing, change ticks from uint16_ to uint32_t
 */
bool timeout_create(timerStruct_t *task, uint32_t ms);

/**
 * \brief Delete the specified timer task so it won't be executed
 *
 * \param[in] timer Pointer to struct describing the task to execute
 *
 * \return Nothing
 */
void timeout_delete(timerStruct_t *task);

/**
 * \brief Delete all scheduled timer tasks
 *
 * \return Nothing
 */
void timeout_flush(void);

/**
 * \brief Execute the next timer task that has been scheduled for execution.
 *
 * If no task has been scheduled for execution, the function
 * returns immediately, so there is no need for any polling.
 *
 * \return Nothing
 */
void timeout_next(void);


#endif // __TIMEOUTDRIVER_H
