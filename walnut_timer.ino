// TODO
// - cleanup pitches.h
// - use timer1 for time counter
// could use a library, but easier to just setup the registers
// http://www.robotshop.com/letsmakerobots/arduino-101-timers-and-interrupts

#include <stdint.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include <Bounce2.h>
#include "pitches.h"

#define DISPLAY_ADDRESS (0x70)
#define BRIGHTNESS (15)

#define AMP_ENABLE_DELAY (5)
#define DEBOUNCE_DELAY (5)

#define PIN_AMP_SHUTDOWN (2)
#define PIN_AUDIO (3)
#define PIN_SW0 (4)
#define PIN_SW1 (5)
#define PIN_SW2 (6)

// SDA (D) - A4
// SCL (C) - A5

#define NOTE_TYPE_TO_DURATION(t) (1000/t)

typedef struct
{
    uint16_t freq;
    uint16_t duration;
} melody_tone_s;

Adafruit_7segment seg_display = Adafruit_7segment();
Bounce sw0_db = Bounce();
Bounce sw1_db = Bounce();
Bounce sw2_db = Bounce();

static const melody_tone_s melody_tones[] =
{
    {NOTE_C4, NOTE_TYPE_TO_DURATION(4)},
    {NOTE_G3, NOTE_TYPE_TO_DURATION(8)},
    {NOTE_G3, NOTE_TYPE_TO_DURATION(8)},
    {NOTE_A3, NOTE_TYPE_TO_DURATION(4)},
    {NOTE_G3, NOTE_TYPE_TO_DURATION(4)},
    {NOTE_OFF, NOTE_TYPE_TO_DURATION(4)},
    {NOTE_B3, NOTE_TYPE_TO_DURATION(4)},
    {NOTE_C4, NOTE_TYPE_TO_DURATION(4)}
};

static void disable_amp(void)
{
    digitalWrite(PIN_AUDIO, LOW);
    digitalWrite(PIN_AMP_SHUTDOWN, LOW);
}

static void enable_amp(void)
{
    digitalWrite(PIN_AUDIO, LOW);
    digitalWrite(PIN_AMP_SHUTDOWN, HIGH);
    delay(AMP_ENABLE_DELAY);
}

static void play_melody(void)
{
    const uint16_t len = (sizeof(melody_tones) / sizeof(melody_tones[1]));

    enable_amp();

    uint16_t idx;
    for(idx = 0; idx < len; idx += 1)
    {
        tone(PIN_AUDIO, melody_tones[idx].freq, melody_tones[idx].duration);

        // plus 30 % of duration between notes
        const float delay_f = ((float) melody_tones[idx].duration) * 1.30f;
        delay((unsigned long) delay_f);

        noTone(PIN_AUDIO);
    }

    disable_amp();
}

void setup()
{
    pinMode(PIN_AUDIO, OUTPUT);
    digitalWrite(PIN_AUDIO, LOW);

    pinMode(PIN_AMP_SHUTDOWN, OUTPUT);
    disable_amp();

    pinMode(PIN_SW0, INPUT_PULLUP);
    pinMode(PIN_SW1, INPUT_PULLUP);
    pinMode(PIN_SW2, INPUT_PULLUP);

    sw0_db.attach(PIN_SW0);
    sw0_db.interval(DEBOUNCE_DELAY);
    sw1_db.attach(PIN_SW1);
    sw1_db.interval(DEBOUNCE_DELAY);
    sw2_db.attach(PIN_SW2);
    sw2_db.interval(DEBOUNCE_DELAY);
    
    seg_display.begin(DISPLAY_ADDRESS);
    seg_display.setBrightness(BRIGHTNESS);
}

void loop()
{
    //seg_display.writeDisplay();

    // returns true on state change
    sw0_db.update();
    sw1_db.update();
    sw2_db.update();

    // time +/- should show/adjust the time until timer is started
    // when timer is running, time counts up, pause on press, resume on press, reset on hold?
    
    // sw0_db.read()

    play_melody();
    
    delay(5000);
}
