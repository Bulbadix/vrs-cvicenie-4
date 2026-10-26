/**
  ******************************************************************************
  * File Name          : USART.c
  * Description        : This file provides code for the configuration
  *                      of the USART instances.
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

/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* Memory buffer used directly by DMA for USART Rx*/
uint8_t bufferUSART2dma[DMA_USART2_BUFFER_SIZE];

/* Declaration and initialization of callback function */
static void (* USART2_ProcessData)(uint8_t data) = 0;

/* Register callback */
void USART2_RegisterCallback(void (*callback)(uint8_t))
{
	if(callback != 0)
	{
		USART2_ProcessData = callback;
	}
}

/* Offset of already forwarded bytes inside the current NORMAL DMA transfer. */
static uint16_t dmaRxReadPos = 0;

/* Half-buffer rearm means at least 64 bytes of free space are reserved.
 * Each incoming PC burst is at most 20 characters (assignment assumption).
 */
#define DMA_RX_REARM_LEVEL (DMA_USART2_BUFFER_SIZE / 2U)



/* USART2 init function */
void MX_USART2_UART_Init(void)
{
  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
  
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  /**USART2 GPIO Configuration  
  PA2   ------> USART2_TX
  PA15   ------> USART2_RX 
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_2|LL_GPIO_PIN_15;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*
   * USART2 DMA configuration. Write configuration for DMA used by USART2 for data Rx/Tx with INTERRUPTS.
   * Rx memory buffer will be handled in normal mode, not circular!
   * You can use configuration from example program and modify it.
   * For more information about DMA registers, refer to reference manual.
   */
  
  /* USART2_RX Init */

  LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_6,
      LL_DMA_DIRECTION_PERIPH_TO_MEMORY |
      LL_DMA_MODE_NORMAL |
      LL_DMA_PERIPH_NOINCREMENT |
      LL_DMA_MEMORY_INCREMENT |
      LL_DMA_PDATAALIGN_BYTE |
      LL_DMA_MDATAALIGN_BYTE |
      LL_DMA_PRIORITY_MEDIUM);
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_6,
      LL_USART_DMA_GetRegAddr(USART2, LL_USART_DMA_REG_DATA_RECEIVE));
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_6,
      (uint32_t)bufferUSART2dma);
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_6, DMA_USART2_BUFFER_SIZE);
  LL_DMA_ClearFlag_GI6(DMA1);
  LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_6);
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_6);
  LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_6);


  /* USART2_TX Init: DMA1 Channel 7, RAM -> USART2_TDR. */
  LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_7,
      LL_DMA_DIRECTION_MEMORY_TO_PERIPH |
      LL_DMA_MODE_NORMAL |
      LL_DMA_PERIPH_NOINCREMENT |
      LL_DMA_MEMORY_INCREMENT |
      LL_DMA_PDATAALIGN_BYTE |
      LL_DMA_MDATAALIGN_BYTE |
      LL_DMA_PRIORITY_MEDIUM);

  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_7,
      LL_USART_DMA_GetRegAddr(USART2, LL_USART_DMA_REG_DATA_TRANSMIT));

  LL_DMA_ClearFlag_GI7(DMA1);
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_7);
  LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_7);


  /* USART2 interrupt Init */
  NVIC_SetPriority(USART2_IRQn, 0);
  NVIC_EnableIRQ(USART2_IRQn);

  USART_InitStruct.BaudRate = 115200;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART2, &USART_InitStruct);
  LL_USART_ConfigAsyncMode(USART2);
  LL_USART_DisableIT_CTS(USART2);

  /* Enable USART2 peripheral and interrupts*/

  LL_USART_EnableIT_IDLE(USART2);
  LL_USART_EnableDMAReq_RX(USART2);
  LL_USART_EnableDMAReq_TX(USART2);
  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_6);
  LL_USART_Enable(USART2);
}


// Send data stored in buffer with DMA
void USART2_PutBuffer(uint8_t *buffer, uint8_t length)
{
    /* Caller must keep buffer alive until DMA IRQ marks channel disabled. */
    if ((buffer == 0) || (length == 0U) ||
        (LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_7) != 0U))
    {
        return;
    }
    LL_DMA_ClearFlag_GI7(DMA1);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_7, (uint32_t)buffer);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_7, length);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_7);
}


/*
 *	Function processing data received via USART2 with DMA and stored in bufferUSART2dma.
 *	Forwards data to callback function.
 *	Keeps track of pointer pointing to Rx memory buffer and resets the pointer if overflow is possible in next Rx.
 *	Refer to reference manual - "normal memory mode" and "increment memory mode".
 */
void USART2_CheckDmaReception(void)
{
    /* Called by RX-DMA HT/TC and USART IDLE interrupts at equal priority.
     * Normal mode: CNDTR only decreases; no circular wrap is possible.
     */
    uint16_t remaining = (uint16_t)LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
    uint16_t received = DMA_USART2_BUFFER_SIZE - remaining;

    /* Forward each new byte exactly once. */
    while (dmaRxReadPos < received)
    {
        uint8_t byte = bufferUSART2dma[dmaRxReadPos++];
        if (USART2_ProcessData != 0)
        {
            USART2_ProcessData(byte);
        }
    }

    /* Proactively restart at half buffer (64/128), not at the last byte.
     * If transfer already stopped (TC/error), restart regardless of offset.
     */
    if ((received >= DMA_RX_REARM_LEVEL) ||
        (LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_6) == 0U))
    {
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_6);

        /* Capture bytes written between the first CNDTR read and disable. */
        remaining = (uint16_t)LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
        received = DMA_USART2_BUFFER_SIZE - remaining;
        while (dmaRxReadPos < received)
        {
            uint8_t byte = bufferUSART2dma[dmaRxReadPos++];
            if (USART2_ProcessData != 0)
            {
                USART2_ProcessData(byte);
            }
        }

        LL_DMA_ClearFlag_GI6(DMA1);
        LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_6,
                                (uint32_t)bufferUSART2dma);
        LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_6, DMA_USART2_BUFFER_SIZE);
        dmaRxReadPos = 0;
        LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_6);
    }
}

/* Snapshot of physically occupied DMA bytes since its most recent rearm.
 * This is NOT the number of unprocessed bytes; bytes may already be parsed.
 */
uint16_t USART2_GetRxBufferOccupancy(void)
{
    uint16_t remaining = (uint16_t)LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
    if (remaining > DMA_USART2_BUFFER_SIZE)
    {
        return 0U;
    }
    return DMA_USART2_BUFFER_SIZE - remaining;
}




/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
