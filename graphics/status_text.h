#pragma once
#include <ctype.h>
#include "ui_style.h"

// Small diagnostic text for the playback smoke test; no font dependency.
template<class Scene>
inline void drawStatusText(Scene *scene, const char *text, int y, int startX = 64, int maxChars = 98, Color color = Color{160,180,190})
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
    // Play, pause, check, favorite star, folder: same 5x7 pixel grid.
    static const unsigned char media[5][5]={{127,62,28,8,0},{127,127,0,127,127},{16,32,16,8,4},{20,24,127,24,20},{126,67,66,66,126}};
    static const unsigned char terminalLetters[26][3]={
      {30,5,30},{31,21,10},{14,17,17},{31,17,14},{31,21,17},{31,5,1},{14,17,29},{31,4,31},{17,31,17},{8,16,15},{31,4,27},{31,16,16},{31,2,31},{31,6,31},{14,17,14},{31,5,2},{14,25,30},{31,13,22},{18,21,9},{1,31,1},{15,16,15},{7,24,7},{31,8,31},{27,4,27},{3,28,3},{25,21,19}
    };
    static const unsigned char terminalDigits[10][3]={{31,17,31},{18,31,16},{29,21,23},{21,21,31},{7,4,31},{23,21,29},{31,21,29},{1,29,3},{31,21,31},{23,21,31}};
    color=uiColor(color);const int font=currentUiSettings().font;
    for (int n = 0; text[n] && n < maxChars; ++n)
    {
        const unsigned char c = (unsigned char)toupper((unsigned char)text[n]);
        if(c>=141&&c<=143){
            const int xx=startX+n*18,yy=y+5;
            scene->DrawRectangle(xx,yy,17,2,color);scene->DrawRectangle(xx,yy+10,17,2,color);scene->DrawRectangle(xx,yy,2,12,color);scene->DrawRectangle(xx+15,yy,2,12,color);
            if(c==142)scene->DrawRectangle(xx+3,yy+3,5,6,color);else if(c==143)scene->DrawRectangle(xx+9,yy+3,5,6,color);else{scene->DrawRectangle(xx+5,yy+5,2,2,color);scene->DrawRectangle(xx+10,yy+5,2,2,color);}continue;
        }
        if(font==1&&((c>='A'&&c<='Z')||(c>='0'&&c<='9'))){for(int x=0;x<3;++x){unsigned bits=c>='A'?terminalLetters[c-'A'][x]:terminalDigits[c-'0'][x];for(int row=0;row<5;++row)if(bits&(1<<row))scene->DrawRectangle(startX+n*18+x*4+1,y+row*4+1,4,4,color);}continue;}
        for (int x = 0; x < 5; ++x)
        {
            unsigned char bits = c >= 'A' && c <= 'Z' ? letters[c-'A'][x]
                               : c >= '0' && c <= '9' ? digits[c-'0'][x]
                               : c >= 128 && c < 136 ? buttons[c-128][x]
                               : c >= 136 && c < 141 ? media[c-136][x]
                               : c == '-' ? 8 : c == '_' ? 64 : c == '>' ? (x == 1 ? 34 : x == 2 ? 20 : x == 3 ? 8 : 0) : c == '.' ? 64
                               : c == '/' ? (64 >> x) : c == ':' ? (x == 2 ? 20 : 0) : 0;
            for (int row = 0; row < 7; ++row)
                if (bits & (1 << row))
                {
                    const int xx=startX+n*18+x*3,yy=y+row*3;
                    if(font==2&&c<128){scene->DrawRectangle(xx+1,yy,1,3,color);scene->DrawRectangle(xx,yy+1,3,1,color);}
                    else scene->DrawRectangle(xx,yy,3,3,color);
                }
        }
    }
}
