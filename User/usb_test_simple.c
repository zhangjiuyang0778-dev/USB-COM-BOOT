/*!
    \file  usb_test_simple.c
    \brief Simple USB test
*/

#include "gd32f10x.h"
#include "systick.h"
#include "cdc_acm_core.h"
#include "usbd_int.h"

extern const usb_descriptor_device_struct device_descriptor;
extern usb_descriptor_configuration_set_struct configuration_descriptor;
extern void* const usbd_strings[];

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

void nvic_config(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE1_SUB3);
    nvic_irq_enable(USBD_LP_CAN0_RX0_IRQn, 1, 0);
}

int main(void)
{
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV1);
    systick_config();
    
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_AF);
    
    rcu_usb_clock_config(RCU_CKUSB_CKPLL_DIV1_5);
    rcu_periph_clock_enable(RCU_USBD);
    
    usbd_core_init(&usb_device_dev);
    nvic_config();
    
    usb_device_dev.status = USBD_CONNECTED;
    
    while(1)
    {
        // 简单循环
    }
}
