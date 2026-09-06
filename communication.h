/*
 *  Copyright (c) 2024, Infineon Technologies AG
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification,are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  - Neither the name of the copyright holders nor the names of its contributors
 *  may be used to endorse or promote products derived from this software without
 *  specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY,OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  To improve the quality of the software, users are encouraged to share
 *  modifications, enhancements or bug fixes with Infineon Technologies AG
 *  dave@infineon.com).
 */

/**-----------------------------------------------------------------------------
 * \file    communication.h
 * \author  Manuel Escudero Rodriguez
 * \date    11.05.2024
 * \brief   Communication code
 */
#ifndef __COMMUNICATION_H_
#define __COMMUNICATION_H_

#include "xmc_flash.h"

/** Message */
typedef union
{
    struct
    {
		uint8_t _fill[3];
        uint8_t command; /**< Protocol command. */
        union
        {
            uint32_t ui32[1];
            uint16_t ui16[2];
            uint8_t ui8[4];
        } payload; /**< Message data. */
    } msg;
    struct
    {
		uint8_t _fill[3];
        uint8_t ui8[5];
    } raw;
} msg_t;

/**-----------------------------------------------------------------------------
 * \name Serial communication commands
 * Communication blocks of 3 bytes. 1 byte command, 2 bytes payload.
 * |-------------|-------------|-------------|-------------|-------------|
 *     0x55				CMD			DATA_0		  DATA_1		 CRC
 * @{
 */

/** @}*/

/**-----------------------------------------------------------------------------
 * \name Serial communication payloads
 * Communication blocks of 3 bytes. 1 byte command, 2 bytes payload.
 * |-------------|-------------|-------------|-------------|-------------|
 *     0x55				CMD			DATA_0		  DATA_1		 CRC
 * @{
 */
/** Clock divider for message transmission frequency. */
#define UART_TRANSMIT_CLK_DIV	(2)

/*----------------------------------------------------------------------------*/
extern uint32_t XMC_USIC_CH_TXFIFO_GetLevel(XMC_USIC_CH_t *const channel);
void uart_DCDC_transmit();
void uart_DCDC_receive();
void load_ROM_parameters();
void check_RAM_parameters();

#endif /* __COMMUNICATION_H_ */
