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

typedef struct {
    char c;
    uint8_t rows[5]; // 5x5 glyph, MSB->LSB
} glyph5_t;

static const glyph5_t g5[] = {
    {'A',{0b01110,0b10001,0b11111,0b10001,0b10001}},
    {'D',{0b11110,0b10001,0b10001,0b10001,0b11110}},
    {'C',{0b01110,0b10001,0b10000,0b10001,0b01110}},
    {'E',{0b11111,0b10000,0b11110,0b10000,0b11111}},
    {'G',{0b01110,0b10001,0b10011,0b10001,0b01110}},
    {'I',{0b11111,0b00100,0b00100,0b00100,0b11111}},
    {'L',{0b10000,0b10000,0b10000,0b10000,0b11111}},
    {'M',{0b10001,0b11011,0b10101,0b10001,0b10001}},
    {'N',{0b10001,0b11001,0b10101,0b10011,0b10001}},
    {'O',{0b01110,0b10001,0b10001,0b10001,0b01110}},
    {'P',{0b11110,0b10001,0b11110,0b10000,0b10000}},
    {'R',{0b11110,0b10001,0b11110,0b10100,0b10010}},
    {'S',{0b01111,0b10000,0b01110,0b00001,0b11110}},
    {'T',{0b11111,0b00100,0b00100,0b00100,0b00100}},
    {'U',{0b10001,0b10001,0b10001,0b10001,0b01110}},
    {'V',{0b10001,0b10001,0b10001,0b01010,0b00100}},
    {':',{0b00000,0b00100,0b00000,0b00100,0b00000}},
    {' ',{0b00000,0b00000,0b00000,0b00000,0b00000}},
};

static const uint8_t *glyph5_find(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    for (unsigned i = 0; i < sizeof(g5)/sizeof(g5[0]); i++) {
        if (g5[i].c == c) return g5[i].rows;
    }
    return g5[sizeof(g5)/sizeof(g5[0]) - 1].rows; // space
}

void l_draw_text(lfb_t *lfb, int x, int y, const char *text, int scale, uint32_t c) {
    if (scale < 1) scale = 1;
    const int w = 5;
    const int h = 5;
    const int spacing = 1;
    for (const char *p = text; *p; p++) {
        const uint8_t *rows = glyph5_find(*p);
        for (int gy = 0; gy < h; gy++) {
            for (int gx = 0; gx < w; gx++) {
                if (rows[gy] & (1u << (w - 1 - gx))) {
                    int px = x + gx * scale;
                    int py = y + gy * scale;
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            l_putpix(lfb, px + sx, py + sy, c);
                        }
                    }
                }
            }
        }
        x += (w + spacing) * scale;
    }
}
