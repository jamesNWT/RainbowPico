#include "Color.h"
#include <math.h>

// Perceptual gamma - human brightness perception is nonlinear (~2.2 exponent is standard)
uint8_t apply_gamma(uint8_t linear_value) {
    return (uint8_t)(powf(linear_value / 255.0f, 2.2f) * 255.0f + 0.5f);
}

// Precompute a gamma correction table to avoid calling powf every main loop iteration.
uint8_t GAMMA_TABLE[256];

void compute_gamma_table(void) {
    for (int i = 0; i < 256; ++i) {
        GAMMA_TABLE[i] = apply_gamma(i);
    }
}

uint8_t correct_channel(uint8_t raw, float channel_scale) {
    uint8_t scaled = (uint8_t)(raw * channel_scale);
    return GAMMA_TABLE[scaled];
}