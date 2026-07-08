#include "stm32f10x.h"
#include "stm32f1_rc522.h"
#include "liquidcrystal_i2c.h"
#include "as608.h"
#include <stdio.h>

#define NUMBER_CARD_ID                 5
#define MAX_SLAVE_CARD                 50
#define FLASH_ADDRESS_SECTOR1          0x8008000U
#define FLASH_ADDRESS_SECTOR2          0x8008800U
#define FLASH_ADDRESS_SECTOR3          0x8009000U
#define FLASH_ADDRESS_SECTOR4          0x800A000U
#define SECTOR_SIZE                    2048

typedef enum { DO_NOT_FIND_CARD = 0, SLAVE_CARD, MASTER_CARD } CardInfoType;
typedef enum { NUMBER_0 = 0U, NUMBER_1, NUMBER_2, NUMBER_3, NUMBER_4, NUMBER_5, NUMBER_6, NUMBER_7, NUMBER_8, NUMBER_9, MODE, CLEAR, NOT_PRESS } KeyPadType;
typedef enum { SECTOR1 = 0xFF01, SECTOR2 = 0xFF02, NO_SECTORS = 0xFFFF } SectorActiveType;
typedef enum { FALSE = 0, TRUE = 1 } Boolean;

// B? EMPTY_FINGERS kh?i danh sách tr?ng thái
typedef enum { DO_NOTHING = 0, ADD_SLAVE_CARD, REMOVE_SLAVE_CARD, ADD_FINGER, DEL_FINGER } MasterCardIDStateType;

CardInfoType CheckInfoCardID(void);
Boolean IsMasterCard(uchar* id); 
void I2C_LCD_Configuration(void);
void AddSlaveCard(void);
void RemoveSlaveCard(void);
void AddFingerprint(void);       
void RemoveFingerprint(void);       
void OpendDoor(void);
void TIM2_IRQHandler(void);
void Timer2RegisterInterupt(void);
void Flash_Unlock(void);
void Flash_Write(volatile uint32_t u32StartAddr, uint8_t* u8BufferWrite, uint32_t u32Length);
void Flash_Read(volatile uint32_t u32StartAddr, uint8_t* u8BufferRead, uint32_t u32Length);
void Flash_Erase(volatile uint32_t u32StartAddr);
void ReadCardFromFlash(void);
void PWM_Init(void) ;
void ChangePassword(KeyPadType KPCal);
void SetNewPassword(void);
void Re_SetNewPassword(void);
void ClearString(uint8_t *u8TempCount, uint16_t *u16Password);
KeyPadType ScanKeypad(void);
void GPIO_Config(void);
void HandlePassword(KeyPadType KPCal);
void UnlockDoorAction(void);

static uchar CardID[NUMBER_CARD_ID];
static uchar MasterCardID[NUMBER_CARD_ID] = {0x56, 0xCC, 0xF5, 0x05, 0x6A};
static uchar SlaveCardID[MAX_SLAVE_CARD][NUMBER_CARD_ID];
static CardInfoType CardIDInfo;
static MasterCardIDStateType CardIDforce = DO_NOTHING;
static MasterCardIDStateType CountStateMasterCard = DO_NOTHING;
static int8_t s8Time = 0;
static uint16_t u32CheckAtive = 0;
static SectorActiveType SectorActive = NO_SECTORS;
static KeyPadType aKeyPad[3][4] = { {NUMBER_1, NUMBER_4, NUMBER_7, MODE}, {NUMBER_2, NUMBER_5, NUMBER_8, NUMBER_0}, {NUMBER_3, NUMBER_6, NUMBER_9, CLEAR} };
static uint16_t aCol[3] = {GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6};

static uint16_t u16RealPassword = 0; 
static uint16_t u16EnterPassword = 0;
static KeyPadType KeyPadCal = NOT_PRESS;
static Boolean ShowMainLCD = FALSE;
static uint16_t u16WrongPassword = 0; 
static uint16_t u16OldPassword = 0; 
static uint16_t u16NewPassword = 0; 
static uint16_t u16Re_NewPassword = 0; 
static uint8_t u8CountHandlePassword = 0;
static uint8_t u8CountSetPassword = 0;
static uint8_t u8CountRe_SetNewPassword = 0;
static uint8_t u8CountChangePassword = 0;
static uint8_t fp_poll_counter = 0;
	
int main(void)
{
	GPIO_Config();
	PWM_Init();
	TIM4->CCR4 = 920; 
	I2C_LCD_Configuration();
    HD44780_Init(2);
    HD44780_Clear();
	MFRC522_Init();	
	Timer2RegisterInterupt();
	
    AS608_UART_Init(57600); 
	
	Flash_Read(FLASH_ADDRESS_SECTOR3, (uint8_t *)&u32CheckAtive, 2U);
	Flash_Read(FLASH_ADDRESS_SECTOR4, (uint8_t *)&u16RealPassword, 4U);
	if (u32CheckAtive == SECTOR2) SectorActive = SECTOR2; else SectorActive = SECTOR1;
	ReadCardFromFlash();
    if (u16RealPassword == 0xFFFF) {
        u16RealPassword = 1234;
        Flash_Write(FLASH_ADDRESS_SECTOR4, (uint8_t *)&u16RealPassword, 4U);
    }
    u16WrongPassword = 0;
	Delay_Ms(100);
	TIM4->CCR4 = 0;
	
	while(1) 
	{
		// Ð?C TH? RFID LIÊN T?C V?I T?C Ð? CAO
		CardIDInfo = CheckInfoCardID();
		
		if ((CardIDInfo == MASTER_CARD) || (CardIDforce != DO_NOTHING))
		{
            if (CardIDInfo == MASTER_CARD) {
                HD44780_Clear();
                HD44780_SetCursor(0,0);
                HD44780_PrintStr("Master Card OK!");
                Delay_Ms(1500); 
            }

            if (CardIDforce != DO_NOTHING) {
                CountStateMasterCard = CardIDforce;
            } else {
			    CountStateMasterCard++;
                // Gi?m gi?i h?n tr?ng thái xu?ng 4 (vì dã xóa bu?c th? 5)
                if (CountStateMasterCard > 4) CountStateMasterCard = DO_NOTHING;
            }
			
			ShowMainLCD = FALSE;
			u8CountHandlePassword = 0;
			u16EnterPassword = 0;
			
			if (CountStateMasterCard == ADD_SLAVE_CARD) AddSlaveCard();
            else if (CountStateMasterCard == REMOVE_SLAVE_CARD) RemoveSlaveCard();
            else if (CountStateMasterCard == ADD_FINGER) AddFingerprint();
            else if (CountStateMasterCard == DEL_FINGER) RemoveFingerprint();
            else {
				CountStateMasterCard = DO_NOTHING;
				CardIDforce = DO_NOTHING;
				HD44780_Clear();
				HD44780_SetCursor(0,0);
				HD44780_PrintStr("Welcome To Room");
				Delay_Ms(800);
			}
		}
		else if (CardIDInfo == SLAVE_CARD) OpendDoor();

        // CH? QUÉT VÂN TAY SAU 3 VÒNG L?P Ð? KHÔNG CH?N T?C Ð? RFID
        fp_poll_counter++;
        if (fp_poll_counter > 3) {
            fp_poll_counter = 0;
            uint8_t finger_status = AS608_IdentifyFinger();
            if (finger_status == 1) {           
                HD44780_SetCursor(0,1);
                HD44780_PrintStr("Finger OK!      ");
                
                Delay_Ms(1000); // NGÂM CH? 1 GIÂY TRU?C KHI M? C?A
                
                UnlockDoorAction();
            } else if (finger_status == 2) {    
                HD44780_Clear();
                HD44780_SetCursor(0,0);
                HD44780_PrintStr("Welcome To Room");
                HD44780_SetCursor(0,1);
                HD44780_PrintStr("Wrong Finger!   ");
                Delay_Ms(1500); 
                ShowMainLCD = FALSE;
                HD44780_Clear();
            }
        }

		if ((CountStateMasterCard == DO_NOTHING) && (CardIDforce == DO_NOTHING) && (ShowMainLCD == FALSE)) 
		{
			HD44780_SetCursor(0,0); HD44780_PrintStr("Welcome To Room");
            HD44780_SetCursor(0,1); HD44780_PrintStr("                ");
		}
		
		KeyPadCal = ScanKeypad();
		HandlePassword(KeyPadCal);
		ChangePassword(KeyPadCal);
		if (KeyPadCal == CLEAR) ClearString(&u8CountHandlePassword, &u16EnterPassword);
	}
}

Boolean IsMasterCard(uchar* id) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUMBER_CARD_ID; i++) { if (MasterCardID[i] == id[i]) count++; }
    return (count == NUMBER_CARD_ID) ? TRUE : FALSE;
}

void AddFingerprint(void)
{
    HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Add Fingerprint"); HD44780_SetCursor(0,1); HD44780_PrintStr("Put Finger 1..."); Delay_Ms(500);
    while (1) {
        if (MFRC522_ReadCardID(CardID) == MI_OK && IsMasterCard(CardID)) {
            CardIDforce = DEL_FINGER; HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Master Card OK!"); Delay_Ms(1500); break;
        }
        if (AS608_GetImage() == 0x00) {
            AS608_GenChar(1);
            HD44780_SetCursor(0,1); HD44780_PrintStr("Remove Finger..");
            while (AS608_GetImage() == 0x00) { Delay_Ms(100); } 
            
            HD44780_SetCursor(0,1); HD44780_PrintStr("Put Again...   ");
            while (AS608_GetImage() != 0x00) { Delay_Ms(100); } 
            
            AS608_GenChar(2);
            if (AS608_RegModel() == 0x00) {
                uint16_t next_id = AS608_GetTemplateCount(); 
                if (AS608_StoreChar(1, next_id) == 0x00) {
                    char str[16]; sprintf(str, "Success! ID:%d", next_id);
                    HD44780_SetCursor(0,1); HD44780_PrintStr(str);
                    Delay_Ms(1500); CardIDforce = DO_NOTHING; CountStateMasterCard = DO_NOTHING; break;
                }
            }
            HD44780_SetCursor(0,1); HD44780_PrintStr("Failed! Retry.."); Delay_Ms(1500);
            HD44780_SetCursor(0,1); HD44780_PrintStr("Put Finger 1...");
        }
        Delay_Ms(100);
    }
}

void RemoveFingerprint(void)
{
    HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Del Fingerprint"); HD44780_SetCursor(0,1); HD44780_PrintStr("Put Finger...  "); Delay_Ms(500);
    while (1) {
        if (MFRC522_ReadCardID(CardID) == MI_OK && IsMasterCard(CardID)) {
            // Ðã b? EMPTY_FINGERS, qu?t Master lúc này s? v? th?ng DO_NOTHING d? thoát Menu
            CardIDforce = DO_NOTHING; CountStateMasterCard = DO_NOTHING; HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Master Card OK!"); Delay_Ms(1500); break;
        }
        if (AS608_GetImage() == 0x00) {
            AS608_GenChar(1);
            uint16_t id = AS608_SearchFinger(1);
            if (id != 0xFFFF) {
                if (AS608_DeleteChar(id) == 0x00) {
                    char str[16]; sprintf(str, "Deleted ID:%d", id);
                    HD44780_SetCursor(0,1); HD44780_PrintStr(str);
                    Delay_Ms(1500); CardIDforce = DO_NOTHING; CountStateMasterCard = DO_NOTHING; break;
                }
            } else {
                HD44780_SetCursor(0,1); HD44780_PrintStr("Finger NotFound"); Delay_Ms(1500);
                HD44780_SetCursor(0,1); HD44780_PrintStr("Put Finger...  ");
            }
        }
        Delay_Ms(100);
    }
}

void AddSlaveCard(void)
{
	uint8_t u8CountMaterID = 0; uint8_t u8Count = 0; uint32_t u8Inter = 0; uint8_t u8CountSlaveCard = 0; uint8_t u8CountExistCard = 0; uint8_t ReadCardID[5] = {0};
	HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Add Slave Card "); HD44780_SetCursor(0,1); HD44780_PrintStr("Waiting ..."); Delay_Ms(300);
	while (1) {
		HD44780_SetCursor(0,0); HD44780_PrintStr("Add Slave Card "); HD44780_SetCursor(0,1); HD44780_PrintStr("Waiting ..."); u8CountMaterID = 0; u8CountExistCard = 0;
		if (MFRC522_ReadCardID(CardID) == MI_OK) {
            if (IsMasterCard(CardID) == TRUE) { CardIDforce = REMOVE_SLAVE_CARD; HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Master Card OK!"); Delay_Ms(1500); break; }
            for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++) {
                    if ((SlaveCardID[u8Count][0] == CardID[0]) && (SlaveCardID[u8Count][1] == CardID[1]) && (SlaveCardID[u8Count][2] == CardID[2]) && (SlaveCardID[u8Count][3] == CardID[3]) && (SlaveCardID[u8Count][4] == CardID[4])) {
                        HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Add Slave Card "); HD44780_SetCursor(0,1); HD44780_PrintStr("Card Was Added");
                        Delay_Ms(1000); HD44780_Clear(); u8CountExistCard++;
                        if (u8CountExistCard > 0) break;
                }
            }
            if (u8CountExistCard == 0) {
                for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++) {
                    if((SlaveCardID[u8Count][0] == 0xFF) && (SlaveCardID[u8Count][1] == 0xFF) && (SlaveCardID[u8Count][2] == 0xFF) && (SlaveCardID[u8Count][3] == 0xFF) && (SlaveCardID[u8Count][4] == 0xFF) ) {
                            for (u8Inter = 0; u8Inter < NUMBER_CARD_ID; u8Inter++) { SlaveCardID[u8Count][u8Inter] = CardID[u8Inter]; }
                            for (u8Inter = 0; u8Inter < SECTOR_SIZE; u8Inter++) {
                                if (SectorActive == SECTOR2) {
                                    Flash_Read((FLASH_ADDRESS_SECTOR2 + u8Inter*8), &ReadCardID[0], 5U);
                                    if ((ReadCardID[0] == 0xFF) && (ReadCardID[1] == 0xFF) && (ReadCardID[2] == 0xFF) && (ReadCardID[3] == 0xFF) && (ReadCardID[4] == 0xFF)) {
                                        Flash_Write((FLASH_ADDRESS_SECTOR2 + u8Inter*8), (uint8_t *)&CardID[0], 8U); Flash_Erase(FLASH_ADDRESS_SECTOR3); Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U); break;
                                    }
                                } else {
                                    Flash_Read((FLASH_ADDRESS_SECTOR1 + u8Inter*8), (uint8_t *)&ReadCardID[0], 5U);
                                    if ((ReadCardID[0] == 0xFF) && (ReadCardID[1] == 0xFF) && (ReadCardID[2] == 0xFF) && (ReadCardID[3] == 0xFF) && (ReadCardID[4] == 0xFF)) {
                                        Flash_Write((FLASH_ADDRESS_SECTOR1 + u8Inter*8), (uint8_t *)&CardID[0], 8U); Flash_Erase(FLASH_ADDRESS_SECTOR3); Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U); break;
                                    }
                                }
                            }
                            HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Add Slave Card "); HD44780_SetCursor(0,1); HD44780_PrintStr("Success!!!");
                            Delay_Ms(1000); HD44780_Clear(); break;
                    } else u8CountSlaveCard++;
                }
                if (u8CountSlaveCard == NUMBER_CARD_ID) { HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Add Slave Card "); HD44780_SetCursor(0,1); HD44780_PrintStr("Failure!!!"); Delay_Ms(1000); HD44780_Clear(); }
            }
		}
		if (CardIDforce == REMOVE_SLAVE_CARD) { Delay_Ms(1000); break; }
	}
}

void RemoveSlaveCard(void)
{
	uint8_t u8CountMaterID = 0; uint8_t u8Count = 0; uint8_t u8CountExistCard = 0; uint32_t u8Inter = 0;
	HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Del Slave Card"); HD44780_SetCursor(0,1); HD44780_PrintStr("Waiting ..."); Delay_Ms(300);
	while (1) {
		HD44780_SetCursor(0,0); HD44780_PrintStr("Del Slave Card"); HD44780_SetCursor(0,1); HD44780_PrintStr("Waiting ..."); u8CountMaterID = 0; u8CountExistCard = 0;
		if (MFRC522_ReadCardID(CardID) == MI_OK) {
            if (IsMasterCard(CardID) == TRUE) { CardIDforce = ADD_FINGER; HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Master Card OK!"); Delay_Ms(1500); break; }
            for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++) {
                    if ((SlaveCardID[u8Count][0] == CardID[0]) && (SlaveCardID[u8Count][1] == CardID[1]) && (SlaveCardID[u8Count][2] == CardID[2]) && (SlaveCardID[u8Count][3] == CardID[3]) && (SlaveCardID[u8Count][4] == CardID[4])) {
                        SlaveCardID[u8Count][0] = 0xFF; SlaveCardID[u8Count][1] = 0xFF; SlaveCardID[u8Count][2] = 0xFF; SlaveCardID[u8Count][3] = 0xFF; SlaveCardID[u8Count][4] = 0xFF;
                            if (SectorActive == SECTOR2) {
                                    SectorActive = SECTOR1; Flash_Erase(FLASH_ADDRESS_SECTOR1);
                                    for (u8Inter = 0; u8Inter < NUMBER_CARD_ID; u8Inter++) { Flash_Write((FLASH_ADDRESS_SECTOR1 + u8Inter*8), (uint8_t *)&SlaveCardID[u8Inter], 6U); }
                                    Flash_Erase(FLASH_ADDRESS_SECTOR3); Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U); Flash_Erase(FLASH_ADDRESS_SECTOR2);
                            } else {
                                SectorActive = SECTOR2; Flash_Erase(FLASH_ADDRESS_SECTOR2);
                                for (u8Inter = 0; u8Inter < NUMBER_CARD_ID; u8Inter++) { Flash_Write((FLASH_ADDRESS_SECTOR2 + u8Inter*8), (uint8_t *)&SlaveCardID[u8Inter], 6U); }
                                Flash_Erase(FLASH_ADDRESS_SECTOR3); Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U); Flash_Erase(FLASH_ADDRESS_SECTOR1);
                            }
                        HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Del Slave Card "); HD44780_SetCursor(0,1); HD44780_PrintStr("Success!!!");
                        Delay_Ms(1000); HD44780_Clear(); u8CountExistCard++;
                        if (u8CountExistCard > 0) break;
                }
            }
            if (u8CountExistCard == 0) { HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Del Slave Card "); HD44780_SetCursor(0,1); HD44780_PrintStr("Not Found"); Delay_Ms(1000); HD44780_Clear(); }
		}
		if (CardIDforce == ADD_FINGER) { Delay_Ms(1000); break; }
	}
}

void UnlockDoorAction(void) {
    char Str[16]; HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Welcome To Room"); HD44780_SetCursor(0,1); HD44780_PrintStr("Door Unlocked ");
    TIM4->CCR4 = 1650; Delay_Ms(2000); HD44780_Clear(); s8Time = 9; TIM2->CNT = 0U;
    while (s8Time > 0) {
        HD44780_SetCursor(0,0); HD44780_PrintStr("Door Will Close"); HD44780_SetCursor(0,1); HD44780_PrintStr("In ");
        sprintf(&Str[0], "%0.1d ", s8Time); HD44780_PrintStr(&Str[0]); HD44780_PrintStr("Sec HurryUp");
    }
    TIM4->CCR4 = 920; Delay_Ms(500); HD44780_Clear(); TIM4->CCR4 = 0;
    HD44780_SetCursor(0,0); HD44780_PrintStr("Welcome To Room"); ShowMainLCD = FALSE; u8CountHandlePassword = 0; u16EnterPassword = 0;
}

void ReadCardFromFlash(void) {
	uint8_t u8Count = 0;
    for (u8Count = 0; u8Count < MAX_SLAVE_CARD; u8Count++) {
		if (SectorActive == SECTOR2) Flash_Read((FLASH_ADDRESS_SECTOR2 + 8*u8Count), &SlaveCardID[u8Count][0], 5U);
		else Flash_Read((FLASH_ADDRESS_SECTOR1 + 8*u8Count), &SlaveCardID[u8Count][0], 5U);
	}
}

void OpendDoor(void) {
	uint8_t u8Count = 0; Boolean u8CountExistCard = FALSE;
    
    for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++) {
        if ((SlaveCardID[u8Count][0] == CardID[0]) && (SlaveCardID[u8Count][1] == CardID[1]) && 
            (SlaveCardID[u8Count][2] == CardID[2]) && (SlaveCardID[u8Count][3] == CardID[3]) && 
            (SlaveCardID[u8Count][4] == CardID[4])) {
            u8CountExistCard = TRUE; break;
        }
    }
    
    if (u8CountExistCard == TRUE) UnlockDoorAction();
    else {
        HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Welcome To Room"); HD44780_SetCursor(0,1); HD44780_PrintStr("Card Not Found! ");
        Delay_Ms(2000); HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Welcome To Room"); ShowMainLCD = FALSE; u8CountHandlePassword = 0; u16EnterPassword = 0;
    }
}

CardInfoType CheckInfoCardID(void) {
	if (MFRC522_ReadCardID(CardID) == MI_OK) {
        if (IsMasterCard(CardID)) return MASTER_CARD;
		return SLAVE_CARD;
	}
	return DO_NOT_FIND_CARD;
}

void I2C_LCD_Configuration(void) {
	GPIO_InitTypeDef  GPIO_InitStructure; I2C_InitTypeDef  I2C_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7; GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD; GPIO_Init(GPIOB, &GPIO_InitStructure);
    I2C_StructInit(&I2C_InitStructure);
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C; I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2; I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable; I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit; I2C_InitStructure.I2C_ClockSpeed = 100000;
    I2C_Init(I2C_Chanel, &I2C_InitStructure); I2C_Cmd(I2C_Chanel, ENABLE); 
}

void Timer2RegisterInterupt(void) {
	RCC->APB1ENR |= 0x01; TIM2->ARR = 10000 - 1; TIM2->CNT = 0U; TIM2->PSC = 7200 - 1; TIM2->DIER = 0x01;
	TIM2->SR &= ~(0x01); TIM2->CR1 = 0x01; TIM2->EGR = 0x01; NVIC->ISER[0] = 0x10000000;
}

void TIM2_IRQHandler(void) {
	if (((TIM2->SR & 0x01U) != 0U) && (((TIM2->DIER)&0x01U) == 0x01U)) { TIM2->SR &= ~(0x01U); s8Time --; }
}	

void Flash_Unlock(void) { FLASH->KEYR = 0x45670123; FLASH->KEYR = 0xCDEF89AB; }
void Flash_Erase(volatile uint32_t u32StartAddr) {
    while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);
    if ((FLASH->CR&FLASH_CR_LOCK) == FLASH_CR_LOCK ) Flash_Unlock();
    FLASH->CR |= FLASH_CR_PER; FLASH->AR = u32StartAddr; FLASH->CR |= FLASH_CR_STRT;
    while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);
    FLASH->CR &= ~(uint32_t)FLASH_CR_PER; FLASH->CR &= ~(uint32_t)FLASH_CR_STRT;
}
void Flash_Write(volatile uint32_t u32StartAddr, uint8_t* u8BufferWrite, uint32_t u32Length) {
    uint32_t u32Count = 0U; if((u8BufferWrite == 0x00U) || (u32Length == 0U) || u32Length%2U != 0U) return;
    while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);
    if ((FLASH->CR&FLASH_CR_LOCK) == FLASH_CR_LOCK ) Flash_Unlock();
    FLASH->CR |= FLASH_CR_PG;
    for(u32Count = 0U; u32Count < (u32Length/2); u32Count ++ ) {
        *(uint16_t*)(u32StartAddr + u32Count*2U) = *(uint16_t*)((uint32_t)u8BufferWrite + u32Count*2U);
        while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);
    }
    FLASH->CR &= ~(uint32_t)FLASH_CR_PG;
}
void Flash_Read(volatile uint32_t u32StartAddr, uint8_t* u8BufferRead, uint32_t u32Length) {
    if((u8BufferRead == 0x00U) || (u32Length == 0U)) return;
    do {
        if(( u32StartAddr%4U == 0U) && ((uint32_t)u8BufferRead%4U == 0U) && (u32Length >= 4U)) {
            *(uint32_t*)((uint32_t)u8BufferRead) = *(uint32_t*)(u32StartAddr); u8BufferRead += 4U; u32StartAddr += 4U; u32Length -= 4U;
        } else if(( u32StartAddr%2U == 0U) && ((uint32_t)u8BufferRead%2U == 0U) && (u32Length >= 2U)) {
            *(uint16_t*)((uint32_t)u8BufferRead) = *(uint16_t*)(u32StartAddr); u8BufferRead += 2U; u32StartAddr += 2U; u32Length -= 2U;
        } else {
            *(uint8_t*)(u8BufferRead) = *(uint8_t*)(u32StartAddr); u8BufferRead += 1U; u32StartAddr += 1U; u32Length -= 1U;
        }
    } while(u32Length > 0U);
}	

void PWM_Init(void) {
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct; TIM_OCInitTypeDef TIM_OCInitStruct; GPIO_InitTypeDef GPIO_InitStruct;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	TIM_TimeBaseInitStruct.TIM_Prescaler = 100; TIM_TimeBaseInitStruct.TIM_Period = 14399; TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1; TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStruct); TIM_Cmd(TIM4, ENABLE);
	TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1; TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable; TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High; TIM_OCInitStruct.TIM_Pulse = 0;
	TIM_OC4Init(TIM4, &TIM_OCInitStruct); TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9; GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP; GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz; GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void GPIO_Config(void) {
    GPIO_InitTypeDef  GPIO_InitStructure; RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3; GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6; GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6);
}

KeyPadType ScanKeypad(void) {
    uint8_t Count = 0; KeyPadType TempKeyPad = NOT_PRESS;
    for (Count = 0; Count < 3; Count++) {
        GPIO_ResetBits(GPIOA, aCol[Count]);
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0U) { Delay_Ms(200); if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0U) TempKeyPad = aKeyPad[Count][0]; }
        else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == 0U) { Delay_Ms(200); if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == 0U) TempKeyPad = aKeyPad[Count][1]; }
        else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 0U) { Delay_Ms(200); if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 0U) TempKeyPad = aKeyPad[Count][2]; }
        else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == 0U) { Delay_Ms(200); if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == 0U) TempKeyPad = aKeyPad[Count][3]; }
        GPIO_SetBits(GPIOA, aCol[Count]);
    }
    return TempKeyPad;
}

void HandlePassword(KeyPadType KPCal) {
    char Str[16];
    if ((KPCal != MODE) && (KPCal != CLEAR)) {
        if (ShowMainLCD == FALSE) { HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Welcome To Room"); HD44780_SetCursor(0,1); ShowMainLCD = TRUE; }
        if (KPCal != NOT_PRESS) {
            u8CountHandlePassword++;
            if ((u8CountHandlePassword == 1) || (u8CountHandlePassword == 2) || (u8CountHandlePassword == 3)) { HD44780_PrintStr("*"); u16EnterPassword = u16EnterPassword*10 + KPCal; }
            else {
                u16EnterPassword = u16EnterPassword*10 + KPCal; HD44780_SetCursor(0,1);
                if (u16EnterPassword == u16RealPassword) { HD44780_PrintStr("Correct Password"); UnlockDoorAction(); u16WrongPassword = 0; }
                else {
                    HD44780_PrintStr("Wrong Password"); Delay_Ms(2000); ShowMainLCD = FALSE; u16WrongPassword++;
                    if (u16WrongPassword > 4) {
                        HD44780_Clear(); s8Time = u16WrongPassword; TIM2->CNT = 0U;
                        while (s8Time > 0) { HD44780_SetCursor(0,0); HD44780_PrintStr("Wrong Password"); HD44780_SetCursor(0,1); HD44780_PrintStr("Try In "); sprintf(&Str[0], "%0.1d ", s8Time); HD44780_PrintStr(&Str[0]); HD44780_PrintStr("Sec"); }
                        HD44780_Clear(); ShowMainLCD = FALSE;
                    }
                }
                u8CountHandlePassword = 0; u16EnterPassword = 0;
            }
        }
    }
}

void ChangePassword(KeyPadType KPCal) {
    u8CountChangePassword = 0;
    if(KPCal == MODE) {
        u8CountHandlePassword = 0; u16EnterPassword = 0; HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Old Password"); HD44780_SetCursor(0,1); u16OldPassword = 0;
        while(1) {
            KPCal = ScanKeypad(); if (KPCal == CLEAR) ClearString(&u8CountChangePassword, &u16OldPassword);
            if ((KPCal != MODE) && (KPCal != CLEAR) && (KPCal != NOT_PRESS)) {
                u8CountChangePassword++;
                if ((u8CountChangePassword == 1) || (u8CountChangePassword == 2) || (u8CountChangePassword == 3)) { u16OldPassword = u16OldPassword*10 + KPCal; HD44780_PrintStr("*"); }
                else {
                    u16OldPassword = u16OldPassword*10 + KPCal;
                    if (u16OldPassword == u16RealPassword) { HD44780_SetCursor(0,1); HD44780_PrintStr("Successful!!"); Delay_Ms(1000); SetNewPassword(); Re_SetNewPassword(); }
                    else { HD44780_SetCursor(0,1); HD44780_PrintStr("Wrong Password"); Delay_Ms(1000); }
                    u16Re_NewPassword = 0; u16OldPassword = 0; u16NewPassword = 0; ShowMainLCD = FALSE; break;
                }
            }
        }
    }
}

void SetNewPassword(void) {
    KeyPadType KPCal; HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Enter New PW"); HD44780_SetCursor(0,1); u8CountSetPassword = 0;
    while(1) {
      KPCal = ScanKeypad(); if (KPCal == CLEAR) ClearString(&u8CountSetPassword, &u16NewPassword);
      if ((KPCal != MODE) && (KPCal != CLEAR) && (KPCal != NOT_PRESS)) {
          u8CountSetPassword++;
          if ((u8CountSetPassword == 1) || (u8CountSetPassword == 2) || (u8CountSetPassword == 3)) { u16NewPassword = u16NewPassword*10 + KPCal; HD44780_PrintStr("*"); }
          else { u16NewPassword = u16NewPassword*10 + KPCal; break; }
      }
    }
}

void Re_SetNewPassword(void) {
    KeyPadType KPCal; HD44780_Clear(); HD44780_SetCursor(0,0); HD44780_PrintStr("Re-Enter New PW"); HD44780_SetCursor(0,1); u8CountRe_SetNewPassword = 0;
    while(1) {
      KPCal = ScanKeypad(); if (KPCal == CLEAR) ClearString(&u8CountRe_SetNewPassword, &u16Re_NewPassword);
      if ((KPCal != MODE) && (KPCal != CLEAR) && (KPCal != NOT_PRESS)) {
          u8CountRe_SetNewPassword++;
          if ((u8CountRe_SetNewPassword == 1) || (u8CountRe_SetNewPassword == 2) || (u8CountRe_SetNewPassword == 3)) { u16Re_NewPassword = u16Re_NewPassword*10 + KPCal; HD44780_PrintStr("*"); }
          else {
              u16Re_NewPassword = u16Re_NewPassword*10 + KPCal;
              if (u16Re_NewPassword == u16NewPassword) {
                  HD44780_SetCursor(0,1); Flash_Erase(FLASH_ADDRESS_SECTOR4); Flash_Write(FLASH_ADDRESS_SECTOR4, (uint8_t *)&u16Re_NewPassword, 4U); u16RealPassword = u16Re_NewPassword;
                  HD44780_PrintStr("Changed Success!"); Delay_Ms(1000);
              } else { HD44780_SetCursor(0,1); HD44780_PrintStr("Wrong Password"); Delay_Ms(1000); }
              break;
          }
      }
    }
}

void ClearString(uint8_t *u8TempCount, uint16_t *u16Password) {
    HD44780_SetCursor(0,1); uint8_t Count = *u8TempCount;
    if(Count == 0) { } else if (Count == 1) { HD44780_PrintStr("      "); HD44780_SetCursor(0,1); *u8TempCount = *u8TempCount - 1; *u16Password = *u16Password/10; }
    else if (Count == 2) { HD44780_PrintStr("* "); HD44780_SetCursor(1,1); *u8TempCount = *u8TempCount - 1; *u16Password = *u16Password/10; }
    else if (Count == 3) { HD44780_PrintStr("** "); HD44780_SetCursor(2,1); *u8TempCount = *u8TempCount - 1; *u16Password = *u16Password/10; }
    else if (Count == 4) { HD44780_PrintStr("*** "); HD44780_SetCursor(3,1); *u8TempCount = *u8TempCount - 1; *u16Password = *u16Password/10; }
}