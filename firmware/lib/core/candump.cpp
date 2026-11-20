#include "candump.h"

#include <stdlib.h>
#include <string.h>

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parseCandumpLine(const char* line, CanFrame& out, double* timeSec) {
    if (timeSec) *timeSec = -1.0;
    const char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#') return false;                // comment (e.g. a "# KROK ..." step marker)
    if (*p == '(') {
        if (timeSec) *timeSec = atof(p + 1);
        p = strchr(p, ')');
        if (!p) return false;
        p++;
    }
    const char* hash = strchr(p, '#');
    if (!hash) return false;
    // identifier: hexadecimal digits directly before the '#' character
    const char* idStart = hash;
    while (idStart > p && hexVal(*(idStart - 1)) >= 0) idStart--;
    if (idStart == hash || hash - idStart > 3) return false;   // 11-bit frames only
    uint32_t id = 0;
    for (const char* q = idStart; q < hash; q++) id = (id << 4) | (uint32_t)hexVal(*q);

    uint8_t len = 0;
    const char* d = hash + 1;
    while (hexVal(d[0]) >= 0 && hexVal(d[1]) >= 0) {
        if (len >= 8) return false;
        out.data[len++] = (uint8_t)((hexVal(d[0]) << 4) | hexVal(d[1]));
        d += 2;
    }
    out.id = id;
    out.len = len;
    return true;
}
