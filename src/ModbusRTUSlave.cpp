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
  _coils = 0;
  _discreteInputs = 0;
  _holdingRegisters = 0;
  _inputRegisters = 0;
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
  _coils = 0;
  _discreteInputs = 0;
  _holdingRegisters = 0;
  _inputRegisters = 0;
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
  _coils = 0;
  _discreteInputs = 0;
  _holdingRegisters = 0;
  _inputRegisters = 0;
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

#ifdef ESP32
void ModbusRTUSlave::begin(uint8_t id, unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, bool invert) {
  if (id >= 1 && id <= 247) _id = id;
  else _id = NO_ID;
  if (_hardwareSerial) {
    _calculateTimeouts(baud, config);
    _hardwareSerial->begin(baud, config, rxPin, txPin, invert);
  }
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
#else
void ModbusRTUSlave::begin(uint8_t id, unsigned long baud, uint32_t config) {
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
#endif

uint8_t ModbusRTUSlave::poll(uint8_t* errCode, uint16_t* startAddress, uint16_t* quantity) {
  uint8_t cmdCode = 0;
  uint8_t cmdErrCode = 0;
  if (_serial->available()) {
    if (_readRequest()) {
      cmdCode = _buf[1];
      switch (cmdCode) {
        case READ_COIL:
          _processReadCoils(&cmdErrCode, startAddress, quantity);
          break;
        case READ_DISCRETE_INPUT:
          _processReadDiscreteInputs(&cmdErrCode, startAddress, quantity);
          break;
        case READ_HOLDING_REG:
          _processReadHoldingRegisters(&cmdErrCode, startAddress, quantity);
          break;
        case READ_INPUT_REG:
          _processReadInputRegisters(&cmdErrCode, startAddress, quantity);
          break;
        case WRITE_SINGLE_COIL:
          _processWriteSingleCoil(&cmdErrCode, startAddress, quantity);
          break;
        case WRITE_SINGLE_HOLDING_REG:
          _processWriteSingleHoldingRegister(&cmdErrCode, startAddress, quantity);
          break;
        case WRITE_MULTI_COIL:
          _processWriteMultipleCoils(&cmdErrCode, startAddress, quantity);
          break;
        case WRITE_MULTI_HOLDING_REG:
          _processWriteMultipleHoldingRegisters(&cmdErrCode, startAddress, quantity);
          break;
        default:
          cmdErrCode = _exceptionResponse(1);
          break;
      }
    }
  }
  if (errCode) {
    *errCode = cmdErrCode;
  }
  return cmdCode;
}

void ModbusRTUSlave::_processReadCoils(uint8_t* errCode, uint16_t* returnStartAddress, uint16_t* returnQuantity) {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (returnStartAddress) { *returnStartAddress = startAddress; }
  if (returnQuantity) { *returnQuantity = quantity; }
  if (!_coils || _numCoils == 0) *errCode = _exceptionResponse(1);
  else if (quantity == 0 || quantity > 2000) *errCode = _exceptionResponse(3);
  else if (quantity > _numCoils || startAddress > (_numCoils - quantity)) *errCode = _exceptionResponse(2);
  else {
    _buf[2] = _div8RndUp(quantity);
    for (uint16_t i = 0; i < quantity; i++) {
      bitWrite(_buf[3 + (i >> 3)], i & 7, _coils[startAddress + i]);
    }
    _writeResponse(3 + _buf[2]);
  }
}

void ModbusRTUSlave::_processReadDiscreteInputs(uint8_t* errCode, uint16_t* returnStartAddress, uint16_t* returnQuantity) {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (returnStartAddress) { *returnStartAddress = startAddress; }
  if (returnQuantity) { *returnQuantity = quantity; }
  if (!_discreteInputs || _numDiscreteInputs == 0) *errCode = _exceptionResponse(1);
  else if (quantity == 0 || quantity > 2000) *errCode = _exceptionResponse(3);
  else if (quantity > _numDiscreteInputs || startAddress > (_numDiscreteInputs - quantity)) *errCode = _exceptionResponse(2);
  else {
    _buf[2] = _div8RndUp(quantity);
    for (uint16_t i = 0; i < quantity; i++) {
      bitWrite(_buf[3 + (i >> 3)], i & 7, _discreteInputs[startAddress + i]);
    }
    _writeResponse(3 + _buf[2]);
  }
}

void ModbusRTUSlave::_processReadHoldingRegisters(uint8_t* errCode, uint16_t* returnStartAddress, uint16_t* returnQuantity) {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (returnStartAddress) { *returnStartAddress = startAddress; }
  if (returnQuantity) { *returnQuantity = quantity; }
  if (!_holdingRegisters || _numHoldingRegisters == 0) *errCode = _exceptionResponse(1);
  else if (quantity == 0 || quantity > 125) *errCode = _exceptionResponse(3);
  else if (quantity > _numHoldingRegisters || startAddress > (_numHoldingRegisters - quantity)) *errCode = _exceptionResponse(2);
  else {
    _buf[2] = quantity * 2;
    for (uint16_t i = 0; i < quantity; i++) {
      _buf[3 + (i * 2)] = highByte(_holdingRegisters[startAddress + i]);
      _buf[4 + (i * 2)] = lowByte(_holdingRegisters[startAddress + i]);
    }
    _writeResponse(3 + _buf[2]);
  }
}

void ModbusRTUSlave::_processReadInputRegisters(uint8_t* errCode, uint16_t* returnStartAddress, uint16_t* returnQuantity) {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (returnStartAddress) { *returnStartAddress = startAddress; }
  if (returnQuantity) { *returnQuantity = quantity; }
  if (!_inputRegisters || _numInputRegisters == 0) *errCode = _exceptionResponse(1);
  else if (quantity == 0 || quantity > 125) *errCode = _exceptionResponse(3);
  else if (quantity > _numInputRegisters || startAddress > (_numInputRegisters - quantity)) *errCode = _exceptionResponse(2);
  else {
    _buf[2] = quantity * 2;
    for (uint16_t i = 0; i < quantity; i++) {
      _buf[3 + (i * 2)] = highByte(_inputRegisters[startAddress + i]);
      _buf[4 + (i * 2)] = lowByte(_inputRegisters[startAddress + i]);
    }
    _writeResponse(3 + _buf[2]);
  }
}

void ModbusRTUSlave::_processWriteSingleCoil(uint8_t* errCode, uint16_t* returnStartAddress, uint16_t* returnQuantity) {
  uint16_t address = _bytesToWord(_buf[2], _buf[3]);
  uint16_t value = _bytesToWord(_buf[4], _buf[5]);
  if (returnStartAddress) { *returnStartAddress = address; }
  if (returnQuantity) { *returnQuantity = 1; }
  if (!_coils ||_numCoils == 0) *errCode = _exceptionResponse(1);
  else if (value != 0 && value != 0xFF00) *errCode = _exceptionResponse(3);
  else if (address >= _numCoils) *errCode = _exceptionResponse(2);
  else {
    _coils[address] = value;
    _writeResponse(6);
  }
}

void ModbusRTUSlave::_processWriteSingleHoldingRegister(uint8_t* errCode, uint16_t* returnStartAddress, uint16_t* returnQuantity) {
  uint16_t address = _bytesToWord(_buf[2], _buf[3]);
  uint16_t value = _bytesToWord(_buf[4], _buf[5]);
  if (returnStartAddress) { *returnStartAddress = address; }
  if (returnQuantity) { *returnQuantity = 1; }
  if (!_holdingRegisters || _numHoldingRegisters == 0) *errCode = _exceptionResponse(1);
  else if (address >= _numHoldingRegisters) *errCode = _exceptionResponse(2);
  else {
    _holdingRegisters[address] = value;
    _writeResponse(6);
  }
}

void ModbusRTUSlave::_processWriteMultipleCoils(uint8_t* errCode, uint16_t* returnStartAddress, uint16_t* returnQuantity) {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (returnStartAddress) { *returnStartAddress = startAddress; }
  if (returnQuantity) { *returnQuantity = quantity; }
  if (!_coils || _numCoils == 0) *errCode = _exceptionResponse(1);
  else if (quantity == 0 || quantity > 1968 || _buf[6] != _div8RndUp(quantity)) *errCode = _exceptionResponse(3);
  else if (quantity > _numCoils || startAddress > (_numCoils - quantity)) *errCode = _exceptionResponse(2);
  else {
    for (uint16_t i = 0; i < quantity; i++) {
      _coils[startAddress + i] = bitRead(_buf[7 + (i >> 3)], i & 7);
    }
    _writeResponse(6);
  }
}

void ModbusRTUSlave::_processWriteMultipleHoldingRegisters(uint8_t* errCode, uint16_t* returnStartAddress, uint16_t* returnQuantity) {
  uint16_t startAddress = _bytesToWord(_buf[2], _buf[3]);
  uint16_t quantity = _bytesToWord(_buf[4], _buf[5]);
  if (returnStartAddress) { *returnStartAddress = startAddress; }
  if (returnQuantity) { *returnQuantity = quantity; }
  if (!_holdingRegisters || _numHoldingRegisters == 0) *errCode = _exceptionResponse(1);
  else if (quantity == 0 || quantity > 123 || _buf[6] != (quantity * 2)) *errCode = _exceptionResponse(3);
  else if (quantity > _numHoldingRegisters || startAddress > (_numHoldingRegisters - quantity)) *errCode = _exceptionResponse(2);
  else {
    for (uint16_t i = 0; i < quantity; i++) {
      _holdingRegisters[startAddress + i] = _bytesToWord(_buf[i * 2 + 7], _buf[i * 2 + 8]);
    }
    _writeResponse(6);
  }
}



bool ModbusRTUSlave::_readRequest() {
  uint16_t numBytes = 0;
  unsigned long startTime = 0;
  do {
    if (_serial->available()) {
      startTime = micros();
      _buf[numBytes] = _serial->read();
      numBytes++;
    }
  } while (micros() - startTime <= _charTimeout && numBytes < MODBUS_RTU_SLAVE_BUF_SIZE);
  while (micros() - startTime < _frameTimeout);
  if (!_serial->available() && (_buf[0] == _id || _buf[0] == 0) && _crc(numBytes - 2) == _bytesToWord(_buf[numBytes - 1], _buf[numBytes - 2])) return true;
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
    while(_serial->available()) {
      _serial->read();
    }
  }
}

uint8_t ModbusRTUSlave::_exceptionResponse(uint8_t code) {
  _buf[1] |= 0x80;
  _buf[2] = code;
  _writeResponse(3);
  return code;        // add return to simplify the changes for the code using this function
}

void ModbusRTUSlave::_clearRxBuffer() {
  unsigned long startTime = micros();
  do {
    if (_serial->available()) {
      startTime = micros();
      _serial->read();
    }
  } while (micros() - startTime < _frameTimeout);
}



void ModbusRTUSlave::_calculateTimeouts(unsigned long baud, uint32_t config) {
  unsigned long bitsPerChar;
  if (config == SERIAL_8E2 || config == SERIAL_8O2) bitsPerChar = 12;
  else if (config == SERIAL_8N2 || config == SERIAL_8E1 || config == SERIAL_8O1) bitsPerChar = 11;
  else bitsPerChar = 10;
  if (baud <= 19200) {
    _charTimeout = (bitsPerChar * 2500000) / baud;
    _frameTimeout = (bitsPerChar * 4500000) / baud;
  }
  else {
    _charTimeout = (bitsPerChar * 1000000) / baud + 750;
    _frameTimeout = (bitsPerChar * 1000000) / baud + 1750;
  }
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
