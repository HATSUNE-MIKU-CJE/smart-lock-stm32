/**
  ******************************************************************************
  * @file    Buzzer.c
  * @brief   蜂鸣器驱动（有源蜂鸣器）
  *          控制引脚：PB10
  *          驱动方式：GPIO推挽输出，低电平导通（响），高电平截止（停）
  *          接线：蜂鸣器正极接VCC，负极接PB10（或通过三极管驱动）
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Buzzer.h"
#include "Delay.h"

#define BUZZER_PIN  GPIO_Pin_10   /**< 蜂鸣器控制引脚：PB10 */
#define BUZZER_PORT GPIOB         /**< 蜂鸣器控制端口 */

/**
  * @brief  蜂鸣器初始化
  * @note   开启GPIOB时钟，配置PB10为推挽输出，默认输出高电平（蜂鸣器不响）
  */
void Buzzer_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    // 推挽输出：可输出高/低电平
	GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);
	
	// 初始状态：输出高电平，蜂鸣器不响
	// （有源蜂鸣器低电平导通时响，高电平时停；若接法相反请改此处）
	GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}

/**
  * @brief  蜂鸣器响（输出低电平导通）
  */
void Buzzer_ON(void)
{
	GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
}

/**
  * @brief  蜂鸣器停（输出高电平截止）
  */
void Buzzer_OFF(void)
{
	GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}

/**
  * @brief  蜂鸣器状态翻转
  * @note   若当前在响则停，若当前停则响。可用于简单的提示音
  */
void Buzzer_Turn(void)
{
	if (GPIO_ReadOutputDataBit(BUZZER_PORT, BUZZER_PIN) == 0)
	{
		GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);      // 当前在响，改为停
	}
    else
	{
		GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);    // 当前停，改为响
	}
}

/*================== 提示音封装函数 ==================*/

/**
  * @brief  按键反馈音（短叫一声）
  * @note   每次按下任意按键时调用，给用户触觉反馈
  */
void Buzzer_KeyPress(void)
{
	Buzzer_ON();
	Delay_ms(50);   // 短鸣50ms
	Buzzer_OFF();
}

/**
  * @brief  开锁成功提示音（短-长-短 三声）
  * @note   节奏：嘀(30ms)-嗒(200ms)-嘀(30ms)
  */
void Buzzer_Unlock(void)
{
	Buzzer_ON();  Delay_ms(50);  Buzzer_OFF();  Delay_ms(70);
	Buzzer_ON();  Delay_ms(200); Buzzer_OFF();  Delay_ms(70);
	Buzzer_ON();  Delay_ms(50);  Buzzer_OFF();
}

/**
  * @brief  密码错误提示音（短叫两声）
  * @note   节奏：嘀(50ms)-嘀(50ms)
  */
void Buzzer_Error(void)
{
	Buzzer_ON();  Delay_ms(50);  Buzzer_OFF();  Delay_ms(70);
	Buzzer_ON();  Delay_ms(50);  Buzzer_OFF();
}

/**
  * @brief  关锁完成提示音（短-短-长 三声）
  * @note   节奏：嘀(30ms)-嘀(30ms)-嗒(200ms)
  */
void Buzzer_Lock(void)
{
	Buzzer_ON();  Delay_ms(50);  Buzzer_OFF();  Delay_ms(70);
	Buzzer_ON();  Delay_ms(50);  Buzzer_OFF();  Delay_ms(70);
	Buzzer_ON();  Delay_ms(200); Buzzer_OFF();
}

/**
  * @brief  退格提示音（中鸣一声）
  * @note   节奏：嘀(100ms)，比按键音(50ms)长，比错误音短
  *         用于密码输入时的退格操作反馈，声音与按键音区分
  */
void Buzzer_Backspace(void)
{
    Buzzer_ON();  Delay_ms(100);  Buzzer_OFF();  
}
