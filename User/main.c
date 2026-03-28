/***************************************************************************//**
  文件: main.c
  作者: Zhengyu https://gzwelink.taobao.com
  版本: V1.0.0
  时间: 20210401
	平台:MINI-GD32F103RCT6
  功能: USB虚拟串口Bootloader，支持Ymodem协议升级

  使用说明:
  1. 短接PA0到GND，上电进入Bootloader模式
  2. USB连接电脑，识别为虚拟串口
  3. 使用串口工具连接，按1开始下载
  4. 使用Ymodem协议发送.bin文件
  5. 下载完成后自动跳转到APP

*******************************************************************************/
#include "gd32f10x.h"
#include "gd32f10x_libopt.h"
#include "gd32f10x_fmc.h"
#include "systick.h"
#include "cdc_acm_core.h"
#include "usbd_int.h"
#include "ymodem.h"

/* USB数据缓冲区 */
extern uint8_t packet_sent, packet_receive;
extern uint32_t receive_length;
extern uint8_t usb_data_buffer[CDC_ACM_DATA_PACKET_SIZE];

/* USB设备句柄 */
usbd_core_handle_struct usb_device_dev = 
{
    .dev_desc = (uint8_t *)&device_descriptor,
    .config_desc = (uint8_t *)&configuration_descriptor,
    .strings = usbd_strings,
    .class_init = cdc_acm_init,
    .class_deinit = cdc_acm_deinit,
    .class_req_handler = cdc_acm_req_handler,
    .class_data_handler = cdc_acm_data_handler
};

/* 应用程序地址 - 在ymodem.h中已定义 */

/* Bootloader入口检测 */
#define BOOTLOADER_ENTRY_PIN    GPIO_PIN_0
#define BOOTLOADER_ENTRY_PORT   GPIOA

/* 跳转函数类型 */
typedef void (*pFunction)(void);

/* 全局变量 */
pFunction Jump_To_Application;
uint32_t JumpAddress;

/* 函数声明 */
void nvic_config(void);
uint8_t CheckBootloaderEntry(void);
uint8_t IsApplicationValid(void);
void JumpToApplication(void);
void USB_CDC_Init(void);
void USB_CDC_Process(void);

/* Flash操作函数 */
void FLASH_If_Init(void);
uint32_t FLASH_If_Erase(uint32_t StartSector);
uint32_t FLASH_If_Write(__IO uint32_t* FlashAddress, uint32_t* Data, uint16_t DataLength);
uint32_t FLASH_If_GetWriteProtectionStatus(void);

/* NVIC配置 */
void nvic_config(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE1_SUB3);
    nvic_irq_enable(USBD_LP_CAN0_RX0_IRQn, 1, 0);
}

/* 检查是否需要进入Bootloader模式 */
uint8_t CheckBootloaderEntry(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    /* 使用内部上拉，不短接时PA0默认为高电平 */
    gpio_init(BOOTLOADER_ENTRY_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, BOOTLOADER_ENTRY_PIN);
    delay_1ms(10);
    
    /* PA0为低电平则进入Bootloader（短接PA0-GND） */
    if(gpio_input_bit_get(BOOTLOADER_ENTRY_PORT, BOOTLOADER_ENTRY_PIN) == RESET) {
        return 1;
    }
    return 0;
}

/* 检查应用程序是否有效 */
uint8_t IsApplicationValid(void)
{
    /* 检查栈指针是否在RAM范围内 */
    if(((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000) != 0x20000000) {
        return 0;
    }
    return 1;
}

/* 跳转到应用程序 */
void JumpToApplication(void)
{
    /* 获取复位向量地址 */
    JumpAddress = *(__IO uint32_t*)(APPLICATION_ADDRESS + 4);
    Jump_To_Application = (pFunction)JumpAddress;
    
    /* 初始化应用程序栈指针 */
    __set_MSP(*(__IO uint32_t*)APPLICATION_ADDRESS);
    
    /* 跳转 */
    Jump_To_Application();
}

/* USB CDC初始化 */
void USB_CDC_Init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_AF);
    
    rcu_usb_clock_config(RCU_CKUSB_CKPLL_DIV1_5);
    rcu_periph_clock_enable(RCU_USBD);
    
    usbd_core_init(&usb_device_dev);
    nvic_config();
    
    usb_device_dev.status = USBD_CONNECTED;
}

/* 启动USB接收 */
void USB_CDC_StartReceive(void)
{
    /* 等待USB配置完成 */
    uint32_t timeout = 100000;
    while((USBD_CONFIGURED != usb_device_dev.status) && (timeout-- > 0));
    
    if(USBD_CONFIGURED == usb_device_dev.status) {
        /* 启动第一次接收 */
        cdc_acm_data_receive(&usb_device_dev);
    }
}

/* USB数据处理 */
void USB_CDC_Process(void)
{
    if(USBD_CONFIGURED == usb_device_dev.status) {
        if(1 == packet_receive && 1 == packet_sent) {
            packet_sent = 0;
            cdc_acm_data_receive(&usb_device_dev);
        } else {
            if(0 != receive_length) {
                cdc_acm_data_send(&usb_device_dev, receive_length);
                receive_length = 0;
            }
        }
    }
}

/* Flash操作函数实现 */
void FLASH_If_Init(void)
{
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_BANK0_END);
    fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
    fmc_flag_clear(FMC_FLAG_BANK0_PGERR);
}

uint32_t FLASH_If_Erase(uint32_t StartSector)
{
    uint32_t flashaddress = StartSector;
    
    while(flashaddress <= 0x0800F800) {
        if(fmc_page_erase(flashaddress) != FMC_READY) {
            return 1;
        }
        flashaddress += 0x400;
    }
    return 0;
}

uint32_t FLASH_If_Write(__IO uint32_t* FlashAddress, uint32_t* Data, uint16_t DataLength)
{
    uint32_t i;
    
    for(i = 0; i < DataLength; i++) {
        if(fmc_word_program(*FlashAddress, *(Data + i)) != FMC_READY) {
            return 1;
        }
        (*FlashAddress) += 4;
    }
    return 0;
}

uint32_t FLASH_If_GetWriteProtectionStatus(void)
{
    return 0;
}

/* 串口输出函数声明 - 在ymodem.c中定义 */
extern void SerialPutChar(uint8_t c);
extern void Serial_PutString(uint8_t *s);
extern uint32_t SerialKeyPressed(uint8_t *key);
extern uint8_t GetKey(void);
extern void Main_Menu(void);
extern void SerialDownload(void);

/* 主菜单 - 在ymodem.c中定义，这里声明外部引用 */
/* ymodem.c中的Main_Menu会调用这里的跳转函数 */

int main(void)
{
    /* 初始化系统时钟为72MHz */
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV1);
    systick_config();
    
    /* 检查是否需要进入Bootloader - PA0为低电平进入 */
    if(!CheckBootloaderEntry()) {
        /* 不进入Bootloader，尝试跳转到应用程序 */
        if(IsApplicationValid()) {
            JumpToApplication();
        }
    }
    
    /* 进入Bootloader模式 */
    
    /* 初始化USB CDC */
    USB_CDC_Init();
    
    /* 等待USB连接建立 */
    {
        uint32_t timeout = 500000;
        while((USBD_CONFIGURED != usb_device_dev.status) && (timeout-- > 0));
    }
    
    /* 启动USB接收 - 必须在Flash初始化前完成 */
    USB_CDC_StartReceive();
    
    /* 初始化Flash */
    FLASH_If_Init();
    
    /* 延时等待USB虚拟串口被PC识别 */
    delay_1ms(500);
    
    /* 显示菜单 */
    Main_Menu();
    
    /* Main_Menu不会返回 */
    while(1);
}
