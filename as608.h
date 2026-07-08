#ifndef __AS608_H
#define __AS608_H

#include "stm32f10x.h"

// Ð?a ch? m?c d?nh c?a module vân tay AS608
#define AS608_ADDR 0xFFFFFFFF 

// Các hàm giao ti?p co b?n
void AS608_UART_Init(uint32_t baudrate);
void AS608_SendByte(uint8_t byte);
void AS608_SendData(uint8_t *data, uint16_t len);

// Các l?nh di?u khi?n vân tay
uint8_t AS608_GetImage(void);
uint8_t AS608_GenChar(uint8_t bufferID);
uint8_t AS608_RegModel(void);
uint8_t AS608_StoreChar(uint8_t bufferID, uint16_t pageID);
uint8_t AS608_DeleteChar(uint16_t pageID);
uint8_t AS608_Empty(void);
uint16_t AS608_GetTemplateCount(void);
uint16_t AS608_SearchFinger(uint8_t bufferID);

// Hàm ti?n ích chính
uint8_t AS608_IdentifyFinger(void); 

#endif