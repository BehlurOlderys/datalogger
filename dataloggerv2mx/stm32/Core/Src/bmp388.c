#include "bmp388.h"
#include "mh_i2c.h"
#include "base.h"

#define BMP388_ADDR            0xEC

#define BMP388_REG_COEFF       0x31
#define BMP388_REG_PRESS_DATA  0x04
#define BMP388_REG_CHIP_ID     0x00
#define BMP388_REG_PWR_CTRL    0x1B

#define BMP388_EXPECTED_ID     0x50
#define BMP388_VAL_PT          0x33

static int64_t calibrationTLin;

static int32_t compensateTemperature(BMP388_Calib_t const* calibration, uint32_t raw_temp)
{

    int64_t t64 = (int64_t)raw_temp;
    int64_t A64 = (int64_t)(calibration->T1);
    int64_t B64 = (int64_t)(calibration->T2);
    int64_t C64 = (int64_t)(calibration->T3);

    int64_t sum_q22 = ((t64 * B64) >> 8)
                      - (A64 * B64)
                      + ((t64 * t64 * C64) >> 26)
                      - ((C64 * A64 * t64) >> 17)
                      + ((A64 * A64 * C64) >> 10);

    calibrationTLin = sum_q22 >> 6;
    return (int32_t)((sum_q22 * 100) >> 22);
}

static uint32_t compensatePressure(BMP388_Calib_t const* calibration, uint32_t rawPressure)
{
    int64_t partial_data1;
    int64_t partial_data2;
    int64_t partial_data3;
    int64_t partial_data4;
    int64_t partial_data5;
    int64_t partial_data6;
    int64_t offset;
    int64_t sensitivity;

    partial_data1 = (int64_t)(calibrationTLin * calibrationTLin);
    partial_data2 = (int64_t)(partial_data1 / 64);
    partial_data3 = (int64_t)((partial_data2 * calibrationTLin) / 256);
    partial_data4 = (int64_t)((calibration->P8 * partial_data3) / 32);
    partial_data5 = (int64_t)((calibration->P7 * partial_data1) * 16);
    partial_data6 = (int64_t)((calibration->P6 * calibrationTLin) * 4194304);
    offset = (int64_t)((calibration->P5 * 140737488355328) + partial_data4 + partial_data5 + partial_data6);
    partial_data2 = (int64_t)((calibration->P4 * partial_data3) / 32);
    partial_data4 = (int64_t)((calibration->P3 * partial_data1) * 4);
    partial_data5 = (int64_t)((calibration->P2 - (int32_t)16384) * calibrationTLin * 2097152);
    sensitivity =
        (int64_t)(((calibration->P1 - (int32_t)16384) * 70368744177664) + partial_data2 + partial_data4 +
                  partial_data5);
    partial_data1 = (int64_t)((sensitivity / 16777216) * rawPressure);
    partial_data2 = (int64_t)(calibration->P10 * calibrationTLin);
    partial_data3 = (int64_t)(partial_data2 + ((int32_t)65536 * calibration->P9));
    partial_data4 = (int64_t)((partial_data3 * rawPressure) / (int32_t)8192);

    /* dividing by 10 followed by multiplying by 10
     * To avoid overflow caused by (uncomp_data->pressure * partial_data4)
     */
    partial_data5 = (int64_t)((rawPressure * (partial_data4 / 10)) / (int32_t)512);
    partial_data5 = (int64_t)(partial_data5 * 10);
    partial_data6 = (int64_t)(rawPressure * rawPressure);
    partial_data2 = (int64_t)((calibration->P11 * partial_data6) / (int32_t)65536);
    partial_data3 = (int64_t)((int64_t)(partial_data2 * rawPressure) / 128);
    partial_data4 = (int64_t)((offset / 4) + partial_data1 + partial_data5 + partial_data3);
    return (uint32_t)(((uint64_t)partial_data4 * 25) / (uint64_t)1099511627776);
//
//    int64_t pd1, pd2, pd3, pd4, pd5, pd6;
//    int64_t t_lin = calibrationTLin; // Podmieniona nazwa zmiennej
//
//    // 1. Obliczenie wpływu temperatury na czujnik (wielomian par_p9, p10, p11)
//    pd1 = (t_lin * t_lin) >> 16;
//    pd2 = (pd1 * (int64_t)calibration->P11) >> 16;
//    pd3 = (t_lin * (int64_t)calibration->P10) >> 12;
//    pd4 = ((int64_t)calibration->P9 << 18) + pd2 + pd3;
//
//    // 2. Korekcja nieliniowości (par_p5, p6, p7, p8)
//    pd1 = (t_lin * t_lin) >> 16;
//    pd2 = (pd1 * (int64_t)calibration->P8) >> 16;
//    pd3 = (t_lin * (int64_t)calibration->P7) >> 12;
//    pd5 = ((int64_t)calibration->P6 << 16) + pd2 + pd3;
//    pd6 = ((pd5 * t_lin) >> 16) + ((int64_t)calibration->P5 << 19);
//
//    // 3. Skalowanie bazy czujnika (par_p1, p2, p3, p4)
//    pd1 = (t_lin * (int64_t)calibration->P3) >> 12;
//    pd2 = (pd1 + ((int64_t)calibration->P2 << 13)) * t_lin;
//    pd3 = ((int64_t)calibration->P1 << 15) + (pd2 >> 20) + (((int64_t)calibration->P4 * pd1) >> 16);
//
//    // 4. Kompensacja surowego odczytu ciśnienia
//    pd1 = ((int64_t)rawPressure * (pd4 >> 10)) >> 14;
//    pd2 = ((int64_t)rawPressure * (int64_t)rawPressure) >> 16;
//    pd5 = (pd2 * (pd6 >> 12)) >> 16;
//
//    int64_t denominator = pd3 + pd1;
//    if (denominator == 0) return 0;
//
//    // Zastąpienie dzielenia przez zmienną szybkim mnożeniem przez odwrotność (stała 2^40)
//    int64_t inv_denom = ((int64_t)1 << 40) / denominator;
//    int64_t final_press = (((int64_t)1 << 31) - pd5) * inv_denom;
//
//    // Wynik w Paskalach (Pa)
//    return (uint32_t)(final_press >> 9);
}


uint8_t bmp388Init(){
    uint8_t chipId = 0;
    uint8_t result = FAILURE;
    uint8_t data[1] = {BMP388_VAL_PT};
    readI2C(BMP388_ADDR, BMP388_REG_CHIP_ID, &chipId, 1);
    result = (chipId == BMP388_EXPECTED_ID) ? SUCCESS : FAILURE;
    if (result == SUCCESS)
    {
        writeI2C(BMP388_ADDR, BMP388_REG_PWR_CTRL, data, 1);
    }
    return result;
}

void bmp388ReadCoeffs(BMP388_Calib_t* calibration){
    uint8_t rawData[21];
    readI2C(BMP388_ADDR, BMP388_REG_COEFF, rawData, 21);

    calibration->T1 = (uint16_t)((rawData[1] << 8) | rawData[0]);
    calibration->T2 = (uint16_t)((rawData[3] << 8) | rawData[2]);
    calibration->T3 = (int8_t)rawData[4];
    calibration->P1 = (int16_t)((rawData[6] << 8) | rawData[5]);
    calibration->P2 = (int16_t)((rawData[8] << 8) | rawData[7]);
    calibration->P3 = (int8_t)rawData[9];
    calibration->P4 = (int8_t)rawData[10];
    calibration->P5 = (uint16_t)((rawData[12] << 8) | rawData[11]);
    calibration->P6 = (uint16_t)((rawData[14] << 8) | rawData[13]);
    calibration->P7 = (int8_t)rawData[15];
    calibration->P8 = (int8_t)rawData[16];
    calibration->P9 = (int16_t)((rawData[18] << 8) | rawData[17]);
    calibration->P10 = (int8_t)rawData[19];
    calibration->P11 = (int8_t)rawData[20];
}

void bmp388Measure(BMP388_Calib_t const* calibration, int32_t* pressure, int16_t* temp)
{
    uint8_t data[6] = {0};
    readI2C(BMP388_ADDR, BMP388_REG_PRESS_DATA, data, 6);

    uint32_t rawPressure = ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[0];
    uint32_t rawTemp = ((uint32_t)data[5] << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[3];
    *temp = compensateTemperature(calibration, rawTemp);
    *pressure = compensatePressure(calibration, rawPressure);
}
