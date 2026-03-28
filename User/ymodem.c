/**
  ******************************************************************************
  * @file    STM32F0xx_IAP/src/ymodem.c 
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    29-May-2012
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/

#include "ymodem.h"
#include "string.h"
#include "cdc_acm_core.h"
#include "usbd_int.h"
#include "usbd_cdc_interface.h"
#include "systick.h"

/* 外部函数声明 */
void Main_Menu(void);
uint32_t FLASH_If_Erase(uint32_t StartSector);
uint32_t FLASH_If_Write(__IO uint32_t* FlashAddress, uint32_t* Data, uint16_t DataLength);
uint32_t FLASH_If_GetWriteProtectionStatus(void);

//#include "flash_if.h"
typedef  void (*pFunction)(void);
extern pFunction Jump_To_Application;
extern  uint32_t JumpAddress;
uint8_t GetKey(void);
uint32_t FlashProtection = 0;
void SerialPutChar(uint8_t c);
void Serial_PutString(uint8_t *s);
uint8_t tab_1024[1024] =
  {
    0
  };
#define SerialPutString(x)   Serial_PutString((uint8_t*)(x))

/* USB CDC ACM设备句柄 */
extern usbd_core_handle_struct usb_device_dev;

/* USB接收环形缓冲区 */
#define USB_RX_BUF_SIZE 2048
static uint8_t usb_rx_buffer[USB_RX_BUF_SIZE];
static volatile uint16_t usb_rx_head = 0;
static volatile uint16_t usb_rx_tail = 0;

/* USB数据接收标志 */
extern uint8_t packet_sent;
extern uint8_t packet_receive;
extern uint32_t receive_length;
extern uint8_t usb_data_buffer[CDC_ACM_DATA_PACKET_SIZE];
/**
  * @brief  Convert an Integer to a string
  * @param  str: The string
  * @param  intnum: The intger to be converted
  * @retval None
  */
void Int2Str(uint8_t* str, int32_t intnum)
{
  uint32_t i, Div = 1000000000, j = 0, Status = 0;

  for (i = 0; i < 10; i++)
  {
    str[j++] = (intnum / Div) + 48;

    intnum = intnum % Div;
    Div /= 10;
    if ((str[j-1] == '0') & (Status == 0))
    {
      j = 0;
    }
    else
    {
      Status++;
    }
  }
}

/**
  * @brief  Convert a string to an integer
  * @param  inputstr: The string to be converted
  * @param  intnum: The intger value
  * @retval 1: Correct
  *         0: Error
  */
uint32_t Str2Int(uint8_t *inputstr, int32_t *intnum)
{
  uint32_t i = 0, res = 0;
  uint32_t val = 0;

  if (inputstr[0] == '0' && (inputstr[1] == 'x' || inputstr[1] == 'X'))
  {
    if (inputstr[2] == '\0')
    {
      return 0;
    }
    for (i = 2; i < 11; i++)
    {
      if (inputstr[i] == '\0')
      {
        *intnum = val;
        /* return 1; */
        res = 1;
        break;
      }
      if (ISVALIDHEX(inputstr[i]))
      {
        val = (val << 4) + CONVERTHEX(inputstr[i]);
      }
      else
      {
        /* return 0, Invalid input */
        res = 0;
        break;
      }
    }
    /* over 8 digit hex --invalid */
    if (i >= 11)
    {
      res = 0;
    }
  }
  else /* max 10-digit decimal input */
  {
    for (i = 0;i < 11;i++)
    {
      if (inputstr[i] == '\0')
      {
        *intnum = val;
        /* return 1 */
        res = 1;
        break;
      }
      else if ((inputstr[i] == 'k' || inputstr[i] == 'K') && (i > 0))
      {
        val = val << 10;
        *intnum = val;
        res = 1;
        break;
      }
      else if ((inputstr[i] == 'm' || inputstr[i] == 'M') && (i > 0))
      {
        val = val << 20;
        *intnum = val;
        res = 1;
        break;
      }
      else if (ISVALIDDEC(inputstr[i]))
      {
        val = val * 10 + CONVERTDEC(inputstr[i]);
      }
      else
      {
        /* return 0, Invalid input */
        res = 0;
        break;
      }
    }
    /* Over 10 digit decimal --invalid */
    if (i >= 11)
    {
      res = 0;
    }
  }

  return res;
}
/**
  * @brief  Get Input string from the HyperTerminal
  * @param  buffP: The input string
  * @retval None
  */
void GetInputString (uint8_t * buffP)
{
  uint32_t bytes_read = 0;
  uint8_t c = 0;
  do
  {
    c = GetKey();
    if (c == '\r')
      break;
    if (c == '\b') /* Backspace */
    {
      if (bytes_read > 0)
      {
        SerialPutString("\b \b");
        bytes_read --;
      }
      continue;
    }
    if (bytes_read >= CMD_STRING_SIZE )
    {
      SerialPutString("Command string size overflow\r\n");
      bytes_read = 0;
      continue;
    }
    if (c >= 0x20 && c <= 0x7E)
    {
      buffP[bytes_read++] = c;
      SerialPutChar(c);
    }
  }
  while (1);
  SerialPutString(("\n\r"));
  buffP[bytes_read] = '\0';
}
/**
  * @brief  Get an integer from the HyperTerminal
  * @param  num: The inetger
  * @retval 1: Correct
  *         0: Error
  */
uint32_t GetIntegerInput(int32_t * num)
{
  uint8_t inputstr[16];

  while (1)
  {
    GetInputString(inputstr);
    if (inputstr[0] == '\0') continue;
    if ((inputstr[0] == 'a' || inputstr[0] == 'A') && inputstr[1] == '\0')
    {
      SerialPutString("User Cancelled \r\n");
      return 0;
    }

    if (Str2Int(inputstr, num) == 0)
    {
      SerialPutString("Error, Input again: \r\n");
    }
    else
    {
      return 1;
    }
  }
}

/**
  * @brief  处理USB接收到的数据，存入环形缓冲区
  * @param  data: 接收到的数据
  * @param  len: 数据长度
  * @retval None
  */
void USB_Receive_Data(uint8_t *data, uint16_t len)
{
  uint16_t i;
  for (i = 0; i < len; i++) {
    uint16_t next_head = (usb_rx_head + 1) % USB_RX_BUF_SIZE;
    if (next_head != usb_rx_tail) {
      usb_rx_buffer[usb_rx_head] = data[i];
      usb_rx_head = next_head;
    }
  }
}

/**
  * @brief  从环形缓冲区读取一个字节
  * @param  key: 读取到的字节
  * @retval 1: 有数据
  *         0: 无数据
  */
static uint32_t RingBuffer_Read(uint8_t *key)
{
  if (usb_rx_head != usb_rx_tail) {
    *key = usb_rx_buffer[usb_rx_tail];
    usb_rx_tail = (usb_rx_tail + 1) % USB_RX_BUF_SIZE;
    return 1;
  }
  return 0;
}

/**
  * @brief  检查USB接收缓冲区是否有数据
  * @param  key: 读取到的字节
  * @retval 1: 有数据
  *         0: 无数据
  */
uint32_t SerialKeyPressed(uint8_t *key)
{
  /* 首先检查环形缓冲区是否有数据 */
  if (RingBuffer_Read(key)) {
    return 1;
  }
  
  /* 环形缓冲区为空，检查USB是否有新数据包到达 */
  if (packet_receive && (USBD_CONFIGURED == usb_device_dev.status)) {
    packet_receive = 0;
    /* 将USB数据包全部存入环形缓冲区 */
    if (receive_length > 0) {
      USB_Receive_Data(usb_data_buffer, receive_length);
      receive_length = 0;
      /* 重新启动USB接收 */
      cdc_acm_data_receive(&usb_device_dev);
      /* 再次尝试从环形缓冲区读取 */
      return RingBuffer_Read(key);
    }
    /* 重新启动USB接收 */
    cdc_acm_data_receive(&usb_device_dev);
  }
  return 0;
}

/**
  * @brief  Get a key from the HyperTerminal
  * @param  None
  * @retval The Key Pressed
  */
uint8_t GetKey(void)
{
  uint8_t key = 0;

  /* Waiting for user input */
  while (1)
  {
    if (SerialKeyPressed((uint8_t*)&key)) break;
  }
  return key;

}

/**
  * @brief  通过USB虚拟串口发送单个字符
  * @param  c: 要发送的字符
  * @retval None
  */
void SerialPutChar(uint8_t c)
{ 
  uint32_t timeout;
  if (USBD_CONFIGURED == usb_device_dev.status) {
    usb_data_buffer[0] = c;
    packet_sent = 0;
    cdc_acm_data_send(&usb_device_dev, 1);
    /* 等待发送完成，添加超时保护 */
    timeout = 100000;
    while ((packet_sent == 0) && (timeout-- > 0));
  }
}

/**
  * @brief  Print a string on the HyperTerminal
  * @param  s: The string to be printed
  * @retval None
  */
void Serial_PutString(uint8_t *s)
{
  while (*s != '\0')
  {
    SerialPutChar(*s);
    s++;
  }
}



  

uint8_t FileName[FILE_NAME_LENGTH];


/**
  * @brief  Receive byte from sender
  * @param  c: Character
  * @param  timeout: Timeout
  * @retval 0: Byte received
  *         -1: Timeout
  */
static  int32_t Receive_Byte (uint8_t *c, uint32_t timeout)
{
  while (timeout-- > 0)
  {
    if (SerialKeyPressed(c) == 1)
    {
      return 0;
    }
  }
  return -1;
}

/**
  * @brief  Send a byte
  * @param  c: Character
  * @retval 0: Byte sent
  */
static uint32_t Send_Byte (uint8_t c)
{
  SerialPutChar(c);
  return 0;
}

/**
  * @brief  Update CRC16 for input byte
  * @param  CRC input value 
  * @param  input byte
  * @retval Updated CRC value
  */
uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
  uint32_t crc = crcIn;
  uint32_t in = byte|0x100;

  do
  {
    crc <<= 1;
    in <<= 1;

    if(in&0x100)
    {
      ++crc;
    }
    
    if(crc&0x10000)
    {
      crc ^= 0x1021;
    }
 } while(!(in&0x10000));

 return (crc&0xffffu);
}

/**
  * @brief  Cal CRC16 for YModem Packet
  * @param  data
  * @param  length
  * @retval CRC value
  */
uint16_t Cal_CRC16(const uint8_t* data, uint32_t size)
{
  uint32_t crc = 0;
  const uint8_t* dataEnd = data+size;
  
  while(data<dataEnd)
  {
    crc = UpdateCRC16(crc,*data++);
  }
  crc = UpdateCRC16(crc,0);
  crc = UpdateCRC16(crc,0);

  return (crc&0xffffu);
}

/**
  * @brief  Cal Check sum for YModem Packet
  * @param  data
  * @param  length
  * @retval None
  */
uint8_t CalChecksum(const uint8_t* data, uint32_t size)
{
  uint32_t sum = 0;
  const uint8_t* dataEnd = data+size;
 
  while(data < dataEnd)
  {
    sum += *data++;
  }

 return (sum&0xffu);
}

/**
  * @brief  Receive a packet from sender
  * @param  data
  * @param  length
  * @param  timeout
  *          0: end of transmission
  *          -1: abort by sender
  *          >0: packet length
  * @retval 0: normally return
  *         -1: timeout or packet error
  *         1: abort by user
  */
static int32_t Receive_Packet (uint8_t *data, int32_t *length, uint32_t timeout)
{
  uint16_t i, packet_size, computedcrc;
  uint8_t c;
  *length = 0;
  if (Receive_Byte(&c, timeout) != 0)
  {
    return -1;
  }
  switch (c)
  {
    case SOH:
      packet_size = PACKET_SIZE;
      break;
    case STX:
      packet_size = PACKET_1K_SIZE;
      break;
    case EOT:
      return 0;
    case CA:
      if ((Receive_Byte(&c, timeout) == 0) && (c == CA))
      {
        *length = -1;
        return 0;
      }
      else
      {
        return -1;
      }
    case ABORT1:
    case ABORT2:
      return 1;
    default:
      return -1;
  }
  *data = c;
  for (i = 1; i < (packet_size + PACKET_OVERHEAD); i ++)
  {
    if (Receive_Byte(data + i, timeout) != 0)
    {
      return -1;
    }
  }
  if (data[PACKET_SEQNO_INDEX] != ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff))
  {
    return -1;
  }

  /* Compute the CRC */
  computedcrc = Cal_CRC16(&data[PACKET_HEADER], (uint32_t)packet_size);
  /* Check that received CRC match the already computed CRC value
     data[packet_size+3]<<8) | data[packet_size+4] contains the received CRC 
     computedcrc contains the computed CRC value */
  if (computedcrc != (uint16_t)((data[packet_size+3]<<8) | data[packet_size+4]))
  {
    /* CRC error */
    return -1;
  }

  *length = packet_size;
  return 0;
}

/**
  * @brief  Receive a file using the ymodem protocol
  * @param  buf: Address of the first byte
  * @retval The size of the file
  */
int32_t Ymodem_Receive (uint8_t *buf)
{
  uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD], file_size[FILE_SIZE_LENGTH], *file_ptr, *buf_ptr;
  int32_t i, packet_length, session_done, file_done, packets_received, errors, session_begin, size = 0;
  uint32_t flashdestination, ramsource;

  /* Initialize flashdestination variable */
  flashdestination = APPLICATION_ADDRESS;
  
  for (session_done = 0, errors = 0, session_begin = 0; ;)
  {
    for (packets_received = 0, file_done = 0, buf_ptr = buf; ;)
    {
      switch (Receive_Packet(packet_data, &packet_length, NAK_TIMEOUT))
      {
        case 0:
          errors = 0;
          switch (packet_length)
          {
            /* Abort by sender */
            case - 1:
              Send_Byte(ACK);
              return 0;
            /* End of transmission */
            case 0:
              Send_Byte(ACK);
              file_done = 1;
              break;
            /* Normal packet */
            default:
              if ((packet_data[PACKET_SEQNO_INDEX] & 0xff) != (packets_received & 0xff))
              {
                Send_Byte(NAK);
              }
              else
              {
                if (packets_received == 0)
                {
                  /* Filename packet */
                  if (packet_data[PACKET_HEADER] != 0)
                  {
                    /* Filename packet has valid data */
                    for (i = 0, file_ptr = packet_data + PACKET_HEADER; (*file_ptr != 0) && (i < FILE_NAME_LENGTH);)
                    {
                      FileName[i++] = *file_ptr++;
                    }
                    FileName[i++] = '\0';
                    for (i = 0, file_ptr ++; (*file_ptr != ' ') && (i < (FILE_SIZE_LENGTH - 1));)
                    {
                      file_size[i++] = *file_ptr++;
                    }
                    file_size[i++] = '\0';
                    Str2Int(file_size, &size);

                    /* Test the size of the image to be sent */
                    /* Image size is greater than Flash size */
                    if (size > (USER_FLASH_SIZE + 1))
                    {
                      /* End session */
                      Send_Byte(CA);
                      Send_Byte(CA);
                      return -1;
                    }
                    /* erase user application area */
                    FLASH_If_Erase(APPLICATION_ADDRESS);
                    Send_Byte(ACK);
                    Send_Byte(CRC16);
                  }
                  /* Filename packet is empty, end session */
                  else
                  {
                    Send_Byte(ACK);
                    file_done = 1;
                    session_done = 1;
                    break;
                  }
                }
                /* Data packet */
                else
                {
                  memcpy(buf_ptr, packet_data + PACKET_HEADER, packet_length);
                  ramsource = (uint32_t)buf;

                  /* Write received data in Flash */
                  if (FLASH_If_Write(&flashdestination, (uint32_t*) ramsource, (uint16_t) packet_length/4)  == 0)
                  {
                    Send_Byte(ACK);
                  }
                  else /* An error occurred while writing to Flash memory */
                  {
                    /* End session */
                    Send_Byte(CA);
                    Send_Byte(CA);
                    return -2;
                  }
                }
                packets_received ++;
                session_begin = 1;
              }
          }
          break;
        case 1:
          Send_Byte(CA);
          Send_Byte(CA);
          return -3;
        default:
          if (session_begin > 0)
          {
            errors ++;
          }
          if (errors > MAX_ERRORS)
          {
            Send_Byte(CA);
            Send_Byte(CA);
            return 0;
          }
          Send_Byte(CRC16);
          break;
      }
      if (file_done != 0)
      {
        break;
      }
    }
    if (session_done != 0)
    {
      break;
    }
  }
  return (int32_t)size;
}

/**
  * @brief  check response using the ymodem protocol
  * @param  buf: Address of the first byte
  * @retval The size of the file
  */
int32_t Ymodem_CheckResponse(uint8_t c)
{
  return 0;
}

/**
  * @brief  Prepare the first block
  * @param  timeout
  * @retval None
  */
void Ymodem_PrepareIntialPacket(uint8_t *data, const uint8_t* fileName, uint32_t *length)
{
  uint16_t i, j;
  uint8_t file_ptr[10];
  
  /* Make first three packet */
  data[0] = SOH;
  data[1] = 0x00;
  data[2] = 0xff;
  
  /* Filename packet has valid data */
  for (i = 0; (fileName[i] != '\0') && (i < FILE_NAME_LENGTH);i++)
  {
     data[i + PACKET_HEADER] = fileName[i];
  }

  data[i + PACKET_HEADER] = 0x00;
  
  Int2Str (file_ptr, *length);
  for (j =0, i = i + PACKET_HEADER + 1; file_ptr[j] != '\0' ; )
  {
     data[i++] = file_ptr[j++];
  }
  
  for (j = i; j < PACKET_SIZE + PACKET_HEADER; j++)
  {
    data[j] = 0;
  }
}

/**
  * @brief  Prepare the data packet
  * @param  timeout
  * @retval None
  */
void Ymodem_PreparePacket(uint8_t *SourceBuf, uint8_t *data, uint8_t pktNo, uint32_t sizeBlk)
{
  uint16_t i, size, packetSize;
  uint8_t* file_ptr;
  
  /* Make first three packet */
  packetSize = sizeBlk >= PACKET_1K_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;
  size = sizeBlk < packetSize ? sizeBlk :packetSize;
  if (packetSize == PACKET_1K_SIZE)
  {
     data[0] = STX;
  }
  else
  {
     data[0] = SOH;
  }
  data[1] = pktNo;
  data[2] = (~pktNo);
  file_ptr = SourceBuf;
  
  /* Filename packet has valid data */
  for (i = PACKET_HEADER; i < size + PACKET_HEADER;i++)
  {
     data[i] = *file_ptr++;
  }
  if ( size  <= packetSize)
  {
    for (i = size + PACKET_HEADER; i < packetSize + PACKET_HEADER; i++)
    {
      data[i] = 0x1A; /* EOF (0x1A) or 0x00 */
    }
  }
}

/**
  * @brief  Transmit a data packet using the ymodem protocol
  * @param  data
  * @param  length
  * @retval None
  */
void Ymodem_SendPacket(uint8_t *data, uint16_t length)
{
  uint16_t i;
  i = 0;
  while (i < length)
  {
    Send_Byte(data[i]);
    i++;
  }
}

/**
  * @brief  Transmit a file using the ymodem protocol
  * @param  buf: Address of the first byte
  * @retval The size of the file
  */
uint8_t Ymodem_Transmit (uint8_t *buf, const uint8_t* sendFileName, uint32_t sizeFile)
{
  uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD];
  uint8_t FileName[FILE_NAME_LENGTH];
  uint8_t *buf_ptr, tempCheckSum ;
  uint16_t tempCRC, blkNumber;
  uint8_t receivedC[2], CRC16_F = 0, i;
  uint32_t errors = 0, ackReceived = 0, size = 0, pktSize;

  for (i = 0; i < (FILE_NAME_LENGTH - 1); i++)
  {
    FileName[i] = sendFileName[i];
  }
  CRC16_F = 1;
  
  /* Prepare first block */
  Ymodem_PrepareIntialPacket(&packet_data[0], FileName, &sizeFile);
  
  do 
  {
    /* Send Packet */
    Ymodem_SendPacket(packet_data, PACKET_SIZE + PACKET_HEADER);
    
    /* Send CRC or Check Sum based on CRC16_F */
    if (CRC16_F)
    {
       tempCRC = Cal_CRC16(&packet_data[3], PACKET_SIZE);
       Send_Byte(tempCRC >> 8);
       Send_Byte(tempCRC & 0xFF);
    }
    else
    {
       tempCheckSum = CalChecksum (&packet_data[3], PACKET_SIZE);
       Send_Byte(tempCheckSum);
    }
  
    /* Wait for Ack and 'C' */
    if (Receive_Byte(&receivedC[0], 1000000) == 0)  
    {
      if (receivedC[0] == ACK)
      { 
        /* Packet transfered correctly */
        ackReceived = 1;
      }
    }
    else
    {
        errors++;
    }
  }while (!ackReceived && (errors < 0x0A));
  
  if (errors >=  0x0A)
  {
    return errors;
  }
  buf_ptr = buf;
  size = sizeFile;
  blkNumber = 0x01;

  /* Here 1024 bytes package is used to send the packets */
  while (size)
  {
    /* Prepare next packet */
    Ymodem_PreparePacket(buf_ptr, &packet_data[0], blkNumber, size);
    ackReceived = 0;
    receivedC[0]= 0;
    errors = 0;
    do
    {
      /* Send next packet */
      if (size >= PACKET_1K_SIZE)
      {
        pktSize = PACKET_1K_SIZE;
       
      }
      else
      {
        pktSize = PACKET_SIZE;
      }
      Ymodem_SendPacket(packet_data, pktSize + PACKET_HEADER);
      /* Send CRC or Check Sum based on CRC16_F */
      if (CRC16_F)
      {
         tempCRC = Cal_CRC16(&packet_data[3], pktSize);
         Send_Byte(tempCRC >> 8);
         Send_Byte(tempCRC & 0xFF);
      }
      else
      {
        tempCheckSum = CalChecksum (&packet_data[3], pktSize);
        Send_Byte(tempCheckSum);
      }
      
      /* Wait for Ack */
      if (Receive_Byte(&receivedC[0], 1000000) == 0)  
      {    if (receivedC[0] == ACK)
      {
        ackReceived = 1;  
        if (size > pktSize)
        {
           buf_ptr += pktSize;  
           size -= pktSize;
           if (blkNumber == (USER_FLASH_SIZE/1024))
           {
             return 0xFF; /*  error */
           }
           else
           {
              blkNumber++;
           }
        }
        else
        {
           buf_ptr += pktSize;
           size = 0;
        }
      }
      }
      else
      {
        errors++;
      }
    }while(!ackReceived && (errors < 0x0A));
    
    /* Resend packet if NAK  for a count of 10 else end of commuincation */
    if (errors >=  0x0A)
    {
      return errors;
    }
    
  }
  ackReceived = 0;
  receivedC[0] = 0x00;
  receivedC[1] = 0x00;
  errors = 0;
  do 
  { 
    Send_Byte(EOT);   
    /* Send (EOT); */
    /* Wait for Ack */
    if (Receive_Byte(&receivedC[0], 1000000) == 0)
    {
      if (receivedC[0] == ACK)
      {
        ackReceived = 1;
      }
    }
    else
    {
      errors++;
    }
  }while (!ackReceived && (errors < 0x0A));
    
  if (errors >=  0x0A)
  {
    return errors;
  }
  
  /* Last packet preparation */
  ackReceived = 0;
  receivedC[0] = 0x00;
  receivedC[1] = 0x00;
  errors = 0;

  packet_data[0] = SOH;
  packet_data[1] = 0;
  packet_data [2] = 0xFF;

  for (i = PACKET_HEADER; i < (PACKET_SIZE + PACKET_HEADER); i++)
  {
     packet_data [i] = 0x00;
  }
  
  do 
  {
    /* Send Packet */
    Ymodem_SendPacket(packet_data, PACKET_SIZE + PACKET_HEADER);

    /* Send CRC or Check Sum based on CRC16_F */
    tempCRC = Cal_CRC16(&packet_data[3], PACKET_SIZE);
    Send_Byte(tempCRC >> 8);
    Send_Byte(tempCRC & 0xFF);
  
    /* Wait for Ack and 'C' */
    if (Receive_Byte(&receivedC[1], 1000000) == 0)  
    {
      if (receivedC[1] == ACK)
      { 
        /* Packet transfered correctly */
        ackReceived = 1;
      }
    }
    else
    {
      errors++;
    }
  }while (!ackReceived && (errors < 0x0A));
  
  /* Resend packet if NAK  for a count of 10  else end of commuincation */
  if (errors >=  0x0A)
  {
    return errors;
  }  
  receivedC[0] = 0x00;
  do 
  { 
    Send_Byte(EOT);   
    /* Send (EOT); */
    /* Wait for Ack */
    if ((Receive_Byte(&receivedC[0], 1000000) == 0)  && receivedC[0] == ACK)
    {
      ackReceived = 1;  
    }
    
    else
    {
      errors++;
    }
    /* USB不需要清除 overrun flag */
  }while (!ackReceived && (errors < 0x0A));
    
  if (errors >=  0x0A)
  {
    return errors;
  }
  return 0; /* file trasmitted successfully */
}

void SerialDownload(void)
{
  uint8_t Number[10] = {0};
  int32_t Size = 0;

  SerialPutString("\r\n");
  SerialPutString("-------------------------------------------\r\n");
  SerialPutString("  Ymodem Download Mode\r\n");
  SerialPutString("-------------------------------------------\r\n");
  SerialPutString("  1. Select Ymodem protocol in your tool\r\n");
  SerialPutString("  2. Choose the .bin file to send\r\n");
  SerialPutString("  3. Start transmission\r\n");
  SerialPutString("-------------------------------------------\r\n");
  SerialPutString("  Waiting for file... (press 'a' to abort)\r\n\r\n");
  
  /* Debug: show we are ready */
  SerialPutString("Device ready. Sending 'C' to request file...\r\n");
  
  Size = Ymodem_Receive(&tab_1024[0]);
  
  SerialPutString("\r\n");
  
  if (Size > 0)
  {
    SerialPutString("\n\n\r Programming Completed Successfully!\n\r--------------------------------\r\n Name: ");
    SerialPutString(FileName);
    Int2Str(Number, Size);
    SerialPutString("\n\r Size: ");
    SerialPutString(Number);
    SerialPutString(" Bytes\r\n");
    SerialPutString("\r\nDownload complete. Remove PA0-GND and reset to run APP.\r\n");
    SerialPutString("Press any key to return to menu...\r\n");
    GetKey();
  }
  else if (Size == -1)
  {
    SerialPutString("\n\n\rError: Image size too large!\n\r");
  }
  else if (Size == -2)
  {
    SerialPutString("\n\n\rError: Flash write verification failed!\n\r");
  }
  else if (Size == -3)
  {
    SerialPutString("\n\n\rError: Transfer aborted by user!\n\r");
  }
  else if (Size == 0)
  {
    SerialPutString("\n\n\rError: No file received (timeout)!\n\r");
  }
  else
  {
    SerialPutString("\n\n\rError: Download failed!\n\r");
  }
}

/**
  * @brief  Upload a file via serial port.
  * @param  None
  * @retval None
  */
void SerialUpload(void)
{
  uint8_t status = 0 ; 

  SerialPutString("\n\n\rSelect Receive File\n\r");

  if (GetKey() == CRC16)
  {
    /* Transmit the flash image through ymodem protocol */
    status = Ymodem_Transmit((uint8_t*)APPLICATION_ADDRESS, (const uint8_t*)"UploadedFlashImage.bin", USER_FLASH_SIZE);

    if (status != 0) 
    {
      SerialPutString("\n\rError Occurred while Transmitting File\n\r");
    }
    else
    {
      SerialPutString("\n\rFile uploaded successfully \n\r");
    }
  }
}

/**
  * @brief  Display the Main Menu on HyperTerminal
  * @param  None
  * @retval None
  */
void Main_Menu(void)
{
  uint8_t key = 0;
  uint8_t writeprotect = 0;
  uint32_t menu_timer = 0;
  uint8_t show_menu = 1;
  (void)writeprotect; /* 抑制未使用变量警告 */

  SerialPutString("\r\n======================================================================");
  SerialPutString("\r\n=              (C) COPYRIGHT 2012 STMicroelectronics                 =");
  SerialPutString("\r\n=                                                                    =");
  SerialPutString("\r\n=  GD32F103 In-Application Programming Application  (Version 1.0.0) =");
  SerialPutString("\r\n=                                                                    =");
  SerialPutString("\r\n=                                   By MCD Application Team          =");
  SerialPutString("\r\n======================================================================");
  SerialPutString("\r\n\r\n");

  /* Test if any sector of Flash memory where user application will be loaded is write protected */
  if (FLASH_If_GetWriteProtectionStatus() != 0)  
  {
    FlashProtection = 1;
  }
  else
  {
    FlashProtection = 0;
  }

  while (1)
  {
    /* 每5秒显示一次菜单 */
    if (show_menu)
    {
      SerialPutString("\r\n================== Main Menu ============================\r\n\n");
      SerialPutString("  Download Image To the GD32F103  Internal Flash ------- 1\r\n\n");
      SerialPutString("  Upload user application from the Flash --------------- 2\r\n\n");

      if(FlashProtection != 0)
      {
        SerialPutString("  Disable the write protection ------------------------- 4\r\n\n");
      }

      SerialPutString("==========================================================\r\n\n");
      SerialPutString("Please input your choice (1-2): ");
      show_menu = 0;
      menu_timer = 0;
    }

    /* 检查是否有按键，带超时 */
    if (SerialKeyPressed(&key))
    {
      SerialPutChar(key);
      SerialPutString("\r\n");

      if (key == 0x31)
      {
        /* Download user application in the Flash */
        SerialDownload();
        show_menu = 1; /* 操作完成后重新显示菜单 */
      }
      else if (key == 0x32)
      {
        /* Upload user application from the Flash */
        SerialUpload();
        show_menu = 1; /* 操作完成后重新显示菜单 */
      }
      else if (key == 0x33)
      {
        /* 跳转功能已删除，提示用户 */
        SerialPutString("\r\nPlease remove PA0-GND short and reset to run APP.\r\n");
        show_menu = 1;
      }
      else if (key == 0x34 && FlashProtection != 0)
      {
        /* Disable write protection */
        SerialPutString("Disable write protection not implemented.\r\n");
        show_menu = 1;
      }
      else
      {
        if (FlashProtection == 0)
        {
          SerialPutString("Invalid Number ! ==> The number should be either 1, 2 or 3\r\n");
        }
        else
        {
          SerialPutString("Invalid Number ! ==> The number should be either 1, 2, 3 or 4\r\n");
        }
        delay_1ms(100);
        show_menu = 1; /* 输入错误，重新显示菜单 */
      }
    }
    else
    {
      /* 每5秒重新显示菜单 */
      delay_1ms(10);
      menu_timer += 10;
      if (menu_timer >= 5000) /* 5秒超时 */
      {
        show_menu = 1;
      }
    }
  }
}

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
