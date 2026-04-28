/**
  ******************************************************************************
  * @file    Motor.c
  * @brief   减速电机驱动（单向旋转凸轮结构）
  *          驱动模块接线：
  *            IN1  -> PB12（方向控制）
  *            IN2  -> PB13（方向控制）
  *            ENA  -> PA11（PWM调速，TIM1_CH4）
  *          工作原理：电机单向旋转，凸轮顶开锁舌后触发微动开关停止
  *                    延时后继续旋转，凸轮离开锁舌，弹簧自动回弹关锁
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Motor.h"
#include "PWM.h"

/**
  * @brief  电机驱动初始化
  * @note   1. 开启GPIOB时钟
  *         2. 配置PB12/PB13为推挽输出（接驱动模块IN1/IN2）
  *         3. 调用PWM_Init()初始化PA11的10kHz PWM输出（接ENA）
  *         4. 初始状态：电机关闭
  */
void Motor_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    // 推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    PWM_Init();     // 初始化TIM1_CH4，PA11输出10kHz PWM
    Motor_Off();    // 初始状态：电机关闭
}

/**
  * @brief  电机启动（单向正转）
  * @note   PB12=0, PB13=1，电机正转
  *         PWM占空比80%，提供足够扭矩推动锁舌
  */
void Motor_On(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);  // IN1 = 0
    GPIO_SetBits(GPIOB, GPIO_Pin_13);    // IN2 = 1，正转方向
    PWM_SetCompare4(80);                  // CCR=80，占空比80%
}

/**
  * @brief  电机关闭
  * @note   PB12=0, PB13=0，电机两端接地制动
  *         PWM占空比0%，停止输出
  */
void Motor_Off(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);  // IN1 = 0
    GPIO_ResetBits(GPIOB, GPIO_Pin_13);  // IN2 = 0，制动停止
    PWM_SetCompare4(0);                   // PWM占空比0%
}
