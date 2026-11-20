// Panel language codes (description in ui_lang.h).
#include "ui_lang.h"

#include <string.h>

namespace uilang {

static bool isLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

bool wellFormed(const char* code) {
    if (code == nullptr) return false;
    size_t n = strlen(code);
    if (n < 2 || n > CODE_MAX) return false;
    if (!isLetter(code[0]) || code[n - 1] == '-') return false;
    for (size_t i = 0; i < n; i++) {
        char c = code[i];
        if (!isLetter(c) && !(c >= '0' && c <= '9') && c != '-') return false;
    }
    return true;
}

bool inList(const char* code, const char* list) {
    if (!wellFormed(code) || list == nullptr) return false;
    size_t n = strlen(code);
    const char* p = list;
    while (*p != 0) {
        const char* end = strchr(p, ',');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len == n && strncmp(p, code, n) == 0) return true;
        if (end == nullptr) break;
        p = end + 1;
    }
    return false;
}

}  // namespace uilang
