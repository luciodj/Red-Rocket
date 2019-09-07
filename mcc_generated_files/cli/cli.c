/*
    \file   cli.c

    \brief  Command Line Interpreter source file.

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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <avr/wdt.h>

#include "cli.h"
#include "../cloud/wifi_service.h"
#include "../cloud/cloud_service.h"
#include "../cloud/crypto_client/crypto_client.h"
#include "../mqtt/mqtt_core/mqtt_core.h"
#include "../debug_print.h"
#include "../mcc.h"

// WINC authentication type codes
#define WIFI_PARAMS_OPEN        1
#define WIFI_PARAMS_PSK         2
#define WIFI_PARAMS_WEP         3

#define MAX_COMMAND_SIZE        100
#define MAX_PUB_KEY_LEN         200
#define NEWLINE                 "\r\n"

#define UNKNOWN_CMD_MSG "Unknown command! Available commands:" NEWLINE\
                        "reset"NEWLINE\
                        "device"NEWLINE\
                        "key"NEWLINE\
                        "reconnect" NEWLINE\
                        "version" NEWLINE\
                        "cli_version" NEWLINE\
                        "wifi <ssid>[,<pass>,[authType]]" NEWLINE\
                        "debug" NEWLINE\
                        NEWLINE"\4"

static char command[MAX_COMMAND_SIZE];
static bool isCommandReceived = false;
static uint8_t index = 0;
static bool commandTooLongFlag = false;

const char * const cli_version_number             = "2.1";
const char * const firmware_version_number        = "RedRocket(1.2)";

static void command_received(char *command_text);
static void reset_cmd(char *pArg);
static void reconnect_cmd(char *pArg);
static void set_wifi_auth(char *ssid_pwd_auth);
static void get_public_key(char *pArg);
static void get_device_id(char *pArg);
static void get_cli_version(char *pArg);
static void get_firmware_version(char *pArg);
static void set_debug_level(char *pArg);

static bool endOfLineTest(char c);
static void enableUsartRxInterrupts(void);

#define CLI_TASK_INTERVAL      50

uint32_t CLI_task(void*);
timerStruct_t CLI_task_timer             = {CLI_task};

struct cmd
{
    const char * const command;
    void (* const handler)(char *argument);
};

const struct cmd commands[] =
{
    { "reset",       reset_cmd},
    { "reconnect",   reconnect_cmd },
    { "wifi",        set_wifi_auth },
    { "key",         get_public_key },
    { "device",      get_device_id },
    { "cli_version", get_cli_version },
    { "version",     get_firmware_version },
    { "debug",       set_debug_level }
};

void CLI_init(void)
{
    enableUsartRxInterrupts();
    timeout_create(&CLI_task_timer, CLI_TASK_INTERVAL);
}

static bool endOfLineTest(char c)
{
   static char test = 0;
   bool retvalue = true;

   if(c == '\n')
   {
      if(test == '\r')
      {
         retvalue = false;
      }
   }
   test = c;
   return retvalue;
}

uint32_t CLI_task(void* param)
{
    bool cmd_rcvd = false;
	char c = 0;
   // read all the EUSART bytes in the queue
   while(USART2_IsRxReady() && !isCommandReceived)
   {
      c = USART2_Read();
      // read until we get a newline
      if(c == '\r' || c == '\n')
      {
         command[index] = 0;

         if(!commandTooLongFlag)
         {
            if( endOfLineTest(c) && !cmd_rcvd )
            {
               command_received((char*)command);
			   cmd_rcvd = true;
            }
         }
         if(commandTooLongFlag)
         {
            printf(NEWLINE"Command too long"NEWLINE);
         }
         index = 0;
         commandTooLongFlag = false;
      }
      else // otherwise store the character
      {
         if(index < MAX_COMMAND_SIZE)
         {
            command[index++] = c;
         }
         else
         {
            commandTooLongFlag = true;
         }
      }
   }

   return CLI_TASK_INTERVAL;
}

static void set_wifi_auth(char *ssid_pwd_auth)
{
    char *credentials[3];
    char *pch;
    uint8_t params = 0;
	uint8_t i;
    bool res = true;

    for(i=0; i<=2; i++)
        credentials[i] = NULL;

    pch = strtok(ssid_pwd_auth, ",");
    while (pch != NULL && params <= 2)
    { // count the comma separated parameters passed
        credentials[params] = pch;
        params++;
        pch = strtok(NULL, ",");
    }

    if (credentials[0] != NULL)
    { // infer the authtype from the number of parameters passed
        authType = 0; // init the authentication type (invalid)
        if (credentials[1]==NULL && credentials[2]==NULL)
            // provided only ssid
            authType = WIFI_PARAMS_OPEN;
        else if (credentials[2]==NULL)
            // provided ssid AND password
            authType = WIFI_PARAMS_PSK;  // default to PSK
        else
            // provided ssid, psw AND authtype code
            authType = atoi(credentials[2]);
    }

    switch (authType)
    {
        case WIFI_PARAMS_OPEN:
                strncpy(ssid, credentials[0], M2M_MAX_SSID_LEN-1);
                pass[0] = '\0';
            break;

        case WIFI_PARAMS_PSK:
		case WIFI_PARAMS_WEP:
                strncpy(ssid, credentials[0], M2M_MAX_SSID_LEN-1);
                strncpy(pass, credentials[1], M2M_MAX_PSK_LEN-1);
            break;

        default:
			res = false;
            break;
    }
    printf("ssid:%s\n", ssid);
    printf("psw :%s\n", pass);
    printf("type:%d\n", authType);
    printf("res :%d\n", res);

	if (res)
	{
		printf("OK\r\n\4");
        if(CLOUD_isConnected())
            MQTT_Close(MQTT_GetClientConnectionInfo());
		wifi_disconnectFromAp();
	}
	else
		printf("Error: command format is wifi <ssid>[,<pass>,[authType]]\r\n\4");
}

static void reconnect_cmd(char *pArg)
{
    (void)pArg;

    MQTT_Disconnect(MQTT_GetClientConnectionInfo());
    printf("OK\r\n");
}

static void reset_cmd(char *pArg)
{
	(void)pArg;

	wdt_enable(WDTO_30MS);
	while(1) {};
}

static void set_debug_level(char *pArg)
{
   debug_severity_t level = SEVERITY_NONE;
   if(*pArg >= '0' && *pArg <= '4')
   {
      level = *pArg - '0';
      debug_setSeverity(level);
      printf("OK\r\n");
   }
   else
   {
      printf("debug parameter must be between 0 (Least) and 4 (Most) \r\n");
   }
}

static void get_public_key(char *pArg)
{
    char key_pem_format[MAX_PUB_KEY_LEN];
//    (void)pArg;

    if (CRYPTO_CLIENT_printPublicKey(key_pem_format) == NO_ERROR)
    {
        printf(key_pem_format);
    }
    else
    {
        printf("Error getting key.\r\n\4");
    }
}

static char *ateccsn = NULL;
void CLI_setdeviceId(char *id)
{
   ateccsn = id;
}

static void get_device_id(char *pArg)
{
   (void) pArg;
    if (ateccsn)
    {
        printf("%s\r\n\4", ateccsn);
    }
    else
    {
        printf("Unknown.\r\n\4");
    }
}

static void get_cli_version(char *pArg)
{
    (void)pArg;
    printf("v%s\r\n\4", cli_version_number);
}

static void get_firmware_version(char *pArg)
{
    (void)pArg;
    printf("%s\r\n\4", firmware_version_number);
}

static void command_received(char *command_text)
{
    char *argument = strstr(command_text, " ");
    uint8_t cmp;
    uint8_t ct_len;
    uint8_t cc_len;
	uint8_t i = 0;

    if (argument != NULL)
    {
        /* Replace the delimiter with string terminator */
        *argument = '\0';
        /* Point after the string terminator, to the actual string */
        argument++;
    }

    for (i = 0; i < sizeof(commands)/sizeof(*commands); i++)
    {
        cmp = strcmp(command_text, commands[i].command);
        ct_len = strlen(command_text);
        cc_len = strlen(commands[i].command);

        if (cmp == 0 && ct_len == cc_len)
        {
            if (commands[i].handler != NULL)
            {
                commands[i].handler(argument);
                return;
            }
        }
    }

    printf(UNKNOWN_CMD_MSG);
}

static void enableUsartRxInterrupts(void)
{
    // Empty RX buffer
    do {
        (void)USART2.RXDATAL;
    } while ((USART2.STATUS & USART_RXCIF_bm) != 0);

    // Enable RX interrupt
    USART2.CTRLA |=  1 << USART_RXCIE_bp;
}
