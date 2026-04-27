#include "stm32f10x.h"
#include "PWM.h"

void Servo_Init(void)
{
	PWM_Init();
}

void Servo_SetAngle(float Angle)
{
	if (Angle < 0) Angle = 0;
	if (Angle > 180) Angle = 180;
	
	uint16_t Compare = (uint16_t)(Angle / 180.0f * 2000.0f + 500.0f);
	PWM_SetCompare4(Compare);
}
