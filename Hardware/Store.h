#ifndef __STORAGE_H
#define __STORAGE_H

#include <stdint.h>

/* Flash存储地址：最后一页起始（1KB页，64KB Flash的末页） */
#define FLASH_STORAGE_ADDR    0x0800FC00

/* 魔数：用于识别Flash是否已初始化 */
#define FLASH_MAGIC_NUM       0xA5A5A5A5

/* 
 * 存储结构体（12字节，紧凑排列，无填充）
 * 布局：magic(4) + pwd[6](6) + checksum(2) = 12字节 = 3个uint32_t
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;         /* 魔数，固定为 0xA5A5A5A5 */
    uint8_t  pwd[6];        /* 6位密码，每位0-9 */
    uint16_t checksum;      /* 校验和：magic + pwd 的累加和 */
} LockConfig_t;
#pragma pack(pop)

/* Flash存储初始化：上电时调用，从Flash读取密码 */
void Storage_Init(void);

/* 
 * 写入新密码到Flash
 * @param newPwd 指向6字节密码数组
 * @return 1成功，0失败（擦除或写入出错）
 */
uint8_t Storage_WritePassword(const uint8_t* newPwd);

#endif
