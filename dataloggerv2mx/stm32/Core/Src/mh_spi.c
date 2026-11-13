#include "mh_spi.h"
#include "stm32c0xx_ll_spi.h"

uint8_t readAndWrite1bSPI(uint8_t data){
    while (!LL_SPI_IsActiveFlag_TXE(SPI1));
    LL_SPI_TransmitData8(SPI1, data);
    while (!LL_SPI_IsActiveFlag_RXNE(SPI1));
    return LL_SPI_ReceiveData8(SPI1);
}
