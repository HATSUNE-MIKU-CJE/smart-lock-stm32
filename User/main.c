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
uint32_t g_idleTick = 0;            /**< 新增：STATE_IDLE状态超时计时，单位ms */
uint8_t g_lastState = 0xFF;          /**< 新增：记录上一轮状态，用于检测是否刚进入IDLE */

#define IDLE_TIMEOUT_MS     180000   /**< 新增：IDLE状态无操作超时进入Stop模式，180000ms = 3分钟 */
// #define IDLE_TIMEOUT_MS  20000    // 测试时用20秒

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
        if (g_state != g_lastState)
        {
            if (g_state == STATE_IDLE)
            {
                g_idleTick = GetTick();
            }
            g_lastState = g_state;
        }
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
                OLED_ShowString(0, 0, "* : 确认  ", OLED_8X16);
                OLED_ShowString(0, 16,"# : 退格  ", OLED_8X16);
                OLED_ShowString(0, 32,"开锁请按确认",OLED_8X16);
                OLED_ShowString(0, 48,"配置请按退格",OLED_8X16);
                OLED_Update();
                
                if (evt == KEY_CONFIRM)
                {
                    OLED_Clear();
                    OLED_ShowString(0, 0, "密码:", OLED_8X16);
                    OLED_Update();
                    Password_Reset();
                    g_state = STATE_PASSWORD_INPUT;
                }
                else if (evt == KEY_BACK)
                {
                    Config_Start();  // 进入配置模式
                }

                /*----- 新增：IDLE状态有按键时重置超时计时 -----
                 *  只要用户在操作（任意键），就不让系统进入睡眠
                 */
                if (evt != KEY_NONE)
                {
                    g_idleTick = GetTick();
                }
                
                /*----- 新增：IDLE超时进入Stop低功耗模式 -----
                 *  机制：IDLE状态下3分钟无操作，系统进入Stop模式省电
                 *  唤醒：任意键按下都会通过EXTI唤醒CPU，但只有*键被接受
                 *  过滤：非*键唤醒后，内层while循环继续睡，实现"软件过滤"
                 *  恢复：唤醒后SystemInit()重新配置72MHz时钟，Delay_ms(20)等稳定
                 */
                if (GetTick() - g_idleTick > IDLE_TIMEOUT_MS)
                {
                    /* 清屏省电，OLED保持黑屏直到被唤醒 */
                    OLED_Clear();
                    OLED_Update();

                    /* 内层循环：反复进入Stop，直到被*键唤醒 */
                    while (1)
                    {
                        PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFI);
                        
                        /* ========== 从Stop唤醒后从这里继续执行 ========== */
                        SystemInit();       /* 恢复72MHz系统时钟（Stop关闭了PLL/HSE） */
                        Delay_ms(20);       /* 等时钟稳定 + 按键消抖 */
                        
                        Key_Scan();         /* 矩阵扫描，看是哪个键唤醒的 */
                        if (Key_GetEvent() == KEY_CONFIRM)
                        {
                            break;          /* 是*键，退出内层循环，正常开机 */
                        }
                        /* 不是*键（1~9、0、#），继续睡，用户无感知 */
                    }

                    /* 被*键唤醒，直接进入密码输入（模拟按了确认键的效果） */
                    OLED_Clear();
                    OLED_ShowString(0, 0, "密码:", OLED_8X16);
                    OLED_Update();
                    Password_Reset();
                    g_idleTick = GetTick();
                    g_state = STATE_PASSWORD_INPUT;
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
                        OLED_ShowString(0, 0, "密码错误", OLED_8X16);
                        OLED_Update();
                        Delay_ms(800);
                        OLED_Clear();
                        OLED_ShowString(0, 0, "密码:", OLED_8X16);
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
