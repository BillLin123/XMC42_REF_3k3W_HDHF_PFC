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
 * \file    control_irq_i_loop_low_prio.h
 * \author  Meneses Herrera, David; Escudero, Manuel
 * \date    22.05.2024
 * \brief   Interrupts code
 */
#ifndef CONTROL_IRQ_I_LOOP_LOW_PRIO_H_
#define CONTROL_IRQ_I_LOOP_LOW_PRIO_H_

#include <DAVE.h>
#include "control_irq_i_loop_high_prio.h"
#include "init_bckgnd_funct.h"
#include "communication.h"
#include "config_adc.h"

/**-----------------------------------------------------------------------------
 * Definitions.
 */
/**-----------------------------------------------------------------------------
 * \name Dead time calculations. USE DAVID TOOL!!!!. No RANDOM NUMBERS !!.
 * @{
 */
/** Limit of the maximum dead time region. */
#define DT_CONST_ILIMIT		(596)
/** Limit of the region 1. */
#define DT_REG_1_ILIMIT		(2682)
/** Limit of the region 2. */
#define DT_REG_2_ILIMIT		(4768)
/** Maximum dead time Diode to Switch. */
#define DT_D2S_MAX			(17)
/** Minimum dead time Diode to Switch. */
#define DT_D2S_MIN			(5)
/** Offset for the slope 1 of the Diode to Switch dead time. */
#define DT_D2S_OFF_REG_1	(17)
/** Slope of the region 1 for the Diode to Switch dead time. */
#define DT_D2S_SLOPE_REG_1	(141)
/** Offset for the region 2 of the Diode to Switch dead time. */
#define DT_D2S_OFF_REG_2	(8)
/** Slope for the region 2 of the Diode to Switch dead time. */
#define DT_D2S_SLOPE_REG_2	(47)
/** Maximum dead time Switch to Diode. */
#define DT_S2D_MAX			(17)
/** Minimum dead time Switch to Diode. */
#define DT_S2D_MIN			(5)
/** Offset for the slope 1 of the Switch to Diode dead time. */
#define DT_S2D_OFF_REG_1	(17)
/** Slope of the region 1 for the Switch to Diode dead time. */
#define DT_S2D_SLOPE_REG_1	(141)
/** Offset for the region 2 of the Switch to Diode dead time. */
#define DT_S2D_OFF_REG_2	(8)
/** Slope for the region 2 of the Switch to Diode dead time. */
#define DT_S2D_SLOPE_REG_2	(47)
/** Shifting for dead time calculation; slope is codified in Q15 */
#define DT_SLOPE_Q      	(15)
/** @} */

#define IREF_PRIORITY   (2)
/** Current loop of phase A. Triggered by iLA (Queue) conversion */
#define current_ref_ISR	i_ref_ISR

/**-----------------------------------------------------------------------------
 * \brief   Current offset calculation based on averaging ADC samples without
 * 			power stage.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void iL_offset_calc()
{
    /*------------------------------------------------------------------------*/
    /*  Current offset (zero level) acquisition before starting               */
    /*  the PFC (ST_STB_BOP)                                                  */
    /*------------------------------------------------------------------------*/
    /* Current offset is calculated during start up sequence, before closing the relay. */
    if (current_ctrl_common.iL_offset_en)
    {
        /* Accumulate a defined number of samples. */
        if (current_ctrl_common.iL_offset_cnt < OFFSET_SAMPLES)
        {
            /* Use current information for offset calculation if Vac is under 60V (432-ADC). */
            if (PFC.vin_AC < OFFSET_VAC)
            {
            	current_ctrl_A.iL_offset_acc += ADC_ILA_RESULT;
            	current_ctrl_B.iL_offset_acc += ADC_ILB_RESULT;
            	current_ctrl_common.iL_offset_cnt ++;
            }
        }
        else
        {
            /* Average of the accumulated current measurement is current offset. Normalization to Q15. */
        	current_ctrl_A.iL_offset = (current_ctrl_A.iL_offset_acc >> (OFFSET_SAMPLES_2 - ADC_TO_Q15));
        	current_ctrl_B.iL_offset = (current_ctrl_B.iL_offset_acc >> (OFFSET_SAMPLES_2 - ADC_TO_Q15));
        	current_ctrl_common.iL_offset_cnt = OFFSET_END;
            /* No current offset calculation in next iteration. */
        	current_ctrl_common.iL_offset_en = false;
        }
    }
}

/**-----------------------------------------------------------------------------
 * \brief   Current reference calculation for next loop iteration.<BR>
 *
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void iL_ref()
{
    /* Calculate I_ref for the current loop in PFC mode: VAC * 1/Vrms2 * V-loop-output. */
    uint32_t iL_ref_int = PFC.vin_inv2 * voltage_vbulk_ctr.output;
    /*
     * Iref in Q15 --> consider iL as Q15, therefore as a gain in the current measurement.
     * Iref is input current reference
     */
    uint16_t iL_ref = ((uint64_t) iL_ref_int * PFC.vin_AC) >> CURRENT_REF_TO_Q15;
    /* Maximum current limitation. AVG input current limitation. */
    if (iL_ref > I_IN_AVG_MAX) iL_ref = I_IN_AVG_MAX;
    /* When PFC-B enabled, the current reference is split between the two chokes */
#if (PFC_B_ENABLE)
        current_ctrl_common.iL_ref = iL_ref >> 1;
#else
        current_ctrl_common.iL_ref = iL_ref;
#endif
}

/**-----------------------------------------------------------------------------
 * \brief   Dead time modification based on sensed current.<BR>
 *          Parameters for calculation can be changed through UART communication.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void iL_dead_time(int16_t iL, uint32_t *falling_dt, uint32_t *rising_dt)
{
    /*------------------------------------------------------------------------*/
    /*  Dead time and switching frequency calculation based on current        */
    /*------------------------------------------------------------------------*/
    if (iL < DT_REG_2_ILIMIT)
    {
        if (iL < DT_CONST_ILIMIT)
        {
            /* Maximum value. */
        	*falling_dt = DT_D2S_MAX;
        	*rising_dt = DT_S2D_MAX;
        }
        else if (iL < DT_REG_1_ILIMIT)
        {
            /* Linear region 1. */
        	*falling_dt = (DT_D2S_OFF_REG_1 - ((DT_D2S_SLOPE_REG_1 * (iL - DT_CONST_ILIMIT)) >> DT_SLOPE_Q));
			*rising_dt = (DT_S2D_OFF_REG_1 - ((DT_S2D_SLOPE_REG_1 * (iL - DT_CONST_ILIMIT)) >> DT_SLOPE_Q));
        }
        else
        {
            /* Linear region 2. */
        	*falling_dt = (DT_D2S_OFF_REG_2 - ((DT_D2S_SLOPE_REG_2 * (iL - DT_REG_1_ILIMIT)) >> DT_SLOPE_Q));
			*rising_dt = (DT_S2D_OFF_REG_2 - ((DT_S2D_SLOPE_REG_2 * (iL - DT_REG_1_ILIMIT)) >> DT_SLOPE_Q));
        }
    }
    else
    {
        /* Minimum DT value. */
    	*falling_dt = DT_D2S_MIN;
		*rising_dt = DT_S2D_MIN;
    }
}

/**-----------------------------------------------------------------------------
 * \brief   Dead time modification based on sensed current.
 *          Parameters for calculation can be changed through UART communication.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void iL_dead_time_AB(int16_t iL, PWM_CCU8_t *pwm_pfc_A, PWM_CCU8_t *pwm_pfc_B)
{
	uint32_t DC1R;
    /*------------------------------------------------------------------------*/
    /*  Dead time and switching frequency calculation based on current        */
    /*------------------------------------------------------------------------*/
    if (iL < DT_REG_2_ILIMIT)
    {
        /* Maximum value. */
        if (iL < DT_CONST_ILIMIT)
        {
        	DC1R =  (((uint32_t)DT_D2S_MAX) << CCU8_CC8_DC1R_DT1F_Pos) \
			| ((uint32_t)DT_S2D_MAX);
        }
        /* Linear region 1. */
        else if (iL < DT_REG_1_ILIMIT)
        {
        	DC1R = (((uint32_t) (DT_D2S_OFF_REG_1 - ((DT_D2S_SLOPE_REG_1 * (iL - DT_CONST_ILIMIT)) >> DT_SLOPE_Q))) << CCU8_CC8_DC1R_DT1F_Pos) \
			| ((uint32_t)(DT_S2D_OFF_REG_1 - ((DT_S2D_SLOPE_REG_1 * (iL - DT_CONST_ILIMIT)) >> DT_SLOPE_Q)));
        }
        /* Linear region 2. */
        else
        {
        	DC1R = (((uint32_t)(DT_D2S_OFF_REG_2 - ((DT_D2S_SLOPE_REG_2 * (iL - DT_REG_1_ILIMIT)) >> DT_SLOPE_Q))) << CCU8_CC8_DC1R_DT1F_Pos) \
			| ((uint32_t)(DT_S2D_OFF_REG_2 - ((DT_S2D_SLOPE_REG_2 * (iL - DT_REG_1_ILIMIT)) >> DT_SLOPE_Q)));
        }
    }
    else
    {
        /* Minimum DT value. */
    	DC1R = (((uint32_t)DT_D2S_MIN) << CCU8_CC8_DC1R_DT1F_Pos) \
		| ((uint32_t)DT_S2D_MIN);
    }
    pwm_pfc_A->ccu8_slice_ptr->DC1R = DC1R;
    pwm_pfc_B->ccu8_slice_ptr->DC1R = DC1R;
}

/**-----------------------------------------------------------------------------
 * \brief   Current reference interrupt.
 *          pwm_count counts number of PWM cycles.
 *          PR event of pwm_count triggers the current_fed_ISR() at a defined
 *          number of switching cycles.
 *          Prio 2.
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void current_ref_inline(void)
{
	/* Current offset (zero level) acquisition before starting the PFC (ST_STB_BOP) */
	iL_offset_calc();
	/* Current reference calculation for next cycle */
	iL_ref();
	/*
	 * Dead time variation according to current level.
	 * WARNING: Updating the dead time while inside the dead time will cause it
	 * to go to maximum length. For the duration of the IRQ at this moment, and
	 * the location of the trigger of the IRQ, that situation is not happening.
	 */
	iL_dead_time(current_ctrl_A.iL, &PFC.dt_falling, &PFC.dt_rising);
} // 825 ns 66 clk

#endif /* CONTROL_IRQ_I_LOOP_LOW_PRIO_H_ */
