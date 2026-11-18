#pragma once
#include <ctype.h>

// Small diagnostic text for the playback smoke test; no font dependency.
template<class Scene>
inline void drawStatusText(Scene *scene, const char *text, int y, int startX = 64, int maxChars = 98)
{
    static const unsigned char letters[26][5] = {
        {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
        {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
        {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
        {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
        {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
        {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
        {3,4,120,4,3},{97,81,73,69,67}
    };
    static const unsigned char digits[10][5] = {
        {62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},
        {24,20,18,127,16},{39,69,69,69,57},{62,73,73,73,50},{1,113,9,5,3},
        {54,73,73,73,54},{38,73,73,73,62}
    };
    // Compact controller symbols in the same 5x7 pixel grid:
    // up, down, left, right, triangle, square, circle, Options.
    static const unsigned char buttons[8][5]={
        {8,4,126,4,8},{8,16,63,16,8},{8,28,42,8,8},{8,8,42,28,8},
        {48,44,34,44,48},{62,34,34,34,62},{28,34,34,34,28},{42,42,42,42,42}
    };
    for (int n = 0; text[n] && n < maxChars; ++n)
    {
        const unsigned char c = (unsigned char)toupper((unsigned char)text[n]);
        for (int x = 0; x < 5; ++x)
        {
            unsigned char bits = c >= 'A' && c <= 'Z' ? letters[c-'A'][x]
                               : c >= '0' && c <= '9' ? digits[c-'0'][x]
                               : c >= 128 && c < 136 ? buttons[c-128][x]
                               : c == '-' ? 8 : c == '.' ? 64
                               : c == '/' ? (64 >> x) : c == ':' ? (x == 2 ? 20 : 0) : 0;
            for (int row = 0; row < 7; ++row)
                if (bits & (1 << row))
                    scene->DrawRectangle(startX + n * 18 + x * 3, y + row * 3, 3, 3, Color{160,180,190});
        }
    }
}
