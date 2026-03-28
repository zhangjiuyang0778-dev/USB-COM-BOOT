/*!
    \file  simple_download.c
    \brief Simple file download via USB CDC
    
    Protocol:
    1. PC sends 'S' to start
    2. Device sends 'R' ready
    3. PC sends file size (4 bytes, little endian)
    4. PC sends data in 256-byte packets
    5. Device writes to flash and sends 'A' (ACK) after each packet
    6. After all data, device sends 'D' (Done)
*/

#include "gd32f10x.h"
#include "systick.h"
#include "cdc_acm_core.h"
#include "usbd_int.h"

extern usbd_core_handle_struct usb_device_dev;
extern uint8_t packet_sent, packet_receive;
extern uint32_t receive_length;
extern uint8_t usb_data_buffer[CDC_ACM_DATA_PACKET_SIZE];

#define PACKET_SIZE     256
#define FLASH_START     0x08004000

/* Simple receive byte with timeout */
static int ReceiveByte(uint8_t *c, uint32_t timeout)
{
    while(timeout--) {
        if(packet_receive && (USBD_CONFIGURED == usb_device_dev.status)) {
            packet_receive = 0;
            if(receive_length > 0) {
                *c = usb_data_buffer[0];
                receive_length = 0;
                cdc_acm_data_receive(&usb_device_dev);
                return 0;
            }
            cdc_acm_data_receive(&usb_device_dev);
        }
    }
    return -1;
}

/* Send byte */
static void SendByte(uint8_t c)
{
    usb_data_buffer[0] = c;
    packet_sent = 0;
    cdc_acm_data_send(&usb_device_dev, 1);
    uint32_t timeout = 100000;
    while((packet_sent == 0) && (timeout-- > 0));
}

/* Simple file download */
void SimpleDownload(void)
{
    uint8_t c;
    uint32_t file_size = 0;
    uint32_t bytes_received = 0;
    uint32_t flash_addr = FLASH_START;
    uint32_t i;
    uint8_t packet[PACKET_SIZE];
    
    /* Wait for 'S' (Start) */
    Serial_PutString((uint8_t*)"\r\nWaiting for 'S' to start...\r\n");
    while(1) {
        if(ReceiveByte(&c, 10000000) == 0) {
            if(c == 'S') break;
        }
    }
    
    /* Send 'R' (Ready) */
    SendByte('R');
    Serial_PutString((uint8_t*)"Start signal received. Ready to receive.\r\n");
    
    /* Receive file size (4 bytes) */
    Serial_PutString((uint8_t*)"Receiving file size...\r\n");
    for(i = 0; i < 4; i++) {
        while(ReceiveByte(&c, 10000000) != 0);
        file_size |= (c << (i * 8));
    }
    
    Serial_PutString((uint8_t*)"File size: ");
    /* Print file size here */
    Serial_PutString((uint8_t*)" bytes\r\n");
    
    if(file_size > 48 * 1024) {
        Serial_PutString((uint8_t*)"File too large! Max 48KB\r\n");
        return;
    }
    
    /* Erase flash */
    Serial_PutString((uint8_t*)"Erasing flash...\r\n");
    FLASH_If_Erase(FLASH_START);
    
    /* Receive data packets */
    Serial_PutString((uint8_t*)"Receiving data...\r\n");
    while(bytes_received < file_size) {
        uint32_t packet_size = (file_size - bytes_received) > PACKET_SIZE ? PACKET_SIZE : (file_size - bytes_received);
        
        /* Receive packet */
        for(i = 0; i < packet_size; i++) {
            while(ReceiveByte(&packet[i], 10000000) != 0);
        }
        
        /* Write to flash */
        for(i = 0; i < packet_size; i += 4) {
            uint32_t data = packet[i] | (packet[i+1] << 8) | (packet[i+2] << 16) | (packet[i+3] << 24);
            fmc_word_program(flash_addr, data);
            flash_addr += 4;
        }
        
        bytes_received += packet_size;
        
        /* Send ACK */
        SendByte('A');
        
        /* Progress */
        if((bytes_received % 1024) == 0) {
            Serial_PutString((uint8_t*)".");
        }
    }
    
    Serial_PutString((uint8_t*)"\r\nDownload complete!\r\n");
    SendByte('D');
}

/* Flash erase function */
void FLASH_If_Erase(uint32_t StartSector)
{
    uint32_t flashaddress = StartSector;
    
    fmc_unlock();
    while(flashaddress < 0x08010000) {
        fmc_page_erase(flashaddress);
        flashaddress += 0x400;
    }
    fmc_lock();
}

/* External function from ymodem.c */
extern void Serial_PutString(uint8_t *s);
