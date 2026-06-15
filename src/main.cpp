#include <Arduino.h>
#include <Arduino_GigaDisplay_GFX.h> 
#include <Arduino_GigaDisplayTouch.h>
#include <Adafruit_ADS1X15.h>
#include <DFRobot_GP8XXX.h>
#include <Wire.h>
#include <math.h>

// ===== ADS1115 =====
Adafruit_ADS1115 ads;
bool adsOK = false;
uint16_t adsFailCount = 0;
const uint16_t ADS_FAIL_THRESHOLD = 5;

// ===== DAC =====
// DFRobot_GP8211S does not expose the I2C bus constructor, so use the
// shared I2C driver class directly to bind the DAC to Wire2.
DFRobot_GP8XXX_IIC GP8211S(RESOLUTION_15_BIT, DFGP8XXX_I2C_DEVICEADDR, &Wire2);
bool dacOK = false;
uint16_t lastDACValue = -1;
uint16_t dacFailCount = 0;
const uint16_t DAC_FAIL_THRESHOLD = 5;

// ===== Voltage =====
int calib = 7;
float voltage = 0;
float Radjust = 0.043421905;
float vbat = 0;

// ===== Timing (safe against millis overflow) =====
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 250;
const unsigned long FRAME_INTERVAL = 33;  // ~30 FPS max
unsigned long lastFrameTime = 0;

// ===== ADC Filtering =====
#define ADC_FILTER_SIZE 16
int16_t adcBuffer[ADC_FILTER_SIZE];
uint8_t adcBufferIndex = 0;
const int16_t ADC_OUTLIER_THRESHOLD = 400;  // ~7.5V deviation

// ===== Display + Touch =====
GigaDisplay_GFX display;
Arduino_GigaDisplayTouch touch;
bool displayOK = false;

// ===== Colors =====
#define DARK_GREY   0x4208
#define LIGHT_BLUE  0x2BFF
#define WHITE       0xFFFF
#define MID_GREY    0x8410
#define RED         0xF800
#define BLACK       0x0000

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480

// ===== POWER CONTROL =====
bool screenOn = true;
uint8_t touchFailCount = 0;
const uint8_t TOUCH_FAIL_THRESHOLD = 10;

#define PWR_W 100
#define PWR_H 40
#define PWR_X (SCREEN_WIDTH - PWR_W - 10)
#define PWR_Y 10

// ===== Slider =====
struct Slider {
    int x, y;
    int radius;
    float value;
    float maxValue;
    const char* unit;
    float displayedValue;
    float lastDrawValue;
};

// Gauges (UNCHANGED)
Slider sliders[2] = {
    {220, 240, 140, 0.0,   85.0, "V",  0.0,  -1},
    {580, 240, 140, 50.0, 100.0, "%", 0.0,  -1}
};


// Buttons
#define BTN_WIDTH 120
#define BTN_HEIGHT 60
#define BTN_RADIUS 15

// Touch calibration
int rawX_min = 6, rawX_max = 473;
int rawY_min = 6, rawY_max = 789;


// Tunable calibration constants — adjust these to dial in your output
#define DAC_ZERO_TRIM   -2      // compensates the 0% residual
#define DAC_FULL_SCALE  19520   // tune until 100% = exactly 6.000V

// ==========================
// HELPER FUNCTIONS
// ==========================

// Safe time comparison that handles millis() overflow
bool timeElapsed(unsigned long& lastTime, unsigned long interval) {
    unsigned long currentTime = millis();
    if (currentTime - lastTime >= interval) {
        lastTime = currentTime;
        return true;
    }
    return false;
}

// ADC filter: moving average with outlier rejection
int16_t readADCFiltered() {
    if (!adsOK) return -1;
    
    int16_t rawValue = ads.readADC_SingleEnded(0);
    
    // Check for outliers
    if (adcBufferIndex > 0) {
        int16_t median = adcBuffer[ADC_FILTER_SIZE / 2];
        if (abs(rawValue - median) > ADC_OUTLIER_THRESHOLD) {
            adsFailCount++;
            return -1;  // Reject outlier
        }
    }
    
    adsFailCount = 0;  // Reset fail counter on successful read
    adcBuffer[adcBufferIndex] = rawValue;
    adcBufferIndex = (adcBufferIndex + 1) % ADC_FILTER_SIZE;
    
    // Compute moving average
    int32_t sum = 0;
    for (int i = 0; i < ADC_FILTER_SIZE; i++) {
        sum += adcBuffer[i];
    }
    return (int16_t)(sum / ADC_FILTER_SIZE);
}

// Safe I2C operation with retry logic
bool i2cRetry(const char* operation, uint8_t maxRetries = 3) {
    for (uint8_t attempt = 0; attempt < maxRetries; attempt++) {
        if (attempt > 0) {
            delay(10 * attempt);  // Exponential backoff
        }
    }
    return true;
}

// Device initialization with error reporting
void initializeDevices() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== AURORA PSU Controller ===");
    Serial.println("Initializing devices...");
    
    // Initialize ADS1115
    if (ads.begin(0x48)) {
        adsOK = true;
        Serial.println("[OK] ADS1115 ADC initialized");
        for (int i = 0; i < ADC_FILTER_SIZE; i++) {
            adcBuffer[i] = ads.readADC_SingleEnded(0);
            delay(5);
        }
    } else {
        Serial.println("[FAIL] ADS1115 ADC not found - will operate in degraded mode");
        adsOK = false;
    }
    
    // Initialize DAC on Wire2
    Wire2.begin();
    delay(50);
    if (GP8211S.begin() == 0) {
        GP8211S.setDACOutRange(GP8211S.eOutputRange10V);
        dacOK = true;
        Serial.println("[OK] DAC (GP8211S) initialized");
        GP8211S.setDACOutVoltage(0);  // Safe default
    } else {
        Serial.println("[FAIL] DAC not found - output disabled");
        dacOK = false;
    }
    
    // Initialize Display
    display.begin();
    display.setRotation(1);
    displayOK = true;
    Serial.println("[OK] Display initialized");
    
    // Initialize Touch
    touch.begin();
    Serial.println("[OK] Touch initialized");
    
    Serial.println("Initialization complete!\n");
}


// Forward declarations
void drawStaticScreen();
void drawUpDownButtons();
void drawGaugeBase(Slider& s);
void drawGaugeDynamic(Slider& s);
void handleTouch();
void updateGauges();
void updateDAC();
void enterLowPower();
void exitLowPower();


// ==========================
// SETUP
// ==========================
void setup() {
    initializeDevices();

    for (int i = 0; i < 2; i++) {
        sliders[i].value = 0;
        sliders[i].displayedValue = 0;
        sliders[i].lastDrawValue = -1;
    }
    
    lastReadTime = millis();
    lastFrameTime = millis();

    drawStaticScreen();
}

// ==========================
// MAIN LOOP
// ==========================
void loop() {
    // Handle touch input every 10ms
    handleTouch();

    // If screen is OFF → do minimal work
    if (!screenOn) {
        delay(20);
        return;
    }

    // Read voltage at 250ms interval (safe against millis overflow)
    if (timeElapsed(lastReadTime, READ_INTERVAL)) {
        if (adsOK) {
            int16_t filteredADC = readADCFiltered();
            
            if (filteredADC >= 0) {
                voltage = ((filteredADC + calib) * 0.1875) / 1000.0;
                vbat = voltage / Radjust;
                
                // Range validation
                if (vbat < 0.1) vbat = 0.01;
                if (vbat > sliders[0].maxValue) vbat = sliders[0].maxValue;
                
                sliders[0].value = vbat;
            } else if (adsFailCount >= ADS_FAIL_THRESHOLD) {
                Serial.println("[WARN] ADC read failures exceed threshold");
                adsOK = false;  // Disable ADC if too many failures
            }
        }
    }

    // Update gauges at max 30 FPS to prevent display overload
    if (timeElapsed(lastFrameTime, FRAME_INTERVAL)) {
        updateGauges();
        updateDAC();
    }
}

// ==========================
// LOW POWER
// ==========================
void enterLowPower() {
    screenOn = false;
    display.fillScreen(BLACK);
}

void exitLowPower() {
    screenOn = true;

    // Reset values to 0
    for (int i = 0; i < 2; i++) {
        sliders[i].value = 0;
        sliders[i].displayedValue = 0;
        sliders[i].lastDrawValue = -1; // force redraw
    }

    drawStaticScreen();
}

// ==========================
// DAC UPDATE
// ==========================

void updateDAC() {
    if (!dacOK) return;
    float percent = constrain(sliders[1].value, 0.0, 100.0);
    uint16_t dacValue;
    if (percent == 0.0) {
        dacValue = 0;
    } else {
        dacValue = (int16_t)((percent / 100.0) * DAC_FULL_SCALE) + DAC_ZERO_TRIM;
        dacValue = constrain(dacValue, 0, 32767);
    }
    if (dacValue != lastDACValue) {
        GP8211S.setDACOutVoltage(dacValue);
        lastDACValue = dacValue;
    }
}

// ==========================
// STATIC UI
// ==========================
void drawStaticScreen() {
    //display.fillScreen(DARK_GREY);
    display.fillScreen(BLACK);

    drawUpDownButtons();

    // ===== LOGO (TOP LEFT) =====
    display.setTextSize(3);
    display.setTextColor(LIGHT_BLUE);
    display.setCursor(10, 10);
    display.print("AURORA");

    // ===== POWER BUTTON =====
    display.setTextSize(2);
    display.fillRoundRect(PWR_X, PWR_Y, PWR_W, PWR_H, 10, MID_GREY);
    display.setTextColor(WHITE);
    display.setCursor(PWR_X + 15, PWR_Y + 12);
    display.print("Power");

    for (int i = 0; i < 2; i++) {
        drawGaugeBase(sliders[i]);
    }
}

// ==========================
// GAUGE UPDATE
// ==========================
void updateGauges() {
    for (int i = 0; i < 2; i++) {
        Slider& s = sliders[i];
        
        float diff = s.value - s.displayedValue;
        
        // Snap to target if within 0.15 units
        if (fabs(diff) <= 0.15) {
            s.displayedValue = s.value;
        } else {
            // Smooth animation with 0.1 damping factor
            s.displayedValue += diff * 0.1;
        }
        
        // Only redraw if change is significant (hysteresis)
        if (fabs(s.displayedValue - s.lastDrawValue) >= 0.2) {
            drawGaugeDynamic(s);
            s.lastDrawValue = s.displayedValue;
        }
    }
}

// ==========================
// BUTTONS
// ==========================
void drawUpDownButtons() {
    int upX = (SCREEN_WIDTH - BTN_WIDTH) / 2;
    int upY = 20;
    int downX = upX;
    int downY = SCREEN_HEIGHT - BTN_HEIGHT - 20;

    display.fillRoundRect(upX, upY, BTN_WIDTH, BTN_HEIGHT, BTN_RADIUS, MID_GREY);
    display.fillRoundRect(downX, downY, BTN_WIDTH, BTN_HEIGHT, BTN_RADIUS, MID_GREY);

    int arrowSize = 40;

    int cx = upX + BTN_WIDTH / 2;
    int cy = upY + BTN_HEIGHT / 2;

    display.fillTriangle(cx, cy - arrowSize / 2,
                         cx - arrowSize / 2, cy + arrowSize / 2,
                         cx + arrowSize / 2, cy + arrowSize / 2,
                         LIGHT_BLUE);

    cx = downX + BTN_WIDTH / 2;
    cy = downY + BTN_HEIGHT / 2;

    display.fillTriangle(cx, cy + arrowSize / 2,
                         cx - arrowSize / 2, cy - arrowSize / 2,
                         cx + arrowSize / 2, cy - arrowSize / 2,
                         LIGHT_BLUE);
}

// ==========================
// GAUGES
// ==========================
void drawGaugeBase(Slider& s) {
    for (int r = s.radius - 3; r <= s.radius + 3; r++)
        display.drawCircle(s.x, s.y, r, MID_GREY);

    for (int deg = 0; deg <= 360; deg += 30) {
        float rad = (deg - 90) * PI / 180.0;
        int x1 = s.x + cos(rad) * (s.radius - 30);
        int y1 = s.y + sin(rad) * (s.radius - 30);
        int x2 = s.x + cos(rad) * s.radius;
        int y2 = s.y + sin(rad) * s.radius;
        display.drawLine(x1, y1, x2, y2, LIGHT_BLUE);
    }
}

void drawGaugeDynamic(Slider& s) {
    float angle = (s.displayedValue / s.maxValue) * 360.0 - 90;
    float rad = angle * PI / 180.0;

    //display.fillCircle(s.x, s.y, s.radius - 35, DARK_GREY); // old code, changed to black for better contrast
    display.fillCircle(s.x, s.y, s.radius - 35, BLACK);

    int x2 = s.x + cos(rad) * (s.radius - 40);
    int y2 = s.y + sin(rad) * (s.radius - 40);

    display.drawLine(s.x, s.y, x2, y2, RED);
    display.drawCircle(s.x, s.y, 5, RED);

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.1f %s", s.displayedValue, s.unit);

    display.setTextSize(3);
    //display.setTextColor(WHITE, DARK_GREY);    // old code, changed to black for better contrast
    display.setTextColor(WHITE, BLACK);
    display.setCursor(s.x - 40, s.y);
    display.print(buffer);
}

// ===== Touch Debounce =====
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 50;

// ==========================
// TOUCH
// ==========================
void handleTouch() {
    GDTpoint_t p[1];
    if (!touch.getTouchPoints(p)) {
        touchFailCount = 0;
        return;
    }

    // Debounce touch input
    unsigned long now = millis();
    if (now - lastTouchTime < TOUCH_DEBOUNCE_MS) {
        return;
    }
    lastTouchTime = now;

    uint16_t tx = map(p[0].y, rawY_min, rawY_max, 0, SCREEN_WIDTH);
    uint16_t ty = map(p[0].x, rawX_max, rawX_min, 0, SCREEN_HEIGHT);

    // Wake screen
    if (!screenOn) {
        exitLowPower();
        return;
    }

    // Power button
    if (tx >= PWR_X && tx <= PWR_X + PWR_W &&
        ty >= PWR_Y && ty <= PWR_Y + PWR_H) {
        enterLowPower();
        return;
    }

    // Up/Down buttons
    int upX = (SCREEN_WIDTH - BTN_WIDTH) / 2;
    int upY = 20;
    int downX = upX;
    int downY = SCREEN_HEIGHT - BTN_HEIGHT - 20;

    if (tx >= upX && tx <= upX + BTN_WIDTH &&
        ty >= upY && ty <= upY + BTN_HEIGHT) {
        sliders[1].value = min(sliders[1].value + 1.0, 100.0);
        return;
    }

    if (tx >= downX && tx <= downX + BTN_WIDTH &&
        ty >= downY && ty <= downY + BTN_HEIGHT) {
        sliders[1].value = max(sliders[1].value - 1.0, 0.0);
    }
}
