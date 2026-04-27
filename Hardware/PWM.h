/**
  ******************************************************************************
  * @file    PWM.h
  * @brief   TIM1_CH4 PWM输出驱动对外接口
  *          用于驱动PA11引脚输出50Hz PWM信号，控制舵机角度
  ******************************************************************************
  */

#ifndef __PWM_H
#define __PWM_H

#include <stdint.h>

void PWM_Init(void);                   /**< PWM初始化：配置TIM1_CH4，50Hz，PA11输出 */
void PWM_SetCompare4(uint16_t Compare); /**< 设置CCR比较值，改变脉宽（500~2500对应0°~180°） */

#endif
