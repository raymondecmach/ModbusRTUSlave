#include "ModbusRTUSlave.h"

ModbusRTUSlave::ModbusRTUSlave(HardwareSerial& serial, uint8_t dePin) {
  _hardwareSerial = &serial;
   #ifdef __AVR__
  _softwareSerial = 0;
   #endif
   #ifdef HAVE_CDCSERIAL
  _usbSerial = 0;
   #endif
  _serial = &serial;
  _dePin = dePin;
}

#ifdef __AVR__
ModbusRTUSlave::ModbusRTUSlave(SoftwareSerial& serial, uint8_t dePin) {
  _hardwareSerial = 0;
  _softwareSerial = &serial;
  #ifdef HAVE_CDCSERIAL
  _usbSerial = 0;
  #endif
  _serial = &serial;
  _dePin = dePin;
}
#endif

#ifdef HAVE_CDCSERIAL
ModbusRTUSlave::ModbusRTUSlave(Serial_& serial, uint8_t dePin) {
  _hardwareSerial = 0;
  #ifdef __AVR__
  _softwareSerial = 0;
  #endif
  _usbSerial = &serial;
  _serial = &serial;
  _dePin = dePin;
}
#endif

void ModbusRTUSlave::configureCoils(bool coils[], uint16_t numCoils) {
  _coils = coils;
  _numCoils = numCoils;
}

void ModbusRTUSlave::configureDiscreteInputs(bool discreteInputs[], uint16_t numDiscreteInputs) {
  _discreteInputs = discreteInputs;
  _numDiscreteInputs = numDiscreteInputs;
}

void ModbusRTUSlave::configureHoldingRegisters(uint16_t holdingRegisters[], uint16_t numHoldingRegisters) {
  _holdingRegisters = holdingRegisters;
  _numHoldingRegisters = numHoldingRegisters;
}

void ModbusRTUSlave::configureInputRegisters(uint16_t inputRegisters[], uint16_t numInputRegisters) {
  _inputRegisters = inputRegisters;
  _numInputRegisters = numInputRegisters;
}

void ModbusRTUSlave::begin(uint8_t id, uint32_t baud, uint8_t config) {
  if (id >= 1 && id <= 247) _id = id;
  else _id = NO_ID;
  if (_hardwareSerial) {
    _calculateTimeouts(baud, config);
    _hardwareSerial->begin(baud, config);
  }
  #ifdef __AVR__
  else if (_softwareSerial) {
    _calculateTimeouts(baud, SERIAL_8N1);
    _softwareSerial->begin(baud);
  }
  #endif
  #ifdef HAVE_CDCSERIAL
  else if (_usbSerial) {
    _calculateTimeouts(baud, config);
    _usbSerial->begin(baud, config);
    while (!_usbSerial);
  }
  #endif
  if (_dePin != NO_DE_PIN) {
    pinMode(_dePin, OUTPUT);
    digitalWrite(_dePin, LOW);
  }
  _clearRxBuffer();
}

void ModbusRTUSlave::poll() {
  if (_serial->available()) {
    if (_readRequest()) {
      switch (_buf[1]) {
        case 1:
          _processReadCoils();
          break;
        case 2:
          _processReadDiscreteInputs();
          break;
        case 3:
          _processReadHoldingRegisters();
          break;
        case 4:
          _processReadInputRegisters();
          break;
        case 5:
          _processWriteSingleCoil();
          break;
        case 6:
          _processWriteSingleHoldingRegister();
          break;
        case 15:
          _processWriteMultipleCoils();
          break;
        case 16:
          _processWriteMultipleHoldingRegisters();
          break;
        default:
          _exceptionResponse(1);
          break;
      }
    }
  }
}

void ModbusRTUSlave::_processReadCoils() {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (quantity == 0 || quantity > ((MODBUS_RTU_SLAVE_BUF_SIZE - 6) * 8)) _exceptionResponse(3);
  else if ((startAddress + quantity) > _numCoils) _exceptionResponse(2);
  else if (!_coils) _exceptionResponse(4);
  else {
    _buf[2] = _div8RndUp(quantity);
    for (uint16_t i = 0; i < quantity; i++) {
      bitWrite(_buf[3 + (i >> 3)], i & 7, _coils[startAddress + i]);
    }
    _writeResponse(3 + _buf[2]);
  }
}

void ModbusRTUSlave::_processReadDiscreteInputs() {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (quantity == 0 || quantity > ((MODBUS_RTU_SLAVE_BUF_SIZE - 6) * 8)) _exceptionResponse(3);
  else if ((startAddress + quantity) > _numDiscreteInputs) _exceptionResponse(2);
  else if (!_discreteInputs) _exceptionResponse(4);
  else {
    _buf[2] = _div8RndUp(quantity);
    for (uint8_t i = 0; i < quantity; i++) {
      bitWrite(_buf[3 + (i >> 3)], i & 7, _discreteInputs[startAddress + i]);
    }
    _writeResponse(3 + _buf[2]);
  }
}

void ModbusRTUSlave::_processReadHoldingRegisters() {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (quantity == 0 || quantity > ((MODBUS_RTU_SLAVE_BUF_SIZE - 6) >> 1)) _exceptionResponse(3);
  else if ((startAddress + quantity) > _numHoldingRegisters) _exceptionResponse(2);
  else if (!_holdingRegisters) _exceptionResponse(4);
  else {
    _buf[2] = quantity * 2;
    for (uint8_t i = 0; i < quantity; i++) {
      _buf[3 + (i * 2)] = highByte(_holdingRegisters[startAddress + i]);
      _buf[4 + (i * 2)] = lowByte(_holdingRegisters[startAddress + i]);
    }
    _writeResponse(3 + _buf[2]);
  }
}

void ModbusRTUSlave::_processReadInputRegisters() {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (quantity == 0 || quantity > ((MODBUS_RTU_SLAVE_BUF_SIZE - 6) >> 1)) _exceptionResponse(3);
  else if ((startAddress + quantity) > _numInputRegisters) _exceptionResponse(2);
  else if (!_inputRegisters) _exceptionResponse(4);
  else {
    _buf[2] = quantity * 2;
    for (uint8_t i = 0; i < quantity; i++) {
      _buf[3 + (i * 2)] = highByte(_inputRegisters[startAddress + i]);
      _buf[4 + (i * 2)] = lowByte(_inputRegisters[startAddress + i]);
    }
    _writeResponse(3 + _buf[2]);
  }
}

void ModbusRTUSlave::_processWriteSingleCoil() {
  uint16_t address = _bytesToWord(_buf[2], _buf[3]);
  uint16_t value = _bytesToWord(_buf[4], _buf[5]);
  if (value != 0 && value != 0xFF00) _exceptionResponse(3);
  else if (address >= _numCoils) _exceptionResponse(2);
  else if (!_coils) _exceptionResponse(4);
  else {
    _coils[address] = value;
    _writeResponse(6);
  }
}

void ModbusRTUSlave::_processWriteSingleHoldingRegister() {
  uint16_t address = _bytesToWord(_buf[2], _buf[3]);
  uint16_t value = _bytesToWord(_buf[4], _buf[5]);
  if (address >= _numHoldingRegisters) _exceptionResponse(2);
  else if (!_holdingRegisters) _exceptionResponse(4);
  else {
    _holdingRegisters[address] = value;
    _writeResponse(6);
  }
}

void ModbusRTUSlave::_processWriteMultipleCoils() {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (quantity == 0 || quantity > ((MODBUS_RTU_SLAVE_BUF_SIZE - 10) << 3) || _buf[6] != _div8RndUp(quantity)) _exceptionResponse(3);
  else if ((startAddress + quantity) > _numCoils) _exceptionResponse(2);
  else if (!_coils) _exceptionResponse(4);
  else {
    for (uint8_t i = 0; i < quantity; i++) {
      _coils[startAddress + i] = bitRead(_buf[7 + (i >> 3)], i & 7);
    }
    _writeResponse(6);
  }
}

void ModbusRTUSlave::_processWriteMultipleHoldingRegisters() {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (quantity == 0 || quantity > ((MODBUS_RTU_SLAVE_BUF_SIZE - 10) >> 1) || _buf[6] != (quantity * 2)) _exceptionResponse(3);
  else if (startAddress + quantity > _numHoldingRegisters) _exceptionResponse(2);
  else if (!_holdingRegisters) _exceptionResponse(4);
  else {
    for (uint8_t i = 0; i < quantity; i++) {
      _holdingRegisters[startAddress + i] = _bytesToWord(_buf[i * 2 + 7], _buf[i * 2 + 8]);
    }
    _writeResponse(6);
  }
}



bool ModbusRTUSlave::_readRequest() {
  uint8_t numBytes = 0;
  uint32_t startTime = 0;
  do {
    if (_serial->available()) {
      startTime = micros();
      _buf[numBytes] = _serial->read();
      numBytes++;
    }
  } while (micros() - startTime < _charTimeout && numBytes < MODBUS_RTU_SLAVE_BUF_SIZE);
  while (micros() - startTime < _frameTimeout);
  if (_serial->available() == 0 && (_buf[0] == _id || _buf[0] == 0) && _crc(numBytes - 2) == _bytesToWord(_buf[numBytes - 1], _buf[numBytes - 2])) return true;
  else return false;
}

void ModbusRTUSlave::_writeResponse(uint8_t len) {
  if (_buf[0] != 0) {
    uint16_t crc = _crc(len);
    _buf[len] = lowByte(crc);
    _buf[len + 1] = highByte(crc);
    if (_dePin != NO_DE_PIN) digitalWrite(_dePin, HIGH);
    _serial->write(_buf, len + 2);
    _serial->flush();
    if (_dePin != NO_DE_PIN) digitalWrite(_dePin, LOW);
  }
}

void ModbusRTUSlave::_exceptionResponse(uint8_t code) {
  _buf[1] |= 0x80;
  _buf[2] = code;
  _writeResponse(3);
}



void ModbusRTUSlave::_calculateTimeouts(uint32_t baud, uint8_t config) {
  if (baud > 19200) {
    _charTimeout = 750;
    _frameTimeout = 1750;
  }
  if (config == SERIAL_8E2 or config == SERIAL_8O2) {
    _charTimeout = 18000000/baud;
    _frameTimeout = 42000000/baud;
  }
  else if (config == SERIAL_8N2 or config == SERIAL_8E1 or config == SERIAL_8O1) {
    _charTimeout = 16500000/baud;
    _frameTimeout = 38500000/baud;
  }
  else {
    _charTimeout = 15000000/baud;
    _frameTimeout = 35000000/baud;
  }
}

void ModbusRTUSlave::_clearRxBuffer() {
  uint32_t startTime = micros();
  do {
    if (_serial->available()) {
      startTime = micros();
      _serial->read();
    }
  } while (micros() - startTime < _frameTimeout);
}



uint16_t ModbusRTUSlave::_crc(uint8_t len) {
  uint16_t value = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    value ^= (uint16_t)_buf[i];
    for (uint8_t j = 0; j < 8; j++) {
      bool lsb = value & 1;
      value >>= 1;
      if (lsb) value ^= 0xA001;
    }
  }
  return value;
}

uint16_t ModbusRTUSlave::_div8RndUp(uint16_t value) {
  return (value + 7) >> 3;
}

uint16_t ModbusRTUSlave::_bytesToWord(uint8_t high, uint8_t low) {
  return (high << 8) | low;
}