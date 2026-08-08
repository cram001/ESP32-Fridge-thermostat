/*!
 * @file DFRobot_VisualRotaryEncoder.cpp
 * @brief Local SEN0502 driver based on DFRobot V1.0.1.
 *
 * Original implementation copyright (c) DFRobot Co.Ltd, MIT License.
 */
#include "DFRobot_VisualRotaryEncoder.h"

namespace {
constexpr uint32_t kBusRecoveryMinimumIntervalMs = 1000;
constexpr uint32_t kBusRecoverySettleMs = 5;
}

DFRobot_VisualRotaryEncoder::DFRobot_VisualRotaryEncoder() {}

int DFRobot_VisualRotaryEncoder::begin(void) {
  uint8_t idBuf[2] = {0};
  if (readReg(VISUAL_ROTARY_ENCODER_PID_MSB_REG, idBuf, sizeof(idBuf)) !=
      sizeof(idBuf)) {
    return ERR_DATA_BUS;
  }

  const uint16_t pid =
      (static_cast<uint16_t>(idBuf[0]) << 8) | idBuf[1];
  if (pid != VISUAL_ROTARY_ENCODER_PID) return ERR_IC_VERSION;

  delay(200);
  return NO_ERR;
}

void DFRobot_VisualRotaryEncoder::refreshBasicInfo(void) {
  uint8_t tempBuf[8] = {0};
  if (readReg(VISUAL_ROTARY_ENCODER_PID_MSB_REG, tempBuf,
              sizeof(tempBuf)) != sizeof(tempBuf)) {
    return;
  }

  basicInfo.PID =
      (static_cast<uint16_t>(tempBuf[0]) << 8) | tempBuf[1];
  basicInfo.VID =
      (static_cast<uint16_t>(tempBuf[2]) << 8) | tempBuf[3];
  basicInfo.version =
      (static_cast<uint16_t>(tempBuf[4]) << 8) | tempBuf[5];
  basicInfo.i2cAddr = tempBuf[7];
}

uint16_t DFRobot_VisualRotaryEncoder::getEncoderValue(void) {
  uint8_t countValue[2] = {0};
  if (readReg(VISUAL_ROTARY_ENCODER_COUNT_MSB_REG, countValue,
              sizeof(countValue)) != sizeof(countValue)) {
    return hasLastEncoderValue_ ? lastEncoderValue_ : 0;
  }

  lastEncoderValue_ =
      (static_cast<uint16_t>(countValue[0]) << 8) | countValue[1];
  hasLastEncoderValue_ = true;
  return lastEncoderValue_;
}

void DFRobot_VisualRotaryEncoder::setEncoderValue(uint16_t value) {
  if (value > 0x3FF) return;

  const uint8_t tempBuf[2] = {
      static_cast<uint8_t>((value & 0xFF00) >> 8),
      static_cast<uint8_t>(value & 0x00FF)};
  writeReg(VISUAL_ROTARY_ENCODER_COUNT_MSB_REG, tempBuf, sizeof(tempBuf));
  lastEncoderValue_ = value;
  hasLastEncoderValue_ = true;
}

uint8_t DFRobot_VisualRotaryEncoder::getGainCoefficient(void) {
  uint8_t rotateGain = 0;
  if (readReg(VISUAL_ROTARY_ENCODER_GAIN_REG, &rotateGain,
              sizeof(rotateGain)) != sizeof(rotateGain)) {
    return 0;
  }
  return rotateGain;
}

void DFRobot_VisualRotaryEncoder::setGainCoefficient(uint8_t gainValue) {
  if (gainValue < 0x01 || gainValue > 0x33) return;
  writeReg(VISUAL_ROTARY_ENCODER_GAIN_REG, &gainValue, sizeof(gainValue));
}

bool DFRobot_VisualRotaryEncoder::detectButtonDown(void) {
  uint8_t buttonStatus = 0;
  if (readReg(VISUAL_ROTARY_ENCODER_KEY_STATUS_REG, &buttonStatus,
              sizeof(buttonStatus)) != sizeof(buttonStatus)) {
    return false;
  }

  if ((buttonStatus & 0x01) == 0) return false;

  const uint8_t clearStatus = 0x00;
  writeReg(VISUAL_ROTARY_ENCODER_KEY_STATUS_REG, &clearStatus,
           sizeof(clearStatus));
  return true;
}

DFRobot_VisualRotaryEncoder_I2C::DFRobot_VisualRotaryEncoder_I2C(
    uint8_t i2cAddr, TwoWire* pWire)
    : _pWire(pWire), _deviceAddr(i2cAddr) {}

int DFRobot_VisualRotaryEncoder_I2C::begin(void) {
  _pWire->begin();
  delay(50);
  return DFRobot_VisualRotaryEncoder::begin();
}

bool DFRobot_VisualRotaryEncoder_I2C::writeRegOnce(
    uint8_t reg, const void* pBuf, size_t size) {
  if (pBuf == nullptr) return false;

  const uint8_t* bytes = static_cast<const uint8_t*>(pBuf);
  _pWire->beginTransmission(_deviceAddr);
  if (_pWire->write(reg) != 1) return false;
  for (size_t i = 0; i < size; ++i) {
    if (_pWire->write(bytes[i]) != 1) return false;
  }
  return _pWire->endTransmission() == 0;
}

size_t DFRobot_VisualRotaryEncoder_I2C::readRegOnce(
    uint8_t reg, void* pBuf, size_t size) {
  if (pBuf == nullptr || size == 0) return 0;

  uint8_t* bytes = static_cast<uint8_t*>(pBuf);
  _pWire->beginTransmission(_deviceAddr);
  if (_pWire->write(reg) != 1 || _pWire->endTransmission() != 0) return 0;

  const size_t requested =
      _pWire->requestFrom(_deviceAddr, static_cast<uint8_t>(size));
  if (requested != size) {
    while (_pWire->available()) _pWire->read();
    return 0;
  }

  size_t count = 0;
  while (_pWire->available() && count < size) {
    bytes[count++] = _pWire->read();
  }
  return count == size ? count : 0;
}

bool DFRobot_VisualRotaryEncoder_I2C::recoverBus() {
  const uint32_t now = millis();
  if (lastRecoveryMs_ != 0 &&
      now - lastRecoveryMs_ < kBusRecoveryMinimumIntervalMs) {
    return false;
  }
  lastRecoveryMs_ = now == 0 ? 1 : now;

  _pWire->end();
  delay(2);
  _pWire->begin();
  delay(kBusRecoverySettleMs);
  return true;
}

void DFRobot_VisualRotaryEncoder_I2C::writeReg(
    uint8_t reg, const void* pBuf, size_t size) {
  if (writeRegOnce(reg, pBuf, size)) return;
  if (!recoverBus()) return;
  writeRegOnce(reg, pBuf, size);
}

size_t DFRobot_VisualRotaryEncoder_I2C::readReg(
    uint8_t reg, void* pBuf, size_t size) {
  const size_t first = readRegOnce(reg, pBuf, size);
  if (first == size) return first;
  if (!recoverBus()) return 0;
  return readRegOnce(reg, pBuf, size);
}
