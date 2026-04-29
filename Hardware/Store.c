#include "stm32f10x.h"
#include "Store.h"
#include "Password.h"

static uint16_t Storage_CalcChecksum(const LockConfig_t* cfg)
{
    uint16_t sum=0;
    const uint8_t* p=(const uint8_t*)cfg;

    for (uint8_t i=0;i<10;i++)
    {
        sum+=p[i];
    }
    return sum;
}

uint8_t Storage_WritePassword(const uint8_t* newPwd)
{
    LockConfig_t cfg;
    uint32_t* pWord;
    uint8_t i;

    cfg.magic=FLASH_MAGIC_NUM;
    for (i=0;i<6;i++)
    {
        cfg.pwd[i]=newPwd[i];
    }
    cfg.checksum=Storage_CalcChecksum(&cfg);

    FLASH_Unlock();

    if (FLASH_ErasePage(FLASH_STORAGE_ADDR) != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return 0;
    }

    pWord = (uint32_t*)&cfg;
    for (i=0;i<3;i++)
    {
        if (FLASH_ProgramWord(FLASH_STORAGE_ADDR+i * 4,pWord[i])!=FLASH_COMPLETE){
            FLASH_Lock();
            return 0;
        }
    }

    FLASH_Lock();
    return 1;
}

void Storage_Init(void)
{
    LockConfig_t* cfg = (LockConfig_t*)FLASH_STORAGE_ADDR;

    if (cfg->magic ==  FLASH_MAGIC_NUM)
    {
        uint16_t calcSum=Storage_CalcChecksum(cfg);
        if (calcSum==cfg->checksum)
        {
            Password_SetPassword(cfg->pwd);
            return;
        }
    }

    uint8_t defaultPwd[6]={1,2,3,4,5,6};
    Password_SetPassword(defaultPwd);
    Storage_WritePassword(defaultPwd);
}
