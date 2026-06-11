# 🔐 Sistem de Securitate cu ESP32

Un sistem complet de securitate pentru uz rezidențial/educațional bazat pe **ESP32**, cu monitorizare prin senzori, interfață locală (keypad + OLED) și dashboard web accesibil în timp real prin WiFi.

---

## 📋 Cuprins

- [Descriere](#descriere)
- [Demo](#demo)
- [Componente Hardware](#componente-hardware)
- [Schema de Conexiuni](#schema-de-conexiuni)
- [Mașina de Stări](#mașina-de-stări)
- [Structura Proiectului](#structura-proiectului)
- [Instalare și Configurare](#instalare-și-configurare)
- [Utilizare](#utilizare)
- [Dashboard Web](#dashboard-web)
- [Comunicare WiFi / WebSocket](#comunicare-wifi--websocket)
- [Librării folosite](#librării-folosite)
- [Detalii Tehnice](#detalii-tehnice)

---

## Descriere

Sistemul monitorizează permanent o zonă protejată prin:
- **Senzor PIR** — detectează mișcarea persoanelor
- **Reed Switch** — detectează deschiderea ușii

Dacă sistemul este armat și detectează o intruziune, pornește un **countdown de 20 secunde** în care utilizatorul poate introduce PIN-ul corect pentru dezarmare. Dacă nu reușește (sau greșește de 3 ori), se declanșează **alarma sonoră** (buzzer continuu). Dacă alarma nu e oprită în 60 de secunde, sistemul simulează un **apel la 112**.

Totul este vizibil și controlabil în timp real dintr-un **dashboard web** — de pe laptop sau telefon, prin WiFi local.

---

## Demo

| OLED — Stare DEZARMAT | OLED — Entry Delay | Dashboard Web |
|---|---|---|
| Afișează status PIR, Reed și instrucțiuni taste | Countdown 20s centrat + asteriscuri PIN + greșeli | Status live, butoane armare, galerie foto/video |

---

## Componente Hardware

| Componentă | Cantitate | Rol |
|---|---|---|
| ESP32 DevKit v1 | 1 | Microcontroller principal (WiFi integrat) |
| Senzor PIR HC-SR501 | 1 | Detectare mișcare prin infraroșu |
| Reed Switch | 1 | Detectare deschidere ușă |
| Buzzer activ | 1 | Semnalizare sonoră (alarmă, beep-uri) |
| Servo Motor SG90 | 1 | Blocare/deblocare fizică ușă |
| OLED SSD1306 128×64 | 1 | Afișaj informații sistem (I2C) |
| Keypad 4×4 | 1 | Introducere PIN și comenzi |
| Breadboard + fire dupont | — | Cablaj prototip |

---

## Schema de Conexiuni

```
ESP32 GPIO 13  ──── PIR HC-SR501 (OUT)
ESP32 GPIO 14  ──── Reed Switch (Pin 1)    | Reed Switch (Pin 2) ──── GND
ESP32 GPIO 12  ──── Buzzer activ (+)       | Buzzer (-)          ──── GND
ESP32 GPIO 27  ──── Servo SG90 (semnal)    | Servo (VCC) ─── 5V | Servo (GND) ─── GND
ESP32 GPIO 21  ──── OLED SDA
ESP32 GPIO 22  ──── OLED SCL
ESP32 3.3V     ──── OLED VCC
ESP32 GND      ──── OLED GND

Keypad 4×4:
  Rând R1 ──── GPIO 19
  Rând R2 ──── GPIO 18
  Rând R3 ──── GPIO 5
  Rând R4 ──── GPIO 17
  Col  C1 ──── GPIO 32
  Col  C2 ──── GPIO 33
  Col  C3 ──── GPIO 25
  Col  C4 ──── GPIO 26
```

---

## Mașina de Stări

Sistemul are **7 stări** distincte, cu tranziții clare:

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│   [DEZARMAT] ──── tasta A / cmd app ────► [ARMAT]             │
│       ▲                                      │                  │
│       │                           Reed deschis / PIR×10        │
│       │                                      ▼                  │
│       │                             [ENTRY DELAY]  ◄── 20s     │
│       │                                      │                  │
│       │                              cifre keypad               │
│       │                                      ▼                  │
│       │◄── PIN corect ────────────── [PIN ENTRY]               │
│       │                                      │                  │
│       │                          20s expirat / 3 greșeli        │
│       │                                      ▼                  │
│       │◄── PIN corect / app ────── [ALARMA ACTIVĂ]             │
│       │                               │          │              │
│       │                        3 greșeli      60 secunde        │
│       │                               ▼          ▼              │
│       │◄──── doar app ────────── [BLOCAT]   [APEL 112]        │
│                                                  │              │
│       │◄──────────── doar app ───────────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

### Detalii stări

| Stare | Hardware | Keypad | Ieșire |
|---|---|---|---|
| **DEZARMAT** | Servo 90° (deschis), Buzzer OFF | A = armează, # = toggle servo | Tasta A sau app |
| **ARMAT** | Servo 0° (blocat), Buzzer OFF | Cifre → PIN_ENTRY | PIR×10 sau Reed → ENTRY DELAY |
| **ENTRY DELAY** | OLED countdown 20s | Cifre PIN | PIN corect → DEZARMAT, expirat → ALARMA |
| **PIN ENTRY** | OLED asteriscuri | 4 cifre → auto verificare | Corect → DEZARMAT, greșit×3 → BLOCAT |
| **ALARMA** | Buzzer continuu, OLED blink | PIN funcționează | PIN corect / app → DEZARMAT, 60s → 112 |
| **BLOCAT** | Buzzer continuu, OLED blink | Total ignorat | Doar app web |
| **APEL 112** | OLED alertă urgență | Total ignorat | Doar app web |

---

## Structura Proiectului

```
SI-securitysystem/
├── src/
│   └── main2.cpp          # Logica principală: senzori, stări, OLED, buzzer, keypad
├── include/
│   └── web_server.h       # WiFi, WebSocket, comenzi JSON
├── platformio.ini          # Configurare PlatformIO + dependențe
└── dashboard.html          # Dashboard web (deschis local în browser)
```

---

## Instalare și Configurare

### Cerințe

- [PlatformIO](https://platformio.org/) (extensie VS Code) sau Arduino IDE
- Python 3 (pentru serverul local de acces de pe telefon)
- ESP32 DevKit conectat prin USB

### 1. Clonează repository-ul

```bash
git clone https://github.com/username/SI-securitysystem.git
cd SI-securitysystem
```

### 2. Configurează WiFi

Deschide `include/web_server.h` și completează liniile 13-14:

```cpp
const char* WIFI_SSID     = "NUMELE_RETELEI_TALE";   // ← schimbă aici
const char* WIFI_PASSWORD = "PAROLA_WIFI";            // ← schimbă aici
```

> ⚠️ ESP32 suportă doar WiFi **2.4GHz**, nu 5GHz.

### 3. Instalează librăriile

PlatformIO le instalează automat din `platformio.ini`. Dacă folosești Arduino IDE, instalează manual din Library Manager:

- `WebSockets` by Markus Sattler
- `ArduinoJson` by Benoit Blanchon
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `Keypad` by Chris--A
- `ESP32Servo`

### 4. Upload pe ESP32

```bash
pio run --target upload
```

Sau apasă butonul **Upload** (→) din bara de jos PlatformIO.

### 5. Verifică Serial Monitor

```bash
pio device monitor
```

Ar trebui să vezi:
```
Calibrare PIR... (15s)
=== WiFi CONECTAT ===
IP: 192.168.x.xxx        ← notează acest IP
WebSocket pornit pe portul 81
```

---

## Utilizare

### De pe keypad

| Stare curentă | Tastă | Efect |
|---|---|---|
| DEZARMAT | `A` | Armează sistemul |
| DEZARMAT | `#` | Toggle servo (blocare/deblocare ușă) |
| ARMAT / ENTRY DELAY / ALARMA | `0-9` | Introduce cifre PIN |
| Orice stare cu PIN activ | `#` | Șterge buffer PIN |
| Orice stare cu PIN activ | `D` | Confirmă PIN manual |
| BLOCAT / APEL 112 | orice | Ignorat complet |

### PIN implicit

```
8756
```

PIN-ul este salvat permanent în memoria flash NVS a ESP32. Se poate schimba din dashboard.

---

## Dashboard Web

### Deschidere pe laptop

1. Descarcă `dashboard.html` din repository
2. Deschide-l în browser (dublu click sau drag în Chrome/Firefox)
3. Introdu IP-ul ESP32 (ex: `192.168.0.104`) și apasă **Conectează**

### Deschidere pe telefon (aceeași rețea WiFi)

1. Pornește un server HTTP local pe laptop în folderul cu `dashboard.html`:
```bash
python3 -m http.server 8080
```
2. Află IP-ul laptopului:
```bash
ip a | grep "inet " | grep -v 127      # Linux/Mac
ipconfig                                 # Windows
```
3. Pe telefon în browser: `http://IP_LAPTOP:8080/dashboard.html`
4. Introdu IP-ul ESP32 și conectează-te

> 📱 Camera nu funcționează pe iPhone prin HTTP (restricție iOS). Pe Android și desktop funcționează complet.

### Funcționalități dashboard

| Secțiune | Descriere |
|---|---|
| Status sistem | Badge colorat cu starea curentă + indicatori senzori live |
| Detecții mișcare | Contor PIR cu progress bar (la 10 → Entry Delay) + buton Confirmă OK |
| Acțiuni | Butoane Armează / Dezarmează / Oprește Alarmă (active contextual) |
| Schimbare PIN | Input 4 cifre, salvat permanent în ESP32 |
| Istoric alarme | Lista cronologică a evenimentelor din sesiune |
| Cameră | Feed video live, pornit automat la deschidere |
| Galerie | Poze + video capturate automat la detecție PIR când sistemul e armat |
| Notificări push | Activabile din browser, funcționează și cu browserul în fundal |
| Log evenimente | Jurnal text cu timestamp pentru toate acțiunile |

---

## Comunicare WiFi / WebSocket

### Arhitectură

```
[Senzori/Keypad]
       │
       ▼
[ESP32 main2.cpp]  ◄──► [web_server.h]  ◄──WebSocket──► [Browser dashboard.html]
       │                      │
   [OLED/Buzzer/Servo]    [WiFi 2.4GHz]
```

Dashboard-ul este un fișier HTML **static** — nu e servit de ESP32. ESP32 rulează doar un **server WebSocket pe portul 81**. Comunicarea este bidirecțională și în timp real.

### Mesaje JSON — ESP32 → Browser

**Status complet** (trimis la 1s + la orice schimbare de stare):
```json
{
  "type": "status",
  "stare": "ARMAT",
  "pir": false,
  "reed": false,
  "usa_deblocata": false,
  "alarma_activa": false,
  "miscari_armat": 3,
  "total_pir": 15,
  "total_reed": 2,
  "greseli": 0
}
```

**Notificare toast:**
```json
{ "type": "notificare", "mesaj": "PIN actualizat", "tip": "success" }
```

**Eveniment alarmă (pentru istoric):**
```json
{ "type": "alarma_log", "motiv": "ENTRY DELAY EXPIRAT" }
```

### Mesaje JSON — Browser → ESP32

```json
{ "cmd": "armeaza" }
{ "cmd": "dezarmeaza" }
{ "cmd": "opreste_alarma" }
{ "cmd": "schimba_pin", "pin": "1234" }
{ "cmd": "confirma_ok" }
```

---

## Librării folosite

| Librărie | Autor | Utilizare |
|---|---|---|
| `WiFi.h` | Espressif (built-in) | Conectare la rețeaua WiFi |
| `WebSocketsServer` | Markus Sattler | Server WebSocket pe portul 81 |
| `ArduinoJson` | Benoit Blanchon | Serializare/deserializare JSON |
| `Preferences.h` | Espressif (built-in) | Salvare PIN în memoria NVS |
| `Adafruit_GFX` | Adafruit | Grafică pentru OLED |
| `Adafruit_SSD1306` | Adafruit | Driver ecran OLED I2C |
| `Keypad` | Chris--A | Scanare matrice keypad 4×4 |
| `ESP32Servo` | madhephaestus | Control servo motor SG90 |

### `platformio.ini`

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    links2004/WebSockets @ ^2.4.1
    bblanchon/ArduinoJson @ ^6.21.0
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306
    Chris--A/Keypad
    madhephaestus/ESP32Servo
```

---

## Detalii Tehnice

### Non-blocking cu millis()

Niciun `delay()` nu este folosit în `loop()`. Toate timing-urile (buzzer, countdown, blink OLED) folosesc `millis()` și variabile de stare, permițând ESP32 să gestioneze simultan senzorii, keypad-ul, OLED-ul și WebSocket-ul.

```cpp
// Pattern-uri sonore non-blocking:
const int PATTERN_ALARMA[] = {300, 200, 300, 200, 300, 500};
// index par = ON (ms), index impar = OFF (ms)
```

### Debouncing senzori

Senzorii PIR și Reed au debounce de 500ms — orice schimbare mai rapidă este ignorată pentru a evita detectările false.

### Mecanism anti-fals PIR

PIR nu declanșează Entry Delay la prima detecție. Sistemul numără detecțiile (`numarMiscariArmat`) și declanșează **doar la multipli de 10**. Utilizatorul poate reseta contorul din dashboard dacă recunoaște că e fals alarm.

### Salvare PIN persistentă (NVS)

```cpp
prefs.begin("securitate", false);
pinCorect = prefs.getString("pin", "8756"); // citire la pornire
prefs.putString("pin", pinCorect);          // scriere la schimbare
```

PIN-ul rămâne salvat după repornire și se pierde doar la re-flash complet al firmware-ului.

### Camera în dashboard

- Pornește automat la deschiderea dashboard-ului
- Capturează o **poză JPEG** + **video WebM 10s** la fiecare detecție PIR în stare armată
- Download automat pe calculator
- Galerie cu previzualizare și lightbox pentru poze

---

## Licență

MIT License — liber de folosit și modificat cu menționarea sursei.

---

## Autor

Proiect realizat ca lucrare pentru cursul de Sisteme Incorporate · 2026
