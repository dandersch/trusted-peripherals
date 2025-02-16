/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
//#define SPI1_SCK_Pin GPIO_PIN_5
//#define SPI1_SCK_GPIO_Port GPIOA
//#define SPI1_MISO_Pin GPIO_PIN_6
//#define SPI1_MISO_GPIO_Port GPIOA
//#define SPI1_MOSI_Pin GPIO_PIN_7
//#define SPI1_MOSI_GPIO_Port GPIOA
#define IIC_SCL_SOFT_Pin GPIO_PIN_6
#define IIC_SCL_SOFT_GPIO_Port GPIOC
#define IIC_SDA_SOFT_Pin GPIO_PIN_8
#define IIC_SDA_SOFT_GPIO_Port GPIOC
//#define OLED_DC_Pin GPIO_PIN_8
//#define OLED_DC_GPIO_Port GPIOA
//#define OLED_RST_Pin GPIO_PIN_9
//#define OLED_RST_GPIO_Port GPIOA
//#define OLED_CS_Pin GPIO_PIN_6
//#define OLED_CS_GPIO_Port GPIOB
#define I2C1_SCL_Pin GPIO_PIN_8
#define I2C1_SCL_GPIO_Port GPIOB
#define I2C1_SDA_Pin GPIO_PIN_9
#define I2C1_SDA_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* NOTE ours */
#define SPI1_SCK_Pin        GPIO_PIN_3
#define SPI1_SCK_GPIO_Port  GPIOB
#define SPI1_MISO_Pin       GPIO_PIN_4
#define SPI1_MISO_GPIO_Port GPIOB
#define SPI1_MOSI_Pin       GPIO_PIN_5
#define SPI1_MOSI_GPIO_Port GPIOB
#define OLED_DC_Pin         GPIO_PIN_1
#define OLED_DC_GPIO_Port   GPIOG
#define OLED_RST_Pin        GPIO_PIN_0
#define OLED_RST_GPIO_Port  GPIOG
#define OLED_CS_Pin         GPIO_PIN_4
#define OLED_CS_GPIO_Port   GPIOA

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
