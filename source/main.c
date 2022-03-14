/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pin_mux.h"
#include "clock_config.h"
#include "peripherals.h"
#include "board.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_lpuart.h"
#include "fsl_sd.h"
#include "fsl_sd_disk.h"
#include "sdmmc_config.h"
#include "fsl_common.h"
#include "fsl_adc.h"
#include "fsl_cache.h"

#include "tos_k.h"
//#include "tos_shell.h"
#include "stdio.h"

#include "shell.h"
#include "shell_port.h"
#include "log.h"

#include "ff.h"
#include "diskio.h"

#include "wav.h"


/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define EXAMPLE_LED_GPIO     BOARD_USER_LED_GPIO
#define EXAMPLE_LED_GPIO_PIN BOARD_USER_LED_GPIO_PIN

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* The PIN status */
volatile bool g_pinSet = false;

extern void application_entry(void *arg);


/*******************************************************************************
 * Code
 ******************************************************************************/

void SysTick_Handler(void) {
	if (tos_knl_is_running()) {
		tos_knl_irq_enter();
		tos_tick_handler();
		tos_knl_irq_leave();
	}
}

/* LPUART1_IRQn interrupt handler */
void INT_0_IRQHANDLER(void) {
//	extern Shell shell;
	uint8_t data;
	if (tos_knl_is_running()) {
		tos_knl_irq_enter();

		if ((kLPUART_RxDataRegFullFlag) & LPUART_GetStatusFlags(LPUART1)) {
			data = LPUART_ReadByte(LPUART1);
//			printf("0x%02x[%c]", data,data);

//			tos_shell_input_byte(data);

			shell_input_byte(data);
		}
		SDK_ISR_EXIT_BARRIER;

		tos_knl_irq_leave();
	}
}


/* ADC1_IRQn interrupt handler */
void ADC1_IRQHANDLER(void) {
  /*  Place your code here */
	if (tos_knl_is_running())
	{
		tos_knl_irq_enter();
		adc1_handler();
		tos_knl_irq_leave();
	}
  /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
     Store immediate overlapping exception return operation might vector to incorrect interrupt. */
  #if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
  #endif
}


#define APPLICATION_TASK_STK_SIZE       4096
k_task_t application_task;
uint8_t application_task_stk[APPLICATION_TASK_STK_SIZE];
void __attribute__((weak)) application_entry(void *arg)
{
    while (1) {
        printf("This is a demo task,please use your task entry!\r\n");
        tos_sleep_ms(1000);
    }
}


#define LEDAPP_TASK_STK_SIZE       256
k_task_t ledapp_task;
uint8_t ledapp_task_stk[LEDAPP_TASK_STK_SIZE];
void led_entry(void *arg)
{
	while (1)
	{
//		SDK_DelayAtLeastUs(100000, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
		tos_sleep_ms(1000);
#if (defined(FSL_FEATURE_IGPIO_HAS_DR_TOGGLE) && (FSL_FEATURE_IGPIO_HAS_DR_TOGGLE == 1))
		GPIO_PortToggle(EXAMPLE_LED_GPIO, 1u << EXAMPLE_LED_GPIO_PIN);
#else
		if (g_pinSet)
		{
			GPIO_PinWrite(EXAMPLE_LED_GPIO, EXAMPLE_LED_GPIO_PIN, 0U);
			g_pinSet = false;
		}
		else
		{
			GPIO_PinWrite(EXAMPLE_LED_GPIO, EXAMPLE_LED_GPIO_PIN, 1U);
			g_pinSet = true;
		}
#endif /* FSL_FEATURE_IGPIO_HAS_DR_TOGGLE */
	}
}



static status_t sdcardWaitCardInsert(void)
{
    BOARD_SD_Config(&g_sd, NULL, BOARD_SDMMC_SD_HOST_IRQ_PRIORITY, NULL);

    /* SD host init function */
    if (SD_HostInit(&g_sd) != kStatus_Success)
    {
    	logError("SD host init fail");
        return kStatus_Fail;
    }

    /* wait card insert */
    if (SD_PollingCardInsert(&g_sd, kSD_Inserted) == kStatus_Success)
    {
    	logInfo("Card inserted.");
        /* power off card */
        SD_SetCardPower(&g_sd, false);
        /* power on the card */
        SD_SetCardPower(&g_sd, true);
    }
    else
    {
    	logError("Card detect fail.");
        return kStatus_Fail;
    }

    return kStatus_Success;
}


void sdcardinit()
{
	FRESULT error;
	const TCHAR driverNumberBuffer[3U] = {SDDISK + '0', ':', '/'};
	if (sdcardWaitCardInsert() != kStatus_Success)
	{
		logError("Please insert SD card!");
		return ;
	}


	/* FATFS Filesystem work area initialization */
	if (f_mount(&FATFS_System_0, driverNumberBuffer, 0U))
	{
		logError("Mount volume failed.");
		return ;
	}
	logInfo("Mount sd card init successful");


#if (FF_FS_RPATH >= 2U)
	error = f_chdrive((char const *)&driverNumberBuffer[0U]);
	if (error)
	{
		logError("Change drive failed.\r\n");
		return ;
	}
#endif
}


/*!
 * @brief Main function
 */
int main(void)
{
    /* Define the init structure for the output LED pin*/
    gpio_pin_config_t led_config = {kGPIO_DigitalOutput, 0, kGPIO_NoIntmode};

    /* Board pin, clock, debug console init */
    BOARD_ConfigMPU();
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

//    L1CACHE_DisableDCache();
//    L1CACHE_CleanDCacheByRange();
//    DCACHE_CleanByRange();

//    SCB_DisableICache();
//    SCB_DisableDCache();


    /* Init output LED GPIO. */
    GPIO_PinInit(EXAMPLE_LED_GPIO, EXAMPLE_LED_GPIO_PIN, &led_config);
    /* Enable RX interrupt. */
    LPUART_EnableInterrupts(LPUART1, kLPUART_RxDataRegFullInterruptEnable);
//    LPUART_EnableInterrupts(LPUART1, kLPUART_RxActiveEdgeInterruptEnable);

//    EnableIRQ(LPUART1_IRQn);



    tos_knl_init(); // TencentOS Tiny kernel initialize


	userShellInit();

	printf("\r\nWelcome to TencentOS tiny(%s)\r\n", TOS_VERSION);

	sdcardinit();

	tos_task_create(&ledapp_task, "led_task", led_entry, NULL, TOS_CFG_TASK_PRIO_MAX-2, ledapp_task_stk, LEDAPP_TASK_STK_SIZE, 0);
    tos_task_create(&application_task, "app_task", application_entry, NULL, 4, application_task_stk, APPLICATION_TASK_STK_SIZE, 0);


//    logVerbose("logVerbose:hello world");
//    logDebug("logDebug:hello world");
//    logInfo("logInfo:hello world");
//    logWarning("logWarning:hello world");
//    logError("logError:hello world");
//    char testddata[5] = {0,1,2,3,4};
//    logHexDumpAll(testddata, 5);

//    extern int nn_main(void);
//    nn_main();

	tos_knl_start();

	//不会运行到这里
	PRINTF("\r\nThe kernel will not run here!!!\r\n");
}
