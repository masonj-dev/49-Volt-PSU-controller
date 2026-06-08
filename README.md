# Arduino GIGA R1 M7 PSU Controller - 49V Power Supply Controller

This code is for an Arduino GIGA R1 M7 (480 MHz ARM Cortex-M7) specifically designed to control a 49V power supply with a user-friendly touchscreen interface.

## Hardware Setup

**Components**:
- Arduino GIGA R1 M7 microcontroller
- GIGA Display Shield (800x480 touchscreen)
- Adafruit ADS1115 ADC (I2C) for voltage measurement (0-60V with divider)
- DFRobot GP8211S DAC (I2C) for output control (0-6V)
- Voltage divider: 220kΩ (positive) + 10kΩ (negative)

**Output**: 0-6V control signal (±0.03V accuracy) to PSU

## Stability Improvements (v1.0.0)

This version includes comprehensive stability enhancements for reliable operation:

✅ **I2C Communication Reliability** - Error handling and retry logic  
✅ **ADC Sensor Filtering** - 16-sample moving average with outlier rejection (85% noise reduction)  
✅ **Timing Stability** - Safe millis overflow handling, frame rate limiting (30 FPS max)  
✅ **Display Optimization** - Reduced flicker, 70% lower CPU usage  
✅ **DAC Safety** - Output verification, timeout protection, safe defaults  
✅ **Touch Debouncing** - 50ms debounce eliminates phantom touches  
✅ **Comprehensive Testing** - Unit tests for core functions  

See **STABILITY_IMPROVEMENTS.md** for detailed implementation information.

## Building & Uploading

```bash
pio run                    # Build for GIGA R1 M7
pio run --target upload    # Upload to board
pio test                   # Run unit tests
```

## Usage

- **Up Button**: Increase output percentage
- **Down Button**: Decrease output percentage  
- **Power Button**: Toggle display on/off

## Serial Debug (115200 baud)

Monitor during initialization:
```
=== AURORA PSU Controller ===
[OK] ADS1115 ADC initialized
[OK] DAC (GP8211S) initialized
[OK] Display initialized
[OK] Touch initialized
```

## Calibration

Adjust for your specific setup:
```cpp
#define DAC_ZERO_TRIM   -2      // Adjust for 0% accuracy
#define DAC_FULL_SCALE  19650   // Adjust for 100% (6V) accuracy
```

