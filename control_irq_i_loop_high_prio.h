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
 * \file    control_irq_i_loop_high_prio.h
 * \author  Meneses Herrera, David; Escudero Rodriguez, Manuel
 * \date    22.05.2024
 * \brief   Interrupts code
 */
#ifndef CONTROL_IRQ_I_LOOP_HIGH_PRIO_H_
#define CONTROL_IRQ_I_LOOP_HIGH_PRIO_H_

#include <DAVE.h>
#include "init_bckgnd_funct.h"
#include "communication.h"
#include "config_adc.h"

#define I_LOOP_DEBUG	(0) // Activate for the AC cycle Debug.
#define I_FSW_DEBUG		(0) // Activate for the FSW cycle Debug.

/**-----------------------------------------------------------------------------
 * \name Shifting related macros.
 * @{
 */
/** ADC normalization shift (Q15) */
#define ADC_TO_Q15          (3)
/** RMS filter coefficient format */
#define IIR_QFORMAT         (14)
/** Shift for RMS filter input */
#define ADC_TO_IIR          (IIR_QFORMAT + ADC_TO_Q15)
/** Shift of RMS filter for Iref calculation */
#define LPF_TO_2ADC         (ADC_TO_Q15 - 1)
/** Shifting for coefficient calculation: Q29 to Q14 */
#define TO_QiLOOP           (15)
/** Current reference adaptation to Q15 representation */
#define CURRENT_REF_TO_Q15  (19)
/** @} */

/**-----------------------------------------------------------------------------
 * \name Current offset and gain macros
 * @{
 */
/** Number of samples to be acquire for zero current offset calculation, in power of 2. */
#define OFFSET_SAMPLES_2    (8)
/** Number of samples to be acquire for zero current offset calculation. */
#define OFFSET_SAMPLES      (256)
/** Count value when the offset calculation is done. */
#define OFFSET_END          (0xFFFF)
/** Vac voltage under which the current offset measurement is allowed, to avoid capacitor charge current. */
#define OFFSET_VAC          (414) // 414 --> 55Vac
/** SW current gain: 1.9. */
#define IL_AVG_GAIN	        (31129) //31129-Q14 --> kI_SW = 1.9
/** SW current gain Q representation. */
#define IL_AVG_GAIN_Q	    (14) //31129-Q14 --> kI_SW = 1.9
/** @} */

/**-----------------------------------------------------------------------------
 * \name Current loop coefficients calculation, CCM
 * @{
 */
/** Current coefficient Q representation. */
#define ILOOP_COEFF_Q               (14) // 1P1Z/400V/3.3Vdd/65kHz/3kHzBW/zero=0.95/TLI48mVA-1.9SW: 14 (CH27GT18-45t) 14:TLI10mVA
#define ILOOP_COEFF_TO_Q15          (16 - ILOOP_COEFF_Q)
#define ILOOP_COEFF_Q_SHIFT_TO_Q29  (14 - ILOOP_COEFF_Q)
/** Current coefficient offset of single linear region (coefficient for 0A operation) */
#define COEFF_OFFSET_3k_Q           (2227) //MCR1101-20-3_66mVA //(3062) TLI48mVA //(4911)  //1P1Z/400V/3.3Vdd/65kHz/3kHzBW/zero=0.95/TLI48mVA-1.9SW: 1023 (CH27GT18-45t) 4911:TLI10mVA
/** Current coefficient slope of single linear region, considering linear variation of the inductance */
#define COEFF_SLOPE_3k_Q            (1455) //MCR1101-20-3_66mVA //(2751) TLI48mVA //(16426)	//1P1Z/400V/3.3Vdd/65kHz/3kHzBW/zero=0.95/TLI48mVA-1.9SW: 712 (CH27GT18-45t) 16426:TLI10mVA
/** Current coefficient after single linear region, considering linear variation of the inductance */
#define COEFF_MIN_3k_Q              (236) //MCR1101-20-3_66mVA //(324) TLI48mVA //(749)	//1P1Z/400V/3.3Vdd/65kHz/3kHzBW/zero=0.95/TLI48mVA-1.9SW: 311 (CH27GT18-45t) 749:TLI10mVA
/** Zero location of the current compensator (ILOOP_COEFF_Q format) */
#define ZERO_LOC_3k_Q               (15564) //1P1Z/400V/3.3Vdd/65kHz/3kHzBW/zero=0.95/TLI48mVA-1.9SW: 15564 (CH27GT18-45t) TLI10mVA
/** Current limit for single linear variation of the inductance */
#define REG1_ILIMIT_Q15             (44826) //MCR1101-20-3_66mVA //(32601) TLI48mVA //(8301) //3.3Vdd/TLI-1.9SW --> 36A-32760; MAX current sensed TLI48mVA (CH27GT18-45t) 8301:TLI10mVA
/** Current coefficient when negative current detected: a0 at max. allowed negative current (around 10A) */
#define COEFF_a0_INEG_Q             (1840) //MCR1101-20-3_66mVA //(2530) TLI48mVA //(4158)  //1P1Z/400V/3.3Vdd/65kHz/3kHzBW/zero=0.95/TLI48mVA-1.9SW: 866 (CH27GT18-45t) 4158:TLI10mVA
/** Negative current for fast converter turn off */
#define INEG_FAULT_Q15              (-4000)// (-8427) MCR1101-20-3_66mVA //(-6129) TLI48mVA //(-1501) //-7.96A/3.3Vdd/TLI48mVA-1.9SW: -7208 (CH27GT18-45t) -1501:TLI10mVA
/** Maximum allowed value of the voltage loop output. */
#define VOLTAGE_INT_OUTPUT_MAX 		(40000) // 25000 is 3.0 kW -- set to 29200 to target 3.5kW for head-room
/** @} */

/**-----------------------------------------------------------------------------
 * \name Duty cycle limitation
 * @{
 */
/** Maximum allowed duty cycle during operation */
#define MAX_DUTY        (1204) // 98 % - 1229U(65kHz) --> 1204;
/** Minimum allowed duty cycle during operation */
#define MIN_DUTY        (25) // 1% - 1229U(65kHz) --> 12;
/** Maximum allowed duty cycle variation of the control loop */
#define MAX_DELTA_DUTY  (MAX_DUTY >> 1) // Current loop will modify the DFF in a fraction of the maximum duty
/** Maximum allowed duty cycle after AC lost */
#define MAX_DUTY_LCDO   (1155) // 94 % - 1475U --> 1387 (65kHz);
/** Switching period to be applied in nominal and high voltage input voltage operation. */
#define PR_MAX          (1229)
/** Maximum allowed peak current reference during operation. */
#define MAX_IPK_REF   	(600) // 1023 is max. DAC value.
/** Minimum allowed peak current reference during operation. */
#define MIN_IPK_REF   	(10)
/** @} */

/**-----------------------------------------------------------------------------
 * \name AC input related macros
 * @{
 */
/** Check the PWM signals without AC input. Un-comment to let the PWM come out. */
#define PWM_DEBUG					(0) // Set to 1 to take the PWM out with no power.
#if ZERO_CROSS_DEBUG
/** MACRO. Checks red LED status. */
#define VIN_AC_POLARITY             debug_AC_plrty
/** Input voltage to resume operation after zero crossing. [ADC_units] */
#define VIN_ON_THRSLD               (100)
#else
/** MACRO. Checks red LED status. */
#define VIN_AC_POLARITY             ((((XMC_GPIO_PORT_t *) PORT2_BASE)->IN >> 4u) & (uint32_t) 1u)
/** Input voltage to resume operation after zero crossing. [ADC_units] */
#define VIN_ON_THRSLD               (120) //120 --> 16Vac //95 --> 12Vac
#endif
/** Input voltage for zero crossing detection. PFC is turned off. [ADC_units] */
#define VIN_OFF_THRSLD				(174) //174 --> 23Vac
#if !(VIN_OFF_THRSLD >=  VIN_ON_THRSLD)
	#error "OFF threshold should be greater than ON threshold"
#endif
/** Input voltage for zero crossing detection. Slope change detection. [ADC_units] */
#define VIN_ZERO_THRSLD				(75) //90 --> 12Vac //75 --> 10Vac //65 --> 8Vac	//20 IS DEBUG VALUE!!!
#if !(VIN_ON_THRSLD >=  VIN_ZERO_THRSLD)
	#error "ON threshold should be greater than ZERO threshold"
#endif
/** Limitation of Vac ON/OFF detection delay after Vac OFF/ON detection. [Tsw]. */
#define PWM_TRANSITION_DLY		    (24) //(18) --> 65kHz(15.3us) rate, 277us
/** Number of samples of AC voltage under OFF_THRSLD to consider AC lost. [Tsw]. */
#define AC_LCDO_DETECT			    (130) //(130) --> 65kHz(15.3us) rate, 2ms
/** Counting time limit after AC lost is detected. [Tsw]. */
#define AFTER_AC_LOST_TIME          (4875) //(4875) --> 65kHz(15.3us) rate, 75ms
/** Number of switching cycles to delay the starting of the PWM/SR. */
#define SR_DELAY_SWCYC              (4) //(6)   //(2)       //3 //
/** Limit of AC voltage for duty feed-forward calculation. */
#define AC_400V                     (3246) //3246 is 400V in qVin_AC units
/** @} */

/**-----------------------------------------------------------------------------
 * Defined states to be used in the starting sequence and state machine.
 */
typedef enum
{
    ST_INIT			= 0x00, //0,
    ST_WAIT_PFC    	= 0x01, //2,
    ST_STB			= 0x02, //4,
    ST_STB_BOP		= 0x04, //8,
    ST_SOFT_START	= 0x08, //16,
    ST_RUNNING		= 0x10, //32,
    ST_LATCH		= 0x20, //64,
	ST_START_EN		= 0x40, //128,
} state_t;

/** PWM is on during converter operation, e.g. soft-start and running operation. */
#define PFC_RUNNING	(ST_SOFT_START | ST_RUNNING)

/** Defined fault states used to manage the converter protections. */
typedef enum
{
    F_INIT = 	0x0,	/**< Initial status / NO FAULT. */
    F_OVP = 	0x1,	/**< Bulk over-voltage triggered (PFC). */
    F_BOP = 	0x2,	/**< RMS input voltage out of range. */
    F_UVP = 	0x4,	/**< Bulk under-voltage triggered (PFC). */
    F_PEAK = 	0x8,	/**< NOT USED in this implementation. */
    F_OTP = 	0x10, 	/**< NOT USED in this implementation. */
	F_NEG_I = 	0x20,	/**< Negative current in respect to reference. Flag to turn off the converter. */
	F_DC_INV = 	0x40	/**< DC voltage is out of range in Inverter operation. */
} fault_t;

/**	Structure for the PFC related variables	*/
typedef struct
{
    uint16_t vin_AC;			/**< Input Voltage AC value. 'k' and 'k-1' values. */
    uint16_t vin_BOP;			/**< Input Voltage AVG value. Used as RMS (proportional). */
    uint32_t vin_inv2;			/**< Inverse of the input voltage RMS square in 32-bit Q31 codification. */
    uint16_t vbulk;				/**< VBulk voltage. */
    int32_t vbulk_filt;			/**< Filtered output Voltage DC value. */
    uint16_t vdcp;				/**< Output voltage. */
    uint16_t temperature;		/**< Temperature sensing in power board. */
    uint16_t temperature_amb;	/**< Temperature sensing in TSE of XMC. */
    uint32_t dt_rising;			/**< Rising dead time calculated in the dead time function but updated in the current loop (update race condition issue). */
    uint32_t dt_falling;		/**< Falling dead time calculated in the dead time function but updated in the current loop (update race condition issue). */
    uint16_t adc_io_measure;	/**< Current measurement from the DCDC stage for fan speed adjust. */
    bool re_strt_PWM_mid_cycl; 	/**< Negative current fault flag is cleared after a defined time and converter operation can be resumed. */
} PFC_var_t;

/**	Structure for the PFC related variables	*/
typedef struct
{
    uint16_t vac_zc_cnt;      	/**< Counting of AC zero crossing transitions. */
} AC_monitoring_t;

/** Enum to hold the input voltage polarity */
typedef enum
{
    NEGATIVE,
    POSITIVE
} grid_voltage_t;

/** Common variables structure for current loop*/
typedef struct
{
    int32_t a0;           	/**< PI 'k' error coefficient. Q14 format. */
    int32_t a1;         	/**< PI 'k-1' error coefficient. Q14 format. */
    uint32_t pr_vdc;       	/**< PR / Vbulk_acADC for DFF calculation during start-up */
    uint16_t vref_acadc;	/**< Bulk reference in AC-ADC units */
    uint16_t iL_ref;  		/**< Current reference value to be used in current loop. Q15 format */
    uint16_t iL_offset_cnt;	/**< Inductor current accumulation count for zero current calculation. */
    uint16_t iL_zero_cnt; 	/**<  */
    bool iL_offset_en;		/**< Inductor current zero calculation enable. */
    bool iL_offset_done;	/**< Allows inductor current zero calculation only after controller reset. */
/** Interleaving operation when PFC B is enabled. */
#define PFC_B_ENABLE    (1)
} common_I_t;

/** Controller structure for current loop. */
typedef struct
{
    union
    {
        int32_t i32;
        int16_t i16[2];
        int8_t i8[4];
    } output;       		/**< Previous ('k'), ('k-1') and ('k-2') outputs. Q15 format. */
    int16_t duty;        	/**< Duty cycle to be applied: ratio_FF+duty_iL. */
    int32_t error;			/**< Previous ('k'), ('k-1') and ('k-2') errors. Q15 format. */
    int16_t iL;		  		/**< Inductor current value after modification to be used in the current loop calculation. Q15 format. */
    uint32_t iL_zero_acc;  	/**<  */
    uint32_t iL_offset_acc;	/**< Inductor current accumulation for zero current calculation. Q15 format. */
    uint16_t iL_offset;	  	/**< Inductor current calculated zero. Q15 format */
} controller_I_t;

/** Voltage controller structure */
typedef struct
{
    int16_t a[2];		/**< PI 'k', 'k-1' and 'k-2' error coefficient. Q(format). */
    int16_t b[2];     	/**< PI 'k', 'k-1' and 'k-2' duty cycle coefficient. Q(format). */
    int16_t error;		/**< Voltage error ('k'), ('k-1') and ('k-2'). Q15. */
    int32_t state[2];	/**< output calculated value ('k'), ('k-1') and ('k-2'). Q(format+15). */
    union
    {
        int32_t i32;
        int16_t i16[2];
        int8_t i8[4];
    } output_int;    	/**< Intermediate output of the regulator in steady state. Q(format+15). */
    uint16_t output;  	/**< Output of the regulator. Used for current reference calculation. */
    uint16_t ref;    	/**< Bulk voltage reference value */
    bool first_iter;  	/**< First iteration of the voltage loop after not normal conditions */
    uint16_t qVbus_BW;	/**< Extra bandwidth during Vout drop. */
} controller_V_t;

/** Voltage controller structure */
typedef struct
{
    int16_t a[3];		/**< PI 'k', 'k-1' and 'k-2' error coefficient. Q(format). */
    int16_t error[2];	/**< Voltage error ('k'), ('k-1') and ('k-2'). Q15. */
    union
    {
        int32_t i32;
        int16_t i16[2];
        int8_t i8[4];
    } output_int;   	/**< Intermediate output of the regulator in steady state. Q(format+15). */
    union
    {
        int32_t i32;
        int16_t i16[2];
        int8_t i8[4];
    } output_ffw;		/**< Intermediate output of the regulator in steady state. Q(format+15). */
    uint16_t ref;    	/**< Bulk voltage reference value */
    bool ramp_up_on;	/**< Ramp up with feed-forward only. */
	bool burst_mode_on;
	uint16_t bby_ffw_slope;
} controller_bby_t;

/**-----------------------------------------------------------------------------
 *  External global variables.
 */
extern PFC_var_t PFC;
extern volatile state_t status_ttp_reg;
extern volatile fault_t fault_ttp_reg;
extern volatile state_t status_bbst_reg;
extern volatile fault_t fault_bbst_reg;
extern controller_V_t voltage_vbulk_ctr;
extern controller_bby_t voltage_vdcp_ctr;
extern controller_I_t current_ctrl_A;
extern controller_I_t current_ctrl_B;
extern common_I_t current_ctrl_common;

extern uint16_t time_INIT;
extern bool ttp_voltage_loop_ON;
extern bool ttp_current_loop_ON;
extern bool bbst_voltage_loop_ON;
/** Instance of the AC zero crossing management */
extern AC_monitoring_t AC_zero_cross;

/**-----------------------------------------------------------------------------
 *  MACRO and inline functions
 */
/*----------------------------------------------------------------------------*/
/** Configure P0.0, P0.1, P0.3 and P0.4 as GPIO. */
#define PWM_OFF()   {PORT0->IOCR0 &= ~(uint32_t)(0x7F007F7F);\
                    PORT0->IOCR4 &= ~(uint32_t)(0x7F); \
					/* Set CSGSTATG.PSLSy bit, so the output of the comparator is clamped by SW TRAP PWM_A and PWM_B */ \
					HRPWM0->CSGSETG = (HRPWM0_CSGSETG_SC1P_Msk | HRPWM0_CSGSETG_SC2P_Msk);}
/** MACRO. Configure P0.0 P0.1 P0.3 P0.4 as PWM output */
#define PWM_ON()    {PORT0->IOCR0 |= 0x98009898; /* ALT3 Push-pull. */ \
                    PORT0->IOCR4 |= 0x98;} /* ALT3 Push-pull. */
/** If PWM is connected to the timers will be true. */
#define PWM_IS_ON()	((PORT0->IOCR0) & (0x08))
/** MACRO. Configure P0.0 P0.1 P0.3 P0.4 as PWM output */
#define PWM_A_ON()  {PORT0->IOCR0 |= 0x9800; /* ALT3 Push-pull. */ \
                    PORT0->IOCR4 |= 0x98; /* ALT3 Push-pull. */ \
                    /* Clear CSGSTATG.PSLSy bit, so the output of the comparator triggers the trap */ \
					HRPWM0->CSGCLRG = (HRPWM0_CSGCLRG_CC1P_Msk);}
/** MACRO. Configure P0.0 P0.1 P0.3 P0.4 as PWM output */
#define PWM_B_ON()  {PORT0->IOCR0 |= 0x98000098; /* ALT3 Push-pull. */ \
					/* Clear CSGSTATG.PSLSy bit, so the output of the comparator triggers the trap */ \
					HRPWM0->CSGCLRG = (HRPWM0_CSGCLRG_CC2P_Msk);}

/** MACRO. Clear P2.0 and P2.1 GPIO */
#define SR_OFF()    {PORT2-> OMR = (uint32_t)((0x10000 << 0) | (0x10000 << 1));}
/** MACRO. Set P2.1 and Clear P2.0 GPIO */
#define SR_HS_ON()  {PORT2-> OMR = (uint32_t)((0x1 << 1) | (0x10000 << 0));}
/** MACRO. Set P2.0 and Clear P2.1 GPIO */
#define SR_LS_ON()  {PORT2-> OMR = (uint32_t)((0x1 << 0) | (0x10000 << 1));}

/** MACRO. Relay on: P0.8 --> high */
#define RELAY_ON()  {PORT0->OMR = (uint32_t)0x1 << 8;}
/** MACRO. Relay off: P0.8 --> low */
#define RELAY_OFF() {PORT0-> OMR = (uint32_t)0x10000 << 8;}

/** MACRO. Static switch on: P2.3 --> high */
#define STSW_ON()   {PORT2->OMR = (uint32_t)0x1 << 3;}
/** MACRO. Static switch off: P2.3 --> low */
#define STSW_OFF()  {PORT2-> OMR = (uint32_t)0x10000 << 3;}
/** Configure P0.2 as GPIO. */
#define PWM_AUX_OFF()   {PORT0->IOCR0 &= ~(uint32_t)(0x7F0000);}
/** Configure P0.2 PWM output. */
#define PWM_AUX_ON()	{PORT0->IOCR0 |= 0xA00000;}
#define HALL_SENSOR_DLY         (20)
/** Modify duty cycle of the PFC_A-PWM and the trigger of the ADC with interleaved operation */
#define SET_DUTY_ADC_A_I(duty)	{CCU80_CC81->CR1S = duty; /* PWM duty cycle. */ \
                                CCU80_CC81->CR2S = (duty >> 1) + HALL_SENSOR_DLY; /* iL_A, Vac and iLaux ADC (Queue) trigger. */ \
                                CCU80->GCSS = CCU8_GCSS_S1SE_Msk; /* Shadow transfer allowed. */}
/** Modify duty cycle of the PFC_A-PWM and the trigger of the ADC without interleaved operation */
#define SET_DUTY_ADC_A_S(duty)	{CCU80_CC81->CR1S = duty; /* PWM duty cycle. */ \
                                CCU80_CC81->CR2S = (duty >> 1) + HALL_SENSOR_DLY; /* iL_A, Vac and iLaux ADC (Queue) trigger. */ \
								CCU80_CC82->CR2S = (duty >> 1) + HALL_SENSOR_DLY; /* iL_B, Vdc1-Vdc2 ADC (Scan) trigger. */ \
		                        CCU80->GCSS = (CCU8_GCSS_S2SE_Msk | CCU8_GCSS_S1SE_Msk); /* Shadow transfer allowed. */}
/** Modify duty cycle of the PFC_B-PWM and the trigger of the ADC with interleaved operation */
#define SET_DUTY_ADC_B_I(duty)	{CCU80_CC82->CR1S = duty; /* PWM duty cycle. */ \
                                CCU80_CC82->CR2S = (duty >> 1) + HALL_SENSOR_DLY; /* iL_B, Vdc1-Vdc2 ADC (Scan) trigger. */ \
                                CCU80->GCSS = (CCU8_GCSS_S2SE_Msk); /* Shadow transfer allowed. */}
/** Modify duty cycle of the PFC_B-PWM and the trigger of the ADC with interleaved operation */
#define SET_DUTY_ADC_I(duty, slice, mask) {slice->CR1S = duty; /* PWM duty cycle. */ \
                                    slice->CR2S = (duty >> 1) + HALL_SENSOR_DLY; /* iL_B, Vdc1-Vdc2 ADC (Scan) trigger. */ \
                                    CCU80->GCSS = (mask); /* Shadow transfer allowed. */}
/** Modify duty cycle and switching frequency of the PFC-PWM. */
#define SET_DUTY_PERIOD_A(duty, period)	{CCU80_CC81->CR1S = duty; /* PWM duty cycle. */ \
	                                    CCU80_CC81->CR2S = (duty >> 1) + HALL_SENSOR_DLY; /* ADC trigger. */ \
	                                    CCU80_CC81->PRS = period; /* PWM switching frequency. */ \
			                            CCU80->GCSS = (CCU8_GCSS_S1SE_Msk | CCU8_GCSS_S0SE_Msk); /* Shadow transfer allowed. */}
/** Modify duty cycle and switching frequency of the PFC-PWM. */
#define SET_DUTY_PERIOD_B(duty, period)	{CCU80_CC82->CR1S = duty; /* PWM duty cycle. */ \
	                                    CCU80_CC82->CR2S = (duty >> 1) + HALL_SENSOR_DLY; /* ADC trigger. */ \
	                                    CCU80_CC82->PRS = period; /* PWM switching frequency. */ \
	                                    CCU80->GCSS = CCU8_GCSS_S2SE_Msk; /* Shadow transfer allowed. */}
/** Modify duty cycle and switching frequency of the PFC-PWM. */
#define SET_DUTY_PERIOD_AUX(duty, period)	{CCU80_CC80->CR1S = duty; /* PWM duty cycle. */ \
	                                    	CCU80_CC80->CR2S = (duty >> 1); /* ADC trigger. */ \
	                                    	CCU80_CC80->PRS = period; /* PWM switching frequency. */ \
			                            	CCU80->GCSS = CCU8_GCSS_S0SE_Msk; /* Shadow transfer allowed. */}

/**-----------------------------------------------------------------------------
 * \name ADC interrupt definitions
 * @{
 */
/** Current loop of phase A. Triggered by iLA (Queue) conversion */
#define current_loop_A_ISR  VADC0_G0_0_IRQHandler
/** Current loop of phase A. Triggered by iLA (Queue) conversion */
#define current_loop_B_ISR  VADC0_G1_0_IRQHandler
/** @} */

#endif /* CONTROL_IRQ_I_LOOP_HIGH_PRIO_H_ */
