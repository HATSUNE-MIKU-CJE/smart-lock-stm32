/**
  ******************************************************************************
  * @file    Config.c
  * @brief   配置模式实现：修改密码的完整状态机
  *          内部维护3阶段：验证旧密码 → 输入新密码 → 确认新密码
  *          支持10秒无操作自动退出，两次输入一致后才写入Flash
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Config.h"
#include "Key.h"
#include "Password.h"
#include "OLED.h"
#include "Buzzer.h"
#include "Store.h"
#include "Delay.h"

/*================== 私有类型与变量 ==================*/

typedef enum {
    CFG_PHASE_IDLE = 0,    /**< 空闲：不在配置模式中 */
    CFG_PHASE_VERIFY,      /**< 第1步：验证旧密码 */
    CFG_PHASE_SET,         /**< 第2步：输入新密码 */
    CFG_PHASE_CONFIRM      /**< 第3步：再次确认新密码 */
} ConfigPhase_t;

static ConfigPhase_t s_phase = CFG_PHASE_IDLE;  /**< 当前配置阶段 */
static uint32_t s_tick = 0;                     /**< 超时计时起点 */
static uint8_t s_newPwd[6];                     /**< 缓存第一次输入的新密码 */

#define CFG_TIMEOUT_MS    10000                 /**< 配置模式超时：10秒 */

/*================== 私有辅助函数 ==================*/

/**
  * @brief  刷新OLED第二行的星号显示
  * @note   根据当前 Password 输入长度，在 y=16 处绘制对应数量的星号
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

/**
  * @brief  退出配置模式，回到空闲
  * @note   清空输入缓存，阶段归零
  */
static void ExitConfig(void)
{
    Password_Reset();
    s_phase = CFG_PHASE_IDLE;
}

/*================== 对外接口 ==================*/

/**
  * @brief  启动配置模式
  * @note   待机界面按返回键时调用，显示 "Old Pwd:" 准备接收旧密码
  */
void Config_Start(void)
{
    s_phase = CFG_PHASE_VERIFY;
    s_tick = GetTick();
    
    OLED_Clear();
    OLED_ShowString(0, 0, "Old Pwd:", OLED_8X16);
    OLED_Update();
    
    Password_Reset();
}

/**
  * @brief  判断配置模式是否激活
  * @retval 1：正在配置流程中
  * @retval 0：空闲
  */
uint8_t Config_IsActive(void)
{
    return (s_phase != CFG_PHASE_IDLE);
}

/**
  * @brief  刷新配置超时计时器
  * @note   主循环检测到配置模式下有按键时调用，10秒从最后一次按键重新计算
  */
void Config_ResetTimeout(void)
{
    s_tick = GetTick();
}

/**
  * @brief  配置模式状态机：处理按键事件并驱动流程
  * @param  evt 按键事件值
  */
void Config_HandleEvent(uint8_t evt)
{
    /*---------- 超时检测：10秒无操作自动退出 ----------*/
    if (GetTick() - s_tick > CFG_TIMEOUT_MS)
    {
        ExitConfig();
        return;
    }
    
    /*---------- 第1步：验证旧密码 ----------*/
    if (s_phase == CFG_PHASE_VERIFY)
    {
        if (evt <= 9)
        {
            if (Password_InputDigit(evt)) RefreshStars();
        }
        else if (evt == KEY_CONFIRM)
        {
            if (Password_Check())  /* 旧密码正确 */
            {
                Password_Reset();
                OLED_Clear();
                OLED_ShowString(0, 0, "New Pwd:", OLED_8X16);
                OLED_Update();
                s_tick = GetTick();
                s_phase = CFG_PHASE_SET;
            }
            else  /* 旧密码错误 */
            {
                Password_Reset();
                Buzzer_Error();
                OLED_Clear();
                OLED_ShowString(0, 0, "ERROR", OLED_8X16);
                OLED_Update();
                Delay_ms(800);
                ExitConfig();
            }
        }
        else if (evt == KEY_BACK)
        {
            if (Password_GetInputLen() == 0)
            {
                Buzzer_Backspace();
                ExitConfig();
            }
            else
            {
                Password_Remove();
                Buzzer_Backspace();
                RefreshStars();
            }
        }
        return;
    }
    
    /*---------- 第2步：输入新密码 ----------*/
    if (s_phase == CFG_PHASE_SET)
    {
        if (evt <= 9)
        {
            if (Password_InputDigit(evt)) RefreshStars();
        }
        else if (evt == KEY_CONFIRM)
        {
            if (Password_GetInputLen() == 6)
            {
                /* 保存第一次输入的新密码 */
                const uint8_t* input = Password_GetInputArray();
                for (uint8_t i = 0; i < 6; i++)
                {
                    s_newPwd[i] = input[i];
                }
                
                Password_Reset();
                OLED_Clear();
                OLED_ShowString(0, 0, "Confirm:", OLED_8X16);
                OLED_Update();
                s_tick = GetTick();
                s_phase = CFG_PHASE_CONFIRM;
            }
            /* 未满6位时忽略，等待用户输满 */
        }
        else if (evt == KEY_BACK)
        {
            if (Password_GetInputLen() == 0)
            {
                Buzzer_Backspace();
                ExitConfig();
            }
            else
            {
                Password_Remove();
                Buzzer_Backspace();
                RefreshStars();
            }
        }
        return;
    }
    
    /*---------- 第3步：确认新密码 ----------*/
    if (s_phase == CFG_PHASE_CONFIRM)
    {
        if (evt <= 9)
        {
            if (Password_InputDigit(evt)) RefreshStars();
        }
        else if (evt == KEY_CONFIRM)
        {
            if (Password_GetInputLen() == 6)
            {
                const uint8_t* input = Password_GetInputArray();
                uint8_t match = 1;
                
                /* 逐位对比两次输入 */
                for (uint8_t i = 0; i < 6; i++)
                {
                    if (input[i] != s_newPwd[i])
                    {
                        match = 0;
                        break;
                    }
                }
                
                if (match)
                {
                    /* 两次一致：写入Flash并生效 */
                    Storage_WritePassword(s_newPwd);
                    Password_SetPassword(s_newPwd);
                    Buzzer_Unlock();  /* 用开锁成功音表示保存成功 */
                    OLED_Clear();
                    OLED_ShowString(0, 0, "Saved", OLED_8X16);
                    OLED_Update();
                }
                else
                {
                    /* 两次不一致 */
                    Buzzer_Error();
                    OLED_Clear();
                    OLED_ShowString(0, 0, "Mismatch", OLED_8X16);
                    OLED_Update();
                }
                
                Delay_ms(800);
                ExitConfig();
            }
        }
        else if (evt == KEY_BACK)
        {
            if (Password_GetInputLen() == 0)
            {
                Buzzer_Backspace();
                ExitConfig();
            }
            else
            {
                Password_Remove();
                Buzzer_Backspace();
                RefreshStars();
            }
        }
        return;
    }
}
