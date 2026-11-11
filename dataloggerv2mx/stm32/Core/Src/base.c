#include "base.h"

void intToAscii(uint64_t val, char* buf, uint8_t size){
    for (; size>0; --size){
        buf[size-1] = (val % 10) + '0'; val /= 10;
    }
}
