#include "audio_out.h"

#include <driver/i2s_std.h>
#include <math.h>

#include "config.h"

static volatile bool s_gate = false;
static volatile uint16_t s_freq = 1000;
static volatile uint8_t s_volume = AUDIO_VOLUME_DEFAULT;
static i2s_chan_handle_t s_tx = nullptr;

static const int BLOCK = 128;                 // samples per block: approx. 6 ms at 22050 Hz

// Sound modelled on the car's buzzer (module under the steering wheel), measured 16.11.2026 from
// in-cabin recordings (phone microphone; recordings not included in the repository):
//  - fundamental 498 Hz, harmonics relative to it: 2x 0.22; 3x 0.30; 4x 0.07; 5x 0.05,
//  - beep approx. 50 ms: rises in approx. 10 ms, decays to approx. half during the beep, dies out in approx. 15 ms.
static const float ATTACK_MS = 3.0f;          // rise time (short: a distinct start of the beep)
static const float BODY_TAU_MS = 30.0f;       // how fast the tone decays during the beep
static const float SUSTAIN = 0.55f;           // level it decays to (a continuous tone plays at this level)
static const float RELEASE_TAU_MS = 6.0f;     // fade-out after switching off
// Harmonics 2x..7x relative to the fundamental, AFTER CORRECTION FOR THE SPEAKER (calibration 16.11.2026).
// Pattern from the car recording: 2x 0.23; 3x 0.26; 4x 0.05; 5x 0.04; 7x 0.03.
// The speaker from the Simple Soft kit plays 500 Hz much weaker than 1-3 kHz: a phone recording showed
// that relative to the fundamental it amplifies 2x approx. 3.9 times, 3x 9.7, 4x 8.3, 5x 12.7, 7x 4.3.
// To get the car's proportions out of the speaker, we feed the harmonics divided by these numbers.
// After changing the speaker or the enclosure the calibration has to be repeated.
// Harmonics 2x..7x relative to the 498 Hz fundamental, AFTER CORRECTION FOR THE SPEAKER (calibration 16.11.2026).
// Pattern from the car recording: 2x 0.23; 3x 0.26; 4x 0.05; 5x 0.04; 7x 0.03.
// The speaker from the Simple Soft kit plays 500 Hz much weaker than 1-3 kHz: relative to the fundamental
// it amplifies 2x approx. 3.9 times, 3x 9.7, 4x 8.3, 5x 12.7, 7x 4.3. We feed the harmonics divided by
// these numbers. After changing the speaker or the enclosure the calibration has to be repeated.
// A test with a tone one octave higher (996 Hz, harmonics 0.11 / 0.07) gave 90 dB at 10 cm at 50 %
// instead of 80 dB at 80 %; design decision (16.11.2026): the original tone stays.
static const int HARM_N = 6;
static const float HARM[HARM_N] = {0.060f, 0.027f, 0.006f, 0.003f, 0.0f, 0.007f};
static const float HARM_NORM = 1.0f / 1.11f;  // the sum of components at the peak does not exceed full scale

// Background task: continuously feeds samples to I2S. When the tone is off, it feeds zeros,
// so the I2S clocks run all the time and the amplifier does not lose synchronisation.
static void audioTask(void*) {
    static int16_t buf[BLOCK * 2];            // two channels: left and right carry the same
    float phase = 0.0f;
    float amp = 0.0f;                         // current volume 0..1
    float ramp = 0.0f;                        // rise 0..1
    float body = 1.0f;                        // decay during the beep 1..SUSTAIN
    bool prevGate = false;
    const float attackStep = 1.0f / (ATTACK_MS * 0.001f * AUDIO_SAMPLE_RATE);
    const float bodyMul = expf(-1.0f / (BODY_TAU_MS * 0.001f * AUDIO_SAMPLE_RATE));
    const float releaseMul = expf(-1.0f / (RELEASE_TAU_MS * 0.001f * AUDIO_SAMPLE_RATE));
    const float twoPi = 2.0f * (float)M_PI;
    for (;;) {
        bool gate = s_gate;
        float phaseStep = twoPi * (float)s_freq / (float)AUDIO_SAMPLE_RATE;
        float peak = 32767.0f * (float)s_volume / 100.0f * HARM_NORM;
        if (gate && !prevGate) {              // start of a beep
            if (amp == 0.0f) phase = 0.0f;    // from silence we start at the wave zero (no click)
            ramp = amp;                       // when the previous one is still fading, we start from its level
            body = 1.0f;
        }
        prevGate = gate;
        for (int i = 0; i < BLOCK; i++) {
            if (gate) {
                ramp = fminf(1.0f, ramp + attackStep);
                body = SUSTAIN + (body - SUSTAIN) * bodyMul;
                amp = ramp * body;
            } else if (amp > 0.0f) {
                amp *= releaseMul;
                if (amp < 0.001f) amp = 0.0f; // full silence
            }
            float w = sinf(phase);
            for (int h = 0; h < HARM_N; h++) w += HARM[h] * sinf((float)(h + 2) * phase);
            int16_t v = (int16_t)(w * amp * peak);
            buf[2 * i] = v;
            buf[2 * i + 1] = v;
            phase += phaseStep;
            if (phase > twoPi) phase -= twoPi;
        }
        size_t written = 0;
        i2s_channel_write(s_tx, buf, sizeof(buf), &written, portMAX_DELAY);
    }
}

bool audioBegin() {
    pinMode(PIN_AMP_SD, OUTPUT);
    digitalWrite(PIN_AMP_SD, LOW);            // amplifier asleep until the first sound

    i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ch.dma_desc_num = 3;                      // small buffer = small beep latency (approx. 17 ms)
    ch.dma_frame_num = BLOCK;
    ch.auto_clear = true;                     // with no data silence instead of repeating the buffer
    if (i2s_new_channel(&ch, &s_tx, nullptr) != ESP_OK) return false;

    // Philips I2S format, 16 bits, two channels: this is how the MAX98357A reads data.
    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)AUDIO_SAMPLE_RATE);
    i2s_std_slot_config_t slot = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    i2s_std_config_t std = {};
    std.clk_cfg = clk;
    std.slot_cfg = slot;
    std.gpio_cfg.mclk = I2S_GPIO_UNUSED;      // the MAX98357A does not need a master clock
    std.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
    std.gpio_cfg.ws = (gpio_num_t)PIN_I2S_LRC;
    std.gpio_cfg.dout = (gpio_num_t)PIN_I2S_DIN;
    std.gpio_cfg.din = I2S_GPIO_UNUSED;
    if (i2s_channel_init_std_mode(s_tx, &std) != ESP_OK) return false;
    if (i2s_channel_enable(s_tx) != ESP_OK) return false;

    return xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, nullptr, 0) == pdPASS;
}

void audioSetTone(bool on, uint16_t freqHz) {
    if (on && freqHz > 0) s_freq = freqHz;
    s_gate = on;
}

void audioSetVolume(uint8_t percent) { s_volume = percent > AUDIO_VOLUME_MAX ? AUDIO_VOLUME_MAX : percent; }

uint8_t audioVolume() { return s_volume; }

void audioSetAmp(bool on) {
    // High level 3.3 V on SD: amplifier on (above 1.4 V the left channel plays;
    // both channels carry the same signal, so the channel selection does not matter).
    digitalWrite(PIN_AMP_SD, on ? HIGH : LOW);
}
