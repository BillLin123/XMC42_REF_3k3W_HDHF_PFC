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
/**
 * \file	Main.c
 * \author 	David Meneses Herrera
 * \date	25.03.2024
 * \brief   Application entry point
 *
 *          3kW Totem-Pole PFC interleaved dual stage.
 *          Auxiliary Baby-boost.
 */

/**
 * \copyright
 *      Copyright (c) 2024, Infineon Technologies AG
 *      <br>All rights reserved.
 *      <br>Redistribution and use in source and binary forms, with or without
 *      modification,are permitted provided that the following conditions are met:
 *      - Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *      - Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *      - Neither the name of the copyright holders nor the names of its contributors
 *      may be used to endorse or promote products derived from this software without
 *      specific prior written permission.
 *      .
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *      AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *      IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *      ARE  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *      LIABLE  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *      CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *      SUBSTITUTE GOODS OR  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *      INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *      CONTRACT, STRICT LIABILITY,OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *      ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *      POSSIBILITY OF SUCH DAMAGE.
 *      <br>To improve the quality of the software, users are encouraged to share
 *      modifications, enhancements or bug fixes with Infineon Technologies AG
 *      dave@infineon.com).
 */
/**
 * \mainpage 3kW Totem-Pole PFC interleaved dual stage.
 */

#include "DAVE.h"
#include "control_irq_v_loop_outer.h"
#include "config_adc.h"
#include "init_bckgnd_funct.h"
#include "communication.h"

/**-----------------------------------------------------------------------------
 * \brief 	Initialisation of the debugger timer for counting execution speed.
 */
void debug_timer_init()
{
	/* Enable the DWT unit. */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	/* Reset the cycle counter. */
	DWT->CYCCNT = 0;
	/* Enable the cycle counter. */
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**-----------------------------------------------------------------------------
 * \brief 	Background loop. Declared as a function to enable the linker to
 * 			locate the code in RAM.
 */
__attribute__((section(".ram_code")))
void background_fnctn()
{
    /*
     * Background low priority tasks.
     */
    while(1U)
    {
        /* Time counting based on current loop ISR. */
        time_counter();
        /* Input voltage monitoring for brown-out protection and start enabling. */
        input_voltage_observer();
        temperature_ctr();
        /* Communication with DCDC stage (load and temperature and voltage). */
        uart_DCDC_transmit();
        uart_DCDC_receive();
        flash_sleep_ctr();
        leds_ctr();
    }
}

/**-----------------------------------------------------------------------------
 * \brief main() - Application entry point
 *
 * <b>Details of function</b><br>
 * This routine is the application entry point. It is invoked by the device
 * startup code. It is responsible for invoking the APP initialisation
 * dispatcher routine - DAVE_Init() and hosting the place-holder for user
 * application code.
 */
int main(void)
{
	/* Initialisation of DAVE APPs.  */
	DAVE_Init();

	/* Enables the counter to divide the iLoopA_IRQ for the execution of the iRef_IRQ. */
    CCU40_CC40->TCCLR = CCU4_CC4_TCCLR_TRBC_Msk;
    CCU40_CC40->CRS = 0;
	CCU40_CC40->PRS = 1;
	CCU40->GCSS = CCU4_GCSS_S0SE_Msk;
	/* Connects the IRQ_CNT (CCU40.40) to the ADC service request (iLoopA_IRQ) as input through the OGU. */
	/*
	 * NOTE: We are executing the iRef_IRQ by a call from the iLoopA_IRQ, cycle by cycle. Therefore the IRQ is deactivated.
	 * Therefore the commented out lines below here. Leave them commented unless you plan to change the schedule.
	 */
	//XMC_ERU_OGU_EnablePeripheralTrigger(OGU_IRQ_CNT.eru, OGU_IRQ_CNT.channel, XMC_ERU_OGU_PERIPHERAL_TRIGGER1);
	//CCU40_CC40->TCSET = CCU4_CC4_TCSET_TRBS_Msk;

	/*
	 * Trap configuration using the CSG clamp functionality, triggered by SW.
	 */
	/* Clear PLCL in PLC register, so the clamp of the comparator is managed by SW. */
	HRPWM0_CSG1->PLC &= ~HRPWM0_CSG_PLC_PLCL_Msk;
	HRPWM0_CSG2->PLC &= ~HRPWM0_CSG_PLC_PLCL_Msk;
	/*
	 * Set CSGSTATG.PSLSy bit, so the output of the comparator is clamped by SW
	 * TRAP PWM_A and PWM_B.
	 */
	HRPWM0->CSGSETG = (HRPWM0_CSGSETG_SC1P_Msk | HRPWM0_CSGSETG_SC2P_Msk);

	/* Start with PWM and SR configured as GPIO with low level. */
	PWM_OFF();
	SR_OFF();
	PWM_AUX_OFF();
	HRPWM_Start(&HRPWM_AUX);
	/* Relay is off at starting, closing the relay decided in state machine. */
	RELAY_OFF();
	/* Baby-boost shall control the static switch. */
	STSW_OFF();
	/*
	 * Configure ADC trigger:
	 * PWM_PFC_A: CMU2 --> SR2 / Queue
	 * PWM_PFC_B: CMU2 --> SR3 / Scan
	 */
	XMC_CCU8_SLICE_EnableEvent(PWM_PFC_A.ccu8_slice_ptr, XMC_CCU8_SLICE_IRQ_ID_COMPARE_MATCH_UP_CH_2);
	XMC_CCU8_SLICE_SetInterruptNode(PWM_PFC_A.ccu8_slice_ptr, XMC_CCU8_SLICE_IRQ_ID_COMPARE_MATCH_UP_CH_2, XMC_CCU8_SLICE_SR_ID_2);
	XMC_CCU8_SLICE_EnableEvent(PWM_PFC_B.ccu8_slice_ptr, XMC_CCU8_SLICE_IRQ_ID_COMPARE_MATCH_UP_CH_2);
	XMC_CCU8_SLICE_SetInterruptNode(PWM_PFC_B.ccu8_slice_ptr, XMC_CCU8_SLICE_IRQ_ID_COMPARE_MATCH_UP_CH_2, XMC_CCU8_SLICE_SR_ID_3);

	/* Initialization of ADC peripheral.  */
	adc_init();
    die_temp_sens_init();

	/* Delay before deciding operation mode --> System to evolve at start-up. */
	delay_100us(10);

	/* Current and voltage control loop initialization. */
	current_ctrl_common.iL_offset_done = false;
	control_loop_ttp_init();
	control_loop_baby_init();
	/* Initialization of the digital filters. */
	IIR_param_init();

	/* Delay before starting the CCU counters --> System to evolve at start-up. */
	delay_100us(10);

	/* Synchronous start of PWM_A and PWM_B timers with half a period offset (phase shift). */
	/* Start PWM-PFC_A counter --> trigger ADC measurements: iLA, Vin-ac, iLaux; iLoop_A_ISR --> pseudo_pll. */
	/* Start PWM-PFC_B counter --> trigger ADC measurements: iLB, Vdc1/2; iLoop_B_ISR. */
	XMC_CCU8_EnableClock(PWM_PFC_A.ccu8_module_ptr, PWM_PFC_A.slice_number);
    XMC_CCU8_EnableClock(PWM_PFC_B.ccu8_module_ptr, PWM_PFC_B.slice_number);
    XMC_CCU8_SLICE_SetTimerPeriodMatch(PWM_PFC_A.ccu8_slice_ptr, PR_MAX);
    XMC_CCU8_SLICE_SetTimerPeriodMatch(PWM_PFC_B.ccu8_slice_ptr, PR_MAX);
    XMC_CCU8_SLICE_SetTimerValue(PWM_PFC_A.ccu8_slice_ptr, 0);
    XMC_CCU8_SLICE_SetTimerValue(PWM_PFC_B.ccu8_slice_ptr, (PR_MAX >> 1) + 1);
    XMC_CCU8_SLICE_StartConfig(PWM_PFC_A.ccu8_slice_ptr, XMC_CCU8_SLICE_EVENT_0, XMC_CCU8_SLICE_START_MODE_TIMER_START);
    XMC_CCU8_SLICE_StartConfig(PWM_PFC_B.ccu8_slice_ptr, XMC_CCU8_SLICE_EVENT_0, XMC_CCU8_SLICE_START_MODE_TIMER_START);
    XMC_CCU8_SLICE_EVENT_CONFIG_t synchronous_input = {.mapped_input = 7, .edge = XMC_CCU8_SLICE_EVENT_EDGE_SENSITIVITY_RISING_EDGE, .level = XMC_CCU8_SLICE_EVENT_EDGE_SENSITIVITY_NONE, .duration = XMC_CCU8_SLICE_EVENT_FILTER_DISABLED};
    XMC_CCU8_SLICE_ConfigureEvent(PWM_PFC_A.ccu8_slice_ptr, XMC_CCU8_SLICE_EVENT_0, &synchronous_input);
    XMC_CCU8_SLICE_ConfigureEvent(PWM_PFC_B.ccu8_slice_ptr, XMC_CCU8_SLICE_EVENT_0, &synchronous_input);

    XMC_SCU_SetCcuTriggerHigh(XMC_SCU_CCU_TRIGGER_CCU80);
    XMC_SCU_SetCcuTriggerLow(XMC_SCU_CCU_TRIGGER_CCU80);

	/* Voltage loop triggered by Systick at 5kHz. */
	SysTick_Config(SYSTICK_VALUE);
	NVIC_SetPriority(SysTick_IRQn, VLOOP_PRIORITY);

	/* Delay after starting the CCU counters --> ADC measurement to stabilise. */
	delay_100us(1);

	/* This timer can be used to count execution cycles. */
	//debug_timer_init();
	/* Faster this way, keeps the loop in RAM. */
    background_fnctn();
}
