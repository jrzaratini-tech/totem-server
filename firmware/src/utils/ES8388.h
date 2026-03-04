#ifndef ES8388_H
#define ES8388_H

#include <Wire.h>

#define ES8388_ADDR 0x10

class ES8388 {
public:
    static bool begin(int sda, int scl) {
        Wire.begin(sda, scl);
        delay(100);
        
        Serial.println("[ES8388] Initializing codec...");
        
        // Reset
        writeReg(0x00, 0x80);
        writeReg(0x00, 0x00);
        delay(100);
        
        // Clock setup (MCLK from I2S)
        writeReg(0x01, 0x58);  // Enable VMIDSEL, disable ENREF
        writeReg(0x02, 0xF3);  // Power up DAC, ADC, Analog, Vref
        
        // Chip power management
        writeReg(0x03, 0x09);  // Enable DAC L/R
        writeReg(0x04, 0x3C);  // Power up DEM, STM
        
        // ADC Control (desabilitar para economizar energia)
        writeReg(0x09, 0x88);
        writeReg(0x0A, 0x00);
        
        // DAC Control
        writeReg(0x17, 0x18);  // DACLRC = ADCLRC
        writeReg(0x18, 0x02);  // DACFORMAT = I2S
        writeReg(0x19, 0x22);  // DAC L/R swap disabled
        writeReg(0x1A, 0x00);  // DAC volume control
        writeReg(0x1B, 0x00);  // DAC volume control
        
        // DAC Volume (0x00 = 0dB, 0xC0 = mute)
        writeReg(0x1A, 0x00);  // Left DAC volume 0dB
        writeReg(0x1B, 0x00);  // Right DAC volume 0dB
        
        // Output Mixer Control
        writeReg(0x26, 0x09);  // LOUT1 = LDAC
        writeReg(0x27, 0x90);  // ROUT1 = RDAC
        writeReg(0x28, 0x38);  // LOUT2 = LDAC
        writeReg(0x29, 0x38);  // ROUT2 = RDAC
        
        // Output Volume (0-33, 0=mute, 33=max, cada step = 1.5dB)
        writeReg(0x2E, 0x1E);  // LOUT1 volume = 30 (45dB)
        writeReg(0x2F, 0x1E);  // ROUT1 volume = 30 (45dB)
        writeReg(0x30, 0x1E);  // LOUT2 volume = 30
        writeReg(0x31, 0x1E);  // ROUT2 volume = 30
        
        // Power up all outputs
        writeReg(0x02, 0x00);  // Power up everything
        writeReg(0x04, 0x3C);  // Power up DEM, STM
        
        delay(100);
        Serial.println("[ES8388] ✓ Codec initialized with full output");
        return true;
    }
    
    static void setVolume(uint8_t volume) {
        if (volume > 33) volume = 33;
        writeReg(0x27, volume);
        writeReg(0x2A, volume);
        Serial.printf("[ES8388] Volume set to %d\n", volume);
    }
    
private:
    static void writeReg(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(ES8388_ADDR);
        Wire.write(reg);
        Wire.write(val);
        Wire.endTransmission();
        delayMicroseconds(100);
    }
};

#endif
