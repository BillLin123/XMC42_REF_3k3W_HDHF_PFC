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
 * \file    control_irq_v_loop_outer.h
 * \author  Meneses Herrera, David; Escudero, Manuel
 * \date    25.03.2024
 * \brief   Interrupts code
 */
#ifndef CONTROL_IRQ_V_LOOP_OUTER_H_
#define CONTROL_IRQ_V_LOOP_OUTER_H_

#include <DAVE.h>
#include "control_irq_i_loop_low_prio.h"
#include "init_bckgnd_funct.h"
#include "communication.h"
#include "config_adc.h"

/**-----------------------------------------------------------------------------
 * Type definition
 */
/** IIR filter structure, used in AC-RMS and DC-Notch filtering */
typedef struct
{
    int16_t k1;        	/**< Filter input gain, section 1. Q format. */
    int16_t a1[3];    	/**< Filter numerator coefficients, section 1. Q format. */
    int16_t b1[3];		/**< Filter denominator coefficients, section 1. Q format. */
    int32_t u1[2];    	/**< Intermediate calculation, section 1. */
    int16_t k2;       	/**< Filter input gain, section 2. Q format. */
    int16_t a2[3];    	/**< Filter numerator coefficients, section 2. Q format. */
    int16_t b2[3];    	/**< Filter denominator coefficients, section 2. Q format. */
    int32_t u2[2];    	/**< Intermediate calculation, section 2. */
    int32_t output_1st;	/**< Output of the filter, section 1. */
    int32_t output;  	/**< Output of the filter. Q format */
} IIR_lowpass_t;

/**-----------------------------------------------------------------------------
 * External global variables.
 */
/* AC-RMS Filter instance. */
extern IIR_lowpass_t AC_filter;
extern IIR_lowpass_t DC_filter;
/* Scaled DC voltage for filter operation. */
extern uint16_t time_count_5kHz;
extern uint16_t time_INIT;
extern uint8_t time_vbulk_step;
extern uint8_t time_vdcp_step;

/**-----------------------------------------------------------------------------
 * Defines
 */
/**-----------------------------------------------------------------------------
 * \name Bulk voltage PFC related macros.
 * @{
 */
/** Bulk voltage target, Q15 representation. Used for the control loop reference and resume value after OVP */
#define VBULK_STEADY         	(25800) // 24130 --> 400VDC / 3.3 Vdd / (309 * 5) + 10
/** Over voltage protection threshold ADC boundaries. Around 450V. [ADC_units] */
#define VBULK_OVP_FAST_STOP  	(3680) // (VBULK_STEADY >> 3) // Circa 460 V  / 3.3 Vdd / (309 * 5) + 10 // 1600 corresponds to 200
/** Maximum bulk voltage allowed, Q15 representation. Reaching this voltage starts "burst mode" */
#define VBULK_OVP_BURST_STOP 	(27600) // 27600 --> 432 VDC / 3.3 Vdd / (309 * 5) + 10
/** Maximum bulk voltage allowed, Q15 representation. Reaching this voltage starts "burst mode" */
#define VBULK_OVP_STOP     		(28500) // 28500 --> 446.5 VDC / 3.3 Vdd / (309 * 5) + 10
#if (VBULK_OVP_STOP <= VBULK_STEADY)
    #error "STOP threshold should be greater than steady state voltage"
#endif
#if ZERO_CROSS_DEBUG
/** Undervoltage protection (300V). Turns PFC off and open relay. (Q15) */
#define VBULK_UVP_MIN          	(0)
#else
/** Undervoltage protection (300V). Turns PFC off and open relay. (Q15) */
#define VBULK_UVP_MIN          	(15980) // 250V // 18097 --> 283 VDC / 3.3 Vdd / (309 * 5) + 10
#endif
/** Output voltage before start-up must be higher than 75 * sqrt(2) (Q15). */
#define VBULK_START_MIN      	(6825) // 6825 --> 107 Vdc / 3.3 Vdd / (309 * 5) + 10
/** Output voltage offset limit to apply non-linear loop error. */
#define VBULK_OFFSET_NL_ON    	(22330) // 350 V // 23225 --> 364 Vdc / 3.3 Vdd / (309 * 5) + 10
/** Output voltage offset limit to stop applying non-linear loop error. */
#define VBULK_OFFSET_NL_OFF   	(603) // 603 --> 10Vdc / 3.3 Vdd / (309 * 5) + 10
#if (VOBULK_OFFSET_NL_ON >= (VBULK_STEADY - VBULK_OFFSET_NL_OFF))
    #error "VOBULK_OFFSET_NL_ON is not smaller than (VBULK_STEADY - VBULK_OFFSET_NL_OFF)"
#endif
/** Minimum PR VDC for DFF calculation. */
#define VBULK_PR_VDC_MIN		(12406)
/** Maximum PR VDC for DFF calculation. */
#define VBULK_PR_VDC_MAX		(39138)
/** Slope of PR_VDC calculation for DFF calculation. */
#define VBULK_PR_VDC_SLOPE		(23938)
#define VBULK_10V				(600)
/** Decimal places of the PR_VDC slope calculation for DFF. */
#define VBULK_PR_VDC_Q			(14)
/** Limiting factor for the maximum Vac to be applied PR_VDC. */
#define VBULK_PR_VDC_AC_FCTR	(4407)
/** Slope of the VBULK_PR_VDC_AC_FCTR. */
#define VBULK_PR_VDC_AC_Q		(15)
#define VLOOP_PRIORITY          (3)
/** Voltage loop ISR. Triggered by Systick. */
#define voltage_loop_ISR	    SysTick_Handler
/** @} */

/**-----------------------------------------------------------------------------
 * \name VDCP voltage baby boost related defines.
 * @{
 */
/** Maximum bulk voltage allowed, Q15 representation. Reaching this voltage starts "burst mode" */
#define VDCP_OVP_STOP     		(27685) // Circa 440 Vdc
/** Steady state voltage regulation of baby boost. */
#define BBY_VDCP_STDY			(25797) // Circa 410Vdc
/** Ramp up  of reference. End of open loop start-up. */
#define BBY_VDCP_STRT_UP_END	(BBY_VDCP_STDY - 100)
/** Threshold to stop the baby boost and come back to wait state. */
#define VBULK_BBY_STOP			(25482) // Circa 405 Vdc //(24539) // Circa 390 Vdc
/** Threshold to open the Static Switch and start the baby boost. */
#define VBULK_BBY_START         (24890) // 380Vdc // (23920) is Circa 370 Vdc
/** Threshold to stop the baby boost and come back to wait state. */
#define VBULK_BBY_MIN			(15700) // 250V // 12584 is Circa 200 Vdc
/** Slope of the duty feed-forward from the input voltage. */
#define BBY_FFW_VIN_SLOPE		(1000) //(2750)//(2640) // Feedforward for 210Vdc //(1355) // Feedforward slope for 400V output of the baby boost.

/** Switching period of the auxiliary boost. */
#define AUX_BOOST_PR			(420) // 175kHz //(530) // 151 kHz
/** Maximum duty of the auxiliary boost. */
#define AUX_BOOST_MAX_DTY		(355) // (450) // 2.5 us -- 85% duty
/** Minimum duty of the auxiliary boost. */
#define AUX_BOOST_MIN_DTY		(4)//(5) // A 3 corresponds to a pulse of 12.5 ns coming out.
/** High resolution part of the auxiliary boost. */
#define AUX_BOOST_MIN_HRES_DTY	(0) // 150 ps duty adjust.
#define AUX_BOOST_MAX_HRES_DTY	(82) // 150 ps duty adjust.
/** PID KP coefficient for the baby boost. */
#define BBY_KP	(200)
/** PID KI coefficient for the baby boost. */
#define BBY_KI	(40)
/** PID KD coefficient for the baby boost. */
#define BBY_KD	(85)
/** @} */

/**-----------------------------------------------------------------------------
 * \name Soft start related macros
 * @{
 */
/**
 * Number of state-machine cycles (approx. 1.6ms - 5Khz/8) before checking the
 * bulk voltage after input voltage is in range and the decision of closing the
 * relay is taken.
 */
#define ST_STB_BOP_CHECK_BULK	(190)
/** Number of cycles before starting the converter after closing the relay. */
#define ST_STB_BOP_SFTSTRT_WAIT (20 + ST_STB_BOP_CHECK_BULK)
/** Cycle after waiting time the converter operation can start. */
#define ST_STB_BOP_END	        (ST_STB_BOP_SFTSTRT_WAIT + 1)
/** Number of cycles set to high value after soft-start process is done. */
#define ST_SFTSTRT_END	        (ST_STB_BOP_END + 7)
/** @} */

/**-----------------------------------------------------------------------------
* \brief		Adjust peak current reference value.
* \param ref  	Analog comparator DAC reference value.
* \return     	None
*
* The DAC accepts values between a minimum 0 and a maximum 0xFFF. This sets the references
* of the internal high resolution comparators.
*/
__attribute__((section(".ram_code")))
static inline void peak_ref_set(uint16_t ref)
{
	HRPWM0_CSG0->SDSV1 = ref; // XMC_HRPWM_CSG_UpdateDACRefDSV1(DAC_CMP_AUX.csg_slice_ptr, (uint32_t)ref);
	HRPWM0->CSGTRG = XMC_HRPWM_SHADOW_TX_DAC0; // XMC_HRPWM_EnableComparatorShadowTransfer(DAC_CMP_AUX.csg_module_ptr, XMC_HRPWM_SHADOW_TX_DAC0);
}

/**-----------------------------------------------------------------------------
* \brief		Adjust peak current reference value.
* \param ref  	Analog comparator DAC reference value.
* \return     	None
*
* The DAC accepts values between a minimum 0 and a maximum 0xFFF. This sets the references
* of the internal high resolution comparators.
*/
__attribute__((section(".ram_code")))
static inline void aux_boost_duty_set(uint16_t duty, uint16_t hres_duty)
{
	CCU80_CC80->CR1S = duty; /* PWM duty cycle. */
    HRPWM0_HRC0->SCR2 = hres_duty;
	CCU80->GCSS = CCU8_GCSS_S0SE_Msk; /* Shadow transfer allowed. */
    HRPWM0->HRCSTRG = HRPWM0_HRCSTRG_H0ES_Msk; /* Shadow transfer of high-resolution part. */
}

/**-----------------------------------------------------------------------------
 * \brief   Baby boost voltage loop.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void Vdcp_ctrl_loop()
{
	//static uint16_t debug_cnt;
	static uint16_t burst_cnt;
	static uint32_t bby_duty_vin_ffw; /**< Feed-forward of the duty by the input voltage. */

	/* Voltage error (Q15). */
	PFC.vdcp = ADC_VDCP_RESULT << ADC_TO_Q15;
	int32_t error = (int32_t)voltage_vdcp_ctr.ref - PFC.vdcp;

	/* Burst mode of the bby_boost. */
	if ((PFC.vdcp > VDCP_OVP_STOP) && !voltage_vdcp_ctr.burst_mode_on)
	{
		if (burst_cnt < 1)
		{
			burst_cnt ++;
		}
		else
		{
			PWM_AUX_OFF();
			voltage_vdcp_ctr.burst_mode_on = true;
			/* Help the loop to move into steady state faster with non-linear decrement. */
			voltage_vdcp_ctr.output_int.i16[1] -= 5;
		}
	}
	/* Coming back to switch when error is positive again. */
	else if (error > 0)
	{
		if(voltage_vdcp_ctr.burst_mode_on)
		{
			PWM_AUX_ON();
			burst_cnt = 0;
			voltage_vdcp_ctr.burst_mode_on = false;
			voltage_vdcp_ctr.error[1] = error;
			voltage_vdcp_ctr.error[0] = error;
		}
	}
	/* End of the ramp_up when error is negative. */
	if(error < 0)
	{
		/*
		 * Voltage loop is enabled after the feedforward has moved the voltage into the regulation area.
		 * Avoid the integral part to over-shoot at end of ramp-up.
		 */
		voltage_vdcp_ctr.ramp_up_on = false;
	}

	if ((!voltage_vdcp_ctr.ramp_up_on) && (!voltage_vdcp_ctr.burst_mode_on))
	{
		/* PID compensator. */
		int32_t int_calc = voltage_vdcp_ctr.a[0] * error + voltage_vdcp_ctr.a[1] * voltage_vdcp_ctr.error[0] + voltage_vdcp_ctr.a[2] * voltage_vdcp_ctr.error[1];
		voltage_vdcp_ctr.output_int.i32 = (int_calc >>  2) + voltage_vdcp_ctr.output_int.i32;

		/* Voltage control loop saturation. */
		if (voltage_vdcp_ctr.output_int.i16[1] > AUX_BOOST_MAX_DTY)
		{
			voltage_vdcp_ctr.output_int.i16[1] = AUX_BOOST_MAX_DTY;
		}
		else if (voltage_vdcp_ctr.output_int.i16[1] < -AUX_BOOST_MAX_DTY)
		{
			voltage_vdcp_ctr.output_int.i16[1] = -AUX_BOOST_MAX_DTY;
		}
		/* Save error for next calculation. */
		voltage_vdcp_ctr.error[1] = voltage_vdcp_ctr.error[0];
		voltage_vdcp_ctr.error[0] = error;
	}

	/* Calculation of voltage feed-forward. */
	if (voltage_vdcp_ctr.ref > PFC.vbulk)
	{
		/* The closer the input voltage to the output voltage, the smaller the feed-forward. */
	    /* Feed-forward of DCDC load computed in the outer voltage loop. */
		bby_duty_vin_ffw = ((((uint32_t)(voltage_vdcp_ctr.ref - PFC.vbulk) * voltage_vdcp_ctr.bby_ffw_slope) + bby_duty_vin_ffw) >> 1);
	}
	voltage_vdcp_ctr.output_ffw.i32 = voltage_vdcp_ctr.output_int.i32 + bby_duty_vin_ffw;
	if (voltage_vdcp_ctr.output_ffw.i16[1] > AUX_BOOST_MAX_DTY)
	{
		voltage_vdcp_ctr.output_ffw.i16[1] = AUX_BOOST_MAX_DTY;
	}
	else if (voltage_vdcp_ctr.output_ffw.i16[1] < AUX_BOOST_MIN_DTY)
	{
		voltage_vdcp_ctr.output_ffw.i16[1] = AUX_BOOST_MIN_DTY;
	}
    /* High resolution part scaled to maximum 82 for the fraction. */
    uint16_t hres_duty;
    hres_duty = ((((uint16_t)voltage_vdcp_ctr.output_ffw.i16[0] >> (16 - 8)) * 82) >> 8);
    aux_boost_duty_set(voltage_vdcp_ctr.output_ffw.i16[1], hres_duty);

    /* Debugging stop of pulses. */
//    debug_cnt ++;
//    if (debug_cnt > 2000)
//    {
//    	PWM_AUX_OFF();
//    	status_bbst_reg = ST_LATCH;
//    }
} // 1.412 us. Some 113 instructions

#endif /* CONTROL_IRQ_V_LOOP_OUTER_H_ */
