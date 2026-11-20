// Settings backup (description in settings_backup.h): a small parser for ONE flat JSON object.
// No recursion, no allocation; every length is checked before copying.
#include "settings_backup.h"

#include <string.h>

namespace backup {

namespace {

struct Reader {
    const char* s;
    size_t n;
    size_t i;

    bool end() const { return i >= n; }
    char peek() const { return i < n ? s[i] : 0; }
    void skipSpace() {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
    }
    bool take(char c) {
        if (i < n && s[i] == c) {
            i++;
            return true;
        }
        return false;
    }
    bool literal(const char* word) {
        size_t k = strlen(word);
        if (n - i < k || strncmp(s + i, word, k) != 0) return false;
        i += k;
        return true;
    }
};

// String after the opening quote, up to the closing quote, into out (capacity cap including the zero).
// Printable ASCII only, no backslash. false = too long / character not allowed / no closing quote.
bool readString(Reader& r, char* out, size_t cap, bool& tooLong) {
    size_t k = 0;
    tooLong = false;
    while (!r.end()) {
        char c = r.s[r.i++];
        if (c == '"') {
            out[k] = 0;
            return true;
        }
        if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E || c == '\\') return false;
        if (k + 1 >= cap) {
            tooLong = true;
            return false;
        }
        out[k++] = c;
    }
    return false;
}

enum class Kind : uint8_t { STRING, NUMBER, TRUE_, FALSE_, NULL_, BAD };

}  // namespace

const char* errorName(ParseError e) {
    switch (e) {
        case ParseError::OK: return "ok";
        case ParseError::SYNTAX: return "syntax";
        case ParseError::VERSION: return "version";
        case ParseError::UNKNOWN_KEY: return "unknown_key";
        case ParseError::DUPLICATE: return "duplicate";
        default: return "bad_value";
    }
}

ParseError parse(const char* text, size_t len, Values& out) {
    out = Values();
    if (text == nullptr) return ParseError::SYNTAX;
    Values v;
    bool seenVersion = false;
    Reader r{text, len, 0};
    r.skipSpace();
    if (!r.take('{')) return ParseError::SYNTAX;
    r.skipSpace();
    if (!r.take('}')) {
        for (;;) {
            r.skipSpace();
            if (!r.take('"')) return ParseError::SYNTAX;
            char key[8];
            bool tooLong = false;
            if (!readString(r, key, sizeof(key), tooLong)) return tooLong ? ParseError::UNKNOWN_KEY : ParseError::SYNTAX;
            r.skipSpace();
            if (!r.take(':')) return ParseError::SYNTAX;
            r.skipSpace();

            // Value: a string goes to a scratch buffer big enough for the longest field (keys).
            char sval[KEYS_MAX + 1];
            sval[0] = 0;
            uint32_t num = 0;
            Kind kind = Kind::BAD;
            bool valueTooLong = false;
            char c = r.peek();
            if (r.take('"')) {
                if (!readString(r, sval, sizeof(sval), valueTooLong)) {
                    if (valueTooLong) return ParseError::BAD_VALUE;
                    // A character not allowed inside a string is a bad value; a missing quote is syntax.
                    return r.end() ? ParseError::SYNTAX : ParseError::BAD_VALUE;
                }
                kind = Kind::STRING;
            } else if (c >= '0' && c <= '9') {
                size_t digits = 0;
                while (!r.end() && r.peek() >= '0' && r.peek() <= '9') {
                    if (++digits > 3) return ParseError::BAD_VALUE;   // no field takes more than 3 digits
                    num = num * 10 + (uint32_t)(r.s[r.i++] - '0');
                }
                kind = Kind::NUMBER;
            } else if (r.literal("true")) {
                kind = Kind::TRUE_;
            } else if (r.literal("false")) {
                kind = Kind::FALSE_;
            } else if (r.literal("null")) {
                kind = Kind::NULL_;
            } else {
                return ParseError::SYNTAX;   // minus sign, fraction, object, array: not this format
            }
            // A number must end at a separator ("1.5" or "1e3" are not accepted).
            if (kind == Kind::NUMBER && r.peek() != ',' && r.peek() != '}' && r.peek() != ' ' &&
                r.peek() != '\t' && r.peek() != '\r' && r.peek() != '\n')
                return ParseError::SYNTAX;

            uint8_t bit = 0;
            if (strcmp(key, "v") == 0) {
                if (seenVersion) return ParseError::DUPLICATE;
                seenVersion = true;
                if (kind != Kind::NUMBER) return ParseError::BAD_VALUE;
                if (num != FORMAT_VERSION) return ParseError::VERSION;
            } else if (strcmp(key, "lang") == 0) {
                bit = F_LANG;
                if (kind == Kind::STRING) {
                    if (!uilang::wellFormed(sval)) return ParseError::BAD_VALUE;
                    memcpy(v.lang, sval, strlen(sval) + 1);   // fits: wellFormed limits it to CODE_MAX
                } else if (kind != Kind::NULL_) {
                    return ParseError::BAD_VALUE;
                }
            } else if (strcmp(key, "vol") == 0) {
                bit = F_VOL;
                if (kind == Kind::NUMBER) {
                    if (num > 100) return ParseError::BAD_VALUE;
                    v.vol = (uint8_t)num;
                } else if (kind != Kind::NULL_) {
                    return ParseError::BAD_VALUE;
                }
            } else if (strcmp(key, "keys") == 0) {
                bit = F_KEYS;
                if (kind == Kind::STRING) memcpy(v.keys, sval, strlen(sval) + 1);   // sval has the size of keys
                else if (kind != Kind::NULL_) return ParseError::BAD_VALUE;
            } else if (strcmp(key, "net") == 0 || strcmp(key, "lan") == 0) {
                bit = key[1] == 'e' ? F_NET : F_LAN;
                if (kind != Kind::TRUE_ && kind != Kind::FALSE_) return ParseError::BAD_VALUE;
                if (bit == F_NET) v.net = kind == Kind::TRUE_;
                else v.lan = kind == Kind::TRUE_;
            } else {
                return ParseError::UNKNOWN_KEY;
            }
            if (bit != 0) {
                if (v.present & bit) return ParseError::DUPLICATE;
                v.present |= bit;
                if (kind == Kind::NULL_) v.isNull |= bit;
            }

            r.skipSpace();
            if (r.take(',')) continue;
            if (r.take('}')) break;
            return ParseError::SYNTAX;
        }
    }
    r.skipSpace();
    if (!r.end()) return ParseError::SYNTAX;   // only white space may follow the object
    out = v;
    return ParseError::OK;
}

bool format(const Values& v, char* out, size_t cap) {
    if (out == nullptr || cap == 0) return false;
    JsonWriter j(out, cap);
    return write(v, j);
}

bool write(const Values& v, JsonWriter& j) {
    j.beginObject();
    j.key("v"); j.unum(FORMAT_VERSION);
    const uint8_t given = (uint8_t)(v.present & (uint8_t)~v.isNull);
    j.key("lang");
    if (given & F_LANG) j.str(v.lang); else j.null();
    j.key("vol");
    if (given & F_VOL) j.unum(v.vol); else j.null();
    j.key("keys");
    if (given & F_KEYS) j.str(v.keys); else j.null();
    j.key("net"); j.boolean(v.net);
    j.key("lan"); j.boolean(v.lan);
    j.endObject();
    return j.ok();
}

}  // namespace backup
