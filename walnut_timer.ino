// TODO
// - list hw/etc/diagram
// - cleanup pitches.h
// - wdt

#include <avr/wdt.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include <Bounce2.h>
#include <elapsedMillis.h>
#include "pitches.h"

#define DISPLAY_ADDRESS (0x70)
#define BRIGHTNESS (15)

#define STARTUP_DELAY (50UL)
#define AMP_ENABLE_DELAY (5UL)
#define RENDER_DELAY (10UL)

#define BUTTON_HOLD_INTERVAL (200UL)
#define DEBOUNCE_INTERVAL (5UL)

#define BUTTON_LONG_HOLD_COUNT (15UL)

// default is 5 minutes and 15 seconds == 315 seconds
#define DEFAULT_TIMER_SETPOINT_SEC (315UL)

#define WATCHDOG_TIMEOUT (WDTO_4S)

#define PIN_AMP_SHUTDOWN (2)
#define PIN_AUDIO (3)
#define PIN_SW0 (4)
#define PIN_SW1 (5)
#define PIN_SW2 (6)
#define PIN_SDA (A4)
#define PIN_SCL (A5)

#define NOTE_TYPE_TO_DUR(t) (1000UL/t)

#define SEC_TO_MS(s) (s*1000UL)
#define MS_TO_SEC(ms) (ms/1000UL)

typedef struct
{
    uint16_t freq;
    uint16_t dur;
} melody_tone_s;

Adafruit_7segment seg_display = Adafruit_7segment();
Bounce btn_start_stop = Bounce();
Bounce btn_adj_down = Bounce();
Bounce btn_adj_up = Bounce();
elapsedMillis timer_elapsed;
elapsedMillis adj_hold_elapsed;

static unsigned long timer_setpoint;
static unsigned long timer_paused_value;
static bool timer_running;
static bool is_adjusting;
static unsigned long adj_hold_count;

static const melody_tone_s melody_tones[] =
{
    {NOTE_C4, NOTE_TYPE_TO_DUR(4)},
    {NOTE_G3, NOTE_TYPE_TO_DUR(8)},
    {NOTE_G3, NOTE_TYPE_TO_DUR(8)},
    {NOTE_A3, NOTE_TYPE_TO_DUR(4)},
    {NOTE_G3, NOTE_TYPE_TO_DUR(4)},
    {NOTE_OFF, NOTE_TYPE_TO_DUR(4)},
    {NOTE_B3, NOTE_TYPE_TO_DUR(4)},
    {NOTE_C4, NOTE_TYPE_TO_DUR(4)}
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
        tone(PIN_AUDIO, melody_tones[idx].freq, melody_tones[idx].dur);

        // plus 30 % of duration between notes
        const float delay_f = ((float) melody_tones[idx].dur) * 1.30f;
        delay((unsigned long) delay_f);

        noTone(PIN_AUDIO);
    }

    disable_amp();
}

static bool check_for_timer_adjustment(void)
{
    bool is_adj = false;
    unsigned long adj = 0;

    if(btn_adj_up.read() == false)
    {
        is_adj = true;

        if(btn_adj_up.fell() == true)
        {
            adj_hold_elapsed = 0;
            adj_hold_count = 0;
            adj = SEC_TO_MS(1);
        }
        else if(adj_hold_elapsed > BUTTON_HOLD_INTERVAL)
        {
            adj_hold_elapsed = 0;
            adj_hold_count += 1;

            if(adj_hold_count >= BUTTON_LONG_HOLD_COUNT)
            {
                adj = SEC_TO_MS(30);
            }
            else
            {
                adj = SEC_TO_MS(2);
            }
        }

        if(adj != 0)
        {
            if(timer_setpoint < (ULONG_MAX - 1))
            {
                timer_setpoint += adj;
            }
        }
    }
    else if(btn_adj_down.read() == false)
    {
        is_adj = true;

        if(btn_adj_down.fell() == true)
        {
            adj_hold_elapsed = 0;
            adj_hold_count = 0;
            adj = SEC_TO_MS(1);
        }
        else if(adj_hold_elapsed > BUTTON_HOLD_INTERVAL)
        {
            adj_hold_elapsed = 0;
            adj_hold_count += 1;

            if(adj_hold_count >= BUTTON_LONG_HOLD_COUNT)
            {
                adj = SEC_TO_MS(30);
            }
            else
            {
                adj = SEC_TO_MS(2);
            }
        }

        if(adj != 0)
        {
            if(timer_setpoint > adj)
            {
                timer_setpoint -= adj;
            }
            else
            {
                if(timer_setpoint > SEC_TO_MS(1))
                {
                    timer_setpoint -= SEC_TO_MS(1);
                }
            }
        }
    }

    return is_adj;
}

static bool check_for_start_stop(void)
{
    bool changed = false;

    if((btn_start_stop.read() == false) && (btn_start_stop.fell() == true))
    {
        changed = true;

        if(timer_running == true)
        {
            timer_paused_value = timer_elapsed;
            timer_running = false;
        }
        else
        {
            timer_running = true;
            timer_elapsed = timer_paused_value;
            timer_paused_value = 0;
        }
    }

    return changed;
}

static void render_time(
        const unsigned long time)
{
    const unsigned long now = time / 1000;
    const unsigned long secs = now % 60;
    const unsigned long mins = (now / 60) % 60;

    char str[8];
    (void) snprintf(
            str,
            sizeof(str),
            "%02lu%02lu",
            mins,
            secs);

    seg_display.writeDigitNum(
            0,
            (uint8_t) (str[0] - '0'),
            false);
    seg_display.writeDigitNum(
            1,
            (uint8_t) (str[1] - '0'),
            false);
    seg_display.writeDigitNum(
            3,
            (uint8_t) (str[2] - '0'),
            false);
    seg_display.writeDigitNum(
            4,
            (uint8_t) (str[3] - '0'),
            false);
}

static void render_timer_setpoint(void)
{
    render_time(timer_setpoint);

    seg_display.drawColon(true);
    seg_display.writeDisplay();
}

static void render_timer_elapsed(void)
{
    unsigned long ms_val;

    if(timer_running == true)
    {
        if(timer_elapsed <= timer_setpoint)
        {
            ms_val = (timer_setpoint - timer_elapsed);
        }
        else
        {
            ms_val = 0;
        }
    }
    else
    {
        if(timer_paused_value <= timer_setpoint)
        {
            ms_val = (timer_setpoint - timer_paused_value);
        }
        else
        {
            ms_val = 0;
        }
    }

    render_time(ms_val);

    seg_display.drawColon(true);
    seg_display.writeDisplay();
}

void setup()
{
    wdt_disable();
    wdt_enable(WATCHDOG_TIMEOUT);
    wdt_reset();

    pinMode(PIN_AUDIO, OUTPUT);
    digitalWrite(PIN_AUDIO, LOW);

    pinMode(PIN_AMP_SHUTDOWN, OUTPUT);
    disable_amp();

    pinMode(PIN_SW0, INPUT_PULLUP);
    pinMode(PIN_SW1, INPUT_PULLUP);
    pinMode(PIN_SW2, INPUT_PULLUP);

    btn_start_stop.attach(PIN_SW0);
    btn_start_stop.interval(DEBOUNCE_INTERVAL);
    btn_adj_down.attach(PIN_SW1);
    btn_adj_down.interval(DEBOUNCE_INTERVAL);
    btn_adj_up.attach(PIN_SW2);
    btn_adj_up.interval(DEBOUNCE_INTERVAL);

    timer_setpoint = SEC_TO_MS(DEFAULT_TIMER_SETPOINT_SEC);
    timer_elapsed = 0;
    adj_hold_elapsed = 0;
    timer_paused_value = 0;
    timer_running = false;
    is_adjusting = true;
    adj_hold_count = 0;

    seg_display.begin(DISPLAY_ADDRESS);
    seg_display.setBrightness(BRIGHTNESS);
    seg_display.clear();
    seg_display.writeDisplay();

    delay(STARTUP_DELAY);

    render_timer_setpoint();
}

void loop()
{
    wdt_reset();

    btn_start_stop.update();
    btn_adj_down.update();
    btn_adj_up.update();

    const bool new_adjustment = check_for_timer_adjustment();

    if(new_adjustment == true)
    {
        timer_running = false;
        is_adjusting = true;
        timer_paused_value = 0;
    }

    if(check_for_start_stop() == true)
    {
        is_adjusting = false;
    }

    if(timer_running == true)
    {
        if(timer_elapsed >= timer_setpoint)
        {
            seg_display.blinkRate(HT16K33_BLINK_2HZ);
            timer_running = false;
            is_adjusting = true;
            render_timer_setpoint();
            play_melody();
            seg_display.blinkRate(HT16K33_BLINK_OFF);
        }
    }

    if(is_adjusting == false)
    {
        render_timer_elapsed();
        delay(RENDER_DELAY);
    }
    else
    {
        if(new_adjustment == true)
        {
            render_timer_setpoint();
        }
    }
}
