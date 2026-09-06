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
 * \file	adc.h
 * \author 	David Meneses Herrera
 * \date	25.03.2024
 * \brief   ADC peripheral configuration header.
 */
#ifndef __CONFIG_ADC_H_
#define __CONFIG_ADC_H_

#include "DAVE.h"
#include <XMC4200.h> // SFR declarations of the selected device.

/**-----------------------------------------------------------------------------
 * \name Channel definitions.
 * @{
 */
#define ILA_CH		(4)
#define ILB_CH		(1)
#define VAC_CH		(3)
#define VDC_CH		(0)
#define T1_CH		(5)
#define T2_CH	    (6)

/** Master group in the synchronization between G0 and G1. */
#define MASTER			(1)
/** Slave group in the synchronization between G0 and G1. */
#define SLAVE			(0)
/** Synchronization between G0 and G1 required. */
#define SYNC_REQUIRED	(1)
/** PFC_A current measurement channel. P14.4 */
#define ADC_ILA_CH		VADC_G0, ILA_CH
/** PFC_B current measurement channel. P14.9 */
#define ADC_ILB_CH		VADC_G1, ILB_CH
/** AC voltage measurement channel. P14.3 */
#define ADC_VAC_CH		VADC_G0, VAC_CH
/** DC1 voltage measurement channel. P14.8 */
#define ADC_VDCP_CH		VADC_G1, VDC_CH
/** DC2 voltage measurement channel. P14.0 */
#define ADC_VDCBULK_CH  VADC_G0, VDC_CH
/** Temperature 1 measurement channel. PFC temperature. P14.5 */
#define ADC_T1_CH		VADC_G0, T1_CH
/** Temperature 2 measurement channel. LLC temperature. P14.6 */
#define ADC_T2_CH	    VADC_G0, T2_CH
/** @} */

/**-----------------------------------------------------------------------------
 * \name Result definitions.
 * @{
 */
#define ILX_RES		(1)
#define VDCX_RES	(15)
#define VAC_RES		(0)
#define NTC1_RES    (7)
#define NTC2_RES    (3)
/** @} */

/**-----------------------------------------------------------------------------
 * \name ADC measurements.
 * @{
 */
/** PFC A current measurement result register. */
#define ADC_ILA_RESULT		(VADC_G0->RES[ILX_RES] & 0xFFFF)
/** PFC A current measurement result register. */
#define ADC_ILB_RESULT		(VADC_G1->RES[ILX_RES] & 0xFFFF)
/** AC voltage result register. */
#define ADC_VAC_RESULT		(VADC_G0->RES[VAC_RES] & 0xFFFF)
/** Output voltage result register. */
#define ADC_VDCP_RESULT		(VADC_G1->RES[VDCX_RES] & 0xFFFF)
/** Output voltage result register. */
#define ADC_VBULK_RESULT    (VADC_G0->RES[VDCX_RES] & 0xFFFF)
/** Thermistor result register. PFC temperature. */
#define ADC_NTC1_RESULT     (VADC_G0->RES[NTC1_RES] & 0xFFFF)
/** Thermistor result register. LLC temperature. */
#define ADC_NTC2_RESULT     (VADC_G0->RES[NTC2_RES] & 0xFFFF)
/** @} */

/**-----------------------------------------------------------------------------
 * \name ADC Background and general definitions.
 * @{
 */
/** Individual result registers are used in the queue. */
#define INDIVIDUAL_RESULT_REGISTERS (0U)
/** Enable the external trigger of the VADC module. */
#define TRIGGER_ENABLE              (1)
/** Enable the external trigger of the VADC module. */
#define TRIGGER_DISABLE             (0)
/** ADC interrupt priority. */
#define VADC_SR0_PRIORITY	        (1U)
/** Group for background source. */
#define GROUP0						(0)
/** Group mask for background source. */
#define CH_BCKGND_G0				(0x20)
/** @} */

/** Output over voltage protection IRQ number. */
#define OVP_VDCP_IRQ_N           	24
#define ovp_vdcp_irq              	IRQ_Hdlr_24
#define OVP_VBULK_IRQ_N            	20
#define ovp_vbulk_irq            	IRQ_Hdlr_20
/** Activates over voltage protection. */
#define OVP_VBULK_ON()           	{NVIC_ClearPendingIRQ(OVP_VBULK_IRQ_N); \
                                  	NVIC_EnableIRQ(OVP_VBULK_IRQ_N);}
/** Activates over voltage protection. */
#define OVP_VDCP_ON()            	{NVIC_ClearPendingIRQ(OVP_VDCP_IRQ_N); \
	                                NVIC_EnableIRQ(OVP_VDCP_IRQ_N);}
/** Deactivates over voltage protection. */
#define OVP_VBULK_OFF()             {NVIC_DisableIRQ(OVP_VBULK_IRQ_N);}
/** Deactivates over voltage protection. */
#define OVP_VDCP_OFF()              {NVIC_DisableIRQ(OVP_VDCP_IRQ_N);}

/**-----------------------------------------------------------------------------
 * API Prototypes.
 */
void adc_init();
void die_temp_sens_init();

#endif /* __CONFIG_ADC_H_ */
