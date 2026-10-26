/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);


/* Function processing DMA Rx data. Counts how many capital and small letters are in sentence.
 * Result is supposed to be stored in global variable of type "letter_count_" that is defined in "main.h"
 *
 * @param1 - received sign
 */
void proccesDmaData(uint8_t sign);


/* One producer (RX interrupt callbacks), one consumer (main loop). */
#define FRAME_MAX_AFTER_START 35U
#define FRAME_QUEUE_SIZE      16U
#define TX_BUFFER_SIZE        160U

typedef struct
{
    char text[FRAME_MAX_AFTER_START + 1U];
    letter_count_ letters;
} received_frame_t;

static received_frame_t frameQueue[FRAME_QUEUE_SIZE];
static volatile uint8_t frameWriteIndex = 0U;
static volatile uint8_t frameReadIndex = 0U;
static volatile uint32_t droppedFrames = 0U;

/* State of streaming parser: '#', up to 34 payload chars, then '$' within
 * the NEXT 35 received characters; unmatched input is ignored.
 */
static uint8_t frameActive = 0U;
static uint8_t frameLength = 0U;
static char frameText[FRAME_MAX_AFTER_START + 1U];
static letter_count_ frameLetters = {0U, 0U};

/* SysTick interrupt increments this every 1 ms. */
volatile uint32_t g_systemMs = 0U;

/* DMA TX uses this persistent buffer, never a stack-local char array. */
static uint8_t txBuffer[TX_BUFFER_SIZE];

static uint8_t sendFrameFromQueue(void)
{
    if (frameReadIndex == frameWriteIndex)
    {
        return 0U;
    }

    const received_frame_t *event = &frameQueue[frameReadIndex];
    int n = snprintf((char *)txBuffer, sizeof(txBuffer),
                     "Valid string: %s, lower-case: %u, upper-case: %u\r\n",
                     event->text,
                     (unsigned)event->letters.small_letter,
                     (unsigned)event->letters.capital_letter);

    /* Free queue slot only after text has been copied into TX buffer. */
    frameReadIndex = (uint8_t)((frameReadIndex + 1U) % FRAME_QUEUE_SIZE);
    if ((n > 0) && (n < (int)sizeof(txBuffer)))
    {
        USART2_PutBuffer(txBuffer, (uint8_t)n);
    }
    return 1U;
}

static void sendBufferState(void)
{
    uint32_t occupied = (uint32_t)USART2_GetRxBufferOccupancy();
    /* Integer fixed-point avoids _printf_float and linker modifications. */
    uint32_t percentTenths = (occupied * 1000U +
                              DMA_USART2_BUFFER_SIZE / 2U) /
                              DMA_USART2_BUFFER_SIZE;
    int n = snprintf((char *)txBuffer, sizeof(txBuffer),
          "Buffer capacity: %u bytes, occupied memory: %lu bytes, "
          "load [in %%]: %lu.%01lu%%\r\n",
          (unsigned)DMA_USART2_BUFFER_SIZE, (unsigned long)occupied,
          (unsigned long)(percentTenths / 10U),
          (unsigned long)(percentTenths % 10U));
    if ((n > 0) && (n < (int)sizeof(txBuffer)))
    {
        USART2_PutBuffer(txBuffer, (uint8_t)n);
    }
}


int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();

  /* Callback is invoked by RX interrupts, one received byte at a time. */
  USART2_RegisterCallback(proccesDmaData);
  uint32_t lastReportMs = g_systemMs;
  uint8_t reportPending = 0U;

  while (1)
  {
	  /* Periodic transmission of information about DMA Rx buffer state.
	   * Transmission frequency - 0.5Hz.
	   * Message format - "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %]: %f%"
	   * Example message (what I wish to see in terminal) - Buffer capacity: 1000 bytes, occupied memory: 231 bytes, load [in %]: 23.1%
	   */

	  /* Valid text string information transmission.
	   * Transmission frequency - when new valid string is received.
	   * Message format - "Valid string: %s, lower-case: %d, upper-case: %d"
	   * Example message (what I wish to see in terminal) - Valid string: Platn15uborZnakov, lower-case: 13, upper-case: 2
	   */

      uint32_t now = g_systemMs;
      if ((uint32_t)(now - lastReportMs) >= 2000U)
      {
          lastReportMs = now;
          reportPending = 1U;
      }

      /* Never overwrite txBuffer while DMA channel 7 is transmitting. */
      if (LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_7) == 0U)
      {
          /* Complete messages take precedence over periodic diagnostics. */
          if (frameReadIndex != frameWriteIndex)
          {
              (void)sendFrameFromQueue();
          }
          else if (reportPending != 0U)
          {
              reportPending = 0U;
              sendBufferState();
          }
      }
  }
  /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  Error_Handler();  
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {
    
  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  
  }
  LL_Init1msTick(8000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
  LL_SYSTICK_EnableIT(); /* Allow SysTick_Handler to increment g_systemMs every 1 ms. */
  LL_SetSystemCoreClock(8000000);
}

/*
 * Implementation of function processing data received via USART.
 */
void proccesDmaData(uint8_t sign)
{
    if (frameActive == 0U)
    {
        if (sign == (uint8_t)'#')
        {
            frameActive = 1U;
            frameLength = 0U;
            frameLetters.small_letter = 0U;
            frameLetters.capital_letter = 0U;
        }
        return;
    }

    if (sign == (uint8_t)'$')
    {
        uint8_t next = (uint8_t)((frameWriteIndex + 1U) % FRAME_QUEUE_SIZE);
        frameText[frameLength] = '\0';
        if (next != frameReadIndex)
        {
            received_frame_t *event = &frameQueue[frameWriteIndex];
            memcpy(event->text, frameText, frameLength + 1U);
            event->letters = frameLetters;
            /* Publish after payload is completely written (IRQ -> main). */
            __DMB();
            frameWriteIndex = next;
        }
        else
        {
            droppedFrames++;
        }
        frameActive = 0U;
        return;
    }

    /* '$' must appear within 35 characters AFTER the start '#'.
     * Therefore 35 non-terminating characters invalidate the frame.
     */
    if (frameLength < FRAME_MAX_AFTER_START)
    {
        frameText[frameLength++] = (char)sign;
        if ((sign >= (uint8_t)'a') && (sign <= (uint8_t)'z'))
        {
            frameLetters.small_letter++;
        }
        else if ((sign >= (uint8_t)'A') && (sign <= (uint8_t)'Z'))
        {
            frameLetters.capital_letter++;
        }
    }
    if (frameLength >= FRAME_MAX_AFTER_START)
    {
        frameActive = 0U; /* Discard entire incomplete frame. */
    }
}


void Error_Handler(void)
{

}

#ifdef  USE_FULL_ASSERT

void assert_failed(char *file, uint32_t line)
{ 

}

#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
