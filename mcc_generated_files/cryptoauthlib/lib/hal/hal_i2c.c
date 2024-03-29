/**
 * \file
 * \brief ATCA Hardware abstraction layer for XMEGA-A3BU I2C over ASF drivers.
 *
 * Prerequisite: add I2C Master Polled support to application in Atmel Studio
 *
 * Copyright (c) 2015 Atmel Corporation. All rights reserved.
 *
 * \atmel_crypto_device_library_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel integrated circuit.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \atmel_crypto_device_library_license_stop
 */

#include <string.h>
#include "atca_hal.h"
#include "hal_i2c.h"
#include "../atca_device.h"
#include "../atca_status.h"
#include "../../../include/twi0_master.h"

#include "../../../config/clock_config.h"
#include <util/delay.h>

void hal_i2c_writeNBytes(twi0_address_t address, void *data, size_t len);
ATCA_STATUS hal_i2c_readNBytes(twi0_address_t address, void *data, size_t len);

/** \brief initialize an I2C interface using given config
 * \param[in] hal - opaque ptr to HAL data
 * \param[in] cfg - interface configuration
 */
ATCA_STATUS hal_i2c_init(void *hal, ATCAIfaceCfg *cfg)
{
	I2C0_Initialize();

	return ATCA_SUCCESS;
}


/** \brief HAL implementation of I2C post init
 * \param[in] iface  instance
 * \return ATCA_SUCCESS
 */
ATCA_STATUS hal_i2c_post_init(ATCAIface iface)
{
    return ATCA_SUCCESS;
}


/** \brief HAL implementation of I2C send over ASF
 * \param[in] iface     instance
 * \param[in] txdata    pointer to space to bytes to send
 * \param[in] txlength  number of bytes to send
 * \return ATCA_STATUS
 */

ATCA_STATUS hal_i2c_send(ATCAIface iface, uint8_t *txdata, int txlength)
{
	txdata[0] = 0x03; // insert the Word Address Value, Command token
	txlength++;       // account for word address value byte.

	hal_i2c_writeNBytes(0x58, txdata, txlength);

	return ATCA_SUCCESS;
}

/** \brief HAL implementation of I2C receive function for ASF I2C
 * \param[in] iface     instance
 * \param[in] rxdata    pointer to space to receive the data
 * \param[in] rxlength  ptr to expected number of receive bytes to request
 * \return ATCA_STATUS
 */

ATCA_STATUS hal_i2c_receive(ATCAIface iface, uint8_t *rxdata, uint16_t *rxlength)
{
	return hal_i2c_readNBytes(0x58, rxdata, *rxlength);
}

/** \brief wake up CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to wakeup
 */

ATCA_STATUS hal_i2c_wake(ATCAIface iface)
{
	I2C_0_wake_up(0x0, 0x0, 1);

	_delay_ms (2*1);

	uint8_t init_data[4];
	uint8_t verif_data[4] = { 0x04, 0x11, 0x33, 0x43 };
	uint8_t verification = true;

	hal_i2c_readNBytes(0x58, &init_data, 4);

	for (uint8_t i=0; i<4; i++)
		{
			if (init_data[i] != verif_data[i])
				verification = false;
		}

	if (verification)
		return ATCA_SUCCESS;
	else
		return ATCA_COMM_FAIL;

}

/** \brief idle CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to idle
 */

ATCA_STATUS hal_i2c_idle(ATCAIface iface)
{
	uint8_t data = 0x02;

	hal_i2c_writeNBytes(0x58, &data, 1);

	return ATCA_SUCCESS;
}

/** \brief sleep CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to sleep
 */

ATCA_STATUS hal_i2c_sleep(ATCAIface iface)
{
	uint8_t data = 0x01;

	hal_i2c_writeNBytes(0x58, &data, 1);

	return ATCA_SUCCESS;
}

void I2C_0_wake_up(uint8_t adr, uint8_t *data, uint8_t size)
{
	//transfer_descriptor_t d = {data, size};
	while (!I2C0_Open(adr))
	; // sit here until we get the bus..

	I2C0_SetDataCompleteCallback(I2C0_SetReturnStopCallback, NULL);

	// Transmit specified number of bytes
	I2C0_SetBuffer((void *)&adr, 1);
	// Start a Write operation
	I2C0_MasterWrite();
	while (I2C_BUSY == I2C0_Close())
	; // sit here until the entire chained operation has finished
}

/** \brief manages reference count on given bus and releases resource if no more refences exist
 * \param[in] hal_data - opaque pointer to hal data structure - known only to the HAL implementation
 * return ATCA_SUCCESS
 */

ATCA_STATUS hal_i2c_release(void *hal_data)
{
    //TODO: For the moment, don't do anything

    return ATCA_SUCCESS;
}

/** \brief discover i2c buses available for this hardware
 * this maintains a list of logical to physical bus mappings freeing the application
 * of the a-priori knowledge
 * \param[in] i2c_buses - an array of logical bus numbers
 * \param[in] max_buses - maximum number of buses the app wants to attempt to discover
 * \return ATCA_SUCCESS
 */

ATCA_STATUS hal_i2c_discover_buses(int i2c_buses[], int max_buses)
{

    //TODO: For the moment, don't do anything

    return ATCA_SUCCESS;
}

/** \brief discover any CryptoAuth devices on a given logical bus number
 * \param[in]  busNum  logical bus number on which to look for CryptoAuth devices
 * \param[out] cfg     pointer to head of an array of interface config structures which get filled in by this method
 * \param[out] found   number of devices found on this bus
 * \return ATCA_SUCCESS
 */

ATCA_STATUS hal_i2c_discover_devices(int busNum, ATCAIfaceCfg cfg[], int *found)
{
	//TODO: For the moment, don't do anything

    return ATCA_SUCCESS;
}

void hal_i2c_writeNBytes(twi0_address_t address, void *data, size_t len)
{
	while (!I2C0_Open(address))
		; // sit here until we get the bus..
	I2C0_SetBuffer(data, len);
	I2C0_SetAddressNackCallback(I2C0_SetRestartWriteCallback, NULL); // NACK polling?
	I2C0_MasterWrite();
	while (I2C_BUSY == I2C0_Close())
		; // sit here until finished.
}

ATCA_STATUS hal_i2c_readNBytes(twi0_address_t address, void *data, size_t len)
// special version that does not retry on nack but return fail
// this is necessary to support the ECC608A POLLING mode
{
    twi0_error_t ret;
	while (!I2C0_Open(address))
		; // sit here until we get the bus..
	I2C0_SetBuffer(data, len);
	I2C0_SetAddressNackCallback(I2C0_SetReturnNackCallback, NULL); // do not retry, return fail
	I2C0_MasterOperation(true);
    while (I2C_BUSY == (ret = I2C0_Close()))
		; // sit here until finished.
    return (ret == I2C_NOERR) ? ATCA_SUCCESS : ATCA_RX_NO_RESPONSE;
}
/** @} */
