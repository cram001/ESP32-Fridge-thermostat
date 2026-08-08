/*!
 * @file DFRobot_VisualRotaryEncoder.h
 * @brief Local SEN0502 driver based on DFRobot V1.0.1.
 *
 * Upstream: https://github.com/DFRobot/DFRobot_VisualRotaryEncoder
 * Original copyright (c) DFRobot Co.Ltd, MIT License.
 *
 * Local reliability changes:
 * - retry failed I2C register transactions after restarting the Wire bus;
 * - rate-limit bus restarts so a disconnected encoder cannot churn the bus;
 * - preserve the last valid encoder count across a transient failed read.
 */
#ifndef __DFROBOT_VISUAL_ROTARY_ENCODER_H__
#define __DFROBOT_VISUAL_ROTARY_ENCODER_H__

#include <Arduino.h>
#include <Wire.h>

#define VISUAL_ROTARY_ENCODER_DEFAULT_I2C_ADDR uint8_t(0x54)
#define VISUAL_ROTARY_ENCODER_PID uint16_t(0x01F6)

#define VISUAL_ROTARY_ENCODER_PID_MSB_REG uint8_t(0x00)
#define VISUAL_ROTARY_ENCODER_PID_LSB_REG uint8_t(0x01)
#define VISUAL_ROTARY_ENCODER_VID_MSB_REG uint8_t(0x02)
#define VISUAL_ROTARY_ENCODER_VID_LSB_REG uint8_t(0x03)
#define VISUAL_ROTARY_ENCODER_VERSION_MSB_REG uint8_t(0x04)
#define VISUAL_ROTARY_ENCODER_VERSION_LSB_REG uint8_t(0x05)
#define VISUAL_ROTARY_ENCODER_ADDR_REG uint8_t(0x07)
#define VISUAL_ROTARY_ENCODER_COUNT_MSB_REG uint8_t(0x08)
#define VISUAL_ROTARY_ENCODER_COUNT_LSB_REG uint8_t(0x09)
#define VISUAL_ROTARY_ENCODER_KEY_STATUS_REG uint8_t(0x0A)
#define VISUAL_ROTARY_ENCODER_GAIN_REG uint8_t(0x0B)

class DFRobot_VisualRotaryEncoder {
 public:
  #define NO_ERR 0
  #define ERR_DATA_BUS (-1)
  #define ERR_IC_VERSION (-2)

  typedef struct {
    uint16_t PID;
    uint16_t VID;
    uint16_t version;
    uint8_t i2cAddr;
  } sBasicInfo_t;

  DFRobot_VisualRotaryEncoder();
  virtual int begin(void);
  void refreshBasicInfo(void);
  uint16_t getEncoderValue(void);
  void setEncoderValue(uint16_t value);
  uint8_t getGainCoefficient(void);
  void setGainCoefficient(uint8_t gainValue);
  bool detectButtonDown(void);

  sBasicInfo_t basicInfo;

 protected:
  virtual void writeReg(uint8_t reg, const void* pBuf, size_t size) = 0;
  virtual size_t readReg(uint8_t reg, void* pBuf, size_t size) = 0;

  uint16_t lastEncoderValue_ = 0;
  bool hasLastEncoderValue_ = false;
};

class DFRobot_VisualRotaryEncoder_I2C : public DFRobot_VisualRotaryEncoder {
 public:
  DFRobot_VisualRotaryEncoder_I2C(
      uint8_t i2cAddr = VISUAL_ROTARY_ENCODER_DEFAULT_I2C_ADDR,
      TwoWire* pWire = &Wire);

  virtual int begin(void);

 protected:
  virtual void writeReg(uint8_t reg, const void* pBuf, size_t size);
  virtual size_t readReg(uint8_t reg, void* pBuf, size_t size);

 private:
  bool writeRegOnce(uint8_t reg, const void* pBuf, size_t size);
  size_t readRegOnce(uint8_t reg, void* pBuf, size_t size);
  bool recoverBus();

  TwoWire* _pWire;
  uint8_t _deviceAddr;
  uint32_t lastRecoveryMs_ = 0;
};

#endif
