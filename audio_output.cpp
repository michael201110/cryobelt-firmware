#include "audio_output.h"

#include <driver/i2s.h>
#include <math.h>

#include "config.h"
#include "pins.h"

namespace {

constexpr i2s_port_t AUDIO_I2S_PORT = I2S_NUM_0;
constexpr size_t FRAMES_PER_BLOCK = 64;

} // namespace

bool AudioOutput::begin() {
  if (!AUDIO_HARDWARE_REWORKED) return false;

  const i2s_config_t config = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = AUDIO_SAMPLE_RATE_HZ,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 128,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0,
  };

  const i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = PIN_AUDIO_BCLK,
    .ws_io_num = PIN_AUDIO_LRCLK,
    .data_out_num = PIN_AUDIO_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE,
  };

  if (i2s_driver_install(AUDIO_I2S_PORT, &config, 0, nullptr) != ESP_OK) {
    return false;
  }
  if (i2s_set_pin(AUDIO_I2S_PORT, &pins) != ESP_OK) {
    i2s_driver_uninstall(AUDIO_I2S_PORT);
    return false;
  }

  i2s_zero_dma_buffer(AUDIO_I2S_PORT);
  ready_ = true;
  return true;
}

void AudioOutput::playTone(uint16_t frequencyHz,
                           uint16_t durationMs,
                           uint8_t volumePercent) {
  if (!ready_ || frequencyHz == 0 || durationMs == 0) return;

  const uint8_t volume = constrain(volumePercent, 0, 100);
  const int16_t amplitude = static_cast<int16_t>(32767L * volume / 100L);
  const uint32_t frameCount =
    static_cast<uint32_t>(AUDIO_SAMPLE_RATE_HZ) * durationMs / 1000U;
  const float phaseStep =
    2.0f * PI * static_cast<float>(frequencyHz) / AUDIO_SAMPLE_RATE_HZ;
  float phase = 0.0f;
  uint32_t framesWritten = 0;
  int16_t samples[FRAMES_PER_BLOCK * 2];

  while (framesWritten < frameCount) {
    const size_t frames = min(
      FRAMES_PER_BLOCK,
      static_cast<size_t>(frameCount - framesWritten)
    );
    for (size_t frame = 0; frame < frames; ++frame) {
      const int16_t sample = static_cast<int16_t>(sinf(phase) * amplitude);
      samples[frame * 2] = sample;
      samples[frame * 2 + 1] = sample;
      phase += phaseStep;
      if (phase >= 2.0f * PI) phase -= 2.0f * PI;
    }

    size_t bytesWritten = 0;
    if (i2s_write(AUDIO_I2S_PORT,
                  samples,
                  frames * 2 * sizeof(int16_t),
                  &bytesWritten,
                  portMAX_DELAY) != ESP_OK) {
      break;
    }
    framesWritten += bytesWritten / (2 * sizeof(int16_t));
  }

  i2s_zero_dma_buffer(AUDIO_I2S_PORT);
}
