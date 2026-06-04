#include <Arduino.h>
#include <Arduino_GigaDisplay_GFX.h> 
#include <Arduino_GigaDisplayTouch.h>
#include <Adafruit_ADS1X15.h>
#include <DFRobot_GP8XXX.h>
#include <Wire.h>
#include <math.h>

// ===== ADS1115 =====
Adafruit_ADS1115 ads;

// ===== DAC =====
DFRobot_GP8211S GP8211S(RESOLUTION_15_BIT, &Wire2);
bool dacOK = false;
uint16_t lastDACValue = -1;

// ===== Voltage =====
int calib = 7;
float voltage = 0;
float Radjust = 0.043421905;
float vbat = 0;

unsigned long previousMillis = 0;
const long interval = 250;

// ===== Display + Touch =====
GigaDisplay_GFX display;
Arduino_GigaDisplayTouch touch;

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
#define DAC_FULL_SCALE  19650   // tune until 100% = exactly 6.000V

// ==========================
// SETUP
// ==========================
void setup() {
    Serial.begin(115200);

    ads.begin(0x48);

    // DAC
    Wire2.begin();   // SDA2 & SCL2 pins  (Changed in DFRobot library) 
    Serial.println("Initializing DAC...");

    if (GP8211S.begin() == 0) {
        GP8211S.setDACOutRange(GP8211S.eOutputRange10V);
        dacOK = true;
        Serial.println("DAC ready!");
    } else {
        Serial.println("DAC not found (continuing safely)");
    }

    display.begin();
    display.setRotation(1);
    touch.begin();

    for (int i = 0; i < 2; i++) {
    sliders[i].value = 0;
    sliders[i].displayedValue = 0;
    sliders[i].lastDrawValue = -1;
}

    drawStaticScreen();
}

// ==========================
// MAIN LOOP
// ==========================
void loop() {

    handleTouch();

    // If screen is OFF → do nothing else
    if (!screenOn) {
        delay(20);
        return;
    }

    unsigned long currentMillis = millis();

    // Voltage reading
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        int16_t adc0 = ads.readADC_SingleEnded(0);
        voltage = ((adc0 + calib) * 0.1875) / 1000.0;
        vbat = voltage / Radjust;

        if (vbat < 0.1) vbat = 0.01;

        sliders[0].value = constrain(vbat, 0, sliders[0].maxValue);
    }

    updateGauges();
    updateDAC();

    delay(10);
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

    float percent = constrain(sliders[1].value, 0, 100);

    // Scale 0–100% → 0–6V
    //uint16_t dacValue = (percent / 100.0) * (32767 * 0.6);

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
    display.fillScreen(DARK_GREY);

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

        if (abs(diff) <= 0.15) {
            s.displayedValue = s.value;  // snap to target
        } else {
            s.displayedValue += diff * 0.1;
        }

        if (abs(s.displayedValue - s.lastDrawValue) >= 0.2) {
            drawGaugeDynamic(s);
            s.lastDrawValue = s.displayedValue;
        }
    }



    //old code
    // for (int i = 0; i < 2; i++) {
    //     Slider& s = sliders[i];

    //     if (abs(s.displayedValue - s.value) > 0.05) {
    //         s.displayedValue += (s.value - s.displayedValue) * 0.1;
    //     }

    //     if (abs(s.displayedValue - s.lastDrawValue) >= 0.2) {
    //         drawGaugeDynamic(s);
    //         s.lastDrawValue = s.displayedValue;
    //     }
    // }




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

    display.fillCircle(s.x, s.y, s.radius - 35, DARK_GREY);

    int x2 = s.x + cos(rad) * (s.radius - 40);
    int y2 = s.y + sin(rad) * (s.radius - 40);

    display.drawLine(s.x, s.y, x2, y2, RED);
    display.drawCircle(s.x, s.y, 5, RED);

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.1f %s", s.displayedValue, s.unit);

    display.setTextSize(3);
    display.setTextColor(WHITE, DARK_GREY);
    display.setCursor(s.x - 40, s.y);
    display.print(buffer);
}

// ==========================
// TOUCH
// ==========================
void handleTouch() {
    GDTpoint_t p[1];
    if (!touch.getTouchPoints(p)) return;

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

    // Original controls
    int upX = (SCREEN_WIDTH - BTN_WIDTH) / 2;
    int upY = 20;
    int downX = upX;
    int downY = SCREEN_HEIGHT - BTN_HEIGHT - 20;

    if (tx >= upX && tx <= upX + BTN_WIDTH &&
        ty >= upY && ty <= upY + BTN_HEIGHT) {
        sliders[1].value = min(sliders[1].value + 1, 100.0);
    }

    if (tx >= downX && tx <= downX + BTN_WIDTH &&
        ty >= downY && ty <= downY + BTN_HEIGHT) {
        sliders[1].value = max(sliders[1].value - 1, 0.0);
    }
}