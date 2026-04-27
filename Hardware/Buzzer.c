#include "stm32f10x.h"                  // Device header

#define BUZZER GPIO_Pin_10

void Buzzer_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;//�?���?3?
	GPIO_InitStructure.GPIO_Pin=BUZZER;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStructure);
	
	GPIO_SetBits(GPIOB,BUZZER);
}

void Buzzer_ON(void)
{
	GPIO_ResetBits(GPIOB,BUZZER);
}

void Buzzer_OFF(void)
{
	GPIO_SetBits(GPIOB,BUZZER);
}

void Buzzer_Turn(void)
{
	if (GPIO_ReadOutputDataBit(GPIOB,BUZZER)==0)
	{
		GPIO_SetBits(GPIOB,BUZZER);
	}
    else
	{
		GPIO_ResetBits(GPIOB,BUZZER);
	}
}
