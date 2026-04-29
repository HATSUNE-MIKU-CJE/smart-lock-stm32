/**
  ******************************************************************************
  * @file    Config.h
  * @brief   配置模式模块（修改密码）
  *          流程：待机按返回键进入 → 验证旧密码 → 输入新密码 → 再次确认 → 写入Flash
  *          对外只需调用 Config_Start() 启动，主循环判断 Config_IsActive() 后分发事件
  ******************************************************************************
  */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdint.h>

/** 启动配置模式（从待机状态调用） */
void Config_Start(void);

/** 判断当前是否处于配置模式中 */
uint8_t Config_IsActive(void);

/** 刷新配置模式的超时计时器（有按键操作时调用） */
void Config_ResetTimeout(void);

/**
  * @brief  处理配置模式下的按键事件
  * @param  evt 按键事件值（来自 Key_GetEvent）
  * @note   主循环检测到 Config_IsActive() 时调用
  *         内部自动处理超时检测、状态流转、Flash写入
  */
void Config_HandleEvent(uint8_t evt);

#endif
