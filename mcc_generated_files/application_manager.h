/*
\file   application_manager.h

\brief  Application Manager header file.

(c) 2018 Microchip Technology Inc. and its subsidiaries.

Subject to your compliance with these terms, you may use Microchip software and any
derivatives exclusively with Microchip products. It is your responsibility to comply with third party
license terms applicable to your use of third party software (including open source software) that
may accompany Microchip software.

THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY
IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS
FOR A PARTICULAR PURPOSE.

IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT
OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS
SOFTWARE.
*/

#ifndef APPLICATION_MANAGER_H_
#define APPLICATION_MANAGER_H_

#include "mcc.h"
#include "led.h"
#include "debug_print.h"
#include "utils/atomic.h"
#include <avr/wdt.h>
#include "cloud/cloud_service.h"

typedef union
{
    uint8_t allBits;
    struct
    {
        unsigned amDisconnecting :1;
        unsigned haveAPConnection :1;
        unsigned haveERROR :1;
        unsigned :5;
    };
} shared_networking_params_t;
extern shared_networking_params_t shared_networking_params;

void application_init(void);
void application_post_provisioning(void);
void runScheduler(void);

#endif /* APPLICATION_MANAGER_H_ */