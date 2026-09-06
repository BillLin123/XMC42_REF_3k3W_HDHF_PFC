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
 * \file    control_interrupts.c
 * \author  Meneses Herrera, David
 * \date	13.02.2024
 * \brief   Control related interrupts code
 */
#include <DAVE.h>
#include <control_irq_v_loop_outer.h>
#include "config_adc.h"
#include "init_bckgnd_funct.h"

/** AC-RMS Filter instance. */
IIR_lowpass_t AC_filter;
/** Bulk-Notch Filter instance. */
IIR_lowpass_t DC_filter;
/** Time counting based on PWM interrupt. */
uint16_t time_count_5kHz = 0;
/** Counter to define time step during voltage ramp in PFC soft-start. */
uint8_t time_vbulk_step = 0;
/** Counter to define time step during voltage ramp in PFC soft-start. */
uint8_t time_vdcp_step = 0;

/**-----------------------------------------------------------------------------
 * \brief   Output voltage out of range protection.
 * \return  None
 *
 * This will protect against short circuit at the output as well as any abnormal
 * situation leading to the output voltage out of the normal range.
 * This will protect also against the output voltage rising above the limit.
 * This is a latching protection.
 */
__attribute__((section(".ram_code")))
void ovp_vdcp_irq()
{
    /* Turn-off PWM and SR */
    PWM_AUX_OFF();
    status_bbst_reg = ST_SOFT_START;
    voltage_vdcp_ctr.burst_mode_on = true;
    /*
     * The interrupt is activated by level, if the output voltage remains outside
     * of the normal range this would continue being triggered.
     * This is a latching protection. After reset the protection would be activated
     * again.
     */
    OVP_VDCP_OFF();
}

/**-----------------------------------------------------------------------------
 * \brief   Output voltage out of range protection.
 * \return  None
 *
 * This will protect against short circuit at the output as well as any abnormal
 * situation leading to the output voltage out of the normal range.
 * This will protect also against the output voltage rising above the limit.
 * This is a latching protection.
 */
__attribute__((section(".ram_code")))
void ovp_vbulk_irq()
{
    /* Set fault flag. */
    fault_ttp_reg |= F_OVP;
    /* Turn-off PWM and SR. */
    PWM_OFF();
    /* Power stage is OFF, loops not executed. */
    ttp_voltage_loop_ON = false;
    ttp_current_loop_ON = false;
    /*
     * The interrupt is activated by level, if the output voltage remains outside
     * of the normal range this would continue being triggered.
     * This is a latching protection. After reset the protection would be activated
     * again.
     */
    OVP_VBULK_OFF();
}

/**-----------------------------------------------------------------------------
 * \brief   Cascaded IIR filter
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void cascade_filter(int16_t input, IIR_lowpass_t *filter)
{
    /* Section 1 calculation. */
    int32_t u1 = ((input * filter->k1) - ((int64_t)filter->u1[0] * filter->b1[1]) - ((int64_t)filter->u1[1] * filter->b1[2])) >> IIR_QFORMAT;
    /* Q15 output is the input of the next section */
    int32_t output_1st = u1 + filter->u1[1] + (((int64_t)filter->u1[0] * filter->a1[1]) >> IIR_QFORMAT);
    /* Save section 1 values for next iteration. */
    filter->u1[1] = filter->u1[0];
    filter->u1[0] = u1;
    /* Section 2 calculation. */
    int32_t u2 = ((filter->k2 * output_1st) - ((int64_t)filter->u2[0] * filter->b2[1]) - ((int64_t)filter->u2[1] * filter->b2[2])) >> IIR_QFORMAT;
    filter->output = u2 + filter->u2[1] + (((int64_t)filter->u2[0] * filter->a2[1]) >> IIR_QFORMAT);
    /* Save section 2 values for next iteration. */
    filter->u2[1] = filter->u2[0];
    filter->u2[0] = u2;
}

/**-----------------------------------------------------------------------------
 * \brief   Cascade notch filter of bulk voltage for voltage loop.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void notch_filter_Vbulk()
{
	/* Input to the filter. Scaled DC voltage for filter operation. */
    cascade_filter(PFC.vbulk, &DC_filter);
    /* Limit to avoid negative and zero numbers. */
    if(DC_filter.output < 0)
    {
    	DC_filter.output = 0;
    }
} // 1.026 us. circa 82 instructions.

/**-----------------------------------------------------------------------------
 * \brief   LPF of AC voltage for "RMS" calculation.
 *          The result is the average of the AC voltage, which is proportional
 *          to the AC RMS value. RMS IIR filter calculation (LPF),
 *          two-section Direct-form II.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void lpf_filter_RMS()
{
	cascade_filter((PFC.vin_AC << ADC_TO_Q15), &AC_filter);
    /* Low limit of AC-filter output to avoid division by zero or a small RMS value. */
    if (AC_filter.output < (VRMS_MIN_LIMIT << LPF_TO_2ADC))  // 1176 (80V_RMS) is half of the minimum voltage considered to start the converter (2589 --> 176V_RMS)
    {
    	AC_filter.output = (VRMS_MIN_LIMIT << LPF_TO_2ADC);
    }
    /* "RMS" value for multiplier input. */
    PFC.vin_BOP = AC_filter.output >> LPF_TO_2ADC;
} // 1.064 us, circa 85 instructions

/**-----------------------------------------------------------------------------
 * \brief   State machine of baby boost.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void state_machine_bbst()
{
	static uint16_t bby_strt_dly_cnt;
	static uint16_t bby_stop_dly_cnt;
	static uint16_t bby_min_dly_cnt;
	static uint16_t bby_wait_cnt;
	static uint16_t bby_wrk_time_cnt;
	static uint16_t bby_vac_lost_cnt;

    switch (status_bbst_reg)
    {
    	/* Default start first time the controller gets powered. */
        case ST_INIT:
            /* Turn off PWM and Static Switch. */
        	PWM_AUX_OFF();
            bbst_voltage_loop_ON = false;
			/* Next state: converter ready to start. */
			status_bbst_reg = ST_WAIT_PFC;
            break;

        /* Check in case the VDCP is higher than Vbulk at power up. */
        case ST_WAIT_PFC:
        	/* Turn off PWM. */
        	PWM_AUX_OFF();
        	bbst_voltage_loop_ON = false;
        	/* Only move on when PFC is already at steady state. */
			if ((status_ttp_reg & ST_RUNNING) && (PFC.vbulk > VBULK_BBY_START))
			{
				if(bby_wait_cnt < 1)
				{
					bby_wait_cnt ++;
				}
				else
				{
					/* Waiting for the PFC to reach steady state before enabling the BBboost first time. */
					status_bbst_reg = ST_STB; // comment this line to avoid BB to kick in
				}
			}
			else
			{
				bby_wait_cnt =  0;
			}
        	break;

        case ST_STB:
        	/*
        	 * Only if Vac is lost shall kick in the bby boost.
        	 * The signal is filtered to avoid miss-trigger by AC cross.
        	 */
        	if (!PWM_IS_ON())
        	{
        		bby_vac_lost_cnt ++;
        	}
        	else
        	{
        		bby_vac_lost_cnt = 0;
        	}

			/* If Vbulk drops below certain value the bby_boost shall start. */
			if ((PFC.vbulk < VBULK_BBY_START) && (!bby_wrk_time_cnt) && (bby_vac_lost_cnt > 1))
			{
				bby_strt_dly_cnt ++;
				if(bby_strt_dly_cnt > 0)
				{
					/* Next state: BBY_BOOST starts switching. Static switch turned off. */
					PWM_AUX_ON();
					STSW_OFF();
				    voltage_vdcp_ctr.output_int.i32 = AUX_BOOST_MIN_DTY << 16;
					/* Error initialization. */
				    voltage_vdcp_ctr.error[0] = 0;
				    voltage_vdcp_ctr.error[1] = 0;
					/* The voltage loop would be enabled only at end of ramp-up. */
					voltage_vdcp_ctr.ramp_up_on = true;
					/* Enabled computation of the faster loop. */
					bbst_voltage_loop_ON = true;
					/* Ramp up of the reference for smoother start. */
					voltage_vdcp_ctr.ref = PFC.vbulk + ((BBY_VDCP_STDY - PFC.vbulk) >> 2);
					status_bbst_reg = ST_SOFT_START;
		            /* Enable already the over-voltage IRQ of the bby-boost. */
		            OVP_VDCP_ON();
				}
			}
			else
			{
				bbst_voltage_loop_ON = false;
				/* Turn off PWM and Static Switch. */
				PWM_AUX_OFF();
				bby_strt_dly_cnt = 0;
			}
			break;

        case ST_SOFT_START:
            /* Ramping up the reference voltage of the bby_boost. */
			if (voltage_vdcp_ctr.ref < BBY_VDCP_STDY)
			{
				voltage_vdcp_ctr.ref += ((BBY_VDCP_STDY - voltage_vdcp_ctr.ref) >> 2);
				/* Limit the maximum reference. */
				if (voltage_vdcp_ctr.ref > BBY_VDCP_STRT_UP_END)
				{
					voltage_vdcp_ctr.ref = BBY_VDCP_STDY;
				}
			}
			/* If ramp-up already finished. */
			else
			{
				/* State change, for the moment no action when running. */
				status_bbst_reg = ST_RUNNING;
			}
            break;

        case ST_RUNNING:
            break;

        case ST_LATCH:
        	/* Turn off PWM, SR and relay in case INIT state is reached. */
			PWM_AUX_OFF();
			/* The voltage loop is only enabled here to not accumulate integral part. */
			bbst_voltage_loop_ON = false;
            break;

        default:
            break;
    }
    /*
     * In running state the static switch is OFF by the baby boost command.
     * Otherwise, the voltage level shall command if time to re-enable it.
     */
    if (!(status_bbst_reg & (ST_RUNNING | ST_SOFT_START)))
    {
    	/* Control of status of the static switch. */
    	if ((PFC.vbulk + VBULK_10V) < PFC.vdcp)
    	{
    		/* Vbulk is lower than Vdcp by some volts. Leave the STSW open. */
            STSW_OFF();
    	}
    	else
    	{
    		/* Voltage difference not high or STSW forwarded. Close STSW. */
    		STSW_ON();
    	}
    	/* Set a maximum working time of the bby-boost. */
		if (bby_wrk_time_cnt > 0)
		{
			bby_wrk_time_cnt --;
		}
    }
    /* If latched will not start again. */
    else if (!(status_bbst_reg & ST_LATCH))
    {
		/* Set a maximum working time of the bby-boost. */
    	if (bby_wrk_time_cnt > 100)  // 63 is circa 50 ms //working time is units * 800e-6 s
    	{
			/* Back to stand-by waiting. */
			PWM_AUX_OFF();
			/* Enable already the over-voltage IRQ of the bby-boost. */
			OVP_VDCP_OFF();
			status_bbst_reg = ST_WAIT_PFC;
    	}
    	else
    	{
    		bby_wrk_time_cnt ++;
    	}
    	/* If during the ramp-up time the vbulk has risen again, stop bby-boost. */
		if (PFC.vbulk > VBULK_BBY_STOP)
		{
			bby_stop_dly_cnt ++;
			if(bby_stop_dly_cnt > 2)
			{
				/* Back to stand-by waiting. */
				PWM_AUX_OFF();
				/* Disable the over-voltage of the bby-boost. */
				OVP_VDCP_OFF();
				status_bbst_reg = ST_STB;
			}
		}
		else
		{
			bby_stop_dly_cnt = 0;
		}
		/* Minimum vbulk voltage for the baby boost to work. */
		if (PFC.vbulk < VBULK_BBY_MIN)
		{
			bby_min_dly_cnt ++;
			if(bby_min_dly_cnt > 1)
			{
				/* Back to stand-by waiting. */
				PWM_AUX_OFF();
				status_bbst_reg = ST_INIT;
			}
		}
		else
		{
			bby_min_dly_cnt = 0;
		}
		/* Load dependent slope feed-forward adjust. */
		uint16_t slope = ((PFC.adc_io_measure * 1) >> 3) + 50;
		if (slope < BBY_FFW_VIN_SLOPE)
		{
			voltage_vdcp_ctr.bby_ffw_slope = slope;
		}
		else
		{
			voltage_vdcp_ctr.bby_ffw_slope = BBY_FFW_VIN_SLOPE;
		}
    }
} // 1.737 us. Some 139 instructions.

#if ZERO_CROSS_DEBUG
static uint8_t aux_state;
#endif

/**-----------------------------------------------------------------------------
 * \brief   State machine for start up process.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void state_machine_ttp()
{
	/** Counter for managing soft-start process in state machine after input voltage is in range. */
	static uint16_t stb_bop_count;

#if ZERO_CROSS_DEBUG
	switch (aux_state)
	{
	case 0:
		status_ttp_reg = ST_INIT;
		aux_state ++;
		break;
	case 1:
		status_ttp_reg = ST_STB;
		aux_state ++;
		break;
	case 2:
		status_ttp_reg = ST_STB;
		aux_state ++;
		break;
	case 3:
		status_ttp_reg = ST_STB;
		aux_state ++;
		break;
	case 4:
		status_ttp_reg = ST_STB_BOP;
		aux_state ++;
		break;
	case 5:
		status_ttp_reg = ST_RUNNING;
		break;
	}
#endif

    /*------------------------------------------------------------------------*/
    /*  Starting sequence / state machine - START                             */
    /*------------------------------------------------------------------------*/
    switch (status_ttp_reg)
    {
        case ST_INIT:
#if PWM_DEBUG == 0
            /* Turn off PWM, SR and relay in case INIT state is reached. */
            PWM_OFF();
            SR_OFF();
            RELAY_OFF();
#else
            PWM_A_ON();
            PWM_B_ON();
#endif
            /* Power stage is OFF, loops not executed. */
            ttp_voltage_loop_ON = false;
            ttp_current_loop_ON = false;
            /* Reset control loops and update status. */
            control_loop_ttp_init();
            /* Waits amount of time which depends on the previous condition. */
            if (!time_INIT)
            {
            	/* Close the relay and move to stand-by based on bulk voltage higher than 110 Vdc. */
            	if (DC_filter.output > VBULK_START_MIN)
				{
					/* Next state: converter ready to start. */
					status_ttp_reg = ST_STB;
					/* Will wait this time in next state. Approximately 240 ms. [ms] */
					time_INIT = 150;
				}
            }
            else
            {
            	time_INIT --;
            }
            break;

        case ST_STB:
            /* Wait for proper AC voltage (background process). */
            /* Keep PWM and relay off. */
            PWM_OFF();
            SR_OFF();
            RELAY_OFF();
            /* Power stage is OFF, loops not executed. */
            ttp_voltage_loop_ON = false;
            ttp_current_loop_ON = false;
            break;

        case ST_STB_BOP:
            /*
             * Relay management after DC and AC voltages are in range.
             * Waits approximately 300 ms.
             */
            if (stb_bop_count < ST_STB_BOP_CHECK_BULK)
            {
                /** \todo PFC.iL_offset_done is never set to true. */
                /* PWM and static switch (bypass of NTC) remain OFF. */
                PWM_OFF();
                SR_OFF();
                RELAY_OFF();
                /* Power stage is OFF, loops not executed. */
                ttp_voltage_loop_ON = false;
                ttp_current_loop_ON = false;
                /* Relay on counter update. */
                stb_bop_count ++;
            }
            else if (stb_bop_count == ST_STB_BOP_CHECK_BULK)
            {
                /* Current offset sampling. */
                if ((current_ctrl_common.iL_offset_cnt == 0) && !current_ctrl_common.iL_offset_done)
                {
                	current_ctrl_common.iL_offset_en = true;
                }
                else if (current_ctrl_common.iL_offset_cnt == OFFSET_END)
                {
                    /* Static switch is switched on after current offset has been obtained. */
                    RELAY_ON();
                    /* Relay on counter update. */
                    stb_bop_count ++;
                    /* No offset calculation after decision to start. */
                    current_ctrl_common.iL_offset_en = false;
                }
            }
            else if (stb_bop_count == ST_STB_BOP_SFTSTRT_WAIT)
            {
                /* Save DC-bus value for V-loop soft-start. */
            	voltage_vbulk_ctr.ref = PFC.vbulk;
                /* Variable PR_Vdc for DFF calculation during start-up. */
                uint32_t pr_vdc = VBULK_PR_VDC_MAX - ((voltage_vbulk_ctr.ref * VBULK_PR_VDC_SLOPE) >> VBULK_PR_VDC_Q);
                if (pr_vdc < VBULK_PR_VDC_MIN)
                {
                    current_ctrl_common.pr_vdc = VBULK_PR_VDC_MIN;
                }
                else
                {
                	current_ctrl_common.pr_vdc = pr_vdc;
                }
                /* Maximum AC voltage for ratio_ff calculation. */
                current_ctrl_common.vref_acadc = (voltage_vbulk_ctr.ref * VBULK_PR_VDC_AC_FCTR) >> VBULK_PR_VDC_AC_Q;
                /* Relay on counter update. */
                stb_bop_count ++;
            }
            else if (stb_bop_count == ST_STB_BOP_END)
            {
                /* Go to soft-start, starting in negative AC cycle. */
                status_ttp_reg = ST_START_EN;
            }
            else
            {
                /* Relay on counter update. */
                stb_bop_count ++;
            }
            break;

        case ST_START_EN:
            /* The pseudo-pll will set to the next state at zero-crossing of the Vac. */
            break;

        case ST_SOFT_START:
            /*----------------------------------------------------------------*/
            /*  Bulk voltage soft-start for PFC operation                     */
            /*----------------------------------------------------------------*/
            if (!time_vbulk_step)
            {
                if (voltage_vbulk_ctr.ref < VBULK_STEADY)
                {
                    /* Ramp-up the output voltage target (SteadyREF / 2^5). */
                	voltage_vbulk_ctr.ref += (VBULK_STEADY >> 5);
                    /* Limit the maximum reference. */
                    if (voltage_vbulk_ctr.ref > VBULK_STEADY)
                    {
                    	voltage_vbulk_ctr.ref = VBULK_STEADY;
                    }
                    /* X ms until next loop step. */
                    time_vbulk_step = 5;
                }
                else
                {
                    /* Soft-start is done, voltage reference is the targeted output voltage. */
                	voltage_vbulk_ctr.ref = VBULK_STEADY;
                    /* State change, for the moment no action when running. */
                    status_ttp_reg = ST_RUNNING;
                    /* Soft-start is done. */
                    stb_bop_count = ST_SFTSTRT_END;
                }
                /* Variable PR_Vdc for DFF calculation during start-up */
                uint32_t pr_vdc = VBULK_PR_VDC_MAX - ((voltage_vbulk_ctr.ref * VBULK_PR_VDC_SLOPE) >> VBULK_PR_VDC_Q);
                if (pr_vdc < VBULK_PR_VDC_MIN)
                {
                    current_ctrl_common.pr_vdc = VBULK_PR_VDC_MIN;
                }
                else
                {
                	current_ctrl_common.pr_vdc = pr_vdc;
                }
                /* Maximum AC voltage for ratio_ff calculation */
                current_ctrl_common.vref_acadc = (voltage_vbulk_ctr.ref * VBULK_PR_VDC_AC_FCTR) >> VBULK_PR_VDC_AC_Q;
            }
            else
            {
            	time_vbulk_step--;
            }
            break;

        case ST_LATCH:
            /* For debugging purposes. Keep converter OFF. */
            PWM_OFF();
            SR_OFF();
            ttp_voltage_loop_ON = false;
            ttp_current_loop_ON = false;
            break;

        case ST_RUNNING:
            break;

        default:
            break;
    }
} // 580 ns circa 26 instructions.

/**-----------------------------------------------------------------------------
 * \brief   Voltage loop.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void Vbulk_ctrl_loop()
{
    /* Voltage error (Q15). */
    int32_t error = voltage_vbulk_ctr.ref - DC_filter.output;
    /* Error management in controller first iteration. */
    if (voltage_vbulk_ctr.first_iter)
    {
        /* Fill the error array for smooth transition at first iteration. */
    	voltage_vbulk_ctr.error = error;
        /* No first iteration any more. */
    	voltage_vbulk_ctr.first_iter = false;
    }
    /*
     * Voltage compensator with integrator, single pole and single zero (PI + pole)
     * Vloop[k] = b1 * Vloop[k-1] + b2 * Vloop[k-2] + a0 * ev[k] + a1 * ev[k-1]
     * error * coeff => Q15 (16b) * Qx (16b) = Q(15 + x) (32b) --> 1cycle 32bit multiplication
     * vloop * coeff => Q15 (16b) * Qx (16b) = Q(15 + x) (32b) --> 1cycle 32bit multiplication
     * output_int = Vloop[k] << (16-x) (Q31)
     * output_int_limit = output (Q15) --> upper byte of output_int
     * integrator-antiwindup
     */
    /* Error can become very big during hold-up time. */
    if (error > 4000)
    {
    	error = 4000;
    }
    /* Extra bandwidth during hold-up events. Needs to recover fast for next event. */
    if (voltage_vbulk_ctr.qVbus_BW)
    {
    	error = (error * 5) >> 2;
    }
    voltage_vbulk_ctr.output_int.i32 = ((voltage_vbulk_ctr.a[0] * error) + (voltage_vbulk_ctr.a[1] * voltage_vbulk_ctr.error) \
            + (voltage_vbulk_ctr.b[0] * voltage_vbulk_ctr.state[0]) + (voltage_vbulk_ctr.b[1] * voltage_vbulk_ctr.state[1]));
    /*
     * Voltage control loop saturation (Current reference limitation).
     * Takes into account the maximum DCDC current feed-forward number.
     * To avoid numeric overflow.
     */
    if (voltage_vbulk_ctr.output_int.i16[1] > 7750)
    {
        voltage_vbulk_ctr.output_int.i16[1] = 7750;
    }
    /* Voltage loop needs to allow negative values to compensate feed-forward. */
    else if (voltage_vbulk_ctr.output_int.i16[1] < -1250)
    {
        voltage_vbulk_ctr.output_int.i16[1] = -1250;
    }
    /* Shifting shall be done after the anti-windup otherwise would overflow and the anti-windup is not proper. */
    voltage_vbulk_ctr.output_int.i32 = (voltage_vbulk_ctr.output_int.i32) <<  2;
    /* Feed-forward of DCDC load. 3 x the load of the DCDC. */
    int32_t feed_forward_out = voltage_vbulk_ctr.output_int.i16[1] + (PFC.adc_io_measure * 3);
    /* PFC not bidirectional as per today, limit to 0 power in negative direction. */
    if (feed_forward_out > VOLTAGE_INT_OUTPUT_MAX)
    {
    	feed_forward_out = VOLTAGE_INT_OUTPUT_MAX;
    }
    if (feed_forward_out < 0)
    {
    	feed_forward_out = 0;
    }
    voltage_vbulk_ctr.output = feed_forward_out;

    /* Save state for next calculation. */
    voltage_vbulk_ctr.state[1] = voltage_vbulk_ctr.state[0];
    voltage_vbulk_ctr.state[0] = voltage_vbulk_ctr.output_int.i16[1];
    /* Save error for next calculation. */
    voltage_vbulk_ctr.error = error;
} // 866 ns. circa 69 instructions.

/**-----------------------------------------------------------------------------
 * \brief   1/RMS^2 for line feed-forward in the current reference calculation.<BR>
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void inv_RMS_acok()
{
    /*
     *  LINEAR CALCULATION OF 1/RMS2:
     *  Linear approx. of 1/RMS2 using the filtered BOP as in simulation.
     */
    if (PFC.vin_BOP < 1270) PFC.vin_inv2 = 6202 - ((32092 * PFC.vin_BOP) >> 13);
    else if (PFC.vin_BOP < 2154) PFC.vin_inv2 = 2423 - ((30885 * PFC.vin_BOP) >> 15);
    else if (PFC.vin_BOP < 4332) PFC.vin_inv2 = 694 - ((4591 * PFC.vin_BOP) >> 15);
    else PFC.vin_inv2 = 87;
} // 290 ns. circa 23 instructions

/**-----------------------------------------------------------------------------
 * \brief   Bulk voltage monitoring.<BR>
 *          OVP and UVP during operation for PFC.<BR>
 *          In inverter operation the DC range is monitored (hysteresis).
 *          Latching for debugging.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void bulk_monitoring()
{
    static uint8_t vout_OVP_stop_cnt; /**< Counter for time filtering of bulk voltage OVP detection. */
    static uint8_t vout_OVP_resume_cnt; /**< Counter for time filtering of bulk voltage OVP operation resume. */

    /* Protection active after soft-start. */
    if (status_ttp_reg & PFC_RUNNING)
    {
    	if(!(fault_ttp_reg & F_OVP))
    	{
        	/* Heavy and light load pulse skipping. */
            if ((PFC.vbulk > VBULK_OVP_STOP) || ((PFC.vbulk > VBULK_OVP_BURST_STOP) && (voltage_vbulk_ctr.output < 6250)))
            {
                /* Turn-off PWM if the Vout is over the limit for X samples in a row. */
            	vout_OVP_stop_cnt ++;
                if (vout_OVP_stop_cnt > 1)
                {
                    /* Set fault flag. */
                    fault_ttp_reg |= F_OVP;
                    /* Turn-off PWM and SR. */
                    PWM_OFF();
                    /* Power stage is OFF, loops not executed. */
                    ttp_voltage_loop_ON = false;
                    ttp_current_loop_ON = false;
                    /* Reset Over Voltage counter. */
                    vout_OVP_stop_cnt = 0;
                }
            }
            else
            {
            	/* Reset counters. */
				vout_OVP_stop_cnt = 0;
				vout_OVP_resume_cnt = 0;
            }
    	}
        else if (PFC.vbulk < (voltage_vbulk_ctr.ref + 1200)) //added 10V to voltage_int_ctr.int_ref to stop earlier OVP condition // 650
        {
            /* Turn-on PWM if the Vout is under the limit for X samples in a row. */
        	vout_OVP_resume_cnt ++;
            if (vout_OVP_resume_cnt > 2)
            {
                /* Voltage error initialization. */
            	voltage_vbulk_ctr.error = 0;
                /* Voltage loop soft start. */
            	voltage_vbulk_ctr.output = 0;
            	voltage_vbulk_ctr.output_int.i32 = 0;
            	voltage_vbulk_ctr.state[0] = 0;
            	voltage_vbulk_ctr.state[1] = 0;
                /* First iteration in resume operation for smoother V-controller transition. */
            	voltage_vbulk_ctr.first_iter = true;
                /* Current compensator output (duty) and input (error) initialization */
                current_ctrl_A.error = 0;
                current_ctrl_B.error = 0;
                /* Very first duty cycle fixed to the minimum allowed. */
                current_ctrl_A.output.i32 = MIN_DUTY << 16; // Starting duty cycle
                current_ctrl_B.output.i32 = MIN_DUTY << 16; // Starting duty cycle
                /*
                 * Zero duty cycle to resume operation.
                 * It will be overwritten by pseudo_pll() at AC zero crossing.
                 */
                /**
                 * \todo This might not be convenient, because the current loop would be setting the Vac feed-forward duty, and if
                 * we re-start near a zero cross then the duty min will let the current go negative and trigger the INEG again.
                 * Most likely better not to do this and let the current loop work in background. Evaluate this at some point!!!!.
                 */
                SET_DUTY_ADC_A_I(current_ctrl_A.output.i16[1]);
                SET_DUTY_ADC_B_I(current_ctrl_B.output.i16[1]);
                /* Reset OverVoltage Counter */
                vout_OVP_resume_cnt = 0;
                /*
                 * Clear the fault flag.
                 * Allow converter to resume operation in next AC zero crossing.
                 */
                PFC.re_strt_PWM_mid_cycl = true;
                fault_ttp_reg &= ~F_OVP;
                /* Re-enable of the over voltage threshold IRQ. */
                //OVP_VBULK_ON();
            }
        }
        else
        {
            /* Reset counters. */
        	vout_OVP_stop_cnt = 0;
        	vout_OVP_resume_cnt = 0;
        }

        /* Under-voltage 300V restarts the converter operation with soft-start sequence. */
        if (PFC.vbulk < VBULK_UVP_MIN)
        {
            /* Turn-off the converter */
            PWM_OFF();
            SR_OFF();
            /* PWM cannot be turned ON --> Decision in PWM interrupt. */
            ttp_voltage_loop_ON = false;
            ttp_current_loop_ON = false;
            /* RELAY OFF for soft-start procedure. */
            RELAY_OFF();
            /* Go to INIT state to start the soft-start sequence including the relay management. */
            status_ttp_reg = ST_INIT;
            /* Wait before starting the sequence. Approximately 880 ms. [1.6 ms] */
            time_INIT = 550;
        }
    }
    /* Extra BW in Vloop timer. */
	if (voltage_vbulk_ctr.qVbus_BW)
	{
		voltage_vbulk_ctr.qVbus_BW --;
	}
} // 752 ns. circa 60 instructions

/**-----------------------------------------------------------------------------
 * \brief   Voltage control interrupt. SysTick
 * 			IIR low-pass filter of the AC voltage to obtain the RMS value.
 * 			IIR notch filter of the DC voltage to remove 100/120 Hz component.
 * 			Voltage loop and voltage monitoring.
 * 			State machine for start-up and event management.
 * 			Prio 3 @ 5kHz
 * 			| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
 * 			| X | - | X | X | - | - | X | - |
 * \return  None
 */
__attribute__((section(".ram_code")))
void voltage_loop_ISR(void)
{
	static uint8_t i_filter; /**< Counter for reducing sampling frequency of the filter */
	/* Vdc measurement in Q15 format (normalized to 0-1 range) --> base for notch and protections. */
	PFC.vbulk = ADC_VBULK_RESULT << ADC_TO_Q15; // Scaled x 8. 1656 approx. 22 V
	PFC.vdcp = ADC_VDCP_RESULT << ADC_TO_Q15; // Scaled x 8. 1472 approx. 22 V

	/* 2.5kHz  -------  VLoop-Notch-RMS-state machine scheduler  ------------ */
	if(i_filter & 1)
	{
		/* Notch filter of Bulk voltage to eliminate 100Hz and 120Hz. */
		notch_filter_Vbulk();
		/* Voltage loop runs only if the converter is ON in PFC mode. */
		if (ttp_voltage_loop_ON) Vbulk_ctrl_loop();
		/* State machine to control totem-pole. Executed at 625 Hz. 1.6 ms period. */
		if (i_filter == 1) state_machine_ttp();
		/* State machine to control baby-boost. Executed at 1250 Hz. 800 us period. */
		else if ((i_filter == 7) || (i_filter == 3)) state_machine_bbst();
	}
	/* 2.5kHz  -------------------------------------------------------------- */
	else
	{
		/* AC RMS value by low pass filter of V_AC measurement */
		lpf_filter_RMS();
		/* 625 Hz execution scheduled */
		if(i_filter == 4) inv_RMS_acok();
		else if(i_filter == 8) i_filter = 0;
	}
	/* Interrupt counter update */
	i_filter++;
	/* DC voltage monitoring: OVP/UVP, offset detection for wake-up circuit */
	bulk_monitoring();
	/* Time counting during PFC operation */
	time_count_5kHz ++;
} /* 2.18us to 3us circa 240 instructions */
