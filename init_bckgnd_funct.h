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
 * \file	init_bckgnd_funct.h
 * \author 	Meneses Herrera, David; Escudero, Manuel.
 * \date	25.03.2024
 * \brief   Header of the functions for SW initialization and running in the
 *          background during operation.
 */

#ifndef INIT_BCKGND_FUNCT_H_
#define INIT_BCKGND_FUNCT_H_

#include <DAVE.h>
#include "xmc_flash.h"

/** Enables debug sequence for the Vac cross, to be generated without supplying the power board. */
#define ZERO_CROSS_DEBUG	(0)
/**-----------------------------------------------------------------------------
 * Debug of execution time using cycle counter of Debug peripheral.
 */
#define  ARM_CM_DEMCR      (*(uint32_t *)0xE000EDFC)
#define  ARM_CM_DWT_CTRL   (*(uint32_t *)0xE0001000)
#define  ARM_CM_DWT_CYCCNT (*(uint32_t *)0xE0001004)

/**-----------------------------------------------------------------------------
 * \name Temperature parameters.
 * @{
 */
/** Fan rotation detection enable/disable. */
#define DEBUG_FAN				(0) // 0: fan rotation check enabled, converter shutdown
/** Minimum number of tachometer pulses to be detected. [rising_edge] */
#define FAN_TACHO_CLK_DIV       (10)
/** Minimum number of tachometer pulses to be detected. [rising_edge] */
#define FAN_PWM_CLK_DIV         (3)
/** Temperature control clock divider. [control_cyc / 2^TEMPERATURE_CLK_DIV] */
#define TEMPERATURE_CLK_DIV     (10)
/** Reading of the NTC at 65 °C */
#define NTC_65_DEG              (2000) /* NTC_65_DEG Aprox. 2000 ADC. */
/** Timer for the speed set of the fan. */
#define FAN_DEC_SPEED_DIV       (2)
/** Power of 2 close to the maximum expected value of output current. [bits] */
#define FAN_NTC_RATIO           (12) /* Is using filtering, then the values are shifted to the left. */
/** Multiplier to escalate fan speed related to Io. [units] */
#define FAN_NTC_FACTOR          (3000) /* Aprox.: (FAN_PWM_MAX - FAN_PWM_MIN) * (1 << IL_FAN_RATIO) / (NTC_35_DEG - NTC_95_DEG) */
/** Minimum number of tachometer pulses to be detected. [rising_edge] */
#define FAN_MIN_TCH_PULSES      (2)
/** Power of 2 close to the maximum expected value of output current. [bits] */
#define FAN_IL_RATIO            (12) /* Is using filtering, then the values are shifted to the left. */
#define FAN_IL_OFFST            (1750)
/** Multiplier to escalate fan speed related to Io. [units] */
#define FAN_IL_FACTOR           (3000) /* Aprox.: (FAN_PWM_MAX - FAN_PWM_MIN) * (1 << IL_FAN_RATIO) / MAX_IO */
/** Maximum Fan speed. [CCU_clock] */
#define FAN_PWM_MAX             (3150)
/** Minimum Fan speed. [CCU_clock] */
#define FAN_PWM_MIN             (700)
/** Minimum Fan speed. [CCU_clock] */
#define FAN_NTC_MIN             (2300)
/** Over temperature limit. [ADC_units] */
#define NTC_TEMP_LATCH          (400) /* 600 is approx. 115 °C; 400 is approx. 130 °C */
/** Over temperature condition filter. [control_cyc] */
#define OVER_TEMP_IN_CNT_LIM    (10)
/** @} */

/**-----------------------------------------------------------------------------
 * \name LEDs
 * Blinking LEDs configuration.
 * @{
 */
/** Switching speed of LEDs. [control_cyc/2^LEDS_SPEED] */
#define LEDS_SPEED      (2)
/** MACRO. Switch OFF green LED. */
#define GREEN_LED_RST() (((XMC_GPIO_PORT_t *) PORT1_BASE)->OMR = 0x10000UL << 3u)
/** MACRO. Toggle green LED. */
#define GREEN_LED_TGL() (((XMC_GPIO_PORT_t *) PORT1_BASE)->OMR = 0x10001UL << 3u)
/** MACRO. Switch ON green LED. */
#define GREEN_LED_SET() (((XMC_GPIO_PORT_t *) PORT1_BASE)->OMR = (uint32_t)1U << 3u)
/** MACRO. Checks green LED status. */
#define GREEN_LED_ON    ((((XMC_GPIO_PORT_t *) PORT1_BASE)->IN >> 3u) & (uint32_t) 1u)
/** @} */

/**-----------------------------------------------------------------------------
 * \name Systick frequency configuration
 * @{
 */
/** CLK frequency. [Hz] */
#define CLK_CORE_HZ     (80000000UL)
/** Vloop frequency. [Hz] */
#define VLOOP_FREQ_HZ	(5000UL)
/** Systick interrupt frequency. Divided by the Clock frequency. [CLK_clk] */
#define SYSTICK_VALUE	(CLK_CORE_HZ / VLOOP_FREQ_HZ)
/** @} */

/**-----------------------------------------------------------------------------
 * \name Input voltage range for Brown IN and Brown OUT of DCDC (Power Good LLC).
 * @{
 */
#if ZERO_CROSS_DEBUG
/** 84Vrms, converter start threshold */
#define VIN_MIN_START       (10)
#else
/** 84Vrms, converter start threshold */
#define VIN_MIN_START       (1201) // 1201 --> 84 Vrms / 3.3 Vdd / 316k - 309k - 17.8k
#endif
/** 80Vrms, converter start threshold */
#define VIN_MIN_START_INIT  (1144) // 1144 --> 80 Vrms / 3.3 Vdd / 316k - 309k - 17.8k
/** 80Vrms; 0.5s if the voltage is between 65V and 85V (Power line disturbance) */
#define VIN_MIN_PLD_130V    (1144) // 1144 --> 80 Vrms / 3.3 Vdd / 316k - 309k - 17.8k
/** 70Vrms; 2s if the voltage is between 150V and 175V (Power line disturbance) */
#define VIN_MIN_PLD_150V    (1001) // 1001 --> 70 Vrms / 3.3Vdd / 316k - 309k - 17.8k
/** 60Vrms; RMS voltage to limit the 1/RMS^2 calculation. */
#define VRMS_MIN_LIMIT	    (876) // 876 --> 60 Vrms / 3.3 Vdd / 316k - 309k - 17.8k
#if (VRMS_MIN_LIMIT >=  VIN_MIN_PLD_130V)
	#error "RMS limitation should be lower than minimum expected RMS during operation"
#endif
/** @} */

/**-----------------------------------------------------------------------------
 * \name Current limit
 * @{
 */
/** Maximum input current limitation */
#define I_IN_AVG_MAX        (41250)	// 14 A // (3395) // 3395 --> 18 A / 3.3 Vdd / TLI4971_120A - 1.9SW; 11772 --> 13 A / 3.3 Vdd / TLI4971_25A - 1.9SW
/** Zero current of DAC for peak current limitation. */
#define IL_PK_ZERO			505	// 20.48 units per ampere MCR-20-3. This is the default zero amps voltage, i.e. 1.65V
/** Peak current for positive AC voltage. DAC units. */
#define IL_PK_POS			(IL_PK_ZERO + 400)	// Limit set to +20A // 20.48 units per ampere MCR-20-3
/** Peak current for negative AC voltage. DAC units. */
#define IL_PK_NEG			(IL_PK_ZERO - 400)	// Limit set to -20A // 20.48 units per ampere MCR-20-3
 /** @} */

/**-----------------------------------------------------------------------------
 * \name Timing definition
 * @{
 */
/** 1ms @ 5kHz */
#define LIMIT_1_MILI_S      (2)
/** 100ms @ 1ms counter */
#define LIMIT_100_MILI_S    (100)
/** @} */

/** Flash memory control clock divider. [control_cyc/2^FLASH_CLK_DIV] */
#define FLASH_CLK_DIV       (0)
/** Flash sleep delay. 30 seconds. */
#define FLASH_SLEEP_DLY     (300)

/**-----------------------------------------------------------------------------
 * External global variables.
 */
extern uint16_t dcdc_fault_reg;

/**-----------------------------------------------------------------------------
 * Function prototypes.
 */
void IIR_param_init();
void control_loop_ttp_init();
void control_loop_baby_init();
void time_counter();
void leds_ctr();
void temperature_ctr();
void delay_100us(uint32_t time);
void input_voltage_observer();
void flash_sleep_ctr();

#endif /* INIT_BCKGND_FUNCT_H_ */
