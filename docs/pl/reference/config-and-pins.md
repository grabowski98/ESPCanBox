# Konfiguracja i piny

> [!NOTE]
> **Tłumaczenie maszynowe, wymaga sprawdzenia**
>
> Tę stronę przetłumaczono automatycznie. Wiążący jest tekst angielski. Jak zgłosić poprawkę:
> [Tłumaczenia](../contributing/translations.md#send-your-work).

Wszystko, co decyduje o tym, co urządzenie może robić, wybierasz **przy kompilacji** programu, a nie
w czasie pracy. Ta strona wymienia środowiska kompilacji, flagi kompilacji, stałe w
`firmware/src/config.h` i piny płytek z `firmware/boards/`.

Jeśli tylko używasz urządzenia, potrzebujesz dwóch części: tabeli
[środowisk kompilacji](#build-environments) (które wgrać) i kolorów [diody stanu](#status-led).
Reszta jest dla osób, które budują własną płytkę albo dodają środowisko.

Zasada: wszystko, co może coś wysłać (UART do radia, wyjścia 12 V, CAN), ma jawny tryb i jest
**domyślnie wyłączone**. Włączasz to, wybierając inne środowisko, nigdy przez edycję `config.h`.

## Środowiska kompilacji {#build-environments}

Zdefiniowane w `firmware/platformio.ini`. Samo `uvx platformio run -d firmware -t upload` bez `-e`
wgrywa `listen`, najbezpieczniejsze środowisko.

| Środowisko | Tryb | Sieć (panel, aktualizacja przez Wi-Fi) | Profil auta / radio | Nadawanie na CAN |
|---|---|---|---|---|
| `listen` (domyślne) | obok skrzynki | nie | `psa_aee2010` / `xp` | nie (program nie ma funkcji nadawania) |
| `decoder` | dekoder | nie | `psa_aee2010` / `xp` | nie |
| `listen_wifi` | obok skrzynki | tak | `psa_aee2010` / `xp` | nie |
| `decoder_wifi` | dekoder | tak | `psa_aee2010` / `xp` | nie |
| `decoder_tx_wifi` | dekoder | tak | `psa_aee2010` / `xp` | tylko ramki z listy dozwolonych profilu, po uzbrojeniu: domyślnie `15B` |
| `listen_generic_wifi` | obok skrzynki | tak | `generic` / `none` | nie |

- **Obok skrzynki** (`MODE_BESIDE_BOX`): urządzenie pracuje obok skrzynki CAN z rynku wtórnego. Tylko
  słucha: magistrali auta i linii od radia do skrzynki. Nic nie wysyła do radia, a wyjścia 12 V
  zostają wyłączone. Pika z własnego głośnika. Konsola wypisuje ten tryb jako
  `alongside the box`, a `ver` jako `listen`: to ten sam tryb.
- **Dekoder** (`MODE_DECODER`): urządzenie zastępuje skrzynkę. Wysyła ramki do radia i steruje
  wyjściami ACC, podświetlenia i biegu wstecznego.

> [!CAUTION]
> **Tryb dekoder tylko z wypiętą skrzynką**
>
> Dwa nadajniki na jednej linii UART i dwa źródła ACC zwierają wyjścia. Wgrywaj środowisko
> dekodera tylko na stole albo po wypięciu skrzynki CAN.

> [!CAUTION]
> **decoder_tx_wifi może nadawać na magistralę auta**
>
> To jedyne środowisko, które kompiluje `firmware/src/can_tx/`. Po `cantx on` i restarcie może
> wysłać ramkę `15B` (sprawdzaną tylko po ID i długości). Zanim je zbudujesz, przeczytaj
> [Nadawanie na magistralę CAN](../safety/transmit.md).

We wszystkich środowiskach poza `decoder_tx_wifi` kontroler CAN pracuje w trybie tylko nasłuchu:
nie wysyła ramek, potwierdzeń ani ramek błędu. Skrypt `firmware/tools/check_no_can_tx.sh`
sprawdza po kompilacji, że te programy nie zawierają `twai_transmit`.

Starsze nazwy środowisk są zapisane w komentarzach przy każdej sekcji `platformio.ini`.

## Flagi kompilacji {#build-flags}

Te flagi ustawiają środowiska. Potrzebujesz ich tylko wtedy, gdy dodajesz własne środowisko,
najlepiej w `firmware/platformio_override.ini` (skopiuj `platformio_override.sample.ini`; plik nie
jest śledzony w repozytorium).

| Flaga | Wartości | Kto ustawia | Znaczenie |
|---|---|---|---|
| `-DBOARD=` | `PROTO_V1`, `TEMPLATE`, `CUSTOM` | wszystkie środowiska: `PROTO_V1` | który nagłówek płytki jest użyty, zobacz [Płytki](#boards) |
| `-DBOARD_CUSTOM_HEADER=` | `'"board_<name>.h"'` | twoje własne środowisko | nagłówek dla `BOARD=CUSTOM` |
| `-DCAR_PROFILE=` | `PSA_AEE2010` (domyślnie), `GENERIC`, `CUSTOM` | `listen_generic_wifi`: `GENERIC` | profil auta: co program wie o aucie |
| `-DCAR_PROFILE_CUSTOM_HEADER=`, `-DCAR_PROFILE_CUSTOM_OBJECT=` | `'"profile_<name>.h"'`, `kCar<Name>` | twoje własne środowisko | własny profil bez edycji `src/`; dodaj też `lib_deps = car_<name>` |
| `-DRADIO_PROTOCOL=` | `XP` (domyślnie), `NONE` | `listen_generic_wifi`: `NONE` | protokół radia: `XP` = UART 38400 8N1; `NONE` = bez UART, nic nie idzie do radia |
| `-DWORK_MODE=` | `MODE_BESIDE_BOX` (domyślnie), `MODE_DECODER` | środowiska `decoder*` | tryb pracy |
| `-DNET_ENABLED=` | `0` (domyślnie), `1` | środowiska `*_wifi` | panel WWW i aktualizacja przez Wi-Fi |
| `-DCAN_TX_MODE=` | `CAN_TX_OFF` (domyślnie), `CAN_TX_SETTINGS` | tylko `decoder_tx_wifi` | nadawanie na CAN ramek z listy dozwolonych profilu |
| `-DCAN_TX_EXPERIMENTAL=` | `0` (domyślnie), `1` | `decoder_tx_wifi`: `0` | `1` dodaje próbną ramkę `1A9` profilu `psa_aee2010` (dwie nagrane treści: przyciski czujników parkowania i martwego pola) |
| `-DARDUINO_USB_MODE=1`, `-DARDUINO_USB_CDC_ON_BOOT=1` | | wszystkie środowiska | konsola (`Serial`) działa przez natywne USB układu ESP32-S3 |
| `-I include`, `-I boards` | | wszystkie środowiska | nagłówek tożsamości projektu i nagłówki płytek |

Resztę robi `build_src_filter`: bez sieci katalog `src/net/` w ogóle nie jest kompilowany; poza
`decoder_tx_wifi` w ogóle nie jest kompilowany katalog `src/can_tx/`.

> [!WARNING]
> **CAN_TX_EXPERIMENTAL=1 służy do pomiarów**
>
> Ustawiaj go tylko we własnym `platformio_override.ini`, świadomie. Ramka `1A9` wysłana raz przez
> urządzenie została w aucie wzorcowym przyjęta dla części przycisków, a dla innych nie (zmierzone
> (measured): konsola urządzenia w aucie). Ramki klimatyzacji nie ma w żadnej wersji: pojedyncze
> ramki `364` nie zmieniły klimatyzacji. Zobacz [Nadawanie na magistralę CAN](../safety/transmit.md).

### Co zatrzymuje kompilację {#what-stops-the-build}

Kontrole w `config.h` i `boards/board_common.h` zatrzymują kompilację z czytelnym komunikatem, gdy:

- `BOARD` nie jest ustawione, jest nieznane albo ma wartość `TEMPLATE` (szablon nie jest wypełniony);
- w nagłówku płytki brakuje pinu, nagłówek używa GPIO 35-37 albo 19/20, używa pinu startowego
  (0, 3, 45, 46) bez makra `BOARD_<PIN>_STRAP_OK`, używa nieistniejącego GPIO albo jednego GPIO dwa
  razy;
- `CAR_PROFILE` jest nieznany albo ma wartość `EXAMPLE` (profil przykładowy służy tylko testom na
  komputerze);
- `CAN_TX_MODE` jest inne niż `CAN_TX_OFF`, a tryb nie jest dekoderem;
- nadawanie na CAN jest połączone z `CAR_PROFILE=GENERIC` albo `CAR_PROFILE=CUSTOM` albo z profilem,
  którego lista dozwolonych ramek jest pusta (nowe auto najpierw potrzebuje nazwanego profilu ze
  zmierzonymi ramkami);
- `CAN_TX_EXPERIMENTAL` jest inne niż 0 albo 1.

## Profil auta i protokół radia {#car-profile-and-radio-protocol}

Profil auta to jedna struktura `const` (`firmware/lib/core/car_profile.h`): prędkość magistrali,
tabela przycisków, pokrętła, lista dozwolonych ramek do nadawania, ID maskowane w udostępnianych
nagraniach oraz funkcje dekodujące czujniki parkowania, zapłon i bieg wsteczny oraz światła.
Profil mówi też, czy program może sterować wyjściami 12 V.

| Profil | Biblioteka | Prędkość CAN | Wyjścia 12 V | Lista dozwolonych ramek |
|---|---|---|---|---|
| `psa_aee2010` | `lib/car_psa_aee2010` | 125 kbit/s | tak (tryb dekoder) | `15B`; z `CAN_TX_EXPERIMENTAL=1` także `1A9` |
| `generic` | `lib/car_generic` | domyślnie 125 kbit/s (zmierzoną prędkość ustaw w `profile_generic.cpp`) | nigdy | pusta |

W aucie wzorcowym (Peugeot 508 SW 2015) magistrala przy radiu pracuje z prędkością 125 kbit/s
(zmierzone: odbiór bez błędów, `firmware/test_data/captures/can_20261110_190421.log`). Napięcia jej
linii wyglądają na CAN low-speed fault-tolerant, a nie high-speed; do potwierdzenia potrzebny jest
oscyloskop. Dlatego profil zapisuje warstwę fizyczną jako nieznaną.

`firmware/src/can_bus.cpp` zna prędkości 50, 100, 125, 250, 500 i 1000 kbit/s. Przy każdej innej
wartości w profilu nie uruchamia kontrolera CAN: urządzenie zostaje w stanie bezpiecznym.

Jak napisać własny profil: [Pisanie profilu auta](../make-it-yours/write-a-profile.md).

## Stałe w config.h {#constants-in-configh}

To wartości, które warto znać. Zmieniaj je tylko z konkretnego powodu i zostaw komentarz, który
wyjaśnia wartość.

| Stała | Wartość | Znaczenie |
|---|---|---|
| `AUDIO_SAMPLE_RATE` | 22050 Hz | wyjście dźwięku (MAX98357A przez I2S) |
| `AUDIO_VOLUME_DEFAULT` | 40 % | głośność, gdy nic nie jest zapisane |
| `AUDIO_VOLUME_MAX` | 80 % | górna granica, chroni głośnik 8 Ω / 0,5 W |
| `AUDIO_VOLUME_MIN_SAVED` | 15 % | najniższa zapisywana głośność; niższa zapisana wartość jest przy starcie podnoszona do tej |
| `SLEEP_ENABLED`, `SLEEP_AFTER_MS` | 1, 30000 ms | głęboki sen po 30 s ciszy na magistrali (i bez komputera na USB) |
| `CAN_TX_HW_WINDOW_US` | 25000 µs | sprzętowy licznik czasu, który zamyka nadajnik CAN nawet wtedy, gdy pętla główna się zawiesi (brama zamyka go po 20 ms) |
| `NET_AP_IP` | 10.50.8.1 | adres urządzenia w jego własnej sieci (/24) |
| `NET_AP_CHANNEL` | 1 | kanał Wi-Fi sieci urządzenia (przy sieci domowej przechodzi na kanał routera) |
| `NET_AP_MAX_STATIONS` | 4 | liczba telefonów lub komputerów naraz w sieci urządzenia |
| `NET_TX_POWER_QDBM` | 44 (11 dBm) | moc nadawania Wi-Fi, w jednostkach 0,25 dBm |
| `NET_MIN_RAIL_V` | 11,0 V | urządzenie czuwa dla klienta panelu tylko powyżej tego napięcia zasilania (za diodą wejściową) |
| `NET_VBAT_VALID_MIN_V` | 6,0 V | poniżej tej wartości urządzenie uznaje, że nie ma 12 V (tylko zasilanie z USB) |
| `NET_AP_SSID_PREFIX`, `NET_HOSTNAME` | z `project.json` | nazwa sieci urządzenia i nazwa `.local` |

Skąd wzięła się granica głośnika (zmierzone: miernik AC na zaciskach głośnika, zasilanie z USB):
ton ciągły przy 50 % dał 0,753 V na głośniku, więc pełna skala to około 3,0 V (1,2 W). Przy 80 %
ton ciągły daje około 1,2 V (0,18 W), a szczyt piknięcia około 2,2 V przez kilka ms.

Urządzenie zasypia, gdy auto ucichnie. W aucie wzorcowym magistrala nadaje jeszcze przez około
100 s po wyjęciu kluczyka (zmierzone: nagranie CAN na postoju), potem milknie.

## Ustawienia zapisane w urządzeniu {#settings-stored-on-the-device}

Kilka ustawień jest zapisanych w pamięci flash urządzenia (NVS, przestrzeń nazw = `nvs_namespace` z
`project.json`). Wgranie programu przez USB albo przez Wi-Fi ich nie kasuje.

| Klucz | Co | Kto zmienia |
|---|---|---|
| `pass` | hasło urządzenia (panel i klucz Wi-Fi) | `wifi pass`, `wifi newpass`, panel |
| `sta_ssid`, `sta_pass` | sieć domowa | `wifi sta`, panel |
| `net_on` | włącznik Wi-Fi | `wifi on`, `wifi off` |
| `lan_admin` | zmiany dozwolone z sieci domowej | `wifi lan on`, `wifi lan off` |
| `keymap` | tabela zamiany przycisków | `map`, panel |
| `vol` | głośność piknięć (od 15 %) | `vol N`, panel |
| `cantx` | uzbrojenie nadawania na CAN: napis kompilacji programu, w którym wpisano `cantx on` | `cantx on`, `cantx off`; każdy inny program kasuje go przy starcie |
| `vsetmask` | maska pozycji ustawień pojazdu | `vset on NN`, `vset off NN` |

Zapis do flash zatrzymuje odbiór CAN na kilka milisekund. Dlatego urządzenie opóźnia albo odrzuca
te zapisy, gdy włączony jest bieg wsteczny albo działają czujniki parkowania.

> [!WARNING]
> **Zmiana nvs_namespace kasuje wszystko**
>
> Program z innym `nvs_namespace` startuje z pustymi ustawieniami: nowe losowe hasło, domyślna
> głośność, brak sieci domowej, brak zamian przycisków.

## Płytki {#boards}

Piny są wyłącznie w nagłówku płytki w `firmware/boards/`. `config.h` bierze z niego wartości.

| Plik | Zastosowanie |
|---|---|
| `board_proto_v1.h` | domowy prototyp v1 (`hardware/prototype-v1/`) z modułem Waveshare ESP32-S3-DEV-KIT-N16R8 |
| `board_template.h` | punkt wyjścia dla własnej płytki; zatrzymuje kompilację, dopóki go nie wypełnisz |
| `board_common.h` | kontrole wspólne dla każdej płytki |
| `board_proto_v1.snapshot.txt` | rozwiązane wartości pinów i płytki v1; `python -I tools/check_board_macros.py` (w `firmware/`) porównuje z nim bieżącą kompilację, więc zmiana nagłówka nie przesunie pinu niezauważenie |

### Piny prototypu v1 {#pins-of-prototype-v1}

| Funkcja | Makro | GPIO | Uwagi |
|---|---|---|---|
| CAN RX (z transceivera) | `BOARD_PIN_CAN_RX` | 4 | moduł Adafruit CAN Pal, TJA1051T/3 |
| CAN TX (do transceivera) | `BOARD_PIN_CAN_TX` | 5 | przez zworkę JP1. Otwarta JP1 = nadawanie niemożliwe: do codziennej pracy zostaw ją otwartą, zewrzyj tylko do celowego nadawania. W prototypie autora JP1 jest zwarta |
| CAN SLNT (silent) | `BOARD_PIN_CAN_SILENT` | 6 | stan wysoki = nadajnik wyłączony. R3 10 kΩ do 3,3 V na płytce |
| I2S BCLK | `BOARD_PIN_I2S_BCLK` | 15 | moduł MAX98357A |
| I2S LRC | `BOARD_PIN_I2S_LRC` | 16 | |
| I2S DIN | `BOARD_PIN_I2S_DIN` | 17 | |
| SD wzmacniacza | `BOARD_PIN_AMP_SD` | 18 | stan niski = wzmacniacz wyłączony; rezystor 10 kΩ do masy, cisza przed startem programu |
| wyjście 12 V ACC | `BOARD_PIN_OUT_ACC` | 12 | klucz na P-MOSFET, stan wysoki = włączone; złącze skrzynki „dolny 3” |
| wyjście 12 V podświetlenie | `BOARD_PIN_OUT_ILL` | 13 | „górny 3”; idzie za bitem świateł ramki `036` |
| wyjście 12 V bieg wsteczny | `BOARD_PIN_OUT_REV` | 14 | „dolny 4” |
| wyjście 12 V zapasowe | `BOARD_PIN_OUT_4` | 21 | „górny 4”, przewód „BRAKE” |
| UART do radia, TX | `BOARD_PIN_RADIO_TX` | 10 | pole J4 „radio RX”; nieużywany w trybie obok |
| UART z radia, RX | `BOARD_PIN_RADIO_RX` | 11 | pole J4 „radio TX” |
| I2C SDA / SCL | `BOARD_PIN_I2C_SDA`, `BOARD_PIN_I2C_SCL` | 8 / 9 | |
| pomiar 12 V | `BOARD_PIN_VBAT_SENSE` | 7 | dzielnik 470 kΩ / 100 kΩ, za diodą wejściową |
| dioda stanu | `BOARD_PIN_STATUS_LED` | 38 | dioda RGB (WS2812) na module Waveshare |
| przycisk | `BOARD_PIN_BUTTON` | 3 | do masy, wewnętrzne podciąganie; pin startowy, zobacz niżej |

„Dolny 3”, „górny 7” i tak dalej to pozycje złącza 2×8 wiązki skrzynki CAN z rynku wtórnego. Kierunki
UART (zmierzone: rejestrator na obu liniach danych): skrzynka nadaje na „dolny 8”, radio nadaje na
„górny 7”. Zworki JP_UART łączą `BOARD_PIN_RADIO_TX` z „dolny 8”, a `BOARD_PIN_RADIO_RX` z
„górny 7”. Prototyp ma na tych liniach rezystory szeregowe 1 kΩ i 4,7 kΩ (zmierzone miernikiem);
który jest na której linii, jeszcze nie sprawdzono.

> [!NOTE]
> **Pin SLNT ma wewnętrzne ściąganie do masy**
>
> Pin S układu TJA1051 jest ściągany do masy wewnątrz układu, więc niepodłączony pin S oznacza
> „nadajnik aktywny” (niezweryfikowane (unverified): karta katalogowa NXP TJA1051, Rev. 9,
> sekcja 7.2.2). Na płytce stan wysoki utrzymują tylko R3 i przewód SLNT. Sprawdź ten przewód
> miernikiem. Karta katalogowa nie podaje czasu, którego układ potrzebuje na wyjście z trybu
> silent: jeszcze niezmierzone.

GPIO 3 jest pinem startowym ESP32-S3. Według karty katalogowej ESP32-S3 jest on próbkowany przy
resecie tylko wtedy, gdy przepalony jest eFuse `STRAP_JTAG_SEL`, więc przy domyślnych eFuse przycisk
nie wpływa na start (niezweryfikowane w tym egzemplarzu: zanim na tym polegniesz, odczytaj eFuse
poleceniem `espefuse.py summary`).

Napięcie na GPIO 7 pokazuje zasilanie **za** diodą wejściową D1, około 0,8 V (0,7-0,9 V) poniżej napięcia
akumulatora (niezmierzone dokładnie). Przy samym zasilaniu z USB pokazuje szum od 0 do około 1,5 V;
poniżej 6 V konsola i panel podają „no 12 V”.

### Dioda stanu {#status-led}

| Kolor | Znaczenie |
|---|---|
| niebieski | start |
| zgaszona | cisza na magistrali (przy wyłączonym zapłonie dioda zostaje ciemna, żeby oszczędzać prąd) |
| zielony | przychodzą ramki |
| żółty | gra dźwięk |
| czerwony | sterownik nie wystartował (wyjścia zostają wyłączone) albo kontroler CAN nie jest w stanie pracy (przy każdej zmianie pojawia się linia `[can] controller: ...`) |

### Zasady pinów dla każdej płytki {#pin-rules-for-any-board}

Dla modułu ESP32-S3 N16R8, sprawdzane przy kompilacji przez `board_common.h`:

- nigdy GPIO 35, 36, 37 (używają ich pamięci PSRAM typu octal i flash modułu);
- nigdy GPIO 19, 20 (natywne USB, przez nie działa konsola);
- GPIO 0, 3, 45, 46 (piny startowe) tylko z makrem `BOARD_<PIN>_STRAP_OK 1` i komentarzem, który
  wyjaśnia, dlaczego poziom przy resecie jest bezpieczny;
- GPIO 22-25 nie istnieją w ESP32-S3;
- jedno GPIO do jednej funkcji.

### Dodaj własną płytkę {#add-your-own-board}

1. Skopiuj `boards/board_template.h` do `boards/board_<name>.h` i ustaw każdy `BOARD_PIN_*`.
2. Usuń linię `#error`.
3. Dla transceivera CAN wybierz pin silent / standby, na którym **stan wysoki oznacza „nadajnik
   wyłączony”**. Jeśli pin ma wewnętrzne ściąganie do masy, dodaj zewnętrzny rezystor podciągający.
4. Kompiluj bez edycji `src/`: w `platformio_override.ini` użyj `-DBOARD=CUSTOM` i
   `'-DBOARD_CUSTOM_HEADER="board_<name>.h"'` (zobacz `platformio_override.sample.ini`, środowisko
   `listen_mine`).
5. Zacznij od środowiska tylko do nasłuchu i sprawdź je na stole:
   [Test na stole](../quick-start/bench-test.md).

Plik przykładowy ma też `board_template_check`, środowisko, którego kompilacja musi się nie udać:
pokazuje, że niewypełniony szablon zatrzymuje kompilację.

## Platforma {#platform}

PlatformIO z frameworkiem Arduino, platforma `espressif32@8.0.0` (przypięta w `platformio.ini`),
płytka `esp32-s3-devkitc-1` z 16 MB flash. Sterowniki TWAI (CAN), I2S, ADC i LEDC zmieniały się
między wersjami platformy: zanim przejdziesz na inną wersję, sprawdź, czy funkcje używane w `src/`
w niej istnieją. Numer wersji programu jest w `firmware/src/version.h`, a nie w `platformio.ini`,
bo każda zmiana `platformio.ini` przebudowuje wszystko od zera.

Dalej: sprawdź nową płytkę, zobacz [Test na stole](../quick-start/bench-test.md), potem wgraj
potrzebne środowisko: [Wgraj program](../quick-start/flash.md).
