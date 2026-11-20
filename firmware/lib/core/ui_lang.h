// Language of the web panel stored in the device (NVS key "ui_lang", src/settings.h).
//
// The device does not translate anything: it only keeps the code of the language the user picked, so that
// every browser (and the login screen, through GET /api/hello) opens in that language. The list of valid
// codes is the list of dictionaries built into the image (web/i18n/<code>.json; tools/embed_web.py writes
// it as WEB_UI_LANGS, for example "de,en,es,pl,ru,uk"). A code that is not in that list is treated as
// "nothing stored" (English), for example after flashing an image without that dictionary.
//
// Pure functions, no hardware, no allocation; tested on the PC (test_host/settings_backup.cpp).
#pragma once
#include <stddef.h>

namespace uilang {

// Longest code without the terminating zero (e.g. "pt-BR" has 5 characters).
const size_t CODE_MAX = 8;

// Shape of a code: 2..CODE_MAX characters, letters, digits and '-', starts with a letter,
// does not end with '-'. nullptr = false.
bool wellFormed(const char* code);

// 'code' is one whole item of the comma-separated 'list' ("de,en,pl"): "e" or "en,pl" never match.
// A code that is not wellFormed is never in a list. nullptr or an empty list = false.
bool inList(const char* code, const char* list);

}  // namespace uilang
