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
 * \file	init_bckgnd_funct.c
 * \author 	Meneses Herrera, David; Escudero, Manuel
 * \date	25.03.2024
 * \brief   Functions for SW initialization and running in the background during
 *          operation.
 */

#include "control_irq_v_loop_outer.h"
#include "init_bckgnd_funct.h"
#include "communication.h"

/**-----------------------------------------------------------------------------
 * \name Time counters.
 * @{
 */
uint8_t _100ms_count = 0;	/**< 100ms counter. */
uint8_t time_PLD_130V = 0;	/**< Counter for PLD timing definition. */
uint8_t time_PLD_150V = 0;	/**< Counter for PLD timing definition. */
/** @} */

/** Initial duty of the fan. [CCU_clock] */
int32_t fan_duty = FAN_PWM_MIN; /* (FAN_PWM_MAX - FAN_PWM_MIN) >> 1; */

/**-----------------------------------------------------------------------------
 * Functions.
 */
/**-----------------------------------------------------------------------------
 * \brief   LEDs control.
 * \return  None
 *
 * Signaling of status through the onboard LEDs. Different patterns for different messages.
 */
__attribute__((section(".ram_code")))
void leds_ctr()
{
	/* LEDs clock edge detector. */
    static uint32_t previous_tick;

    /* Time division of the controller clock */
    if((_100ms_count >> LEDS_SPEED) != previous_tick)
    {
        previous_tick = (_100ms_count >> LEDS_SPEED);
        /* Different conditions for LEDs patterns. */
        if(fault_ttp_reg & F_OTP)
        {
            /* Temperature fault. */
            GREEN_LED_SET();
        }
        else if(fault_ttp_reg & F_OVP)
        {
            /* Bulk over voltage. */
        	GREEN_LED_SET();
        }
        else if(fault_ttp_reg & F_BOP)
        {
            /* Input voltage out of range. */
        	GREEN_LED_SET();
        }
        else if(fault_ttp_reg & F_PEAK)
        {
            /* Bulk under voltage protection. */
        	GREEN_LED_SET();
        }
        else if(fault_ttp_reg & F_UVP)
        {
            /* Under or over voltage protection triggered. */
        	GREEN_LED_SET();
        }
        else if(status_ttp_reg & ST_LATCH)
        {
            /* Converter latched. */
        	GREEN_LED_TGL();
        }
        else if (status_ttp_reg & ST_RUNNING)
        {
            if (dcdc_fault_reg != 0)
            {
                /* DCDC stage fault. */
            	GREEN_LED_TGL();
            }
            else
            {
                /* Normal ON conditions. */
                GREEN_LED_RST();
            }
        }
        else
        {
            if (dcdc_fault_reg != 0)
            {
                /* Normal OFF conditions. */
                GREEN_LED_SET();
            }
            else
            {
                /* Normal OFF conditions. */
            	GREEN_LED_SET();
            }
        }
    }
}

/**-----------------------------------------------------------------------------
 * \brief 	Assign value to the coefficients of voltage and current loops.
 *   		Reset the state and output of both controllers.
 *   		Reset current offset calculation variables.
 *   		Set initial power for inverter operation.
 * \return 	None
 */
__attribute__((section(".ram_code")))
void control_loop_baby_init()
{
	/* PID coefficients. */
	voltage_vdcp_ctr.a[0] = (BBY_KP + BBY_KI + BBY_KD);
	voltage_vdcp_ctr.a[1] = (-BBY_KP - 2 * BBY_KD);
	voltage_vdcp_ctr.a[2] = (BBY_KD);
	/* Current soft-start is not used --> output of the voltage loop is always the voltage loop path. */
	/* Start of the voltage loop in zero. */
    voltage_vdcp_ctr.output_int.i32 = 0;
    voltage_vdcp_ctr.output_int.i16[1] = AUX_BOOST_MIN_DTY;
	/* Target DC bulk voltage */
    voltage_vdcp_ctr.ref = 0;
	/* Error initialization. */
    voltage_vdcp_ctr.error[0] = 0;
    voltage_vdcp_ctr.error[1] = 0;
    /* Default value of the feed-forward slope. */
    voltage_vdcp_ctr.bby_ffw_slope = BBY_FFW_VIN_SLOPE;
    /* Set maximum duty of baby boost. */
    SET_DUTY_PERIOD_AUX(AUX_BOOST_MIN_DTY, AUX_BOOST_PR);
    /* The peak current would stop the PWM duty as well. But bbyboost is controlled by duty. */
    peak_ref_set(MAX_IPK_REF);
}

/**-----------------------------------------------------------------------------
 * \brief 	Assign value to the coefficients of voltage and current loops.
 *   		Reset the state and output of both controllers.
 *   		Reset current offset calculation variables.
 *   		Set initial power for inverter operation.
 * \return 	None
 */
__attribute__((section(".ram_code")))
void control_loop_ttp_init()
{
    /*------------------------------------------------------------------------*/
    /*  PARAMETERS OF V-LOOP (PFC mode only)                                  */
    /*------------------------------------------------------------------------*/
    /* Error coefficient (kmult = 2^-3 * 3.3 / PI + pole / cascaded notch filter 10kHz/2). */
	voltage_vbulk_ctr.a[0] = 26214; // 3276; // 6881 --> Q14; 2 * 180 uF; 400 Vbulk
	voltage_vbulk_ctr.a[1] = -26057; // -3257; // -6839 --> Q14; 2 * 180 uF; 400 Vbulk
    /* State feedback coefficients */
	voltage_vbulk_ctr.b[0] = 27033; // 27033 --> Q14; 2 * 180 uF; 400 Vbulk
	voltage_vbulk_ctr.b[1] = -10649; // -10649 --> Q14; 2 * 180 uF; 400 Vbulk

	/* Current soft-start is not used --> output of the voltage loop is always the voltage loop path. */
	voltage_vbulk_ctr.output = 0;
	/* Start of the voltage loop in zero. */
	voltage_vbulk_ctr.output_int.i32 = 0;
	/* Save state for next calculation. */
	voltage_vbulk_ctr.state[0] = 0;
	voltage_vbulk_ctr.state[1] = 0;
	voltage_vbulk_ctr.ref = 0;
	/* Error initialization. */
	voltage_vbulk_ctr.error = 0;
	/*------------------------------------------------------------------------*/
	/* I-LOOP PARAMETERS INITIALIZATION: PFC-A				                  */
	/*------------------------------------------------------------------------*/
	/* Compensator output (duty) and input (error) initialisation */
	/* output[k] value. */
	current_ctrl_A.output.i32 = 0;
	/* e[k] value. */
	current_ctrl_A.error = 0;
	/* Duty information in "upper" 16bits of the output information. */
	/* Very first duty cycle fixed to the minimum allowed. */
	current_ctrl_A.output.i16[1] = MIN_DUTY;
#if !PWM_DEBUG
	/* Set minimum frequency and initial duty cycle. */
	SET_DUTY_PERIOD_A(current_ctrl_A.output.i16[1], PR_MAX);
#endif
	/*------------------------------------------------------------------------*/
	/* I-LOOP PARAMETERS INITIALIZATION: PFC-B				                  */
	/*------------------------------------------------------------------------*/
	/* Compensator output (duty) and input (error) initialization */
	/* output[k] value. */
	current_ctrl_B.output.i32 = 0;
	/* e[k] value. */
	current_ctrl_B.error = 0;
	/* Duty information in "upper" 16bits of the output information. */
	/* Very first duty cycle fixed to the minimum allowed. */
	current_ctrl_B.output.i16[1] = MIN_DUTY;
#if !PWM_DEBUG
	/* Set minimum frequency and initial duty cycle. */
	SET_DUTY_PERIOD_B(current_ctrl_B.output.i16[1], PR_MAX);
#endif

	/*------------------------------------------------------------------------*/
	/* I-LOOP PARAMETERS INITIALIZATION: Common PFC-A/B		                  */
	/*------------------------------------------------------------------------*/
	/* Current loop coefficients initialization for 0A operation */
	current_ctrl_common.a0 = COEFF_OFFSET_3k_Q;
	current_ctrl_common.a1 = -((ZERO_LOC_3k_Q * current_ctrl_common.a0) >> ILOOP_COEFF_Q);
	/* Multiplier intermediate and output result. */
	current_ctrl_common.iL_ref = 0;
    /* Reset AC zero crossing counting */
    AC_zero_cross.vac_zc_cnt = 0;

	/*------------------------------------------------------------------------*/
	/* Zero current offset calculation						                  */
	/*------------------------------------------------------------------------*/
    /** \todo PFC.iL_offset_done is never set to TRUE */
	if(!current_ctrl_common.iL_offset_done)
	{
		current_ctrl_A.iL_offset = 0;
		current_ctrl_A.iL_offset_acc = 0;
		current_ctrl_B.iL_offset = 0;
		current_ctrl_B.iL_offset_acc = 0;
		current_ctrl_common.iL_offset_cnt = 0;
		current_ctrl_common.iL_offset_en = false;
		current_ctrl_common.iL_zero_cnt = 0;
		current_ctrl_A.iL_zero_acc = 0;
		current_ctrl_B.iL_zero_acc = 0;

        /* Just an initialization. */
        PFC.dt_rising = 20;
        PFC.dt_falling = 20;
	}
}

/**-----------------------------------------------------------------------------
 * \brief   IIR coefficients initialization.
 * 			IIR cascaded low-pass filter of the AC voltage to obtain the RMS value.
 * 			IIR notch filter of the DC voltage to remove 100/120Hz ripple.
 * 			Sampling frequency of both filters is 2500Hz, every 2 cycles of CCU40.CC41 @ 5kHz.
 * \return  None
 */
void IIR_param_init(void)
{
	/* AC Filter coefficients IIR section 1, Q14; 80dB design. */
	/* 5kHz / 10kHz (comment) */
	AC_filter.k1  	= 392;
	AC_filter.a1[0]  = 16384; // 2^14
	AC_filter.a1[1]  = -31565;
	AC_filter.a1[2]  = 16384; // 2^14
	AC_filter.b1[0]  = 16384; // 2^14
	AC_filter.b1[1]  = -32224;
	AC_filter.b1[2]  = 15869;
	/* AC Filter coefficients IIR section 2, Q14; 80dB design. */
	AC_filter.k2  	= 73;
	AC_filter.a2[0]  = 16384; // 2^14
	AC_filter.a2[1]  = -26328;
	AC_filter.a2[2]  = 16384; // 2^14
	AC_filter.b2[0]  = 16384; // 2^14
	AC_filter.b2[1]  = -31499;
	AC_filter.b2[2]  = 15143;
	/* AC filter intermediate and output state initialization. */
	AC_filter.u1[0] = 0;
	AC_filter.u1[1] = 0;
	AC_filter.u2[0] = 0;
	AC_filter.u2[1] = 0;
	AC_filter.output_1st = 0;
	AC_filter.output = 0;

	/* DC Filter coefficients IIR section 1 (notch 100Hz), Q14. */
	/* 5kHz / 10kHz (comment) */
	DC_filter.k1  	= 15368;
	DC_filter.a1[0]  = 16384; // 2^14
	DC_filter.a1[1]  = -31738;
	DC_filter.a1[2]  = 16384; // 2^14
	DC_filter.b1[0]  = 16384; // 2^14
	DC_filter.b1[1]  = -29771;
	DC_filter.b1[2]  = 14352;
	/* DC Filter coefficients IIR section 2 (notch 120Hz), Q14. */
	DC_filter.k2  	= 15368;
	DC_filter.a2[0]	= 16384; // 2^14
	DC_filter.a2[1]  = -31289;
	DC_filter.a2[2]  = 16384; // 2^14
	DC_filter.b2[0]  = 16384; // 2^14
	DC_filter.b2[1]  = -29349;
	DC_filter.b2[2]  = 14352;
	/* DC filter intermediate and output state initialization. */
	DC_filter.u1[0] = 0;
	DC_filter.u1[1] = 0;
	DC_filter.u2[0] = 0;
	DC_filter.u2[1] = 0;
	DC_filter.output_1st = 0;
	DC_filter.output = 0;
}

/**-----------------------------------------------------------------------------
 * \brief	Time counter is a background process to count
 * 			"real-time" in the application.
 * \return	None
 */
__attribute__((section(".ram_code")))
void time_counter()
{
    static uint32_t previous_tick; /**< Previous counter tick. */
    static uint8_t _1ms_cnt; /**< 1ms counter. */

    /* 1ms Timing counting (approx. 1.15ms with 65kHz/5kHz iLoop/vLoop) */
    if ((time_count_5kHz >> LIMIT_1_MILI_S) != previous_tick)
    {
        previous_tick = (time_count_5kHz >> LIMIT_1_MILI_S);
    	/* 1ms counter */
        _1ms_cnt ++;
    }
    /* 100ms Timing counting using the 1ms base (approx 120ms with 65kHz/5kHz iLoop/vLoop) */
    if (_1ms_cnt > LIMIT_100_MILI_S)
    {
    	/* 100ms counter */
    	_1ms_cnt = 0;
        _100ms_count ++;
        /* Timer used for PLD input voltage monitoring */
        if(time_PLD_130V) time_PLD_130V --;
        if(time_PLD_150V) time_PLD_150V --;
    }
}

/**-----------------------------------------------------------------------------
 * \brief		Delay function
 * \param time	Wait time in multiples of 100us
 * \return		None
 */
void delay_100us(uint32_t time)
{
	volatile uint32_t t, m;
	/* This is an approximation, it is delaying based on execution time of instructions. */
    for (t = time; t > 0; t--)
    {
        for (m = (CLK_CORE_HZ / 10000); m > 0; m--);
    }
}

/**-----------------------------------------------------------------------------
 * \brief	BOP protection, checking AVG input voltage. <BR>
 *   		The function modifies the control status register.
 * \return 	None
 */
__attribute__((section(".ram_code")))
void input_voltage_observer()
{
	static bool BOP_flag_150V; /**< PLD voltage flag 150V. */
	static bool BOP_flag_130V; /**< PLD voltage flag 130V. */

    /* Leave stand-by state if the input voltage is in range before start-up. */
    if ((status_ttp_reg & ST_STB) && (PFC.vin_BOP > VIN_MIN_START_INIT))
    {
        /* Correct input voltage at starting (after checking grid frequency). */
    	/** \todo Set back for normal operation. */
        status_ttp_reg = ST_STB_BOP; // ST_STB_BOP to run state machine ---- ST_STB to avoid starting of the state machine */
        /* Clear the BOP fault flag when the input voltage is in range. */
        fault_ttp_reg &= ~F_BOP;
        /* Clear the input voltage out of range flag. */
        BOP_flag_130V = false;
        BOP_flag_150V = false;
    }
    /* Input voltage monitoring during operation. */
    if (status_ttp_reg & (ST_SOFT_START | ST_RUNNING))
    {
        if ((PFC.vin_BOP < VIN_MIN_PLD_130V) && !BOP_flag_130V)
        {
            /* Set 500ms timer for 130V PLD. */
            time_PLD_130V = 5;
            /* Set input voltage out of range flag. */
            BOP_flag_130V = true;
        }
        else if (PFC.vin_BOP < VIN_MIN_PLD_150V)
        {
            if (!BOP_flag_150V)
            {
                /* Set 2s timer for 150V PLD. */
                time_PLD_150V = 18;
                /* Set input voltage out of range flag. */
                BOP_flag_150V = true;
            }
            if ((PFC.vin_BOP > VIN_MIN_PLD_130V) && BOP_flag_130V)
            {
                /* Reset 130V PLD timer. */
                time_PLD_130V = 0;
                /* Clear input voltage out of range flag. */
                BOP_flag_130V = false;
            }
        }
        else if (PFC.vin_BOP > VIN_MIN_START)
        {
            /* Reset 130V and 150V PLD timers. */
            time_PLD_130V = 0;
            time_PLD_150V = 0;
            /* Clear input voltage out of range flags. */
            BOP_flag_130V = false;
            BOP_flag_150V = false;
        }
    }
    /* Check timers if input voltage fault was detected. */
    if (BOP_flag_130V && (!time_PLD_130V))
    {
        if (PFC.vin_BOP < VIN_MIN_START)
        {
            /* Turn-off PWM in case input voltage is still out of range after the defined time. */
            PWM_OFF();
            SR_OFF();
            /* Ensure the PWM is turned OFF in next PWM ISR. */
            ttp_voltage_loop_ON = false;
            ttp_current_loop_ON = false;
            /* Fault active. */
            fault_ttp_reg |= F_BOP;
            /* Wait before starting the sequence. Approximately 880 ms. [1.6 ms] */
            time_INIT = 550;
            /* Go to state with no decision. */
            status_ttp_reg = ST_INIT;
        }
        else
        {
            /* Reset 130V timer and flag. */
            time_PLD_130V = 0;
            BOP_flag_130V = false;
        }
    }
    /* Check timers if input voltage fault was detected. */
    if (BOP_flag_150V && (!time_PLD_150V))
    {
        if (PFC.vin_BOP < VIN_MIN_START)
        {
            /* Turn-off PWM in case input voltage is still out of range after the defined time. */
            PWM_OFF();
            SR_OFF();
            /* Ensure the PWM is turned OFF in next PWM ISR. */
            ttp_voltage_loop_ON = false;
            ttp_current_loop_ON = false;
            /* Fault active. */
            fault_ttp_reg |= F_BOP;
            /* Wait before starting the sequence. Approximately 880 ms. [1.6 ms] */
            time_INIT = 550;
            /* Go to state with no decision. */
            status_ttp_reg = ST_INIT;
        }
        else
        {
            /* Reset 150V timer and flag. */
            time_PLD_150V = 0;
            BOP_flag_150V = false;
        }
    }
    /* Turbo bandwidth when Vbulk drops below threshold. */
    if (status_ttp_reg & (ST_RUNNING))
    {
		if (PFC.vbulk < VBULK_OFFSET_NL_ON)
		{
			/* Extra BW in voltage loop for a defined time: 1250 -> 250ms */
			voltage_vbulk_ctr.qVbus_BW = 1000; // 30 ms
		}
    }
}

/**-----------------------------------------------------------------------------
 * \brief   Configuration and initialisation of the die temperature sensor.
 * \return  None
 */
__attribute__((section(".ram_code")))
inline uint16_t die_temp_sens_value()
{
    uint16_t die_temperature;
    /* If the die temperature sensor is not busy. */
    if (!(SCU_GENERAL->DTSSTAT & SCU_GENERAL_DTSSTAT_BUSY_Msk))
    {
        /* Start a new temperature measurement. */
        SCU_GENERAL->DTSCON |= (uint32_t)SCU_GENERAL_DTSCON_START_Msk;
    }
    die_temperature = (uint16_t)((SCU_GENERAL->DTSSTAT & SCU_GENERAL_DTSSTAT_RESULT_Msk) >> SCU_GENERAL_DTSSTAT_RESULT_Pos);
    return die_temperature;
}

/**-----------------------------------------------------------------------------
 * \brief   Adjust Fan PWM duty
 * \return  None
 */
__attribute__((section(".ram_code")))
static inline void fan_duty_set(int16_t duty)
{
	((XMC_CCU4_SLICE_t*) CCU41_CC40)->CRS = (uint16_t)(3199u - duty); // PWM_FAN.ccu4_slice_ptr->CRS = (uint16_t)(PWM_FAN.config_ptr->period_value - duty);
	((XMC_CCU4_MODULE_t*) CCU41_BASE)->GCSS = (uint32_t)XMC_CCU4_SHADOW_TRANSFER_SLICE_0; // PWM_FAN.ccu4_module_ptr->GCSS = PWM_FAN.shadow_txfr_msk;
}

/**-----------------------------------------------------------------------------
 * \brief   Temperature monitoring: protection and fan control
 * \return  None
 */
__attribute__((section(".ram_code")))
void temperature_ctr()
{
    static uint8_t over_temp_in_cnt; /* Filtering of over-temperature protection. */
    static uint32_t previous_tick_fan_tacho; /* Adjusting of fan tacho check delay. [control_cyc] */
    static uint32_t previous_tick_fan_pwm; /* Adjusting of fan PWM delay. [control_cyc] */
    static uint16_t fan_error_cnt; /* Filtering of tachometer detection error. [control_cyc] */
    static uint8_t fan_dec_speed_cnt; /* Filtering of fan lowering speed. */

    /* NTC temperature measurement. */
    uint16_t temperature_PFC = ADC_NTC1_RESULT;
    /* NTC temperature measurement. */
	uint16_t temperature_LLC = ADC_NTC2_RESULT << 2;
    PFC.temperature_amb = die_temp_sens_value();
    uint16_t temperature_min = (temperature_PFC > temperature_LLC) ? temperature_LLC : temperature_PFC;
    PFC.temperature = ((PFC.temperature * 3) + temperature_min) >> 2;

	/* Check if fan is rotating every 100ms */
    /* 102.4ms Timing counting using the 10us (ISR) base */
    if ((time_count_5kHz >> FAN_TACHO_CLK_DIV) != previous_tick_fan_tacho)
    {
        /* If this condition is triggered the fan is not moving or moving slow */
        if ((((XMC_CCU4_SLICE_t*) CCU41_CC43)->TIMER < FAN_MIN_TCH_PULSES) && (status_ttp_reg & ST_RUNNING)) //if ((CNT_FAN.ccu4_handle->slice_ptr->TIMER < FAN_MIN_TCH_PULSES) && (status_ttp_reg & ST_RUNNING))
        {
            if(fan_error_cnt > 10)
            {
                /* Fan protection can be deactivated during debugging activities. */
                if(!DEBUG_FAN)
                {
                    PWM_OFF();
                    SR_OFF();
                    ttp_voltage_loop_ON = false;
                    ttp_current_loop_ON = false;
                    status_ttp_reg = ST_LATCH;
                }
            }
            else
            {
                fan_error_cnt ++;
            }
        }
        else
        {
            if(fan_error_cnt > 0)
            {
                fan_error_cnt --;
            }
        }
		/* Clears counter value. */
        ((XMC_CCU4_SLICE_t*) CCU41_CC43)->TCCLR = CCU4_CC4_TCCLR_TCC_Msk; //CNT_TACHO.ccu4_handle->slice_ptr->TCCLR = CCU4_CC4_TCCLR_TCC_Msk;
		previous_tick_fan_tacho = (time_count_5kHz >> FAN_TACHO_CLK_DIV);
	}
    /* Time division of the controller clock. */
    if((time_count_5kHz >> FAN_PWM_CLK_DIV) != previous_tick_fan_pwm)
    {
        /* Adjust the fan speed proportionally to the NTC measured temperature. */
        int32_t ntc_fan_pwm = (FAN_NTC_MIN + (((int32_t)(NTC_65_DEG - PFC.temperature) * FAN_NTC_FACTOR) >> FAN_NTC_RATIO));
        /* Adjust the fan speed proportionally to the average current measurement. */
        int32_t il_fan_pwm = ((int32_t)((PFC.adc_io_measure - FAN_IL_OFFST) * FAN_IL_FACTOR) >> FAN_IL_RATIO);
        /* Set the fan speed to the maximum of the above requested. */
        int32_t fan_new_duty = (ntc_fan_pwm > il_fan_pwm) ? ntc_fan_pwm : il_fan_pwm;

        /* Slow down the changes in the fan duty. */
        if(fan_new_duty > fan_duty)
        {
            fan_duty ++;
            fan_dec_speed_cnt = FAN_DEC_SPEED_DIV;
        }
        else if(fan_new_duty < fan_duty)
        {
            if(fan_dec_speed_cnt > 0)
            {
                fan_dec_speed_cnt --;
            }
            else
            {
                fan_duty --;
                fan_dec_speed_cnt = FAN_DEC_SPEED_DIV;
            }
        }
        /* Maximum speed limit. */
        fan_duty = (fan_duty > FAN_PWM_MAX) ? FAN_PWM_MAX : fan_duty;
        fan_duty = (fan_duty < FAN_PWM_MIN) ? FAN_PWM_MIN : fan_duty;
        /* Updates the PWM FAN signal generation. */
        fan_duty_set(fan_duty);
        previous_tick_fan_pwm = (time_count_5kHz >> FAN_PWM_CLK_DIV);
    }
    /*------------------------------------------------------------------------*/
    /*
     * Over temperature limit checking.
     * Note: NTC value decreases for higher temperatures.
     */
    if(PFC.temperature < NTC_TEMP_LATCH)
    {
        if (over_temp_in_cnt > 0)
        {
            over_temp_in_cnt --;
        }
        else
        {
            PWM_OFF();
            SR_OFF();
            /* Temperature too high. Latching protection. */
            status_ttp_reg = ST_LATCH;
            fault_ttp_reg |= F_OTP;
        }
    }
    else
    {
        over_temp_in_cnt = OVER_TEMP_IN_CNT_LIM;
    }
}

/**-----------------------------------------------------------------------------
 * \brief   Flash memory sleep state control.
 * \return  None
 */
__attribute__((section(".ram_code")))
void flash_sleep_ctr()
{
    static uint32_t time_tick;
    static uint16_t flash_sleep_cnt; /**< 30 seconds delay to set flash into sleep. */

    /* Flash timer tick division. */
    if((_100ms_count >> FLASH_CLK_DIV) != time_tick)
    {
        time_tick = (_100ms_count >> FLASH_CLK_DIV);
        if(flash_sleep_cnt > 0)
        {
            flash_sleep_cnt --;
        }
        else
        {
            /* Flash into sleep mode after delay. */
            if(!(XMC_FLASH_GetStatus() & XMC_FLASH_STATUS_SLEEP_MODE))
            {
                //XMC_FLASH_EnableSleepRequest();
            }
        }
    }
}
