/**
  ******************************************************************************
  * @file    PWM.c
  * @brief   TIM1_CH4 PWM输出驱动（用于直流电机调速）
  *          输出引脚：PA11（接电机驱动模块ENA使能端）
  *          频率：10kHz，适合直流电机调速（静音、低损耗）
  *          占空比范围：CCR 0~99 对应 0%~100%
  *          时钟计算：72MHz / 72分频 / 100计数 = 10kHz
  ******************************************************************************
  */

#include "stm32f10x.h"

/**
  * @brief  TIM1 PWM初始化
  * @note   配置TIM1为PWM模式1，输出通道CH4，引脚PA11
  *         高级定时器TIM1必须调用TIM_CtrlPWMOutputs开启主输出使能(MOE)
  */
void PWM_Init(void)
{
	// 开启TIM1时钟（APB2总线）和GPIOA时钟（PA11需要）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOA, ENABLE);
	
	// 配置PA11为复用推挽输出（TIM1_CH4功能）
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	// TIM1使用内部时钟源（72MHz）
	TIM_InternalClockConfig(TIM1);
	
	// 时基单元配置：10kHz PWM频率
	// 72MHz / 72(PSC) = 1MHz计数时钟
	// 1MHz / 100(ARR) = 10kHz PWM频率
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;      // 时钟不分频
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;  // 向上计数
	TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;                  // ARR=99，计数100次
	TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;                // PSC=71，72分频
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;             // 重复计数器
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
	
	// PWM输出比较通道4配置
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);                          // 先填充默认值
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;                // PWM模式1
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;        // 输出极性：高电平有效
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;    // 使能输出
	TIM_OCInitStructure.TIM_Pulse = 0;                               // CCR初始值：0（停止）
	TIM_OC4Init(TIM1, &TIM_OCInitStructure);                         // 初始化通道4
	
	// 高级定时器特有：必须开启主输出使能(MOE)，否则PWM无输出
	TIM_CtrlPWMOutputs(TIM1, ENABLE);
	// 启动定时器
	TIM_Cmd(TIM1, ENABLE);
}

/**
  * @brief  设置TIM1通道4的比较值（即占空比）
  * @param  Compare CCR比较值，范围0~99
  *                 0   -> 0%占空比  -> 电机停止
  *                 50  -> 50%占空比 -> 半速
  *                 80  -> 80%占空比 -> 接近全速
  *                 99  -> 99%占空比 -> 全速
  * @note   占空比 = Compare / (ARR+1) = Compare / 100
  */
void PWM_SetCompare4(uint16_t Compare)
{
	TIM_SetCompare4(TIM1, Compare);
}
