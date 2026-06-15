# Arduino GIGA R1 M7 - Stability Testing Guide

## Pre-Flight Checklist

Before deployment, verify these aspects of the system:

### 1. Compilation & Build
```bash
# Should complete without errors
pio run

# Should upload without issues
pio run --target upload

# Monitor for initialization messages
pio device monitor -b 115200
```

**Expected Output**:
```
=== AURORA PSU Controller ===
Initializing devices...
[OK] ADS1115 ADC initialized
[OK] DAC (GP8211S) initialized
[OK] Display initialized
[OK] Touch initialized
Initialization complete!
```

### 2. Device Initialization Tests

**ADC (ADS1115) Verification**:
- [ ] Message shows "[OK] ADS1115 ADC initialized"
- [ ] Voltage reading appears on screen
- [ ] Reading updates every 250ms
- [ ] No "read failures" warnings in serial

**DAC (GP8211S) Verification**:
- [ ] Message shows "[OK] DAC (GP8211S) initialized"
- [ ] DAC output measures ~0V on multimeter at startup
- [ ] Output changes when adjusting slider
- [ ] Output stays stable (doesn't drift)

**Display Verification**:
- [ ] Screen displays without artifacts
- [ ] Gauges render correctly
- [ ] Text is readable
- [ ] No visual flicker

**Touch Verification**:
- [ ] Message shows "[OK] Touch initialized"
- [ ] Buttons are responsive
- [ ] Single tap triggers one action (no double-fire)
- [ ] Touch calibration is accurate

### 3. Voltage Measurement Tests

**Test ADC Filter & Stability**:
1. Apply stable 30V to input (via divider)
2. Monitor voltage display for 30 seconds
3. Expected behavior:
   - [ ] Display shows ~30V (±0.5V)
   - [ ] No rapid flickering
   - [ ] Updates smoothly every 250ms
   - [ ] Smooth gauge animation

**Test Outlier Rejection**:
1. Apply 30V to input
2. Briefly touch ADC wiring (simulate noise spike)
3. Expected behavior:
   - [ ] Voltage reading briefly dips
   - [ ] Recovers within 1 second
   - [ ] No crash or hang

**Test Range Limits**:
1. Apply 0V to input
   - [ ] Display shows ~0V, gauge at minimum
2. Apply 60V to input (or max available)
   - [ ] Display shows ~60V, gauge near maximum
   - [ ] Output clamped to 85V max (no overflow)

### 4. Output Control Tests

**Test DAC Output Scaling**:
1. Set slider to 0%
   - [ ] DAC output measures ~0.0V on multimeter
2. Set slider to 50%
   - [ ] DAC output measures ~3.0V (±0.05V)
3. Set slider to 100%
   - [ ] DAC output measures ~6.0V (±0.05V)

**Test DAC Smooth Response**:
1. Tap Up button 5 times (5% increments)
   - [ ] Output steps up smoothly
   - [ ] No glitching or drops
   - [ ] Gauge animates smoothly
2. Tap Down button 5 times
   - [ ] Output steps down smoothly
   - [ ] Gauge retracts smoothly

**Test DAC Retry Logic**:
1. While connected, briefly disconnect DAC I2C line
   - [ ] System should attempt retry
   - [ ] Serial shows "[WARN] DAC failures"
   - [ ] Reconnect and verify recovery

### 5. Display & UI Tests

**Test Frame Rate & Responsiveness**:
1. Adjust slider continuously (rapid taps)
   - [ ] Display updates at ~30 FPS (smooth, not jerky)
   - [ ] CPU load reasonable (controller responsive)
   - [ ] No lag in gauge updates (max ~100ms)

**Test Hysteresis/Snap-to-Target**:
1. Adjust slider to 50%
2. Set back to 0%
3. Expected behavior:
   - [ ] Gauge snaps to endpoints smoothly
   - [ ] Animation is fluid (dampening works)
   - [ ] No stutter or jitter

**Test Low-Power Mode**:
1. Tap Power button
   - [ ] Display turns black
   - [ ] Power consumption drops visibly (measure with ammeter if available)
   - [ ] Serial shows low activity
2. Tap anywhere on screen
   - [ ] Display turns back on
   - [ ] Gauges reinitialize
   - [ ] System responsive again

### 6. Touch Input Tests

**Test Debouncing**:
1. Rapidly tap Up button 10 times quickly
   - [ ] Should increment by 10 (one per tap)
   - [ ] No double-increments
   - [ ] No phantom touches
2. Repeat with Down button
   - [ ] Should decrement correctly
   - [ ] No overshoot

**Test Button Accuracy**:
1. Hold Up button for 2 seconds
   - [ ] Continues incrementing
   - [ ] Reaches 100% and stops
2. Hold Down button for 2 seconds
   - [ ] Continues decrementing
   - [ ] Reaches 0% and stops

**Test Power Button**:
1. Tap Power button in corners/edges
   - [ ] Must tap center of button to activate
   - [ ] Edge taps don't accidentally trigger
   - [ ] Debounce prevents double-presses

### 7. Error Handling Tests

**Test I2C Timeout**:
1. Disconnect ADS1115 from I2C
   - [ ] No crash
   - [ ] Serial shows "[FAIL] ADS1115"
   - [ ] System continues (DAC still works)
   - [ ] Voltage display shows fallback value
2. Reconnect ADS1115
   - [ ] Automatic recovery on next init? (if you reset)

**Test DAC Failure Mode**:
1. Disconnect GP8211S from I2C
   - [ ] No crash
   - [ ] Serial shows "[WARN] DAC failures"
   - [ ] After 5 failures: "[WARN] DAC failures exceed threshold"
   - [ ] Output disabled safely (0V)
2. Reconnect and reset
   - [ ] Recovery on reboot

**Test Low Voltage**:
1. Supply <0.1V
   - [ ] Display shows 0V, no negative values
   - [ ] Gauge at minimum
   - [ ] No crash

**Test Over Voltage**:
1. Supply >100V (simulate)
   - [ ] Display clamps to 85V max
   - [ ] Gauge doesn't exceed maximum
   - [ ] No overflow

### 8. Long-Duration Stability Tests

**24-Hour Stability Test**:
1. Apply stable 30V to input
2. Set output to 50%
3. Let run for 24 hours
4. Expected behavior:
   - [ ] No crashes or resets
   - [ ] Voltage reading stays stable (±1V)
   - [ ] Output stays at 50% (±0.1V)
   - [ ] Display remains responsive
   - [ ] Serial shows no warnings

**Millis Overflow Test** (if testing simulation):
- Verify timeElapsed() handles edge cases
- Run unit tests: `pio test`

### 9. Performance Metrics

**Monitor These During Tests**:

Using serial monitor at 115200 baud:
```cpp
// Add temporary debug output if needed:
Serial.print("Free RAM: ");
Serial.println(freeRam());  // Requires freeRam() function

Serial.print("ADC Fails: ");
Serial.print(adsFailCount);
Serial.print(" | DAC Fails: ");
Serial.println(dacFailCount);
```

**Expected Metrics**:
- Display refresh: ~30 FPS (33ms intervals)
- ADC reads: 1 per 250ms (4 reads/sec)
- Touch response: <50ms debounce
- Memory: No growth over time (no leaks)
- Temperature: Stable at room temp

### 10. Final Validation

**Regression Tests** (Ensure existing functionality works):
- [ ] Voltage reading still accurate
- [ ] Output control still responsive
- [ ] Display still flicker-free
- [ ] Touch still responsive
- [ ] No new error messages in serial
- [ ] Power button still works

**Documentation**:
- [ ] README.md reflects current state
- [ ] STABILITY_IMPROVEMENTS.md accurate
- [ ] Code comments are clear
- [ ] All configurable constants documented

---

## Test Failure Troubleshooting

### "DAC not found" on startup
**Diagnostics**:
- Check Wire2 I2C pins (SDA2/SCL2)
- Verify DFRobot library version: `adafruit/Adafruit ADS1X15@^2.6.2`
- Check DAC address with I2C scanner

**Fix**:
```cpp
// Verify in setup
Wire2.begin();
delay(100);
if (!GP8211S.begin()) {
    Serial.println("Check I2C connection on Wire2");
}
```

### Voltage reading always shows same value
**Diagnostics**:
- Check ADC buffer initialization
- Verify ADS1115 address (0x48)
- Monitor serial for "[FAIL] ADS1115"

**Fix**:
- Move ADC away from power cables (EMI)
- Add 100nF capacitor across ADS1115 power pins
- Increase ADC_FILTER_SIZE to 32

### Display flickers rapidly
**Diagnostics**:
- Check if frame interval is being respected
- Verify FRAME_INTERVAL = 33ms

**Fix**:
- Increase FRAME_INTERVAL to 50ms if necessary
- Check for drawing operations outside updateGauges()
- Reduce display redraw threshold from 0.2 to 0.5

### Touch doesn't respond
**Diagnostics**:
- Verify touch library initialized
- Check touch calibration values (rawX_min/max, rawY_min/max)

**Fix**:
```cpp
// Re-calibrate touch:
int rawX_min = 10, rawX_max = 470;
int rawY_min = 10, rawY_max = 785;
```

### Occasional watchdog reboots
**Diagnostics**:
- Check for blocking loops
- Verify delay() calls are minimal

**Fix**:
- Remove any long delay() calls
- Break long operations into smaller steps
- Consider hardware watchdog (if not already implemented)

---

## Test Report Template

Use this template to document your test results:

```
TEST REPORT - Arduino GIGA R1 M7 PSU Controller
Date: _______________
Tester: _______________
Hardware: Arduino GIGA R1 M7 + Display Shield + ADS1115 + GP8211S

COMPILATION:
[ ] Build successful
[ ] No warnings
[ ] Upload successful

INITIALIZATION:
[ ] ADC initialized
[ ] DAC initialized
[ ] Display initialized
[ ] Touch initialized

VOLTAGE READING:
[ ] Shows correct value
[ ] Updates smoothly
[ ] No flicker
[ ] Filter working

OUTPUT CONTROL:
[ ] 0% = 0V
[ ] 50% = 3V
[ ] 100% = 6V
[ ] Smooth transitions

DISPLAY:
[ ] No artifacts
[ ] Gauges animate smoothly
[ ] Text readable
[ ] Frame rate acceptable

TOUCH:
[ ] Buttons responsive
[ ] Debouncing works
[ ] Power button functions
[ ] No phantom touches

ERROR HANDLING:
[ ] Graceful I2C failures
[ ] No crashes
[ ] Error messages clear
[ ] Recovery possible

ISSUES FOUND:
1. _______________
2. _______________

NOTES:
_______________
```

---

**Version**: 1.0.0  
**Last Updated**: 2026-06-08  
**For**: Arduino GIGA R1 M7 with GigaDisplay Shield
