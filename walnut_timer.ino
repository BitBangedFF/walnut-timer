// TODO
// - reasonable conversion and display of seconds/minutes/etc
// - hold for fast +/-
// - list hw/etc/diagram
// - cleanup pitches.h
// - wdt
// - use timer1 for time counter or just elapsed time?
// could use a library, but easier to just setup the registers
// http://www.robotshop.com/letsmakerobots/arduino-101-timers-and-interrupts

#include <stdint.h>
#include <limits.h>
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

#define DEFAULT_TIMER_SETPOINT_SEC (30UL)

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
Bounce sw0_db = Bounce();
Bounce sw1_db = Bounce();
Bounce sw2_db = Bounce();
elapsedMillis timer_elapsed;
elapsedMillis adj_hold_elapsed;

static unsigned long timer_setpoint;
static unsigned long timer_paused_value;
static bool timer_running;
static bool is_adjusting;

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

    if(sw2_db.read() == false)
    {
        is_adj = true;

        if(sw2_db.fell() == true)
        {
            adj_hold_elapsed = 0;
            adj = SEC_TO_MS(1);
        }
        else if(adj_hold_elapsed > BUTTON_HOLD_INTERVAL)
        {
            adj_hold_elapsed = 0;
            adj = SEC_TO_MS(2);
        }

        if(adj != 0)
        {
            if(timer_setpoint < (ULONG_MAX - 1))
            {
                timer_setpoint += adj;
            }
        }
    }
    else if(sw1_db.read() == false)
    {
        is_adj = true;

        if(sw1_db.fell() == true)
        {
            adj_hold_elapsed = 0;
            adj = SEC_TO_MS(1);
        }
        else if(adj_hold_elapsed > BUTTON_HOLD_INTERVAL)
        {
            adj_hold_elapsed = 0;
            adj = SEC_TO_MS(2);
        }

        if(adj != 0)
        {
            if(timer_setpoint > SEC_TO_MS(1))
            {
                timer_setpoint -= adj;
            }
        }
    }

    return is_adj;
}

static bool check_for_start_stop(void)
{
    bool changed = false;

    if((sw0_db.read() == false) && (sw0_db.fell() == true))
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

static void render_timer_setpoint(void)
{
    seg_display.print(
            MS_TO_SEC(timer_setpoint),
            DEC);

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

    seg_display.print(
            MS_TO_SEC(ms_val),
            DEC);

    seg_display.drawColon(true);
    seg_display.writeDisplay();
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
    sw0_db.interval(DEBOUNCE_INTERVAL);
    sw1_db.attach(PIN_SW1);
    sw1_db.interval(DEBOUNCE_INTERVAL);
    sw2_db.attach(PIN_SW2);
    sw2_db.interval(DEBOUNCE_INTERVAL);

    timer_setpoint = SEC_TO_MS(DEFAULT_TIMER_SETPOINT_SEC);
    timer_elapsed = 0;
    timer_paused_value = 0;
    timer_running = false;
    is_adjusting = true;

    seg_display.begin(DISPLAY_ADDRESS);
    seg_display.setBrightness(BRIGHTNESS);
    seg_display.clear();
    seg_display.writeDisplay();

    delay(STARTUP_DELAY);

    render_timer_setpoint();
}

void loop()
{
    sw0_db.update();
    sw1_db.update();
    sw2_db.update();

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
            seg_display.blinkRate(1);
            timer_running = false;
            is_adjusting = true;
            render_timer_setpoint();
            //play_melody();
            delay(3000);
            seg_display.blinkRate(0);
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
