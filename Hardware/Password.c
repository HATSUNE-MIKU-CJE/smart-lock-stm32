/**
  ******************************************************************************
  * @file    Password.c
  * @brief   密码管理模块（RAM版本，固定密码）
  *          密码格式：6位数字，固定为 1-2-3-4-5-6
  *          输入方式：逐位录入，支持退格删除最后一位
  *          验证方式：逐位数组比较，6位全对则通过
  *          后续扩展：可接入Flash实现掉电保存、支持修改密码
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Password.h"

/*================== 私有数据（外部不可直接访问） ==================*/

/**
  * @brief  固定密码数组
  * @note   当前为硬编码的6位密码：123456
  *         后续若支持修改密码，可从Flash读取或提供设置接口
  */
static uint8_t s_password[6];

/**
  * @brief  用户输入缓存数组
  * @note   记录当前已输入的数字，最大容量6位
  */
static uint8_t s_input[6];

/**
  * @brief  当前已输入的位数计数器
  * @note   范围 0~6，0表示尚未输入，6表示输满
  */
static uint8_t s_len = 0;

/*================== 对外接口函数 ==================*/

/**
  * @brief  密码模块初始化
  * @note   清空输入缓存，将输入长度归零
  *         应在主函数初始化阶段调用一次
  */
void Password_Init(void)
{
    s_len = 0;
}

/**
  * @brief  录入一位数字
  * @param  digit 输入的数字，范围 0~9
  * @retval 1：录入成功
  * @retval 0：录入失败（已满6位，超出不录入）
  * @note   将数字存入 s_input[s_len] 位置，然后 s_len++
  */
uint8_t Password_InputDigit(uint8_t digit)
{
    if (s_len >= 6)
    {
        return 0;  // 已满6位，拒绝录入
    }
    
    s_input[s_len] = digit;  // 存入当前位置
    s_len++;                 // 长度+1
    return 1;
}

/**
  * @brief  验证当前输入的密码是否正确
  * @retval 1：密码正确（6位全对）
  * @retval 0：密码错误，或未满6位
  * @note   先判断长度是否为6，再逐位比较 s_input 和 s_password
  */
uint8_t Password_Check(void)
{
    if (s_len != 6)
    {
        return 0;  // 未满6位，直接判错
    }
    
    // 逐位比较：6位全对才返回正确
    for (uint8_t i = 0; i < 6; i++)
    {
        if (s_input[i] != s_password[i])
        {
            return 0;  // 发现某一位不匹配
        }
    }
    
    return 1;  // 6位全部匹配，密码正确
}

/**
  * @brief  清空当前输入缓存
  * @note   将 s_len 清0，缓存中的旧数据会被下次输入覆盖
  *         密码错误后或开锁成功后调用，准备下一轮输入
  */
void Password_Reset(void)
{
    s_len = 0;
}

/**
  * @brief  获取当前已输入的密码长度
  * @retval 当前输入位数，范围 0~6
  * @note   主函数根据此值决定显示几个星号
  */
uint8_t Password_GetInputLen(void)
{
    return s_len;
}

/**
  * @brief  退格删除最后一位输入
  * @note   将 s_len 减1，逻辑上删除最后一位数字
  *         调用前需确保 s_len > 0，否则无效果
  *         被删除位置的旧数据不用清理，下次输入会直接覆盖
  */
void Password_Remove(void)
{
    if (s_len > 0)
    {
        s_len--;  // 长度减1，最后一位被逻辑删除
    }
}

void Password_SetPassword(const uint8_t* pwd)
{
  for (uint8_t i=0;i<6;i++)
  {
    s_password[i]=pwd[i];
  }
}

const uint8_t* Password_GetPassword(void)
{
  return s_password;
}

/**
  * @brief  获取当前输入缓存数组指针
  * @retval 指向 s_input 数组的常量指针（有效长度由 Password_GetInputLen 决定）
  * @note   主要用于配置模式：输满6位后，主函数通过此接口读取输入的密码
  */
const uint8_t* Password_GetInputArray(void)
{
    return s_input;
}
