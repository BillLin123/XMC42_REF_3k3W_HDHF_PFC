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
 * \date	11.05.2024
 * \brief   Control related interrupts code
 */

#include <DAVE.h>
#include "control_irq_i_loop_high_prio.h"
#include "control_irq_i_loop_low_prio.h"
#include "control_irq_v_loop_outer.h"
#include "config_adc.h"
#include "init_bckgnd_funct.h"

/**-----------------------------------------------------------------------------
 * Variable definition
 */
PFC_var_t PFC; /**< Instance of the PFC needed variables. */

/**-----------------------------------------------------------------------------
 * \name Control and Fault status registers
 * @{
 */
volatile state_t status_ttp_reg = ST_INIT; /**< Variable that holds the operation state of the PFC. */
volatile fault_t fault_ttp_reg = F_INIT; /**< Variable that holds the possible faults of the PFC. */
volatile state_t status_bbst_reg = ST_INIT; /**< Variable that holds the operation state of the Baby Boost. */
volatile fault_t fault_bbst_reg = F_INIT; /**< Variable that holds the possible faults of the Baby Boost. */
/** @} */

/**-----------------------------------------------------------------------------
 * \name Input frequency and polarity check.
 * @{
 */
grid_voltage_t input_polarity[2]; /**< Check the polarity to differentiate the AC zero edge and detect positive or negative input voltage. */
static uint8_t neg_iL_fault_cnt; /**< Time count after negative current fault is set. */
AC_monitoring_t AC_zero_cross; /**< Instance of the AC zero crossing management. */
/** @} */

#if (I_LOOP_DEBUG || I_FSW_DEBUG)
static uint16_t current_loop_cnt = 0;
/** Counter for debugging. */
static uint8_t iDebug = 0;
static uint8_t offset_cnt = 0;
#endif

//static uint32_t meas_begin;
//static uint32_t meas_end;
//static uint32_t delta_meas;

/**-----------------------------------------------------------------------------
 * \name Controller structures
 * @{
 */
controller_V_t voltage_vbulk_ctr; /**< Voltage controller. */
controller_bby_t voltage_vdcp_ctr; /**< Voltage controller. */
controller_I_t current_ctrl_A; /**< Current controller: PFC-A. */
controller_I_t current_ctrl_B; /**< Current controller: PFC-B. */
common_I_t current_ctrl_common; /**< Current controller: Common to PFC-A and PFC-B. */
bool ttp_voltage_loop_ON = false; /**< State Machine sets if the Power Stage (PWM) can be turned on. */
bool ttp_current_loop_ON = false; /**< State Machine sets if the current control loop is evaluated */
bool bbst_voltage_loop_ON = false; /**< State Machine sets if the Power Stage (PWM) can be turned on. */
static int32_t ratio_FF; /**< Ratio Vac/Vdc for DFF calculation. Vdc assumed constant in inverter operation. */
/** @} */

uint16_t time_INIT = 200; /**< Counter to start/resume operation. Approximately 320 ms. [1.6 ms] */

/**-----------------------------------------------------------------------------
 * \brief   Current rectification according to polarity and offset removal.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void iL_modif(controller_I_t *i_ctrl, uint16_t adc_result)
{
    /* Rectified current measurement according to AC polarity for PFC mode. */
    if (input_polarity[0])
    {
    	/* Remove the measurement offset. */
    	/* Inductor current to be used in the current loop (Q15) --> SW gain!. */
    	i_ctrl->iL = (((int32_t)(adc_result << ADC_TO_Q15) - i_ctrl->iL_offset) * IL_AVG_GAIN) >> IL_AVG_GAIN_Q;
    }
    else
    {
    	/* Remove the measurement offset. */
        /* Inductor current to be used in the current loop (Q15) --> SW gain!. */
    	i_ctrl->iL = (((int32_t)i_ctrl->iL_offset - (adc_result << ADC_TO_Q15)) * IL_AVG_GAIN) >> IL_AVG_GAIN_Q;
    }
}

/**-----------------------------------------------------------------------------
 * \brief   Current loop and duty feed-forward (DFF) calculation.
 *          DFF is calculated assuming fixed 400V output.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void iL_loop(controller_I_t *i_ctrl, CCU8_CC8_TypeDef *slice, uint16_t mask)
{
    /* Current error (Q15) for control loop calculation. */
    int32_t error = (int32_t)current_ctrl_common.iL_ref - i_ctrl->iL;
    /*
     * Current compensator with one zero (PI)
     * d[k] = d[k-1] + a0 * e[k] + a1 * e[k-1]
     * error * coeff => Q15 (16b) * Q14 (16b) = Q29 (32b) --> 1 cycle 32 bit multiplication
     * state_output Q29 - 32b --> duty Q13 16b
     */
#if (ILOOP_COEFF_Q < 14)
    /* Not necessary if Q14 coeff. */
    i_ctrl->state_output.i32 = (((state_error * i_ctrl->a0) + (i_ctrl->state_error * i_ctrl->a1)) << ILOOP_COEFF_Q_SHIFT_TO_Q29) + (i_ctrl->state_output.i32);
#else
    i_ctrl->output.i32 = (error * current_ctrl_common.a0) + (i_ctrl->error * current_ctrl_common.a1) + i_ctrl->output.i32;
#endif
    /* Current controller limitation. */
    if(i_ctrl->output.i16[1] > MAX_DELTA_DUTY) i_ctrl->output.i16[1] = MAX_DELTA_DUTY;
    else if(i_ctrl->output.i16[1] < -MAX_DELTA_DUTY) i_ctrl->output.i16[1] = -MAX_DELTA_DUTY;
    /*------------------------------------------------------------------------*/
    /*  Duty cycle calculation: feed-foward + current loop                    */
    /*------------------------------------------------------------------------*/
    /* Duty composes of feed-forward plus current loop. */
    i_ctrl->duty = ratio_FF + i_ctrl->output.i16[1];
    /* Duty cycle limitation. */
    if(i_ctrl->duty > MAX_DUTY) i_ctrl->duty = MAX_DUTY;
    else if(i_ctrl->duty < MIN_DUTY) i_ctrl->duty = MIN_DUTY;

    /* Duty cycle update in close-loop operation without feed-forward. */
    SET_DUTY_ADC_I(i_ctrl->duty, slice, mask);
    /* Save error values for the next calculation. */
    i_ctrl->error = error;
} // 852 ns

/**-----------------------------------------------------------------------------
 * \brief   Current loop and duty feed-forward (DFF) calculation.<BR>
 *          DFF is calculated assuming fixed 400V output.
 *
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void iL_feed_forward()
{
    /* Calculate ratio of AC measurement to constant 400V. duty = Vac/Vdc in inverter mode. */
    if (PFC.vin_AC < current_ctrl_common.vref_acadc)
    {
#if ZERO_CROSS_DEBUG
        /* d = PR / AC_400V = (1229 / 3246 * 2^15) * Vac * 2^-15 */
        ratio_FF = 100;
#else
        /* d = PR / AC_400V = (1229 / 3246 * 2^15) * Vac * 2^-15 */
        /* Boost PFC: d = 1 - Vac / Vdc */
        ratio_FF = PR_MAX - (((uint32_t)PFC.vin_AC * current_ctrl_common.pr_vdc) >> VBULK_PR_VDC_AC_Q);
#endif
    }
    else
    {
#if ZERO_CROSS_DEBUG
        /* Vac close to 400V, ratio is limited to maximum duty. */
        ratio_FF = 100;
#else
        /* Vac close to 400V, ratio is limited to maximum duty. */
        /* Boost PFC: d = 1 - Vac / Vdc */
        ratio_FF = PR_MAX - MAX_DUTY;
#endif
    }
}

#if ZERO_CROSS_DEBUG
static bool debug_plrty = false;
#endif

/**-----------------------------------------------------------------------------
 * \brief   AC zero crossing management.<BR>
 *          High-frequency and Low-frequency signals management.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void pseudo_pll()
{
	static uint8_t input_polarity_cnt; /**< Time filter for polarity change detection. */
    static uint16_t plrty_trnstn_dly_OFF; /**< Counter delay for PWM turn OFF. */
    static uint16_t plrty_trnstn_dly_ON; /**< Counter delay for PWM turn ON. */
    static uint16_t SR_on_cnt; /**< Counting of switching cycles after operation is resumed to manage SR signals. */
    static bool pwm_ZC_disabled; /**< PWM is disabled during AC zero crossing transition. */
    static uint16_t SR_off_cnt; /**< Counting of switching cycles after operation is resumed to manage SR signals. */
    static bool SR_ZC_disabled; /**< PWM is disabled during AC zero crossing transition. */
    static bool vin_on_allowed; /**< The converter operation can resume after AC zero crossing and polarity detection. */
    static bool vac_zero_cross;	/**< AC zero crossing detected. */

#if ZERO_CROSS_DEBUG
    static uint16_t debug_AC;
    static bool debug_AC_plrty;

    if(!debug_plrty)
    {
        if(debug_AC < 1000)
        {
            debug_AC ++;
        }
        else
        {
            debug_plrty = true;
        }
    }
    else
    {
        if(debug_AC > 0)
        {
            debug_AC --;
        }
        else
        {
            debug_plrty = false;
            if(debug_AC_plrty)
                debug_AC_plrty = false;
            else
                debug_AC_plrty = true;
        }
    }
    /* Burst mode debug. */
    if(debug_AC > 250 && debug_AC < 300)
    {
    	PFC.vbulk = VBULK_OVP_BURST_STOP + 10;
    }
    else
    {
    	voltage_vbulk_ctr.ref = 25800;
    	PFC.vbulk = 25810;
    }
    PFC.vin_AC = debug_AC;
#endif

    /* Vac voltage drops under OFF threshold, going into a zero crossing. */
    if(PFC.vin_AC < VIN_OFF_THRSLD)
    {
    	/* Switches OFF PWM and starts transition to opposite polarity after delay. */
		if(!plrty_trnstn_dly_OFF)
		{
			/* Checks first sample going under the OFF threshold. */
			if(!SR_ZC_disabled)
			{
                /* Disable PWM and SR signals. */
                PWM_OFF();

                /* Prepare the next un-trap with low side starting always (Requirement of GaN). */
				/* PWM_TRAP start from PWM_PFC timers is deactivated. */
            	((XMC_CCU8_SLICE_t*) CCU80_CC83)->INS = 0x0000000D; //PWM_TRAP_A.ccu8_slice_ptr->INS = 0x0000000D; // XMC_CCU4_SLICE_EVENT_EDGE_SENSITIVITY_NONE for the event 0. Rest of the events unchanged.
            	((XMC_CCU4_SLICE_t*) CCU40_CC43)->INS = 0x00000009; //PWM_TRAP_B.ccu4_slice_ptr->INS = 0x00000009; // XMC_CCU4_SLICE_EVENT_EDGE_SENSITIVITY_NONE for the event 0. Rest of the events unchanged.
            	((XMC_CCU8_SLICE_t*) CCU80_CC83)->TCCLR = CCU8_CC8_TCCLR_TRBC_Msk; //PWM_TRAP_A.ccu8_slice_ptr->TCCLR = CCU8_CC8_TCCLR_TRBC_Msk; // Stop timer.
            	((XMC_CCU8_SLICE_t*) CCU80_CC83)->TCCLR = CCU8_CC8_TCCLR_TCC_Msk; //PWM_TRAP_A.ccu8_slice_ptr->TCCLR = CCU8_CC8_TCCLR_TRBC_Msk; // Clear timer.
            	((XMC_CCU8_MODULE_t*) CCU80_BASE)->GCSS = CCU8_GCSS_S3ST1S_Msk; //PWM_TRAP_A.ccu8_module_ptr->GCSS = CCU8_GCSS_S3ST1S_Msk; // Set timer status.
            	((XMC_CCU4_SLICE_t*) CCU40_CC43)->TCCLR = CCU4_CC4_TCCLR_TRBC_Msk; //PWM_TRAP_B.ccu4_slice_ptr->TCCLR = CCU4_CC4_TCCLR_TRBC_Msk; // Stop timer.
            	((XMC_CCU4_SLICE_t*) CCU40_CC43)->TCCLR = CCU4_CC4_TCCLR_TCC_Msk; //PWM_TRAP_B.ccu4_slice_ptr->TCCLR = CCU4_CC4_TCCLR_TRBC_Msk; // Clear timer.
            	((XMC_CCU4_MODULE_t*) CCU40_BASE)->GCSS = CCU4_GCSS_S3STS_Msk; //PWM_TRAP_B.ccu4_module_ptr->GCSS = CCU4_GCSS_S3STS_Msk; // Set timer status.
                /* Assign the trap event from the auxiliary PWM_TRAP PWMs. Note the trap level is "active low". */
            	((XMC_CCU8_SLICE_t*) CCU80_CC81)->INS = 0x00000F00; //PWM_PFC_A.ccu8_slice_ptr->INS = 0x00000F00; // Set trap event from PWM_TRAP_A (event 2). Rest of the events unchanged.
            	((XMC_CCU8_SLICE_t*) CCU80_CC82)->INS = 0x00000500; //PWM_PFC_B.ccu8_slice_ptr->INS = 0x00000500; // Set trap event from ERU1.PDOUT2 - PWM_TRAP_B (event 2). Rest of the events unchanged.
            	((XMC_CCU8_SLICE_t*) CCU80_CC81)->TC &= ~CCU8_CC8_TC_TRPSE_Msk; //PWM_PFC_A.ccu8_slice_ptr->TC &= ~CCU8_CC8_TC_TRPSE_Msk; // Remove synchronisation for coming out with low side always.
            	((XMC_CCU8_SLICE_t*) CCU80_CC82)->TC &= ~CCU8_CC8_TC_TRPSE_Msk; //PWM_PFC_B.ccu8_slice_ptr->TC &= ~CCU8_CC8_TC_TRPSE_Msk; // Remove synchronisation for coming out with low side always.
                /* Sets a delay before starting PWM polarity transition. circa 369 us [15.385 us] */
                plrty_trnstn_dly_ON = PWM_TRANSITION_DLY;
                /* Clear current fault. Allow restart after zero crossing. */
                fault_ttp_reg &= ~F_NEG_I;
                neg_iL_fault_cnt = 0;
                /* Save polarity as input to next polarity decision filter. */
                input_polarity[1] = input_polarity[0];

                /* Peak comparison levels to zero. Preparation for the comparison level to move to the other side around zero. */
                HRPWM0_CSG1->SDSV1 = IL_PK_ZERO;
                HRPWM0_CSG2->SDSV1 = IL_PK_ZERO;
                HRPWM0->CSGTRG = (HRPWM0_CSGTRG_D1SES_Msk | HRPWM0_CSGTRG_D2SES_Msk);

                /* Normal PWM modulation disabled during Vac cross transition. */
                SR_ZC_disabled = true;
                /* Delayed turn-off of the slow FETs. */
                SR_off_cnt = 0;
            }
            /* Flag raised when the AC voltage gets close to zero. Enables PWM start when AC voltage goes over ON_THRSLD. */
            if (PFC.vin_AC < VIN_ZERO_THRSLD)
            {
                vac_zero_cross = true;
                /* Averaging of zero current samples when PWM_OFF to modify the current offset level during operation */
                if(current_ctrl_common.iL_zero_cnt < OFFSET_SAMPLES)
                {
                	current_ctrl_A.iL_zero_acc += ADC_ILA_RESULT;
                	current_ctrl_B.iL_zero_acc += ADC_ILB_RESULT;
                    current_ctrl_common.iL_zero_cnt ++;
                }
                else
                {
                    /* Apply the offset modification after steady-state is reached */
                    if (AC_zero_cross.vac_zc_cnt > 80)
                    {
                    	current_ctrl_A.iL_offset = (current_ctrl_A.iL_zero_acc >> (OFFSET_SAMPLES_2 - ADC_TO_Q15));
                    	current_ctrl_B.iL_offset = (current_ctrl_B.iL_zero_acc >> (OFFSET_SAMPLES_2 - ADC_TO_Q15));
                    }
                    current_ctrl_common.iL_zero_cnt = 0;
                    current_ctrl_A.iL_zero_acc = 0;
					current_ctrl_B.iL_zero_acc = 0;
                }
            }
            /* Delayed turn-off of the SRs in favor of EMI. */
            if (SR_ZC_disabled && (SR_off_cnt > 2))
            {
                SR_OFF();
                /* Normal PWM modulation disabled during Vac cross transition. */
				pwm_ZC_disabled = true;
            }
            SR_off_cnt ++;
        }
        /*
         * Choose the starting duty cycle and switching frequency for restart.
         * In the current implementation, Fsw is kept constant for all the conditions.
         */
        if(pwm_ZC_disabled)
        {
            /* No current loop if the PWM is off. */
            ttp_current_loop_ON = false;
            /* Current compensator output (duty) and input (error) initialization */
            current_ctrl_A.error = 0;
            current_ctrl_B.error = 0;
            /* Starting loop value in steady state with duty feed-forward operation. */
            current_ctrl_B.output.i32 = 0;
            current_ctrl_A.output.i32 = 0;
            /* MAX duty cycle to resume in PFC OPERATION */
            /* Loop will not execute until AC voltage goes over VIN_ON_THRSLD. */
            SET_DUTY_PERIOD_A(MAX_DUTY_LCDO, PR_MAX);
            SET_DUTY_PERIOD_B(MAX_DUTY_LCDO, PR_MAX);
        }
    }

    /*
     * Vac above ON threshold.
     * Note: This could be happening left side or right side of the actual
     * Vac crossing, as Vac OFF threshold is > than Vac ON threshold.
     * Wait for polarity change decision.
     */
    if (PFC.vin_AC > VIN_ON_THRSLD)
    {
        if((vac_zero_cross && !plrty_trnstn_dly_ON) || PFC.re_strt_PWM_mid_cycl)
        {
            /*----------------------------------------------------------------*/
            /*  Polarity detection before decision of starting PWM/SR.        */
            /*----------------------------------------------------------------*/
            if(VIN_AC_POLARITY)
            {
                if(input_polarity[1]) input_polarity_cnt ++;
                else input_polarity_cnt = 1;
                input_polarity[1] = POSITIVE;
            }
            else
            {
                if(input_polarity[1]) input_polarity_cnt = 1;
                else input_polarity_cnt ++;
                input_polarity[1] = NEGATIVE;
            }
            /* Filter of x-times HIGH/LOW detected. */
            if (input_polarity_cnt > 2)
            {
                /* Positive AC voltage detected. */
                /* In this polarity we should precede with the Low Frequency FETs */
                if(input_polarity[1])
                {
                	/*High Side Diode - Low Side Switch*/
                    input_polarity[0] = POSITIVE;
                    /* Boost switch is LS (Outx0). Inverted output of the channel. */
                    /** \todo Replace by the appropriate numbers to remove the read logic. */
                    CCU80_CC81->CHC &= ~(CCU8_CC8_CHC_OCS1_Msk | CCU8_CC8_CHC_OCS2_Msk); /*PFC_A*/
                    CCU80_CC82->CHC |= ((1 << CCU8_CC8_CHC_OCS1_Pos) | (1 << CCU8_CC8_CHC_OCS2_Pos)); /*PFC_B*/

                    /* Maximum dead times at the start. Lowest current. */
                    PFC.dt_falling = DT_D2S_MAX;
                    PFC.dt_rising = DT_S2D_MAX;
                    CCU80_CC81->DC1R = (PFC.dt_falling << CCU8_CC8_DC1R_DT1F_Pos) | PFC.dt_rising;
                    CCU80_CC82->DC1R = (PFC.dt_falling << CCU8_CC8_DC1R_DT1F_Pos) | PFC.dt_rising;

                    /* Inverse comparator output for trap function */
                    /** \todo Replace by the number to remove the read and write logic. */
					HRPWM0_CSG1->CC |= HRPWM0_CSG_CC_OIE_Msk;
					HRPWM0_CSG2->CC |= HRPWM0_CSG_CC_OIE_Msk;
                    /* Configure peak comparison levels. */
                    HRPWM0_CSG1->SDSV1 = IL_PK_POS;
                    HRPWM0_CSG2->SDSV1 = IL_PK_POS;
                    HRPWM0->CSGTRG = (HRPWM0_CSGTRG_D1SES_Msk | HRPWM0_CSGTRG_D2SES_Msk);
                }
                /* Negative AC voltage detected. */
                /* In this polarity we should precede with the high-frequency FETs */
                else
                {
                	/*High Side Switch - Low Side Diode*/
                    input_polarity[0] = NEGATIVE;
                    /* Boost switch is HS (OUTx1) */
                    CCU80_CC81->CHC |= ((1 << CCU8_CC8_CHC_OCS1_Pos) | (1 << CCU8_CC8_CHC_OCS2_Pos)); /*PFC_A*/
                    CCU80_CC82->CHC &= ~(CCU8_CC8_CHC_OCS1_Msk | CCU8_CC8_CHC_OCS2_Msk); /*PFC_B*/

                    PFC.dt_falling = DT_D2S_MAX;
                    PFC.dt_rising = DT_S2D_MAX;
                    CCU80_CC81->DC1R = (PFC.dt_falling << CCU8_CC8_DC1R_DT1F_Pos) | PFC.dt_rising;
                    CCU80_CC82->DC1R = (PFC.dt_falling << CCU8_CC8_DC1R_DT1F_Pos) | PFC.dt_rising;

                    /* Comparator output for trap function */
                    HRPWM0_CSG1->CC &= ~HRPWM0_CSG_CC_OIE_Msk;
                    HRPWM0_CSG2->CC &= ~HRPWM0_CSG_CC_OIE_Msk;
                    /* Configure peak comparison levels. */
                    HRPWM0_CSG1->SDSV1 = IL_PK_NEG;
                    HRPWM0_CSG2->SDSV1 = IL_PK_NEG;
                    HRPWM0->CSGTRG = (HRPWM0_CSGTRG_D1SES_Msk | HRPWM0_CSGTRG_D2SES_Msk);
                }
                /* Reset polarity change time filter. */
                input_polarity_cnt = 0;
                /* Flag for starting the PWM/SR after polarity decision. */
                vin_on_allowed = true;
            }
            /* Turn on PWM/SR if delay happened and polarity detected. */
            if(!plrty_trnstn_dly_ON && vin_on_allowed)
            {
                /* Voltage has risen after a Vac crossing detection. */
                if(pwm_ZC_disabled || PFC.re_strt_PWM_mid_cycl)
                {
                    /* Force the converter to start with positive AC cycle (LF_LS). */
                    if((status_ttp_reg & ST_START_EN) && input_polarity[0])
                    {
                        /* Clear START_ALLOWED flag to avoid this step in the next cycles. Clear OFF to start the converter. */
                        status_ttp_reg = ST_SOFT_START;
                    }
                    if((!fault_ttp_reg) && (status_ttp_reg & PFC_RUNNING))
                    {
                    	/*
                    	 * In this polarity low frequency FET goes ahead.
                    	 * Low side would be hard-switched in the high-frequency.
                    	 * Mostly concern for the particularities of the GaN driving.
                    	 */
                        if(input_polarity[0])
                        {
                            /* Appropriate slow MOSFET is turned ON. */
                            SR_LS_ON();
                        }
                        else
                        {
                        	/* Diode Low side First */
                            PWM_A_ON();
#if (PFC_B_ENABLE)
                            PWM_B_ON();
#endif
                            /* Reconfigure PWM_TRAP to be started by PWM_PFC timers. */
                        	((XMC_CCU8_SLICE_t*) CCU80_CC83)->INS = 0x0001000D; //PWM_TRAP_A.ccu8_slice_ptr->INS = 0x0001000D; // XMC_CCU8_SLICE_EVENT_EDGE_SENSITIVITY_RISING_EDGE for the event 0. Rest of the events unchanged
                        	((XMC_CCU4_SLICE_t*) CCU40_CC43)->INS = 0x00010009; //PWM_TRAP_B.ccu4_slice_ptr->INS = 0x00010009; // XMC_CCU8_SLICE_EVENT_EDGE_SENSITIVITY_RISING_EDGE for the event 0. Rest of the events unchanged.
                        }

                        /* Current loop can be evaluated again */
                        ttp_current_loop_ON = true;
                        /* Voltage loop can be evaluated again. */
                        ttp_voltage_loop_ON = true;
                        /* Reset counting of switching cycles after starting PWM. */
                        SR_on_cnt = 0;
#if ZERO_CROSS_DEBUG
                        /* Sets a delay before starting PWM polarity transition. Circa 2 ms [15.385 us] */
                        plrty_trnstn_dly_OFF = 106 + PWM_TRANSITION_DLY;
#else
                        /* Sets a delay before starting PWM polarity transition. Circa 461.5 us [15.385 us] */
                        plrty_trnstn_dly_OFF = PWM_TRANSITION_DLY + 6;
#endif

                        /* Count AC crossing events to use the variable current offset adaptation */
                        if((AC_zero_cross.vac_zc_cnt < 200) && (status_ttp_reg & ST_RUNNING))
                        {
                            AC_zero_cross.vac_zc_cnt ++;
                        }
#if I_LOOP_DEBUG
                        /*----------------------------------------------------------*/
                        /*  COUNT THE NUMBER OF AC CHANGES FOR DEBUGGING - START    */
                        /*----------------------------------------------------------*/
                        if(iDebug < 200)// && (status_ttp_reg & ST_RUNNING))//
                        {
                            iDebug++;
                        }
                        if (iDebug > 40) //((status_ttp_reg & ST_RUNNING) && (iDebug > 6))  7 //
                        {
                            PWM_OFF();
                            SR_OFF();

                            ttp_voltage_loop_ON = false;
                            ttp_current_loop_ON = false;

                            status_ttp_reg = ST_LATCH;
                        }
                        /*----------------------------------------------------------*/
                        /*  COUNT THE NUMBER OF AC CHANGES FOR DEBUGGING - END      */
                        /*----------------------------------------------------------*/
#endif
                    }
                    /* Removes Vac crossing PWM disabled status flag. */
                    pwm_ZC_disabled = false;
                    SR_ZC_disabled = false;
                    /* Wait for new zero crossing or current fault for starting PWM/SR. */
                    vac_zero_cross = false;
                    PFC.re_strt_PWM_mid_cycl = false;
                }
                /* Clear AC zero crossing flags */
                vin_on_allowed = false;
            }
        }
        /* Delay of LF pulses in PFC operation. */
        if(ttp_voltage_loop_ON && (SR_on_cnt < SR_DELAY_SWCYC))
        {
            /* Update cycle count. */
            SR_on_cnt ++;
            if(SR_on_cnt == (SR_DELAY_SWCYC - 2))
            {
                /* Low frequency switches activated before the PWM. */
                /* POSITIVE AC. */
                if(input_polarity[0])
                {
                    /* Start PWM after SR is started. */
                    PWM_A_ON();
#if (PFC_B_ENABLE)
                    PWM_B_ON();
#endif
                    /* Switch Low side first. */
					/* Reconfigure PWM_TRAP to be started by PWM_PFC timers with ST inverted. */
                    ((XMC_CCU8_SLICE_t*) CCU80_CC83)->INS = 0x0002000D; //PWM_TRAP_A.ccu8_slice_ptr->INS = 0x0002000D; // XMC_CCU8_SLICE_EVENT_EDGE_SENSITIVITY_FALLING_EDGE for the event 0. Rest of the events unchanged
                    ((XMC_CCU4_SLICE_t*) CCU40_CC43)->INS = 0x00020009; //PWM_TRAP_B.ccu4_slice_ptr->INS = 0x00020009; // XMC_CCU8_SLICE_EVENT_EDGE_SENSITIVITY_FALLING_EDGE for the event 0. Rest of the events unchanged.
                }
            }
            if(SR_on_cnt == SR_DELAY_SWCYC)
            {
                /* NEGATIVE AC. */
                if(!input_polarity[0])
                {
                    /* Appropriate slow MOSFET is turned ON. PWM config in polarity detection. */
                    SR_HS_ON();
                }
                /* Assign the trap event from the external current sensors. Note the trap level is "active low". */
            	((XMC_CCU8_SLICE_t*) CCU80_CC81)->INS = 0x01000D00; //PWM_PFC_A.ccu8_slice_ptr->INS = 0x01000D00; // Set trap event from HRPWM0.CSG1 (event 2). Rest of the events unchanged. 0x01000D00;
            	((XMC_CCU8_SLICE_t*) CCU80_CC82)->INS = 0x01000E00; //PWM_PFC_B.ccu8_slice_ptr->INS = 0x01000E00; // Set trap event from HRPWM0.CSG2 (event 2). Rest of the events unchanged. 0x01000E00;
            	((XMC_CCU8_SLICE_t*) CCU80_CC81)->TC |= CCU8_CC8_TC_TRPSE_Msk; //PWM_PFC_A.ccu8_slice_ptr->TC |= CCU8_CC8_TC_TRPSE_Msk; // Set back synchronisation trap exit for peak current limit.
            	((XMC_CCU8_SLICE_t*) CCU80_CC82)->TC |= CCU8_CC8_TC_TRPSE_Msk; //PWM_PFC_B.ccu8_slice_ptr->TC |= CCU8_CC8_TC_TRPSE_Msk; // Set back synchronisation trap exit for peak current limit.
                /* Update cycle count. */
                SR_on_cnt ++;
            }
        }
        /* Timer after negative current fault is detected. */
        if(neg_iL_fault_cnt)
        {
        	neg_iL_fault_cnt --;
        }
        /* After timer, check if there was a negative current fault. */
        else if(fault_ttp_reg & F_NEG_I)
        {
            /* Raise a flag a certain time after the fault is detected and still voltage present. */
            PFC.re_strt_PWM_mid_cycl = true;
            /* Clear the fault, so converter can restart. */
            fault_ttp_reg &= ~F_NEG_I;
            /* Save polarity as input to next polarity decision filter. */
            input_polarity[1] = input_polarity[0];
            /* Reset polarity change time filter. */
            input_polarity_cnt = 0;
        }
    }
    /* Transition timers. 65kHz [15.385 us] */
    if(plrty_trnstn_dly_OFF) plrty_trnstn_dly_OFF --;
    if(plrty_trnstn_dly_ON) plrty_trnstn_dly_ON --;
}

/**-----------------------------------------------------------------------------
 * \brief   Current loop coefficients according to the sensed currents.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void iL_coeff(controller_I_t *i_ctrl)
{
    /*------------------------------------------------------------------------*/
    /* Current loop coefficients based on current (variable inductance - CCM,
     * variable BW according to current level)
     */
    /*------------------------------------------------------------------------*/
    /* Region below 0A. */
    /* Fast turn off if high negative current detected during operation. */
    if ((i_ctrl->iL < INEG_FAULT_Q15) && ttp_current_loop_ON)
    {
        /* Turn off as protection. Something is not working as it should. */
        PWM_OFF();
        SR_OFF();
        ttp_current_loop_ON = false;
        /* Do not allow turn ON in the pseudo-PLL function. */
        fault_ttp_reg |= F_NEG_I;
        /* Time to restart the converter operation. circa 446 us [15.385 us] */
        neg_iL_fault_cnt = PWM_TRANSITION_DLY + 5;
    }
    /* a0 value at highest current (lowest coefficient). */
    else if (i_ctrl->iL < 0)
    {
    	current_ctrl_common.a0 = COEFF_OFFSET_3k_Q;
    }
    /* Linear Region (below 36A). */
    else if (i_ctrl->iL <= REG1_ILIMIT_Q15)
    {
    	current_ctrl_common.a0 = COEFF_OFFSET_3k_Q - ((COEFF_SLOPE_3k_Q * i_ctrl->iL) >> TO_QiLOOP);
    }
    /* Constant Region (over 36A) */
    else
    {
    	current_ctrl_common.a0 = COEFF_MIN_3k_Q;
    }
    /* a1 coefficient calculation based on a0 and zero location. */
    current_ctrl_common.a1 = -((ZERO_LOC_3k_Q * current_ctrl_common.a0) >> ILOOP_COEFF_Q);
}

/**-----------------------------------------------------------------------------
 * \brief   Current loop coefficients according to the sensed currents.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void iL_neg_ctrl(int16_t iL)
{
    /*------------------------------------------------------------------------*/
    /* Fast turn off if high negative current detected during operation. */
    if (iL < INEG_FAULT_Q15)
    {
        /* Turn off as protection. Something is not working as it should. */
        PWM_OFF();
        SR_OFF();
        /* Voltage loop should keep running in AC-LCDO, no current loop since something went wrong. */
        ttp_current_loop_ON = false;
        /* Do not allow turn ON in the pseudo-PLL function. */
        fault_ttp_reg |= F_NEG_I;
        /* Time to restart the converter operation. circa 446 us [15.385 us] */
        neg_iL_fault_cnt = PWM_TRANSITION_DLY + 5;
    }
}

/**-----------------------------------------------------------------------------
 * \brief   Current loop interrupt.
 *          CR2 while counting up in CCU80.CC81 (PWM-PFC) triggers the ADC queue.
 *          A result event of the queue triggers this interrupt.
 *          Prio 1.
 * \return  None
 */
__attribute__((section(".ram_code")))
void current_loop_A_ISR(void)
{
	/* Read AC voltage. */
	PFC.vin_AC = ADC_VAC_RESULT << 1; // AC sensing gain is multiplied by 2.
	/* Current sensing modification */
	iL_modif(&current_ctrl_A, ADC_ILA_RESULT);
	iL_feed_forward();
	/* Current control loop */
    if (ttp_current_loop_ON)
    {
        iL_loop(&current_ctrl_A, CCU80_CC81, CCU8_GCSS_S1SE_Msk);
        /*
         * Dead time update in timer. Bug of XMC updating dead time while in
         * dead time makes it to miss the pulse.
         */
        CCU80_CC81->DC1R = (PFC.dt_falling << CCU8_CC8_DC1R_DT1F_Pos) | PFC.dt_rising;
    }
	/* Current coefficients to be applied in the next cycle, current dependent */
	iL_coeff(&current_ctrl_A);
	/* AC crossing detection for SR management --- Pseudo-PLL */
	pseudo_pll();

#if I_FSW_DEBUG
    /*----------------------------------------------------------*/
    /*  COUNT THE NUMBER OF Fsw CYCLES FOR DEBUGGING - START	*/
    /*----------------------------------------------------------*/
    /* iDebug counts the number of AC cycles, check pseudo_pll() function for AC cycle limitation */
    if ((ttp_current_loop_ON))// && (iDebug > 1))//
    {
        current_loop_cnt ++;
    }

    if (current_loop_cnt > 200)
    {
        PWM_OFF();
        SR_OFF();

        ttp_voltage_loop_ON = false;
        ttp_current_loop_ON = false;

        status_ttp_reg = ST_LATCH;
    }
    /*----------------------------------------------------------*/
    /*  COUNT THE NUMBER OF Fsw CYCLES FOR DEBUGGING - END		*/
    /*----------------------------------------------------------*/
#endif
}; // 2.62 us. 210 clk

/**-----------------------------------------------------------------------------
 * \brief   Current loop interrupt.
 *          CR2 while counting up in CCU80.CC81 (PWM-PFC) triggers the ADC queue.
 *          A result event of the queue triggers this interrupt.
 *          Prio 1.
 * \return  None
 */
__attribute__((section(".ram_code")))
void current_loop_B_ISR(void)
{
    //__asm__ __volatile__ ("" : : : "memory");
    //GREEN_LED_SET();
	//__asm__ __volatile__ ("" : : : "memory");
	//meas_begin = DWT->CYCCNT;

	/* Current sensing modification */
    iL_modif(&current_ctrl_B, ADC_ILB_RESULT);
#if (PFC_B_ENABLE)
    /* Current control loop */
    if (ttp_current_loop_ON)
    {
        iL_loop(&current_ctrl_B, CCU80_CC82, CCU8_GCSS_S2SE_Msk);
        iL_neg_ctrl(current_ctrl_B.iL);
        /*
		 * Dead time update in timer. Bug of XMC updating dead time while in
		 * dead time makes it to miss the pulse.
		 */
        CCU80_CC82->DC1R = (PFC.dt_falling << CCU8_CC8_DC1R_DT1F_Pos) | (PFC.dt_rising);
    }
#else
    /* Current control loop */
    if (ttp_current_loop_ON)
    {
        /* If stage B is off, update the stage B ADC trigger position here. */
        SET_DUTY_ADC_I(current_ctrl_A.duty, CCU80_CC82, CCU8_GCSS_S2SE_Msk);
    }
#endif
//
//#if I_FSW_DEBUG
//    /*----------------------------------------------------------*/
//    /*  COUNT THE NUMBER OF Fsw CYCLES FOR DEBUGGING - START	*/
//    /*----------------------------------------------------------*/
//    /* iDebug counts the number of AC cycles, check pseudo_pll() function for AC cycle limitation */
//    if ((ttp_current_loop_ON))// && (iDebug > 1))
//    {
//        current_loop_cnt++;
//    }
//
//    if (current_loop_cnt > 20)
//    {
//        PWM_OFF();
//        SR_OFF();
//
//        ttp_voltage_loop_ON = false;
//        ttp_current_loop_ON = false;
//
//        status_ttp_reg = ST_LATCH;
//    }
//    /*----------------------------------------------------------*/
//    /*  COUNT THE NUMBER OF Fsw CYCLES FOR DEBUGGING - END		*/
//    /*----------------------------------------------------------*/
//#endif
    /* This has been moved into a function call instead of IRQ, saves time. */
    current_ref_inline();
	/* Voltage loop runs only if the converter is ON in BBBOOST mode. */
	if (bbst_voltage_loop_ON) Vdcp_ctrl_loop();

    //__asm__ __volatile__ ("" : : : "memory");
    //GREEN_LED_RST();
	//__asm__ __volatile__ ("" : : : "memory");
	//meas_end = DWT->CYCCNT;
	//delta_meas = meas_end - meas_begin; // 71
}; // 1.47 us 118 clk
