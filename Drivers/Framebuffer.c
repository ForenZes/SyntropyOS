#include "Framebuffer.h"
#include "Kernel.h"

uint16_t SyntropyFrameBuffer[FB_WIDTH * FB_HEIGHT];

FbPoint FrameBufferCenter(int w, int h){
    FbPoint p;
    p.x = (FB_WIDTH - w) / 2;
    p.y = (FB_HEIGHT - h) / 2;
    return p;
}

FbPoint FrameBufferCenterInBox(FbBox parent, int w, int h){
    FbPoint p;
    p.x = parent.x + (parent.w - w) / 2;
    p.y = parent.y + (parent.h - h) / 2;
    return p;
}

void FrameBufferIcon(int x, int y, const uint32_t *rows, int w, int h, uint16_t color, int scale){
    for(int r = 0; r < h; r++){
        uint32_t bits = rows[r];
        for(int c = 0; c < w; c++){
            if(bits & (1u << c)){
                if(scale <= 1){
                    FrameBufferPixel(x + c, y + r, color);
                } else{
                    FrameBufferFillRect(x + c * scale, y + r * scale, scale, scale, color);
                }
            }
        }
    }
}

int FrameBufferCenterX(int w){
    return (FB_WIDTH - w) / 2;
}

int FrameBufferCenterY(int h){
    return (FB_HEIGHT - h) / 2;
}

void FrameBufferClear(uint16_t color){
    for(int i = 0; i < FB_WIDTH * FB_HEIGHT; i++){
        SyntropyFrameBuffer[i] = color;
    }
}

void FrameBufferPixel(int x, int y, uint16_t color){
    if(x < 0 || y < 0 || x >= FB_WIDTH || y >= FB_HEIGHT){
        return;
    }
    SyntropyFrameBuffer[y * FB_WIDTH + x] = color;
}

void FrameBufferHLine(int x, int y, int w, uint16_t color){
    if(y < 0 || y >= FB_HEIGHT){
        return;
    }

    int x1 = x + w;
    if(x < 0){
        x = 0;
    }

    if(x1 > FB_WIDTH){
        x1 = FB_WIDTH;
    }

    uint16_t *p = &SyntropyFrameBuffer[y * FB_WIDTH + x];
    while(x < x1){
        *p++ = color;
        x++;
    }
}

void FrameBufferVLine(int x, int y, int h, uint16_t color){
    if(x < 0 || x >= FB_WIDTH){
        return;
    }

    int y1 = y + h;
    if(y < 0){
        y = 0;
    }

    if(y1 > FB_HEIGHT){
        y1 = FB_HEIGHT;
    }
    uint16_t *p = &SyntropyFrameBuffer[y * FB_WIDTH + x];
    while(y < y1){
        *p = color;
        p += FB_WIDTH;
        y++;
    }
}

void FrameBufferLine(int x0, int y0, int x1, int y1, uint16_t color){
    int dx = x1 - x0;
    if(dx < 0){
        dx = -dx;
    }
    
    int dy = y1 - y0;

    if(dy < 0){
        dy = -dy;
    }

    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while(1){
        FrameBufferPixel(x0, y0, color);
        if(x0 == x1 && y0 == y1){
            break;
        }

        int e2 = err * 2;
        if(e2 > -dy){
            err -= dy;
            x0 += sx;
        }

        if(e2 < dx){
            err += dx;
            y0 += sy;
        }
    }
}

void FrameBufferFillRect(int x, int y, int w, int h, uint16_t color){
    int x1 = x + w;
    int y1 = y + h;
    if(x < 0){
        x = 0;
    }

    if(y < 0){
        y = 0;
    }

    if(x1 > FB_WIDTH){
        x1 = FB_WIDTH;
    }

    if(y1 > FB_HEIGHT){
        y1 = FB_HEIGHT;
    }

    for(int yy = y; yy < y1; yy++){
        uint16_t *p = &SyntropyFrameBuffer[yy * FB_WIDTH + x];
        for(int xx = x; xx < x1; xx++){
            *p++ = color;
        }
    }
}

void FrameBufferChar(int x, int y, char c, uint16_t color, int scale){
    if(c < 0x20 || c > 0x7E){
        c = '?';
    }

    const uint8_t *g = defaultFontData[(int)c - 0x20];
    for(int row = 0; row < 8; row++){
        uint8_t bits = g[row];
        for(int col = 0; col < 8; col++){
            if(bits & (1 << col)){
                if(scale <= 1){
                    FrameBufferPixel(x + col, y + row, color);
                } else{
                    FrameBufferFillRect(x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
    }
}

void FrameBufferText(int x, int y, const char *s, uint16_t color, int scale){
    int step = 8 * scale;
    int cx = x;

    while(*s){
        if(*s == '\n'){
            y += step;
            cx = x;
        } else{
            FrameBufferChar(cx, y, *s, color, scale);
            cx += step;
        }
        s++;
    }
}

int FrameBufferTextWidth(const char *s, int scale){
    int step = 8 * scale;
    int w = 0;
    int best = 0;

    while(*s){
        if(*s == '\n'){
            if(w > best){
                best = w;
            }
            w = 0;
        } else{
            w += step;
        }
        s++;
    }

    if(w > best){
        best = w;
    }
    return best;
}

int FrameBufferTextHeight(int scale){
    return 8 * scale;
}

void FrameBufferRect(int x, int y, int w, int h, uint16_t color){
    FrameBufferHLine(x, y, w, color);
    FrameBufferHLine(x, y + h - 1, w, color);
    FrameBufferVLine(x, y, h, color);
    FrameBufferVLine(x + w - 1, y, h, color);
}

void FrameBufferFillCircle(int cx, int cy, int r, uint16_t color){
    int x = r;
    int y = 0;
    int err = 1 - r;

    while(x >= y){
        FrameBufferHLine(cx - x, cy + y, 2 * x + 1, color);
        FrameBufferHLine(cx - x, cy - y, 2 * x + 1, color);
        FrameBufferHLine(cx - y, cy + x, 2 * y + 1, color);
        FrameBufferHLine(cx - y, cy - x, 2 * y + 1, color);
        y++;

        if(err < 0){
            err += 2 * y + 1;
        } else{
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void FrameBufferCircle(int cx, int cy, int r, uint16_t color){
    int x = r;
    int y = 0;
    int err = 1 - r;

    while(x >= y){
        FrameBufferPixel(cx + x, cy + y, color);
        FrameBufferPixel(cx + y, cy + x, color);
        FrameBufferPixel(cx - y, cy + x, color);
        FrameBufferPixel(cx - x, cy + y, color);
        FrameBufferPixel(cx - x, cy - y, color);
        FrameBufferPixel(cx - y, cy - x, color);
        FrameBufferPixel(cx + y, cy - x, color);
        FrameBufferPixel(cx + x, cy - y, color);
        y++;
        if(err < 0){
            err += 2 * y + 1;
        } else{
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void FrameBufferFlush(void){
    lcdWindow(0, 0, FB_WIDTH - 1, FB_HEIGHT - 1);
    lcdWritePixels(SyntropyFrameBuffer, FB_WIDTH * FB_HEIGHT);
}

void FrameBufferFlushRect(int x, int y, int w, int h){
    if(x < 0){
        w += x;
        x = 0;
    }

    if(y < 0){
        h += y;
        y = 0;
    }

    if(x + w > FB_WIDTH){
        w = FB_WIDTH - x;
    }

    if(y + h > FB_HEIGHT){
        h = FB_HEIGHT - y;
    }

    if(w <= 0 || h <= 0){
        return;
    }

    lcdWindow(x, y, x + w - 1, y + h - 1);

    for(int yy = y; yy < y + h; yy++){
        lcdWritePixels(&SyntropyFrameBuffer[yy * FB_WIDTH + x], w);
    }
}
