#include "base.h"
#include "stm32c0xx.h"

void uintToAscii(uint64_t val, char* buf, uint8_t size){
    for (; size>0; --size){
        buf[size-1] = (val % 10) + '0';
        val /= 10;
    }
}
void intToAscii(int16_t val, char* buf, uint8_t size) {
    uint32_t uval;
    if (val < 0) {
        buf[0] = '-';
        uval = (uint32_t)(-val);
    } else {
        buf[0] = '+';
        uval = (uint32_t)val;
    }
    for (; size > 1; --size) {
        buf[size - 1] = (uval % 10) + '0';
        uval /= 10;
    }
}

void delay1ms(void) {
    SysTick->LOAD = 12000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    SysTick->CTRL = 0;
}
