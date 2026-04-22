#include "stm32f10x.h"                  // Device header
#include "Motor.h"


int main()
{
    Motor_Init();
    Motor_SetLeftSpeed(50);
    Motor_SetRightSpeed(0);
    while (1)
    {

    }
}
