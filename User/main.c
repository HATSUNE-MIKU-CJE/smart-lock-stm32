/**
  ******************************************************************************
  * @file    main.c
  * @brief   智能密码锁主程序入口
  *          职责：初始化所有外设和模块，进入主循环做事件分发
  *          状态调度逻辑：
  *            1. 开锁流程独立 → 调用 Unlock_Tick() 自动驱动
  *            2. 配置模式独立 → 调用 Config_HandleEvent() 处理
  *            3. 主状态机只管理：待机(IDLE) 和 密码输入(PASSWORD_INPUT)
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Delay.h"
#include "Buzzer.h"
#include "OLED.h"
#include "Key.h"
#include "Motor.h"
#include "Switch.h"
#include "Password.h"
#include "Store.h"
#include "Unlock.h"
#include "Config.h"

/*================== 主状态机定义 ==================*/

#define STATE_IDLE            0  /**< 待机：等待按键进入功能 */
#define STATE_PASSWORD_INPUT  1  /**< 密码输入：接收6位数字 */

/*================== 全局变量 ==================*/

uint8_t g_state = STATE_IDLE;   /**< 当前主状态 */

/*================== 私有辅助函数 ==================*/

/**
  * @brief  刷新OLED第二行的星号显示
  * @note   密码输入状态下，根据当前已输入位数绘制星号
  */
static void RefreshStars(void)
{
    uint8_t len = Password_GetInputLen();
    OLED_ClearArea(0, 16, 128, 16);
    for (uint8_t i = 0; i < len; i++)
    {
        OLED_ShowString(i * 8, 16, "*", OLED_8X16);
    }
    OLED_Update();
}

/*================== 主函数 ==================*/

int main(void)
{
    Systick_Init();  // TIM4提供1ms时基
    
    /*----- 初始化所有外设 -----*/
    Buzzer_Init();
    OLED_Init();
    Switch_Init();
    Key_Init();
    Motor_Init();
    Unlock_Init();     // 开锁模块初始化（确保电机停止）
    Storage_Init();    // 从Flash加载密码
    Password_Init();   // 清空输入缓存
    
    /*----- 开机画面 -----*/
    OLED_Clear();
    OLED_ShowString(0, 0, "STM32 Lock", OLED_8X16);
    OLED_Update();
    Delay_s(1);
    OLED_Clear();
    OLED_Update();
    
    /*----- 主循环：事件扫描 + 状态分发 -----*/
    while (1)
    {
        Key_Scan();
        uint8_t evt = Key_GetEvent();
        
        /*--- 任意按键反馈 + 配置模式超时刷新 ---*/
        if (evt != KEY_NONE)
        {
            Buzzer_KeyPress();
            if (Config_IsActive())
            {
                Config_ResetTimeout();
            }
        }
        
        /*--- 开锁流程独立驱动（最高优先级） ---*/
        Unlock_Tick();
        
        /*--- 配置模式事件分发 ---*/
        if (Config_IsActive())
        {
            Config_HandleEvent(evt);
            continue;  // 配置模式下不处理主状态机
        }
        
        /*--- 开锁过程中不处理主状态机按键 ---*/
        if (Unlock_IsBusy())
        {
            continue;
        }
        
        /*--- 主状态机：待机 / 密码输入 ---*/
        switch (g_state)
        {
            /*========== 待机状态 ==========*/
            case STATE_IDLE:
                OLED_ShowString(0, 0, "Press CONFIRM", OLED_8X16);
                OLED_Update();
                
                if (evt == KEY_CONFIRM)
                {
                    OLED_Clear();
                    OLED_ShowString(0, 0, "Password:", OLED_8X16);
                    OLED_Update();
                    Password_Reset();
                    g_state = STATE_PASSWORD_INPUT;
                }
                else if (evt == KEY_BACK)
                {
                    Config_Start();  // 进入配置模式
                }
                break;
            
            /*========== 密码输入状态 ==========*/
            case STATE_PASSWORD_INPUT:
                if (evt <= 9)
                {
                    if (Password_InputDigit(evt))
                    {
                        RefreshStars();
                    }
                }
                else if (evt == KEY_CONFIRM)
                {
                    if (Password_Check())
                    {
                        Password_Reset();
                        Buzzer_Unlock();
                        Unlock_Start();  // 启动开锁流程
                        g_state = STATE_IDLE;
                    }
                    else
                    {
                        Password_Reset();
                        Buzzer_Error();
                        OLED_Clear();
                        OLED_ShowString(0, 0, "ERROR", OLED_8X16);
                        OLED_Update();
                        Delay_ms(800);
                        OLED_Clear();
                        OLED_ShowString(0, 0, "Password:", OLED_8X16);
                        OLED_Update();
                    }
                }
                else if (evt == KEY_BACK)
                {
                    if (Password_GetInputLen() == 0)
                    {
                        Buzzer_Backspace();
                        g_state = STATE_IDLE;
                    }
                    else
                    {
                        Password_Remove();
                        Buzzer_Backspace();
                        RefreshStars();
                    }
                }
                break;
        }
    }
}
