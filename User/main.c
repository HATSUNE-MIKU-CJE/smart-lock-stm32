#include "stm32f10x.h"
#include "Buzzer.h"
#include "Servo.h"
#include "Delay.h"

int main(void)
{
	Buzzer_Init();
	Servo_Init();
    Servo_SetAngle(0);
    Delay_s(1);
	for (int i=0;i<45;i++)
    {
        Delay_ms(30);
        Servo_SetAngle(i*2);
    }
	Buzzer_Turn();
    Delay_ms(100);
    Buzzer_Turn();
    Delay_ms(100);
    Buzzer_Turn();
    Delay_ms(100);
    Buzzer_Turn();
	while (1)
	{
	}
}
