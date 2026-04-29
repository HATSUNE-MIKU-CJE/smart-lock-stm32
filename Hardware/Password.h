/**
  ******************************************************************************
  * @file    Password.h
  * @brief   密码管理模块对外接口
  *          支持6位数字密码的录入、退格、验证和重置
  ******************************************************************************
  */

#ifndef __PASSWORD_H
#define __PASSWORD_H

#include <stdint.h>

void Password_Init(void);                   /**< 初始化密码模块（清空输入缓存） */
uint8_t Password_InputDigit(uint8_t digit); /**< 录入一位数字（0~9），满6位返回0 */
uint8_t Password_Check(void);               /**< 验证密码，正确返回1，错误返回0 */
void Password_Reset(void);                  /**< 清空当前输入缓存 */
uint8_t Password_GetInputLen(void);         /**< 获取当前输入长度（0~6） */
void Password_Remove(void);                 /**< 退格删除最后一位输入 */
void Password_SetPassword(const uint8_t* pwd);
const uint8_t* Password_GetPassword(void);
const uint8_t* Password_GetInputArray(void);  /**< 获取当前输入缓存数组指针（6字节），用于配置模式保存新密码 */

#endif
