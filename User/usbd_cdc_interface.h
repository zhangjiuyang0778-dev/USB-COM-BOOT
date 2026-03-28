/*!
    \file  usbd_cdc_interface.h
    \brief USB CDC ACM interface for Ymodem

    \version 2024-03-28, V1.0.0
*/

#ifndef USBD_CDC_INTERFACE_H
#define USBD_CDC_INTERFACE_H

#include "gd32f10x.h"

/* USB CDC ACM device handle */
extern usbd_core_handle_struct usb_device_dev;

/* USB数据接收缓冲区 */
extern uint8_t usb_data_buffer[];
extern uint8_t packet_sent;
extern uint8_t packet_receive;
extern uint32_t receive_length;

/* 函数声明 */
void USB_Receive_Data(uint8_t *data, uint16_t len);
uint32_t SerialKeyPressed(uint8_t *key);
void SerialPutChar(uint8_t c);
void Serial_PutString(uint8_t *s);

#endif /* USBD_CDC_INTERFACE_H */
