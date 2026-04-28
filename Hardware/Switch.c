/**
  ******************************************************************************
  * @file    Switch.c
  * @brief   微动开关（到位检测）驱动
  *          引脚：PA12
  *          接法：开关一脚接PA12，另一脚接GND
  *          配置：内部上拉输入，未按时为高电平(1)，按下/闭合时为低电平(0)
  *          功能：检测电机偏心轮是否旋转到位，触发停止
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Switch.h"
#include "Delay.h"

/**
  * @brief  微动开关初始化
  * @note   开启GPIOA时钟，配置PA12为内部上拉输入
  *         开关未按下时，内部上拉电阻将PA12保持在高电平
  *         开关按下时，PA12被拉低到GND
  */
void Switch_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;   // 内部上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * @brief  读取微动开关当前电平
  * @retval 0：开关闭合（按下/到位）
  * @retval 1：开关断开（未按下/未到位）
  */
uint8_t Switch_Get(void)
{
    return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_12);
}

/**
  * @brief  检测开关是否确认闭合（带软件消抖）
  * @retval 1：确认闭合（连续5ms读到低电平）
  * @retval 0：未闭合或抖动中
  * @note   机械开关按下瞬间存在抖动（通断通断），连续采样5次（间隔1ms）
  *         若5次都为低电平，才认为是真实闭合，防止误触发
  */
uint8_t Switch_IsClosed(void)
{
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < 5; i++) {
        if (Switch_Get() == 0) {
            count++;
        }
        Delay_ms(1);  // 每隔1ms采样一次，总共检测5ms
    }
    
    return (count >= 5) ? 1 : 0;  // 5次全为低电平，确认闭合
}
