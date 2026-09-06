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
 * \file	adc.c
 * \author 	David Meneses Herrera
 * \date	25.03.2024
 * \brief   ADC peripheral configuration
 */

#include <XMC4200.h>
#include <xmc_vadc.h>
#include "config_adc.h"
#include "control_irq_v_loop_outer.h"

/**-----------------------------------------------------------------------------
 * \name Global ADC configuration
 * @{
 */
/** Initialization data of VADC Global resources. */
const XMC_VADC_GLOBAL_CONFIG_t g_global_config =
{
    .disable_sleep_mode_control = true,
    .module_disable = false,
    .class0 =
    {
        .conversion_mode_standard = XMC_VADC_CONVMODE_12BIT,
        .sample_time_std_conv = 2U,
        .conversion_mode_emux = XMC_VADC_CONVMODE_12BIT,
        .sampling_phase_emux_channel = 3U,
    },
    .class1 =
    {
        .conversion_mode_standard = XMC_VADC_CONVMODE_12BIT,
        .sample_time_std_conv = 2U,
        .conversion_mode_emux = XMC_VADC_CONVMODE_12BIT,
        .sampling_phase_emux_channel = 3U
    },
    .clock_config =
    {
        .analog_clock_divider = 1, /**< Clock for the converter. fADC_I = fADC / 2 */
        .msb_conversion_clock = 0, /**< Additional clock cycle for analog converter: 1 CLK for the MSB */
        .arbiter_clock_divider = 0 /**< Request source arbiter clock divider. fADC_D = fADC  */
    },
};
/** @} */

/**-----------------------------------------------------------------------------
 * \name Group ADC configuration
 * @{
 */
/** Initialization data of VADC group 0. */
const XMC_VADC_GROUP_CONFIG_t g_group_config_0 =
{
    .class0 =
    {
        .conversion_mode_standard = XMC_VADC_CONVMODE_12BIT,
        .sample_time_std_conv = 3U /* Additional cycles at sample time. */
    },
    .class1 =
    {
       .conversion_mode_standard = XMC_VADC_CONVMODE_10BIT,
       .sample_time_std_conv = 3U /* Additional cycles at sample time. */
    }
};
/** Initialization data of VADC group 1. */
const XMC_VADC_GROUP_CONFIG_t g_group_config_1 =
{
    .class0 =
    {
        .conversion_mode_standard = XMC_VADC_CONVMODE_12BIT,
        .sample_time_std_conv = 3U /* Additional cycles at sample time. */
    },
    .class1 =
    {
       .conversion_mode_standard = XMC_VADC_CONVMODE_FASTCOMPARE,
       .sample_time_std_conv = 3U /* Additional cycles at sample time. */
    }
};
/** @} */

/**-----------------------------------------------------------------------------
 * \name Channel ADC configuration.
 * @{
 */
/** G0.CH4 and G1.CH1 (iL_A and iL_B) configuration data. */
const XMC_VADC_CHANNEL_CONFIG_t adc_ch_iLx_config =
{
    .input_class = XMC_VADC_CHANNEL_CONV_GROUP_CLASS0,
    .result_reg_number = ILX_RES,
    .channel_priority = true,
    .alias_channel = XMC_VADC_CHANNEL_ALIAS_DISABLED,
};

/** G1.CH0 (400 V) configuration data. */
const XMC_VADC_CHANNEL_CONFIG_t adc_ch_vo_config =
{
    .input_class = XMC_VADC_CHANNEL_CONV_GROUP_CLASS0,
    .result_reg_number = VDCX_RES,
    .channel_priority = true,
    .alias_channel = XMC_VADC_CHANNEL_ALIAS_DISABLED,
	.sync_conversion = SYNC_REQUIRED, // Synchronized with G0.CH0 for parallel measurement of output voltages
    .upper_boundary_select = XMC_VADC_CHANNEL_BOUNDARY_GLOBAL_BOUND1,
    .lower_boundary_select = XMC_VADC_CHANNEL_BOUNDARY_GLOBAL_BOUND0,
};

/** G0.CH3 (AC voltage) configuration data. */
const XMC_VADC_CHANNEL_CONFIG_t adc_ch_vac_config =
{
    .input_class = XMC_VADC_CHANNEL_CONV_GROUP_CLASS0,
    .result_reg_number = VAC_RES,
    .channel_priority = true,
    .alias_channel = XMC_VADC_CHANNEL_ALIAS_DISABLED,
};

/** G0.CH0 (static switch voltage) configuration data. */
const XMC_VADC_CHANNEL_CONFIG_t adc_ch_vstsw_config =
{
    .input_class = XMC_VADC_CHANNEL_CONV_GROUP_CLASS0,
    .result_reg_number = VDCX_RES,
    .channel_priority = true,
    .alias_channel = XMC_VADC_CHANNEL_ALIAS_DISABLED,
    .upper_boundary_select = XMC_VADC_CHANNEL_BOUNDARY_GLOBAL_BOUND1,
    .lower_boundary_select = XMC_VADC_CHANNEL_BOUNDARY_GLOBAL_BOUND0,
};
/** G0.CH5 (Temperature) configuration data. */
const XMC_VADC_CHANNEL_CONFIG_t adc_ch_ntc1_config =
{
    .input_class = XMC_VADC_CHANNEL_CONV_GROUP_CLASS0,
    .result_reg_number = NTC1_RES,
    .channel_priority = false,	// Background source conversion
    .alias_channel = XMC_VADC_CHANNEL_ALIAS_DISABLED,
};
/** G0.CH6 (Temperature) configuration data */
const XMC_VADC_CHANNEL_CONFIG_t adc_ch_ntc2_config =
{
    .input_class = XMC_VADC_CHANNEL_CONV_GROUP_CLASS0,
    .result_reg_number = NTC2_RES,
    .channel_priority = false,  // Background source conversion
    .alias_channel = XMC_VADC_CHANNEL_ALIAS_DISABLED,
};
/** @} */

/**-----------------------------------------------------------------------------
 * \name Result ADC configuration
 * @{
 */
/** Inductor currents result configuration data. */
const XMC_VADC_RESULT_CONFIG_t g_result_iLx_handle =
{
    .post_processing_mode = XMC_VADC_DMM_REDUCTION_MODE,
    .data_reduction_control = 0, /* No filter */
    .part_of_fifo = 0, /* No FIFO */
    .wait_for_read_mode = (bool) false, /* WFS */
    .event_gen_enable = (bool) false /* result event */
};
/** AC voltage result configuration data. */
const XMC_VADC_RESULT_CONFIG_t g_result_vac_handle =
{
    .post_processing_mode = XMC_VADC_DMM_REDUCTION_MODE,
    .data_reduction_control = 0x0, /* No Filter */
    .part_of_fifo = 0, /* No FIFO */
    .wait_for_read_mode = (bool) false, /* WFS */
    .event_gen_enable = (bool) false /* result event */
};
/** Output voltages (output and static switch) result configuration data. */
const XMC_VADC_RESULT_CONFIG_t g_result_vdcx_handle =
{
    .post_processing_mode = XMC_VADC_DMM_REDUCTION_MODE,
    .data_reduction_control = 0, /* Normal operation */
    .part_of_fifo = 0, /* No FIFO */
    .wait_for_read_mode = (bool) false, /* WFS */
    .event_gen_enable = (bool) false /* result event */
};
/** NTC temperature result configuration data. */
const XMC_VADC_RESULT_CONFIG_t g_result_ntc1_handle =
{
    .post_processing_mode = XMC_VADC_DMM_FILTERING_MODE,
    .data_reduction_control = 0xE, /* Filter selection */
    .part_of_fifo = 0, /* No FIFO */
    .wait_for_read_mode = (bool) false, /* WFS */
    .event_gen_enable = (bool) false /* result event */
};
/** NTC temperature result configuration data. */
const XMC_VADC_RESULT_CONFIG_t g_result_ntc2_handle =
{
    .post_processing_mode = XMC_VADC_DMM_REDUCTION_MODE,
    .data_reduction_control = 0x0, //0x3 /* Accumulation mode */
    .part_of_fifo = 0, /* No FIFO */
    .wait_for_read_mode = (bool) false, /* WFS */
    .event_gen_enable = (bool) false /* result event */
};
/** @} */

/**-----------------------------------------------------------------------------
 * \name Queue ADC configuration
 * @{
 */
/** Queue configuration. */
const XMC_VADC_QUEUE_CONFIG_t g_queue_config =
{
    .conv_start_mode = XMC_VADC_STARTMODE_CIR, 				/* Interrupt ongoing conversion and resume later */
    .req_src_priority = XMC_VADC_GROUP_RS_PRIORITY_3, 		/* Highest priority for the queue in the arbitration round */
    .src_specific_result_reg = INDIVIDUAL_RESULT_REGISTERS, /* Individual configured result registers used */
    .trigger_signal = XMC_VADC_REQ_TR_I,					/* CCU80.SR2 as trigger for the queue */
    .trigger_edge = XMC_VADC_TRIGGER_EDGE_RISING,
    .external_trigger = TRIGGER_ENABLE
};

/** Queue Entry 1: iL_A. */
const XMC_VADC_QUEUE_ENTRY_t g_queue_iLA_entry =
{
    .channel_num = ILA_CH,
    .refill_needed = true,		/* Refill is needed */
    .generate_interrupt = true, /* Interrupt generation is needed */
    .external_trigger = true	/* External trigger is required */
};

/** Queue Entry 2: output current. */
const XMC_VADC_QUEUE_ENTRY_t g_queue_vac_entry =
{
    .channel_num = VAC_CH,
    .refill_needed = true,			/* Refill is needed */
    .generate_interrupt = false,	/* NO Interrupt generation is needed */
    .external_trigger = false		/* External trigger is NOT required */
};
/** @} */

/**-----------------------------------------------------------------------------
 * \name Scan ADC configuration.
 * @{
 */
/* Scan configuration */
const XMC_VADC_SCAN_CONFIG_t adc_scan_g1_config =
{
    .conv_start_mode = XMC_VADC_STARTMODE_CIR, 				/* Interrupt ongoing conversion and resume later */
    .req_src_priority = XMC_VADC_GROUP_RS_PRIORITY_2,		/* Second highest priority for the scan in the arbitration round */
    .src_specific_result_reg = INDIVIDUAL_RESULT_REGISTERS,	/* Individual configured result registers used */
    .trigger_signal = XMC_VADC_REQ_TR_J,					/* CCU80.SR3 as trigger for the queue */
    .trigger_edge = XMC_VADC_TRIGGER_EDGE_RISING,
    .external_trigger = TRIGGER_ENABLE
};
/** @} */

/**-----------------------------------------------------------------------------
 * \name Background ADC configuration.
 * @{
 */
/* Scan configuration */
const XMC_VADC_BACKGROUND_CONFIG_t adc_bckgnd_config =
{
    .conv_start_mode = XMC_VADC_STARTMODE_WFS, 				/* Interrupt ongoing conversion and resume later */
    .req_src_priority = XMC_VADC_GROUP_RS_PRIORITY_1,		/* Second highest priority for the scan in the arbitration round */
    .src_specific_result_reg = INDIVIDUAL_RESULT_REGISTERS,	/* Individual configured result registers used */
    .trigger_signal = XMC_VADC_REQ_TR_J,					/* CCU80.SR3 as trigger for the queue */
    .trigger_edge = XMC_VADC_TRIGGER_EDGE_NONE,
    .external_trigger = TRIGGER_DISABLE
};
/** @} */

/**-----------------------------------------------------------------------------
 * \brief   Configuration and initialization of the die temperature sensor.
 * \return  None
 */
void die_temp_sens_init()
{
    XMC_SCU_EnableTemperatureSensor();
    XMC_SCU_CalibrateTemperatureSensor(0, 0);
}
/**-----------------------------------------------------------------------------
 * \brief   Configuration and initialization of the ADC peripheral.
 * 			Global, group, channels and results are configured.
 * 			Queue is configured and initialize.
 * 			Pointer to the ADC measurements are defined.
 * \return  None
 */
/* Function to initialize the ADC module */
void adc_init()
{
	/* Remove reset from VADC */
	XMC_SCU_CLOCK_UngatePeripheralClock(XMC_SCU_PERIPHERAL_CLOCK_VADC);
	/* Initialize the VADC global registers */
	XMC_VADC_GLOBAL_Init(VADC, &g_global_config);
	/* Configure both conversion kernels */
	XMC_VADC_GROUP_Init(VADC_G0, &g_group_config_0);
	XMC_VADC_GROUP_Init(VADC_G1, &g_group_config_1);
    /* Enable the analog converters */
    XMC_VADC_GROUP_SetPowerMode(VADC_G0, XMC_VADC_GROUP_POWERMODE_NORMAL);
    XMC_VADC_GROUP_SetPowerMode(VADC_G1, XMC_VADC_GROUP_POWERMODE_NORMAL);
    /* Perform calibration of the converter */
    XMC_VADC_GLOBAL_StartupCalibration(VADC);

	/* Configuration of the channels to be measured */
    XMC_VADC_GROUP_ChannelInit(ADC_ILA_CH, &adc_ch_iLx_config);
    XMC_VADC_GROUP_ChannelInit(ADC_ILB_CH, &adc_ch_iLx_config);
    XMC_VADC_GROUP_ChannelInit(ADC_VAC_CH, &adc_ch_vac_config);
    XMC_VADC_GROUP_ChannelInit(ADC_VDCP_CH, &adc_ch_vo_config);
    XMC_VADC_GROUP_ChannelInit(ADC_VDCBULK_CH, &adc_ch_vstsw_config);
	XMC_VADC_GROUP_ChannelInit(ADC_T1_CH, &adc_ch_ntc1_config);
	XMC_VADC_GROUP_ChannelInit(ADC_T2_CH, &adc_ch_ntc2_config);

	/* Synchronization of measurements G1CH0 (VDC1) and G0CH0 (VDC2) */
	/* Define Master group */
	XMC_VADC_GROUP_SetSyncMaster(VADC_G1);
	/* Define Slave group */
	XMC_VADC_GROUP_SetSyncSlave(VADC_G0, MASTER, SLAVE);

	/* Result registers initialization */
	XMC_VADC_GROUP_ResultInit(VADC_G0, ILX_RES, &g_result_iLx_handle);
    XMC_VADC_GROUP_ResultInit(VADC_G1, ILX_RES, &g_result_iLx_handle);
    XMC_VADC_GROUP_ResultInit(VADC_G0, VAC_RES, &g_result_vac_handle);
	XMC_VADC_GROUP_ResultInit(VADC_G1, VDCX_RES, &g_result_vdcx_handle);
	XMC_VADC_GROUP_ResultInit(VADC_G0, VDCX_RES, &g_result_vdcx_handle);
	XMC_VADC_GROUP_ResultInit(VADC_G0, NTC1_RES, &g_result_ntc1_handle);
	XMC_VADC_GROUP_ResultInit(VADC_G0, NTC2_RES, &g_result_ntc2_handle);

	/* Initialization of the queue */
	XMC_VADC_GROUP_QueueInit(VADC_G0, &g_queue_config);
	/* Fill the queue */
	XMC_VADC_GROUP_QueueInsertChannel(VADC_G0, g_queue_iLA_entry);
	XMC_VADC_GROUP_QueueInsertChannel(VADC_G0, g_queue_vac_entry);

	/* Connects the QUEUE to the CNT_IRQ divider through an OGU. */
    XMC_VADC_GROUP_EnableResultEvent(VADC_G0, ILX_RES);
    XMC_VADC_GROUP_SetResultInterruptNode(VADC_G0, ILX_RES, XMC_VADC_SR_SHARED_SR0);
	/* Connect Request Source Event to the NVIC nodes */
	XMC_VADC_GROUP_QueueSetReqSrcEventInterruptNode(VADC_G0, XMC_VADC_SR_GROUP_SR0);
	/* Set IRQ priority and Enable IRQ */
	NVIC_SetPriority(VADC0_G0_0_IRQn, VADC_SR0_PRIORITY);
	NVIC_EnableIRQ(VADC0_G0_0_IRQn);

    /* Configure output voltage measurement out of range protection. */
    XMC_VADC_GLOBAL_SetBoundaries(VADC, 0, VBULK_OVP_FAST_STOP);
    /* Connect Request Source Event to the NVIC node of the protection interrupt */
    XMC_VADC_GROUP_ChannelTriggerEventGenCriteria(ADC_VDCP_CH, XMC_VADC_CHANNEL_EVGEN_OUTBOUND);
    XMC_VADC_GROUP_ChannelSetEventInterruptNode(ADC_VDCP_CH, XMC_VADC_SR_GROUP_SR2);
    /* Out of range output voltage has the highest priority, is a latching protection. */
    NVIC_SetPriority(OVP_VDCP_IRQ_N, 0);
    /* Connect Request Source Event to the NVIC node of the protection interrupt */
    XMC_VADC_GROUP_ChannelTriggerEventGenCriteria(ADC_VDCBULK_CH, XMC_VADC_CHANNEL_EVGEN_OUTBOUND);
    XMC_VADC_GROUP_ChannelSetEventInterruptNode(ADC_VDCBULK_CH, XMC_VADC_SR_GROUP_SR2);
    /* Out of range output voltage has the highest priority, is a latching protection. */
    NVIC_SetPriority(OVP_VBULK_IRQ_N, 0);
    // OVP_VBULK_ON();

    /* Scan source for measuring */
    XMC_VADC_GROUP_ScanInit(VADC_G1, &adc_scan_g1_config);
    /* Fill Scan source*/
    XMC_VADC_GROUP_ScanAddChannelToSequence(VADC_G1, ILB_CH);
    XMC_VADC_GROUP_ScanAddChannelToSequence(VADC_G1, VDC_CH);
	/* Connect Request Source Event to the NVIC nodes */
    XMC_VADC_GROUP_EnableResultEvent(VADC_G1, ILX_RES);
    XMC_VADC_GROUP_ScanSetReqSrcEventInterruptNode(VADC_G1, XMC_VADC_SR_GROUP_SR0);
    /* Set IRQ priority and Enable IRQ */
	NVIC_SetPriority(VADC0_G1_0_IRQn, VADC_SR0_PRIORITY);
	NVIC_EnableIRQ(VADC0_G1_0_IRQn);

	/* Background for temperature sensing */
	XMC_VADC_GLOBAL_BackgroundInit(VADC, &adc_bckgnd_config);
	XMC_VADC_GLOBAL_BackgndAddMultipleChannels(VADC, GROUP0, (1 << T2_CH | 1 << T1_CH));
	XMC_VADC_GLOBAL_BackgroundEnableContinuousMode(VADC);
	XMC_VADC_GLOBAL_BackgroundTriggerConversion(VADC);
}
