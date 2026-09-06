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
 * \file    control_irq_i_loop_low_prio.c
 * \author  Meneses Herrera, David; Escudero, Manuel
 * \date	11.05.2024
 * \brief   Control related interrupts code
 */

#include <DAVE.h>
#include "control_irq_i_loop_low_prio.h"
#include "config_adc.h"
#include "init_bckgnd_funct.h"

/**-----------------------------------------------------------------------------
 * \brief   Current reference interrupt.
 *          pwm_count counts number of PWM cycles.
 *          PR event of pwm_count triggers the current_fed_ISR() at a defined number of switching cycles.
 *          Prio 2.
 *          Note that this IRQ is not getting executed if the lines in main are commented out !!!
 * \return  None
 */
__attribute__((section(".ram_code")))
void current_ref_ISR(void)
{
	/* Current offset (zero level) acquisition before starting the PFC (ST_STB_BOP) */
	iL_offset_calc();
	/* Current reference calculation for next cycle */
	iL_ref();
	/* Dead time variation according to current level */
	iL_dead_time(current_ctrl_A.iL, &PFC.dt_falling, &PFC.dt_rising);
} // 1.162 us circa 93 instructions
