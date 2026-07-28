#include "io/Mcp23017.h"

#include <Arduino.h>
#include <Wire.h>
#include "HardwarePins.h"

namespace {

// MCP23017 register addresses with IOCON.BANK = 0 (the power-on
// default -- this driver never touches IOCON, so bank stays 0).
constexpr uint8_t REG_IODIRA = 0x00;
constexpr uint8_t REG_IODIRB = 0x01;
constexpr uint8_t REG_GPPUA  = 0x0C;
constexpr uint8_t REG_GPIOA  = 0x12;
constexpr uint8_t REG_GPIOB  = 0x13;

void writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(HardwarePins::MCP23017_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg)
{
    Wire.beginTransmission(HardwarePins::MCP23017_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(static_cast<int>(HardwarePins::MCP23017_I2C_ADDR), 1);
    return Wire.available() ? static_cast<uint8_t>(Wire.read()) : 0;
}

} // namespace

Mcp23017& Mcp23017::instance()
{
    static Mcp23017 inst;
    return inst;
}

void Mcp23017::begin()
{
    if (began_) {
        return;
    }
    began_ = true;

    Wire.begin(HardwarePins::I2C_SDA, HardwarePins::I2C_SCL);

    writeRegister(REG_IODIRA, 0xFF); // bank A: all inputs (switches)
    writeRegister(REG_GPPUA, 0xFF);  // bank A: internal pull-ups on
    writeRegister(REG_IODIRB, 0x00); // bank B: all outputs (lights)
    writeRegister(REG_GPIOB, 0x00);  // bank B: start all off
}

uint8_t Mcp23017::readGpioA()
{
    return readRegister(REG_GPIOA);
}

void Mcp23017::writeGpioB(uint8_t value)
{
    writeRegister(REG_GPIOB, value);
}
