// TODO
// - cleanup pitches.h

#include <stdint.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

#include "pitches.h"

#define DISPLAY_ADDRESS (0x70)
#define BRIGHTNESS (15)

#define AMP_ENABLE_DELAY (5)

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

        // plus 30 % of duration
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
    
    seg_display.begin(DISPLAY_ADDRESS);
    seg_display.setBrightness(BRIGHTNESS);
}

void loop()
{
    //seg_display.writeDisplay();

    play_melody();
    
    delay(5000);
}
