#include "as608.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"

#define AS608_RX_BUFFER_SIZE 128
volatile uint8_t as608_rx_buffer[AS608_RX_BUFFER_SIZE];
volatile uint8_t as608_rx_index = 0;

void AS608_UART_Init(uint32_t baudrate) {
    // Kích ho?t xung nh?p cho GPIOB và USART3
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // TX c?a STM32 -> PB10
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // RX c?a STM32 -> PB11
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);
}

void AS608_SendByte(uint8_t byte) {
    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    USART_SendData(USART3, byte);
}

void AS608_SendData(uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        AS608_SendByte(data[i]);
    }
}

// Xóa s?ch b? d?m tru?c khi g?i d? tránh nh?n mã rác gây k?t màn hình
uint8_t AS608_SendCommand(uint8_t *cmd, uint16_t len, uint8_t expected_len) {
    as608_rx_index = 0;
    for(uint8_t i = 0; i < AS608_RX_BUFFER_SIZE; i++) as608_rx_buffer[i] = 0; 
    
    AS608_SendData(cmd, len);
    uint32_t timeout = 0x8FFFFF; // Timeout l?n d? tránh m?t k?t n?i
    while(as608_rx_index < expected_len && timeout > 0) timeout--;
    if(timeout == 0) return 0xFF; 
    return as608_rx_buffer[9];   
}

uint8_t AS608_GetImage(void) {
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
    return AS608_SendCommand(cmd, 12, 12);
}

uint8_t AS608_GenChar(uint8_t bufferID) {
    uint16_t sum = 0x01 + 0x00 + 0x04 + 0x02 + bufferID;
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, 0x02, bufferID, (sum>>8)&0xFF, sum&0xFF};
    return AS608_SendCommand(cmd, 13, 12);
}

uint8_t AS608_RegModel(void) {
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x05, 0x00, 0x09};
    return AS608_SendCommand(cmd, 12, 12);
}

uint8_t AS608_StoreChar(uint8_t bufferID, uint16_t pageID) {
    uint16_t sum = 0x01 + 0x00 + 0x06 + 0x06 + bufferID + (pageID>>8) + (pageID&0xFF);
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x06, 0x06, bufferID, (pageID>>8)&0xFF, pageID&0xFF, (sum>>8)&0xFF, sum&0xFF};
    return AS608_SendCommand(cmd, 15, 12);
}

uint8_t AS608_DeleteChar(uint16_t pageID) {
    uint16_t sum = 0x01 + 0x00 + 0x07 + 0x0C + (pageID>>8) + (pageID&0xFF) + 0x00 + 0x01;
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, 0x0C, (pageID>>8)&0xFF, pageID&0xFF, 0x00, 0x01, (sum>>8)&0xFF, sum&0xFF};
    return AS608_SendCommand(cmd, 16, 12);
}

uint8_t AS608_Empty(void) {
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x0D, 0x00, 0x11};
    return AS608_SendCommand(cmd, 12, 12);
}

uint16_t AS608_GetTemplateCount(void) {
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x1D, 0x00, 0x21};
    if (AS608_SendCommand(cmd, 12, 14) == 0x00) {
        return (as608_rx_buffer[10] << 8) | as608_rx_buffer[11];
    }
    return 0;
}

uint16_t AS608_SearchFinger(uint8_t bufferID) {
    uint16_t sum = 0x01 + 0x00 + 0x08 + 0x04 + bufferID + 0x00 + 0x00 + 0x01 + 0x2C;
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x08, 0x04, bufferID, 0x00, 0x00, 0x01, 0x2C, (sum>>8)&0xFF, sum&0xFF};
    if (AS608_SendCommand(cmd, 17, 16) == 0x00) {
        return (as608_rx_buffer[10] << 8) | as608_rx_buffer[11];
    }
    return 0xFFFF;
}

// Logic ki?m tra vân tay dã du?c làm ch?t ch? hon
uint8_t AS608_IdentifyFinger(void) {
    if (AS608_GetImage() != 0x00) return 0; // Tr? v? 0 n?u KHÔNG có ngón tay ho?c l?i
    if (AS608_GenChar(1) != 0x00) return 0; // Tr? v? 0 n?u rác
    
    // Ch? khi ch?p thành công m?i di tìm ki?m
    if (AS608_SearchFinger(1) != 0xFFFF) return 1; // Kh?p!
    
    return 2; // Có vân tay rõ nét nhung sai
}

void USART3_IRQHandler(void) {
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
        uint8_t temp = USART_ReceiveData(USART3);
        if (as608_rx_index < AS608_RX_BUFFER_SIZE) {
            as608_rx_buffer[as608_rx_index++] = temp;
        }
    }
}