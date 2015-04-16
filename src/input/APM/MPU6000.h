#include "input/InertialManager.h"
#include "input/Sensor.h"
#include "input/SPIcontroller.h"
#include "util/byteConv.h"
#include <SPI.h>
#include "MPUregs.h"
//MPU6000 Accelerometer and Gyroscope on SPI
namespace{
    //this holds raw mpu data (accl x,y,z; temp; gyro x,y,z);
    union rawData{
        struct { uint8_t bytes[14]; };
        struct { int16_t accl[3], temp, gyro[3]; };
    };
}
class MPU6000 : public Sensor {
protected:
    static const uint8_t  APM26_CS_PIN    = 53;
    static const float    SAMPLE_RATE     = 200; //sample at 200Hz
    static const uint16_t CAL_SAMPLE_SIZE = 200; //for gyro calibration
                            //+- 2000 dps per least sig bit, in ms
    static const float dPlsb = 2.f*(2.f/65535.f);
    static const float GYRO_CONVERSION_FACT =  2.f*(2.f/65535.f) *PI/180.l;

    SPIcontroller spiControl;
    LTATune LTA;
    float   gCal[3];
    bool    writeTo(uint8_t addr, uint8_t msg);
    bool    writeTo(uint8_t addr, uint8_t len, uint8_t* msg);
    bool    readFrom(uint8_t addr, uint8_t len, uint8_t* data);
    rawData readSensors(); //optimized for just sensor data
public:
    //clock speed 8E6 instead of default(4E6) makes readSensors about 50% faster
    MPU6000()
        : spiControl(APM26_CS_PIN, SPISettings(8E6, MSBFIRST, SPI_MODE0)) {}
    MPU6000(uint8_t chip_select)
        : spiControl(chip_select , SPISettings(8E6, MSBFIRST, SPI_MODE0)) {}
    void init();
    void stop();
    bool status();
    void calibrate();
    void update(InertialManager& man);
    //end of sensor interface
    void getSensors(int16_t* accl, int16_t* gyro);//arrays of 3 int16_t's
    void tuneAccl(LTATune t);
};
rawData
MPU6000::readSensors(){
    //Note: its faster to read and ignore temp than make two transfers
    //Note: this is unrolled for efficiency
    rawData data;
    spiControl.capture();
    SPI.transfer(REG_DATA_START | 0x80); //last bit set to specify a read
    //the order in memory is accl x,y,z; temp; gyro x,y,z
    data.bytes[1]  = SPI.transfer(0); data.bytes[0]  = SPI.transfer(0);
    data.bytes[3]  = SPI.transfer(0); data.bytes[2]  = SPI.transfer(0);
    data.bytes[5]  = SPI.transfer(0); data.bytes[4]  = SPI.transfer(0);
    data.bytes[7]  = SPI.transfer(0); data.bytes[6]  = SPI.transfer(0);
    data.bytes[9]  = SPI.transfer(0); data.bytes[8]  = SPI.transfer(0);
    data.bytes[11] = SPI.transfer(0); data.bytes[10] = SPI.transfer(0);
    data.bytes[13] = SPI.transfer(0); data.bytes[12] = SPI.transfer(0);
    spiControl.release();
    return data;
}
bool
MPU6000::readFrom(uint8_t addr, uint8_t len, uint8_t* data){
    spiControl.capture();
    SPI.transfer(addr | 0x80); //last bit set to specify a read
    for(int i=0; i<len; i++) data[i] = SPI.transfer(0);
    return spiControl.release();
}
bool
MPU6000::writeTo(uint8_t addr, uint8_t len, uint8_t* msg){
    spiControl.capture();
    SPI.transfer(addr & ~0x80); //clear last bit to specify a write
    for(int i=0; i<len; i++) SPI.transfer(msg[i]);
    return spiControl.release();
}
bool
MPU6000::writeTo(uint8_t addr, uint8_t msg){
    writeTo(addr, 1, &msg);
}
// ---- public functions below -------------------------------------------------
void
MPU6000::tuneAccl(LTATune t){
    LTA = t;
}
void
MPU6000::init(){
    //rewrite all of this
    pinMode(40, OUTPUT);
    digitalWrite(40, HIGH); //Turn off barometer SPI line

    writeTo(REG_PWR_MGMT_1  , BIT_H_RESET); //chip reset
    delay(100);
    writeTo(REG_PWR_MGMT_1  , MPU_CLK_SEL_PLLGYROZ); //set GyroZ clock
    writeTo(REG_USER_CTRL   , BIT_I2C_DIS); //Disable I2C as recommended on datasheet
    writeTo(REG_SMPLRT_DIV  , ((1000/SAMPLE_RATE)-1) ); // Set Sample rate; 1khz/(value+1) = (rate)Hz
    writeTo(REG_CONFIG      , BITS_DLPF_CFG_188HZ); //set low pass filter to 188hz
    writeTo(REG_GYRO_CONFIG , BITS_FS_2000DPS); //Gyro scale 1000º/s
    writeTo(REG_ACCEL_CONFIG, 0x08); //Accel scale 4g
}
void
MPU6000::stop(){
}
bool
MPU6000::status(){
    return STATUS_OK;
}
void
MPU6000::calibrate(){
    //calibrate gyro
}
void
MPU6000::update(InertialManager& man){
    rawData data = readSensors();
    float accl[3];
    float gyro[3];
    for(int i=0; i<3; i++){
        accl[i] = (((float)data.accl[i])*LTA.values.scalar[i]) + LTA.values.scalar[i];
        gyro[i] = (((float)data.gyro[i])*GYRO_CONVERSION_FACT) + gCal[i];
    }
    man.updateRotRates(gyro[0], gyro[1], gyro[2]);
    man.updateLinAccel(accl[0], accl[1], accl[2]);
}
void
MPU6000::getSensors(int16_t* accl, int16_t* gyro){//arrays of 3 int16_t's
    rawData data = readSensors();
    for (int i = 0; i < 3; i++){
        accl[i] = data.accl[i];
        gyro[i] = data.gyro[i];
    }
}
