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
 * \file	communication.c
 * \author 	Manuel Escudero Rodriguez; David Meneses Herrera
 * \date	15.02.2024
 * \brief   Communication functions
 */
#include <control_irq_v_loop_outer.h>
#include <DAVE.h>
#include "communication.h"

/** UART reception buffer */
static msg_t UART_rx_buffer;

/**-----------------------------------------------------------------------------
 * \name UART DCDC variables
 * @{
 */
/** Counter to select the type of data sent to the secondary controller. */
uint8_t msg_dcdc_cnt = 0;
/** Status received from secondary controller. */
uint16_t dcdc_fault_reg = 0;
/** @} */

/**-----------------------------------------------------------------------------
 * \brief   UART transmit data.
 * \return  None
 * \param   command Protocol command to be transmitted.
 * \param   msg     Two byte payload of the message.
 *
 * Non blocking function. No interrupts involved.
 * |-------------|-------------|-------------|-------------|-------------|
 *     0x55             CMD         DATA_0        DATA_1         CRC
 */
__attribute__((section(".ram_code")))
static inline void uart_tx_half_word(uint16_t msg)
{
    /* Writes fixed header. */
    XMC_UART_CH_Transmit(UART_DCDC.channel, 0x55);
    /* Writes 2 byte payload. */
    XMC_UART_CH_Transmit(UART_DCDC.channel, ((uint8_t *)&msg)[0]);
    XMC_UART_CH_Transmit(UART_DCDC.channel, ((uint8_t *)&msg)[1]);
}

/**-----------------------------------------------------------------------------
 * \brief   UART transmit data.
 * \return  None
 *
 * Non blocking function. No interrupts involved.
 * Update msg_cnt to 0, function will iterate over all possible output messages.
 */
__attribute__((section(".ram_code")))
void uart_DCDC_transmit()
{
    static uint32_t previous_tick; /* Previous transmission tick. */
	static uint32_t vin_filtered; /* Vbulk filtered. */
	static bool vin_in_range; /* PFC must end the start-up before allowing the DCDC to start. */

    /*
     * Time division of the controller clock. May limit the speed of
     * message transmission to avoid overflow of the FIFO in the PC side.
     */
    if((time_count_5kHz >> 2) != previous_tick)
    {
        previous_tick = (time_count_5kHz >> 2);
        /* If the FIFO is already empty fills it again */
        if(!XMC_USIC_CH_TXFIFO_GetLevel(UART_DCDC.channel))
        {
            switch (msg_dcdc_cnt)
            {
                case 0:
                    /* Send DC bus voltage, including OTP. */
                    /* PFC keeps off until the temperature goes under certain value (hysteresis) (50°C). */
                    if (fault_ttp_reg & F_OTP)
                    {
                        vin_filtered = 0;
                    }
                    /* If no OTP, send DC value. */
                    else
                    {
                    	/*----------------------------------------------------*/
                    	/* For the LLC to start only when the PFC has achieved the steady state voltage. */
						if ((status_ttp_reg == ST_RUNNING) && (voltage_vbulk_ctr.error < 0))	//comment for LLC only in debug mode
						{																	//comment for LLC only in debug mode
							vin_in_range = true;
						}																	//comment for LLC only in debug mode

                    	/**\todo Comment for normal mode. */
//                    	vin_in_range = false; // Comment this out for normal mode.
						if (vin_in_range)
						{
							vin_filtered = (PFC.vdcp + (vin_filtered * 3)) >> 2;
						}
						else
						{
							vin_filtered = 0;
						}
                    }
                    uart_tx_half_word((0x8000 | ((uint16_t)(vin_filtered >> ADC_TO_Q15) & 0x7FFF)));
                    msg_dcdc_cnt = 0;
                    break;

                default:
                    msg_dcdc_cnt = 0;
                    break;
            }
        }
    }
}

/**-----------------------------------------------------------------------------
 * \brief   UART receive data.
 * \return  None
 *
 * Non blocking function. No interrupts involved.
 * Checks available data in the reception FIFO and execute command when a
 * message is received.
 * |-------------|-------------|-------------|-------------|-------------|
 *     0x55             CMD         DATA_0        DATA_1         CRC
 */
__attribute__((section(".ram_code")))
void uart_DCDC_receive()
{
    static uint8_t UART_rx_index = 0xFF; /* UART reception buffer index */
    uint16_t read_val;

    /* Checks new data in the reception FIFO. */
    if(XMC_USIC_CH_RXFIFO_GetLevel(UART_DCDC.channel))
    {
        read_val = (uint16_t) UART_DCDC.channel->OUTR;
        /* Heading of a UART message. */
        if (UART_rx_index == 0xFF)
        {
            /* Message header. */
            if (read_val == 0x55) UART_rx_index = 1;
        }
        /* Body of the message. Each message consist of 4 bytes plus 1 byte header. */
        else if (UART_rx_index < 2)
        {
            UART_rx_buffer.raw.ui8[UART_rx_index] = read_val;
            UART_rx_index ++;
        }
        /* Ending of a 3 byte message. */
        else
        {
            UART_rx_buffer.raw.ui8[UART_rx_index] = read_val;
            if (UART_rx_buffer.msg.payload.ui16[0] & 0x8000)
            {
                /* In this message there is failure info. */
                dcdc_fault_reg = (UART_rx_buffer.msg.payload.ui16[0] & 0x6FFF);
            }
            /* Body of the message. */
            else
            {
                //__asm__ __volatile__ ("" : : : "memory");
                //GREEN_LED_TGL();
            	//__asm__ __volatile__ ("" : : : "memory");
                /* Receive output current value (Four samples moving average). */
            	PFC.adc_io_measure = (PFC.adc_io_measure + UART_rx_buffer.msg.payload.ui16[0]) >> 1;
            }
            UART_rx_index = 0xFF;
        }
    }
}
