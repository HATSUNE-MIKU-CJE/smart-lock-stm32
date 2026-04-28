/**
  ******************************************************************************
  * @file    main.c
  * @brief   智能密码锁主程序入口
  *          当前阶段：密码输入 + 自动开锁 + 微动开关到位检测 + 延时自动关锁
  *          工作原理：
  *            1. 按确认键进入密码输入，输入6位数字后按确认验证
  *            2. 密码正确：电机启动正转，凸轮顶开锁舌
  *            3. 凸轮顶开锁舌，触发微动开关（到位检测），电机停止
  *            4. 等待5秒（保持开锁状态）
  *            5. 电机继续同向旋转，凸轮离开锁舌，弹簧自动弹回关锁
  *            6. 电机停止，回到待机状态
  *          蜂鸣器提示：
  *            - 每次按键：短叫一声
  *            - 密码正确开锁：短-长-短 三声
  *            - 密码错误：短叫两声
  *            - 关锁完成：短-短-长 三声
  *          后续将逐步加入Flash存储、指纹、语音等功能
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

/*================== 状态机状态定义 ==================*/

#define STATE_IDLE            0  /**< 待机：等待确认键进入密码输入 */
#define STATE_PASSWORD_INPUT  1  /**< 密码输入：接收6位数字，显示星号 */
#define STATE_UNLOCKING       2  /**< 开锁中：电机转动，检测微动开关到位 */
#define STATE_UNLOCKED        3  /**< 已开锁：到位停止，等待自动关锁延时 */
#define STATE_LOCKING         4  /**< 关锁中：电机继续旋转，凸轮离开锁舌 */

/*================== 全局变量 ==================*/

uint8_t g_state = STATE_IDLE;   /**< 当前状态机状态 */
uint32_t g_tick = 0;            /**< 时间戳记录，用于延时判断 */

/**
  * @brief  主函数
  * @note   初始化所有外设后进入主循环，状态机驱动整个开锁/关锁流程
  */
int main(void)
{
	Systick_Init();  // 启动TIM4的1ms中断，为GetTick()提供时基
	
	// 初始化所有外设
	Buzzer_Init();
	OLED_Init();
	Switch_Init();
	Key_Init();
	Motor_Init();
	Password_Init();
	
	// 开机画面
	OLED_Clear();
	OLED_ShowString(0, 0, "STM32 Lock", OLED_8X16);
	OLED_Update();
	Delay_s(1);
	OLED_Clear();
	OLED_Update();
	
	Motor_Off();  // 确保电机初始状态为停止
	
	// 主循环：状态机驱动
	while (1)
	{
		Key_Scan();
		uint8_t evt = Key_GetEvent();
		
		// 任意按键按下，先短鸣一声作为反馈（KEY_NONE时不响）
		if (evt != KEY_NONE)
		{
			Buzzer_KeyPress();
		}
		
		switch (g_state)
		{
			/*========== 待机状态：等待进入密码输入 ==========*/
			case STATE_IDLE:
				OLED_ShowString(0, 0, "Press CONFIRM", OLED_8X16);
				OLED_Update();
				
				if (evt == KEY_CONFIRM)
				{
					// 切换状态时清屏并显示密码输入标题，避免每轮都清屏导致星号闪烁
					OLED_Clear();
					OLED_ShowString(0, 0, "Password:", OLED_8X16);
					OLED_Update();
					
					Password_Reset();          // 清空上次输入缓存
					g_state = STATE_PASSWORD_INPUT;
				}
				break;
			
			/*========== 密码输入状态：接收6位数字 ==========*/
			case STATE_PASSWORD_INPUT:
				if (evt <= 9)  // 按了数字键0~9
				{
					if (Password_InputDigit(evt))  // 尝试录入数字
					{
						uint8_t len = Password_GetInputLen();
						
						// 只刷新星号区域（第二行），不清标题，避免闪烁
						OLED_ClearArea(0, 16, 128, 16);
						for (uint8_t i = 0; i < len; i++)
						{
							OLED_ShowString(i * 8, 16, "*", OLED_8X16);
						}
						OLED_Update();  // 6个星号画完后统一刷新一次
					}
					// 如果已满6位，Password_InputDigit返回0，不理睬（超出不录入）
				}
				else if (evt == KEY_CONFIRM)  // 按确认键，开始验证
				{
					if (Password_Check())  // 密码正确
					{
						Password_Reset();
						
						// 开锁成功提示音：短-长-短 三声
						Buzzer_Unlock();
						
						Motor_On();
						g_state = STATE_UNLOCKING;
					}
					else  // 密码错误
					{
						Password_Reset();
						
						// 密码错误提示音：短叫两声
						Buzzer_Error();
						
						OLED_Clear();
						OLED_ShowString(0, 0, "ERROR", OLED_8X16);
						OLED_Update();
						
						Delay_ms(500);
						
						// 重新显示输入界面，等待重新输入
						OLED_Clear();
						OLED_ShowString(0, 0, "Password:", OLED_8X16);
						OLED_Update();
					}
				}
				/*----- 返回键：双重功能复用 -----*/
				/*  有输入时（len > 0）：退格删除最后一位
				    无输入时（len == 0）：返回待机状态（放弃输入）
				    一键两用，减少物理按键数量 */
				else if (evt == KEY_BACK)
				{
					uint8_t len = Password_GetInputLen();
					
					if (len == 0)
					{
						// 当前无输入，返回键作用为"返回待机"
						Buzzer_Backspace();
						g_state = STATE_IDLE;
						break;
					}
					
					// 当前有输入，返回键作用为"退格"
					Password_Remove();  // 删除最后一位
					Buzzer_Backspace(); // 退格提示音
					
					// 刷新星号显示（少一个星号）
					OLED_ClearArea(0, 16, 128, 16);
					for (uint8_t i = 0; i < Password_GetInputLen(); i++)
					{
						OLED_ShowString(i * 8, 16, "*", OLED_8X16);
					}
					OLED_Update();
				}
				break;
			
			/*========== 开锁中：电机转动，检测微动开关到位 ==========*/
			case STATE_UNLOCKING:
				if (Switch_IsClosed())  // 微动开关确认闭合（凸轮已顶到位）
				{
					Motor_Off();             // 立刻停止电机，凸轮卡在锁舌上方保持开锁
					g_tick = GetTick();      // 记录到位时刻，用于后续5秒延时
					g_state = STATE_UNLOCKED;
					
					// OLED显示开锁成功
					OLED_Clear();
					OLED_ShowString(0, 0, "OPEN", OLED_8X16);
					OLED_Update();
				}
				break;
			
			/*========== 已开锁：等待自动关锁延时 ==========*/
			case STATE_UNLOCKED:
				if (GetTick() - g_tick > 5000)  // 5秒延时到
				{
					Motor_On();              // 电机继续同向旋转
					g_tick = GetTick();      // 记录关锁开始时刻
					g_state = STATE_LOCKING;
				}
				break;
			
			/*========== 关锁中：凸轮离开锁舌 ==========*/
			case STATE_LOCKING:
				if (GetTick() - g_tick > 500)  // 继续旋转500ms
				{
					Motor_Off();             // 停止电机，凸轮已离开，弹簧弹回锁舌
					g_state = STATE_IDLE;    // 回到待机状态
					
					// OLED显示关锁完成
					OLED_Clear();
					OLED_ShowString(0, 0, "CLOSED", OLED_8X16);
					OLED_Update();
					
					// 关锁完成提示音：短-短-长 三声
					Buzzer_Lock();
				}
				break;
		}
	}
}
