/**
  ******************************************************************************
  * @file    Servo.c
  * @brief   舵机角度控制驱动（基于PWM.c）
  *          适用SG90/MG995等模拟舵机，控制引脚PA11
  *          角度范围：0° ~ 180°
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "PWM.h"

/**
  * @brief  舵机初始化
  * @note   内部调用PWM_Init()，配置TIM1输出50Hz PWM
  */
void Servo_Init(void)
{
	PWM_Init();
}

/**
  * @brief  设置舵机转动到指定角度
  * @param  Angle 目标角度，单位：度。范围0~180，超出范围会被限制
  * @note   角度与CCR的换算关系：
  *         0°   -> 0.5ms脉宽 -> CCR = 500
  *         90°  -> 1.5ms脉宽 -> CCR = 1500
  *         180° -> 2.5ms脉宽 -> CCR = 2500
  *         公式：CCR = Angle / 180 * 2000 + 500
  *         例如20°：20/180*2000+500 ≈ 722
  */
void Servo_SetAngle(float Angle)
{
	// 限幅保护：防止传入非法角度导致舵机堵转或损坏
	if (Angle < 0) Angle = 0;
	if (Angle > 180) Angle = 180;
	
	// 角度转CCR值：把0~180度线性映射到500~2500
	uint16_t Compare = (uint16_t)(Angle / 180.0f * 2000.0f + 500.0f);
	PWM_SetCompare4(Compare);
}
