#include "font.h"

void l_draw_digit(lfb_t *lfb, int x0, int y0, int d, uint32_t c) {
    static const uint8_t glyphs[10][5] = {
        {0b111,0b101,0b101,0b101,0b111}, //0
        {0b010,0b110,0b010,0b010,0b111}, //1
        {0b111,0b001,0b111,0b100,0b111}, //2
        {0b111,0b001,0b111,0b001,0b111}, //3
        {0b101,0b101,0b111,0b001,0b001}, //4
        {0b111,0b100,0b111,0b001,0b111}, //5
        {0b111,0b100,0b111,0b101,0b111}, //6
        {0b111,0b001,0b001,0b001,0b001}, //7
        {0b111,0b101,0b111,0b101,0b111}, //8
        {0b111,0b101,0b111,0b001,0b111}, //9
    };
    if ((unsigned)d > 9u) return;
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 3; x++) {
            if (glyphs[d][y] & (1u << (2 - x))) l_putpix(lfb, x0 + x, y0 + y, c);
        }
    }
}

void l_draw_score(lfb_t *lfb, int x, int y, int score, uint32_t c) {
    if (score < 0) score = 0;
    int digits[8], n = 0;
    if (score == 0) { digits[n++] = 0; }
    while (score > 0 && n < 8) { digits[n++] = score % 10; score /= 10; }
    for (int i = n - 1; i >= 0; i--) {
        l_draw_digit(lfb, x, y, digits[i], c);
        x += 4; // logical pixels
    }
}
