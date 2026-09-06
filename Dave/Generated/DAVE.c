
/**
 * @cond
 ***********************************************************************************************************************
 *
 * Copyright (c) 2015-2020, Infineon Technologies AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,are permitted provided that the
 * following conditions are met:
 *
 *   Redistributions of source code must retain the above copyright notice, this list of conditions and the  following
 *   disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *   following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 *   Neither the name of the copyright holders nor the names of its contributors may be used to endorse or promote
 *   products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE  FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY,OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * To improve the quality of the software, users are encouraged to share modifications, enhancements or bug fixes
 * with Infineon Technologies AG (dave@infineon.com).
 ***********************************************************************************************************************
 *
 * Change History
 * --------------
 *
 * 2014-06-16:
 *     - Initial version<br>
 * 2015-08-28:
 *     - Added CLOCK_XMC1_Init conditionally
 * 2018-08-08:
 *     - Add creation of projectData.bak file
 * 2019-01-30:
 *     - Fix creation of projectData.bak file
 * 2019-04-29:
 *     - Make DAVE_Init() weak, the user can reimplement the function
 *
 * @endcond
 *
 */

/***********************************************************************************************************************
 * HEADER FILES
 **********************************************************************************************************************/
#include "DAVE.h"

/***********************************************************************************************************************
 * API IMPLEMENTATION
 **********************************************************************************************************************/

/*******************************************************************************
 * @brief This function initializes the APPs Init Functions.
 *
 * @param[in]  None
 *
 * @return  DAVE_STATUS_t <BR>
 ******************************************************************************/
__WEAK DAVE_STATUS_t DAVE_Init(void)
{
  DAVE_STATUS_t init_status;
  
  init_status = DAVE_STATUS_SUCCESS;
     /** @Initialization of APPs Init Functions */
     init_status = (DAVE_STATUS_t)CLOCK_XMC4_Init(&CLOCK_XMC4_0);

  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of PWM_CCU8 APP instance PWM_PFC_A */
	 init_status = (DAVE_STATUS_t)PWM_CCU8_Init(&PWM_PFC_A); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of PWM_CCU8 APP instance PWM_PFC_B */
	 init_status = (DAVE_STATUS_t)PWM_CCU8_Init(&PWM_PFC_B); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance IN_OCD_A */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&IN_OCD_A); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance IN_OCD_B */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&IN_OCD_B); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance OUT_SR_HS */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&OUT_SR_HS); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance OUT_SR_LS */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&OUT_SR_LS); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of UART APP instance UART_DCDC */
	 init_status = (DAVE_STATUS_t)UART_Init(&UART_DCDC); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance OUT_RELAY */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&OUT_RELAY); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance O_GREEN_LED */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&O_GREEN_LED); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance OUT_STSW */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&OUT_STSW); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of EVENT_GENERATOR APP instance OGU_IRQ_CNT */
	 init_status = (DAVE_STATUS_t)EVENT_GENERATOR_Init(&OGU_IRQ_CNT); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of COUNTER APP instance IRQ_CNT */
	 init_status = (DAVE_STATUS_t)COUNTER_Init(&IRQ_CNT); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of INTERRUPT APP instance IRQ_IREF */
	 init_status = (DAVE_STATUS_t)INTERRUPT_Init(&IRQ_IREF); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of EVENT_DETECTOR APP instance ERU_PWM_TRAP_B_START */
	 init_status = (DAVE_STATUS_t)EVENT_DETECTOR_Init(&ERU_PWM_TRAP_B_START); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of EVENT_GENERATOR APP instance OGU_PWM_TRAP_B_START */
	 init_status = (DAVE_STATUS_t)EVENT_GENERATOR_Init(&OGU_PWM_TRAP_B_START); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of PWM_CCU8 APP instance PWM_TRAP_A */
	 init_status = (DAVE_STATUS_t)PWM_CCU8_Init(&PWM_TRAP_A); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of PWM_CCU4 APP instance PWM_FAN_SPEED */
	 init_status = (DAVE_STATUS_t)PWM_CCU4_Init(&PWM_FAN_SPEED); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of COMP_SLOPE_GEN APP instance DAC_CMP_PWM_A */
	 init_status = (DAVE_STATUS_t)COMP_SLOPE_GEN_Init(&DAC_CMP_PWM_A); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of COMP_SLOPE_GEN APP instance DAC_CMP_PWM_B */
	 init_status = (DAVE_STATUS_t)COMP_SLOPE_GEN_Init(&DAC_CMP_PWM_B); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance IN_VAC_PLRTY */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&IN_VAC_PLRTY); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of ANALOG_IO APP instance ADC_IO_CS_A */
	 init_status = (DAVE_STATUS_t)ANALOG_IO_Init(&ADC_IO_CS_A); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of ANALOG_IO APP instance ADC_IO_CS_B */
	 init_status = (DAVE_STATUS_t)ANALOG_IO_Init(&ADC_IO_CS_B); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of COMP_SLOPE_GEN APP instance DAC_CMP_AUX */
	 init_status = (DAVE_STATUS_t)COMP_SLOPE_GEN_Init(&DAC_CMP_AUX); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of PWM_CCU4 APP instance PWM_TRAP_B */
	 init_status = (DAVE_STATUS_t)PWM_CCU4_Init(&PWM_TRAP_B); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of HRPWM APP instance HRPWM_AUX */
	 init_status = (DAVE_STATUS_t)HRPWM_Init(&HRPWM_AUX); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of ANALOG_IO APP instance ADC_IO_VDC_2 */
	 init_status = (DAVE_STATUS_t)ANALOG_IO_Init(&ADC_IO_VDC_2); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of ANALOG_IO APP instance ADC_IO_VAC */
	 init_status = (DAVE_STATUS_t)ANALOG_IO_Init(&ADC_IO_VAC); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of ANALOG_IO APP instance ADC_IO_TEMP_PFC */
	 init_status = (DAVE_STATUS_t)ANALOG_IO_Init(&ADC_IO_TEMP_PFC); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of ANALOG_IO APP instance ADC_IO_TEMP_LLC */
	 init_status = (DAVE_STATUS_t)ANALOG_IO_Init(&ADC_IO_TEMP_LLC); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of ANALOG_IO APP instance ADC_IO_CS_AUX */
	 init_status = (DAVE_STATUS_t)ANALOG_IO_Init(&ADC_IO_CS_AUX); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of ANALOG_IO APP instance ADC_IO_VDC_1 */
	 init_status = (DAVE_STATUS_t)ANALOG_IO_Init(&ADC_IO_VDC_1); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance O_PWM_AUX */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&O_PWM_AUX); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of COUNTER APP instance CNT_FAN_TACHO */
	 init_status = (DAVE_STATUS_t)COUNTER_Init(&CNT_FAN_TACHO); 
   } 
  if (init_status == DAVE_STATUS_SUCCESS)
  {
	 /**  Initialization of DIGITAL_IO APP instance IO_FAN_TACHO */
	 init_status = (DAVE_STATUS_t)DIGITAL_IO_Init(&IO_FAN_TACHO); 
   }  
  return init_status;
} /**  End of function DAVE_Init */

