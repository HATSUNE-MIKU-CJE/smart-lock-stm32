/**
  ******************************************************************************
  * @file    main.c
  * @brief   智能密码锁主程序入口
  *          当前阶段：舵机测试 + 按键扫描验证
  *          后续将逐步加入密码验证、状态机、指纹、语音等功能
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Buzzer.h"
#include "Servo.h"
#include "Delay.h"
#include "Key.h"
#include "OLED.h"

int main(void)
{
	Systick_Init();  // 启动TIM4的1ms中断，为GetTick()提供时基
	
	// 初始化外设
	Buzzer_Init();
	Servo_Init();
    OLED_Init();
    OLED_ShowString(0,0,"stm32-lock",OLED_8X16);
    OLED_Update();
    Delay_s(2);
    OLED_Clear();
    OLED_Update();
    Key_Init();
	

	// 舵机复位到0度（闭锁位置）
    Servo_SetAngle(0);
    Delay_s(1);  // 等待1秒，让舵机转到到位
	
	// 舵机从0°缓慢转动到90°（模拟开锁过程）
	for (int i = 0; i < 45; i++)
    {
        Delay_ms(30);               // 每30ms转2度，形成平滑转动效果
        Servo_SetAngle(i * 2);      // i=0时0°，i=44时88°
    }
	
	
	// 主循环：后续将在此加入按键扫描、状态机、密码验证等逻辑
	while (1)
	{
        Key_Scan();

        uint8_t evt = Key_GetEvent();

        if (evt != KEY_NONE)
        {
            OLED_Clear();

            if (evt <= 9){
                OLED_ShowNum(0,0,evt,1,OLED_8X16);
                OLED_Update();
            }
            else if (evt==KEY_CONFIRM){
                OLED_ShowString(0,0,"CONFIRM",OLED_8X16);
                OLED_Update();
            }
        }


	}
}
