/*
\file   application_manager.c

\brief  Application Manager source file.

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

#include <string.h>
#include <time.h>
#include <stdio.h>

#include "application_manager.h"
#include "config/IoT_Sensor_Node_config.h"
#include "config/conf_winc.h"
#include "cloud/crypto_client/cryptoauthlib_main.h"
#include "cloud/crypto_client/crypto_client.h"
#include "cloud/wifi_service.h"
#if CFG_ENABLE_CLI
#include "cli/cli.h"
#endif


#define MAIN_DATATASK_INTERVAL 100L
// The debounce time is currently close to 2 Seconds.
#define SW_DEBOUNCE_INTERVAL   1460000L

#define SW0_TOGGLE_STATE	   SW0_GetValue()
#define SW1_TOGGLE_STATE	   SW1_GetValue()

// This will contain the device ID, before we have it this dummy value is the init value which is non-0
char attDeviceID[20] = "BAAAAADD1DBAAADD1D";
shared_networking_params_t shared_networking_params;
ATCA_STATUS retValCryptoClientSerialNumber;

uint32_t MAIN_dataTask(void *payload);
timerStruct_t MAIN_dataTasksTimer = {MAIN_dataTask};

void  wifiConnectionStateChanged(uint8_t status);

void application_init(){
	uint8_t mode = WIFI_DEFAULT;
	uint32_t sw0CurrentVal = 0;
	uint32_t sw1CurrentVal = 0;
	uint32_t i = 0;

   wdt_disable();

   // Initialization of modules before interrupts are enabled
   SYSTEM_Initialize();

   LED_test();
#if CFG_ENABLE_CLI
   CLI_init();
   CLI_setdeviceId(attDeviceID);
#endif
   debug_init(attDeviceID);

   ENABLE_INTERRUPTS();

   // Initialization of modules where the init needs interrupts to be enabled
   cryptoauthlib_init();

   if (cryptoDeviceInitialized == false)
   {
      debug_printError("APP: CryptoAuthInit failed");
   }
   // Get serial number from the ECC608 chip
   retValCryptoClientSerialNumber = CRYPTO_CLIENT_printSerialNumber(attDeviceID);
   if( retValCryptoClientSerialNumber != ATCA_SUCCESS )
   {
      switch(retValCryptoClientSerialNumber)
      {
         case ATCA_GEN_FAIL:
             debug_printError("APP: DeviceID generation failed, unspecified error");
         break;
         case ATCA_BAD_PARAM:
             debug_printError("APP: DeviceID generation failed, bad argument");
         default:
             debug_printError("APP: DeviceID generation failed");
         break;
      }

   }
#if CFG_ENABLE_CLI
   CLI_setdeviceId(attDeviceID);
#endif
   debug_setPrefix(attDeviceID);

   // Blocking debounce
   for(i = 0; i < SW_DEBOUNCE_INTERVAL; i++)
   {
	   sw0CurrentVal += SW0_TOGGLE_STATE;
	   sw1CurrentVal += SW1_TOGGLE_STATE;
   }
   if(sw0CurrentVal < (SW_DEBOUNCE_INTERVAL/2))
   {
	   if(sw1CurrentVal < (SW_DEBOUNCE_INTERVAL/2))
	   {
		   strcpy(ssid, CFG_MAIN_WLAN_SSID);
		   strcpy(pass, CFG_MAIN_WLAN_PSK);
		   authType = CFG_MAIN_WLAN_AUTH;
           LED_startBlinkingGreen();
	   }
	   else
	   {
		   mode = WIFI_SOFT_AP;
	   }
   }
   wifi_init(wifiConnectionStateChanged, mode);

   if (mode == WIFI_DEFAULT) {
      CLOUD_init(attDeviceID);
      timeout_create(&MAIN_dataTasksTimer, MAIN_DATATASK_INTERVAL);
   }

   LED_test();          // second LED sequence
}

void application_post_provisioning(void)
{
	CLOUD_init(attDeviceID);
	timeout_create(&MAIN_dataTasksTimer, MAIN_DATATASK_INTERVAL);
}


// React to the WIFI state change here. Status of 1 means connected, Status of 0 means disconnected
void  wifiConnectionStateChanged(uint8_t status)
{
   // If we have no AP access we want to retry
   if (status != 1)
   {
      // Restart the WIFI module if we get disconnected from the WiFi Access Point (AP)
      CLOUD_reset();
   }
}


// This scheduler will check all tasks and timers that are due and service them
void runScheduler(void)
{
    timeout_next();
}


// This could be better done with a function pointer (DI) but in the interest of simplicity
//     we avoided that. This is being called from MAIN_dataTask below
void sendToCloud(void);

// This gets called by the scheduler approximately every 100ms
uint32_t MAIN_dataTask(void *payload)
{
//    static time_t previousTransmissionTime = 0;
    static unsigned main_counter = 0;

   // Get the current time. This uses the C standard library time functions
   time_t timeNow = time(NULL);

#define TASK_PERIOD_MULTIPLE   (CFG_SEND_INTERVAL*1000/MAIN_DATATASK_INTERVAL)
// i.e. SEND_INTERVAL==1 sec, MAIN_DATATASK_INTERVAL==100ms -> PERIOD_MULTIPLE==10

    if (CLOUD_isConnected()) {
        // increment internal time keeper
        main_counter++;
        if (main_counter == (TASK_PERIOD_MULTIPLE)) {
            main_counter = 0;
            // update the system time
            timeNow += CFG_SEND_INTERVAL;
            set_system_time(timeNow);
            sendToCloud();
        }
    }
    else {
        main_counter = 0;     // reset timer when not connected
    }

   if (!shared_networking_params.haveAPConnection) {
        LED_BLUE_SetHigh();
    } else {
        LED_BLUE_SetLow();
    }
    if (!shared_networking_params.haveERROR) {
        LED_RED_SetHigh();
    } else {
        LED_RED_SetLow();
    }
    if (LED_isBlinkingGreen() == false) {
        if (!CLOUD_isConnected()) {
            LED_GREEN_SetHigh();
        } else {
            LED_GREEN_SetLow();
        }
    }

   // This is milliseconds managed by the RTC and the scheduler, this return makes the
   //      timer run another time, returning 0 will make it stop
   return MAIN_DATATASK_INTERVAL;
}
