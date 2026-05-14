/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : mf_config.c
  * @brief          : MCU FUNCTION CONFIG
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 FMSH.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by FMSH under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "mf_config.h"
#include "fm33le0xx_fl.h"

/* Private function prototypes -----------------------------------------------*/



/**
  * @brief  LCD Initialization function
  * @param  void
  * @retval None
  */
void MF_LCD_Init(void)
{
    FL_GPIO_InitTypeDef gpioInitStruction = {0};
    FL_LCD_InitTypeDef lcdInitStruction;

    // GPIO Init
    gpioInitStruction.mode = FL_GPIO_MODE_ANALOG;
    gpioInitStruction.outputType = FL_GPIO_OUTPUT_PUSHPULL;
    gpioInitStruction.pull = FL_DISABLE;

    gpioInitStruction.pin = FL_GPIO_PIN_8 | FL_GPIO_PIN_9;
    FL_GPIO_Init(GPIOA, &gpioInitStruction);

    gpioInitStruction.pin = FL_GPIO_PIN_4 | FL_GPIO_PIN_5 |
                            FL_GPIO_PIN_8 | FL_GPIO_PIN_9 | FL_GPIO_PIN_10 |
                            FL_GPIO_PIN_11 ;
    FL_GPIO_Init(GPIOB, &gpioInitStruction);
    
		gpioInitStruction.pin = FL_GPIO_PIN_7 | FL_GPIO_PIN_8 ;
    FL_GPIO_Init(GPIOC, &gpioInitStruction);

    // LCD Init
    lcdInitStruction.biasCurrent = FL_LCD_BIAS_CURRENT_HIGH;
    lcdInitStruction.biasMode = FL_LCD_BIAS_MODE_3BIAS;
    lcdInitStruction.biasVoltage = FL_LCD_BIAS_VOLTAGE_LEVEL15;

    lcdInitStruction.COMxNum = FL_LCD_COM_NUM_4COM;

    lcdInitStruction.waveform = FL_LCD_WAVEFORM_TYPEA;
    lcdInitStruction.displayFreq = 64;
    lcdInitStruction.mode = FL_LCD_DRIVER_MODE_INNER_RESISTER;
    FL_LCD_Init(LCD, &lcdInitStruction);

    // COM and SEG Init
    FL_LCD_EnableCOMEN(LCD, FL_LCD_COMEN_COM0);
    FL_LCD_EnableCOMEN(LCD, FL_LCD_COMEN_COM1);
    FL_LCD_EnableCOMEN(LCD, FL_LCD_COMEN_COM2);
    FL_LCD_EnableCOMEN(LCD, FL_LCD_COMEN_COM3);


    //FL_LCD_EnableCOMEN(LCD, FL_LCD_COMEN_COM4);
    //FL_LCD_EnableCOMEN(LCD, FL_LCD_COMEN_COM5);


    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG0);
    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG1);

    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG3);
    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG4);
    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG7);
    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG8);
    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG9);
    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG10);
    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG16);
    FL_LCD_EnableSEGEN0(LCD, FL_LCD_SEGEN0_SEG17);


    FL_LCD_Enable(LCD);

}

/**
  * @brief  The application entry point.
  * @retval int
  */
void MF_Clock_Init(void)
{
    /* MCU Configuration--------------------------------------------------------*/
    FL_RCC_EnableGroup1BusClock(FL_RCC_GROUP1_BUSCLK_RTC);
    FL_RTC_WriteAdjustValue(RTC, 0);
	  //FL_RCC_LPOSC_WriteTrimValue(0);
	  FL_RCC_SetLSCLKClockSource(FL_RCC_LSCLK_CLK_SOURCE_XTLF);
	  //FL_CMU_SetLSCLKClockSource(FL_CMU_LSCLK_SOURCE_XTLF);
    FL_RCC_DisableGroup1BusClock(FL_RCC_GROUP1_BUSCLK_RTC);
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */

    /* System interrupt init*/

    /* Initialize all configured peripherals */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void MF_SystemClock_Config(void)
{

}

void MF_Config_Init(void)
{
    MF_LCD_Init();
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */

    /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN Assert_Failed */
    /* User can add his own implementation to report the file name and line number,
       tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END Assert_Failed */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT FMSH *****END OF FILE****/
