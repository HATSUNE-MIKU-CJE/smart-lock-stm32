/**
  ******************************************************************************
  * @file    Buzzer.h
  * @brief   蜂鸣器驱动对外接口
  *          低电平导通（响），高电平截止（停）
  ******************************************************************************
  */

#ifndef __Buzzer_H
#define __Buzzer_H

void Buzzer_Init(void);      /**< 蜂鸣器初始化（PB10推挽输出，默认不响） */
void Buzzer_ON(void);        /**< 蜂鸣器响 */
void Buzzer_OFF(void);       /**< 蜂鸣器停 */
void Buzzer_Turn(void);      /**< 蜂鸣器状态翻转 */

void Buzzer_KeyPress(void);  /**< 按键反馈音（短叫一声） */
void Buzzer_Unlock(void);    /**< 开锁成功提示音（短-长-短 三声） */
void Buzzer_Error(void);     /**< 密码错误提示音（短叫两声） */
void Buzzer_Lock(void);      /**< 关锁完成提示音（短-短-长 三声） */
void Buzzer_Backspace(void);  /**< 退格提示音（中鸣一声） */

#endif
