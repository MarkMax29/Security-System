/*
 ================================================================
  PAS 4 — ESP32 + OLED + PIR + Reed Switch + Buzzer Activ

  Ce adaugam fata de Pasul 3:
    - Controlul unui buzzer cu digitalWrite
    - Conceptul de "pattern" sonor non-blocking cu millis()
    - Buzzer diferit pentru PIR vs Reed (sa stii ce s-a intamplat
      fara sa te uiti la ecran)
    - De ce nu folosim delay() pentru tonuri
 ================================================================
*/
//TREBE INCA FACUTA CU CODUL NOSTRU BUZZERUL!!!!!!!


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include "web_server.h"
#include <Preferences.h>

// ── OLED ─────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Prototipuri pentru funcțiile definite mai jos — necesare în fișiere .cpp
void afiseazaCalibrare(int secunde);
void pornesteBuzzer(int pin, int durata);
void actualizeazaOLED();
void pornestePattern(const int* pattern, int len);
void ruleazaPattern();
void tranzitieSpre(int stareNoua);
void handleKeypad();
void handlePinKey(char tasta);
void verificaPin();
void armeazaSistem();
void dezarmeazaSistem();
void declansaAlarma(const char* motiv);
void toggleServo();
void afiseazaEcranStare();
void citesteSenzori();
void handleTimer();

// ── PINI ─────────────────────────────────────────────────────
#define PIN_PIR       13
#define PIN_REED      14
#define PIN_BUZZER    12    // Buzzer activ
#define PIN_SERVO     27  

// ── keypad 4x4 ───────────────────────────────────────────

const byte ROWS=4;
const byte COLS=4;
char hexaKeys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

//PINII PENTRU LINII SI COLOANE
byte rowPins[ROWS]={19,18,5,17};
byte colPins[COLS]={32,33,25,26};
//CREAM OBIECTUL KEYPAD
Keypad keypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

//── Servo ───────────────────────────────────────────
Servo doorServo;
#define SERVO_BLOCAT 0 //0 grade=> usa e blocata
#define SERVO_DEBLOCAT 90 //90 grad=> usa e deblocata
bool usaDeblocata=false;

//MASINA DE STARI!!!

#define STARE_DEZARMAT 0
#define STARE_ARMAT 1
#define STARE_ENTRY_DELAY 2
#define STARE_PIN_ENTRY 3
#define STARE_ALARMA 4
#define STARE_BLOCAT 5
#define STARE_APEL_112 6

int stareCurenta=STARE_DEZARMAT;

//PIN SECURITATE

String pinCorect="";//pin implicit-poate fi schimbat din aplicatia web 
String pinIntrodusPanaAcum="";
int greseliCountConsecutiv=0;
const int MAX_GRESELI=3;


// ── TIMING SENZORI ───────────────────────────────────────────
#define DEBOUNCE_MS   500
#define DURATA_ENTRY_DELAY    20000  // 20 secunde
#define DURATA_ALARMA_112     60000  // 60s alarma → simulare 112
#define DURATA_AFISARE_PIR    3000   // 3s afisare MISCARE pe OLED
 
unsigned long timpStartEntryDelay = 0;
unsigned long timpStartAlarma     = 0;


// ── TIMING BUZZER ────────────────────────────────────────────
//
// In loc sa facem delay() pentru tonuri, definim "pattern-uri":
// secvente de ON/OFF cu durate specifice.
// Le executam non-blocking cu millis().
//
// Fiecare array de mai jos defineste duratele in milisecunde:
//   index par (0,2,4...)  = cat timp e ON (suna)
//   index impar (1,3,5..) = cat timp e OFF (pauza)
//
// Exemplu: {100, 100, 100, 0}
//   100ms ON, 100ms OFF, 100ms ON, stop
//   = 2 beep-uri scurte

// Pattern pentru PIR (miscare detectata) — 2 beep-uri scurte
const int PATTERN_PIR[]  = {100, 100, 100, 0};
const int PATTERN_PIR_LEN = 4;

// Pattern pentru Reed (usa deschisa) — 1 beep lung
const int PATTERN_REED[] = {500, 0};
const int PATTERN_REED_LEN = 2;

const int PATTERN_ARMAT[]={200,100,200,0};
const int PATTERN_ARMAT_LEN=4;

const int PATTERN_DEZARMAT[]={100,50,100,5,0,500,0};
const int PATTERN_DEZARMAT_LEN=6;

const int PATTERN_GRESIT[]= {50, 50, 50, 50, 50, 0};
const int PATTERN_GRESIT_LEN= 6;
// Pattern pentru alarma — beep-uri rapide continue
const int PATTERN_ALARMA[] = {300,200,300,200,300,500};
const int PATTERN_ALARMA_LEN = 6;
bool alarmaActiva=false;

// ── Variabile pentru buzzer non-blocking ──────────────────────
//
// Ideea: in loc sa blocam ESP32-ul cu delay()-uri pentru tonuri,
// tinem minte "unde suntem" in pattern-ul curent si
// verificam in fiecare iteratie a loop() daca e timpul
// sa trecem la pasul urmator.
//
const int* patternCurent     = nullptr; // pointer la array-ul de pattern
                                         // nullptr = niciun pattern activ
int patternLen               = 0;       // lungimea pattern-ului curent
int patternIndex             = 0;       // la ce pas suntem in pattern
unsigned long buzzerUltimPas = 0;       // cand am trecut la pasul curent


// ── VARIABILE GLOBALE -SENZORI ─────────────────────────────────────────

bool pirStareFizica     = false;  // starea REALA a senzorului
bool pirAfisatPeEcran   = false;  // ce AFISAM (cu timer)
unsigned long pirUltimSchimb  = 0;
unsigned long timpStartAfisarePIR = 0;

bool reedStare              = false;
unsigned long reedUltimSchimb = 0;

int numarDetectiiPIR        = 0;
int numarDeschideri         = 0;

bool afisareIntrusActiva = false;
unsigned long timpStartAfisareIntrus = 0;
const unsigned long DURATA_AFISARE_INTRUS = 5000;

int numarMiscariArmat = 0;  // counter PIR in stare armata
const int MISCARI_PANA_LA_ENTRY = 10;
//OLED BLINK PENTRU ALARMA ─────────────────────────────────────────
unsigned long ultimBlink=0;
bool blinkStare=false;

Preferences prefs;

void setup() {
    Serial.begin(115200);
    prefs.begin("securitate",false);
    pinCorect=prefs.getString("pin","8756");
    Serial.println("=== Pas 5: + Keypad + State Machine ===");

    pinMode(PIN_PIR,    INPUT);
    pinMode(PIN_REED,   INPUT);

    // ── Configurare buzzer ────────────────────────────────────
    // OUTPUT = ESP32 controleaza tensiunea pe acest pin
    // HIGH = ~3.3V = buzzer suna
    // LOW  = 0V    = buzzer tace
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW); // pornind cu buzzer oprit!
    // Fara aceasta linie, pinul ar putea porni in stare
    // nedefinita si buzzer-ul ar putea suna la pornire

    //Servo
    doorServo.attach(PIN_SERVO);
    usaDeblocata = true;
    doorServo.write(SERVO_DEBLOCAT);
    Serial.println("Servo: pozitie DEBLOCAT (90 grade) — stare initiala");

    Wire.begin(21, 22);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("EROARE: OLED nu gasit!");
        while (true);
    }

    afiseazaCalibrare(15);

    // Beep scurt de confirmare — sistemul e pornit
    // Aceasta e singura data cand folosim delay() pentru buzzer
    // si e in setup(), nu in loop() — acceptabil
    pornesteBuzzer(PIN_BUZZER, 200); // 200ms beep

    reedStare = (digitalRead(PIN_REED) == HIGH);
    actualizeazaOLED();
    Serial.println("Sistem activ. Stare: DEZARMAT");
    Serial.println("Taste: A=Armeaza, #=Toggle usa, cifre=PIN, D=Confirma, *=Sterge");

    setupWebServer();
    sendStatusWeb();
}


// ════════════════════════════════════════════════════════════
void loop() 
{
    //CITIRE KEYPAD
    handleWebServer();

    static unsigned long ultimStatusWeb = 0;

    if (millis() - ultimStatusWeb >= 1000) 
    {
         ultimStatusWeb = millis();
         sendStatusWeb();
     }

    handleKeypad();

    // ── 2. Citire senzori (doar cand sistemul e armat) ────────
    //
    // Senzorul PIR si Reed genereaza alarma NUMAI in starile
    // ARMAT si ENTRY_DELAY. In DEZARMAT, le citim dar nu reactionam.
    //
    citesteSenzori();

    ruleazaPattern();

    //Timere de stare
    handleTimer();

    //Blink OLED pentru alarma
    if (stareCurenta == STARE_ALARMA || stareCurenta == STARE_BLOCAT) 
    {
        if (millis() - ultimBlink >= 500) 
        {
            blinkStare = !blinkStare;
            ultimBlink = millis();
            actualizeazaOLED();
        }
    }
    
}

///KEYPAD ────────────────────────────────────────────

void handleKeypad()
{
    char tasta = keypad.getKey();
    if (tasta == NO_KEY) return;

    Serial.print("Tasta apasata: ");
    Serial.println(tasta);

    // BLOCAT — keypad complet ignorat
    if (stareCurenta == STARE_BLOCAT || stareCurenta == STARE_APEL_112) 
    {
        Serial.println("Keypad ignorat — sistem blocat/112");
        return;
    }

    if (tasta == 'A') 
    {
        if (stareCurenta == STARE_DEZARMAT) 
            armeazaSistem();
        else 
            Serial.println("A: ignorat");
        return;
    }

    if (tasta == '#') 
    {
        if (stareCurenta == STARE_DEZARMAT) 
            toggleServo();
        else 
            Serial.println("#: ignorat");
        return;
    }

    if (tasta == 'B' || tasta == 'C') 
    return;

    if (stareCurenta == STARE_ARMAT || stareCurenta == STARE_ENTRY_DELAY || stareCurenta == STARE_ALARMA) 
    {
        if (stareCurenta != STARE_PIN_ENTRY) 
        {
            tranzitieSpre(STARE_PIN_ENTRY);
        }
        handlePinKey(tasta);
    }

    if (stareCurenta == STARE_PIN_ENTRY) 
        handlePinKey(tasta);
}

void handlePinKey(char tasta)
{
    if(tasta=='#')
    {   
        //Sterge tot ce s-a introdus
        pinIntrodusPanaAcum="";
        Serial.println("PIN: sters");
        actualizeazaOLED();
        return;
    }
    if (tasta=='D') 
    {
        // Confirma PIN
        verificaPin();
        return;
    }

    if (tasta >= '0' && tasta <= '9') 
    {
        if (pinIntrodusPanaAcum.length() < 4) 
        {
            pinIntrodusPanaAcum += tasta;
            Serial.print("PIN buffer: ");

            // Afisam asteriscuri in Serial, nu PIN-ul real
            for (int i = 0; i < pinIntrodusPanaAcum.length(); i++) 
            {
                Serial.print("*");
            }
            Serial.println();
            actualizeazaOLED();
        
        }

        if (pinIntrodusPanaAcum.length() == 4) 
        {
            delay(200); // mica pauza ca sa simta utilizatorul
            verificaPin();
        }
    }
}

void verificaPin()
{
    Serial.print("Verificare PIN: ");
    for (int i = 0; i < pinIntrodusPanaAcum.length(); i++) 
    { 
        Serial.print("*");
    }
    Serial.println();

    if(pinIntrodusPanaAcum==pinCorect)
    {
        // PINUL E CORECT
        Serial.println("PIN CORECT! Dezarmare sistem.");
        greseliCountConsecutiv = 0;  // resetam contorul
        dezarmeazaSistem();
    }
    else
    {
        //PIN GRESIT
        greseliCountConsecutiv++;
        pinIntrodusPanaAcum = "";
        pornestePattern(PATTERN_GRESIT, PATTERN_GRESIT_LEN);
 
        Serial.print("PIN GRESIT! Greseli consecutive: ");
        Serial.print(greseliCountConsecutiv);
        Serial.print(" / ");
        Serial.println(MAX_GRESELI);
 
        if (greseliCountConsecutiv >= MAX_GRESELI) 
        {
            // 3 greseli consecutive → ALARMA + BLOCAT
            Serial.println("3 GRESELI! Sistem BLOCAT.");
            greseliCountConsecutiv = 0;  // resetam
            declansaAlarma("3 GRESELI PIN");
            tranzitieSpre(STARE_BLOCAT);
        } 
        else 
        {
            // Mai are incercari
            actualizeazaOLED();
        }
    }
}

void citesteSenzori()
{
    // ── Citire PIR ────────────────────────────────────────────
    bool pirCitire = (digitalRead(PIN_PIR) == HIGH);
if (pirCitire != pirStareFizica && millis() - pirUltimSchimb >= DEBOUNCE_MS) {
    pirStareFizica = pirCitire;
    pirUltimSchimb = millis();

    if (pirStareFizica) {
        numarDetectiiPIR++;
        pirAfisatPeEcran = true;
        timpStartAfisarePIR = millis();

        Serial.print("PIR: MISCARE! Total: ");
        Serial.println(numarDetectiiPIR);

        if (stareCurenta == STARE_ARMAT) {
            numarMiscariArmat++;
            Serial.print("Miscari in stare armata: ");
            Serial.println(numarMiscariArmat);

            // Notificare app la fiecare miscare
             sendStatusWeb();

            if (numarMiscariArmat % MISCARI_PANA_LA_ENTRY == 0) {
                Serial.println("10 miscari atinse! ENTRY DELAY!");
                tranzitieSpre(STARE_ENTRY_DELAY);
            } else {
                // Doar beep + OLED, fara entry delay
                pornestePattern(PATTERN_PIR, PATTERN_PIR_LEN);
                actualizeazaOLED();
            }
        } else if (stareCurenta == STARE_DEZARMAT) {
            pornestePattern(PATTERN_PIR, PATTERN_PIR_LEN);
            actualizeazaOLED();
        }
    }
}
    // ── Citire Reed ───────────────────────────────────────────
    bool reedCitire = (digitalRead(PIN_REED) == HIGH);
    if (reedCitire != reedStare && millis() - reedUltimSchimb >= DEBOUNCE_MS) 
    {

        reedStare       = reedCitire;
        reedUltimSchimb = millis();

        if (reedStare) {
            numarDeschideri++;
            Serial.print("REED: USA DESCHISA! Total: ");
            Serial.println(numarDeschideri);

            if (stareCurenta == STARE_ARMAT) 
            {
                tranzitieSpre(STARE_ENTRY_DELAY);
            } 
            else if (stareCurenta == STARE_DEZARMAT) 
            {
                pornestePattern(PATTERN_REED, PATTERN_REED_LEN);
                actualizeazaOLED();
            }
        } 
        else 
        {
            Serial.println("REED: usa inchisa");
            actualizeazaOLED();
            sendStatusWeb();
        }
    }
    
    //timer afisare PIR pe OLED

    if(pirAfisatPeEcran && millis()-timpStartAfisarePIR >=DURATA_AFISARE_PIR)
    {
        pirAfisatPeEcran=false;
        actualizeazaOLED();
    }
}

void handleTimer()
{
    //ENTRY DELAY : COUNTDOWN 20 SEC

     if (stareCurenta == STARE_ENTRY_DELAY) 
     {
        unsigned long trecut = millis() - timpStartEntryDelay;

        static unsigned long ultimSecunda = 0;
        if (millis() - ultimSecunda >= 1000) 
        {
            ultimSecunda = millis();
            actualizeazaOLED();
        }

        if (trecut >= DURATA_ENTRY_DELAY) 
        {
            // Delay expirat fara PIN → ALARMA
            Serial.println("Entry delay expirat! ALARMA!");
            declansaAlarma("ENTRY DELAY EXPIRAT");
        }
    }

    // ── Alarma: dupa 60s → simulare 112 ──────────────────────
    if ((stareCurenta == STARE_ALARMA || stareCurenta == STARE_BLOCAT) && timpStartAlarma > 0) 
    {    
        if (millis() - timpStartAlarma >= DURATA_ALARMA_112) 
        {
            Serial.println("60s alarma! Simulare APEL 112!");
            tranzitieSpre(STARE_APEL_112);
        }
    }

    //BUZZER ALARMA CONTINUU

    if (alarmaActiva && patternCurent == nullptr) 
    {
        pornestePattern(PATTERN_ALARMA, PATTERN_ALARMA_LEN);
    }
  
}


//TRANZITII DE STARE !!!

void tranzitieSpre(int stareNoua)
{
    Serial.print("Tranzitie: ");
    Serial.print(stareCurenta);
    Serial.print(" → ");
    Serial.println(stareNoua);
 
    stareCurenta = stareNoua;
    pinIntrodusPanaAcum = "";  // curatam buffer PIN la orice tranzitie

    switch(stareNoua)
    {
        case STARE_ENTRY_DELAY:
            timpStartEntryDelay = millis();
            Serial.println("ENTRY DELAY pornit — 20 secunde!");
            break;
 
        case STARE_PIN_ENTRY:
            Serial.println("Introducere PIN...");
            break;
 
        case STARE_ALARMA:
            break;  // logica in declansaAlarma()
 
        case STARE_BLOCAT:
            Serial.println("SISTEM BLOCAT — doar app web poate opri!");
            break;
 
        case STARE_APEL_112:
            alarmaActiva = false;  // oprim repetarea
            digitalWrite(PIN_BUZZER, LOW);
            Serial.println("=== SIMULARE APEL 112 ===");
            // Pattern diferit pentru 112
            pornestePattern(PATTERN_ALARMA, PATTERN_ALARMA_LEN);
            break;
    }
 
    actualizeazaOLED();
    sendStatusWeb();
}
    
void armeazaSistem() {
    stareCurenta = STARE_ARMAT;
    pinIntrodusPanaAcum = "";
    greseliCountConsecutiv = 0;
    alarmaActiva = false;
    numarMiscariArmat = 0;  // ← adaugă asta
    Serial.println("SISTEM ARMAT!");
    pornestePattern(PATTERN_ARMAT, PATTERN_ARMAT_LEN);
    actualizeazaOLED();
    sendStatusWeb();
}


void dezarmeazaSistem() 
{
    stareCurenta   = STARE_DEZARMAT;
    alarmaActiva   = false;
    timpStartAlarma = 0;
    pinIntrodusPanaAcum = "";
    greseliCountConsecutiv = 0;
    digitalWrite(PIN_BUZZER, LOW);
    patternCurent = nullptr;
 
    // Deblocam usa la dezarmare
    if (!usaDeblocata) {
        usaDeblocata = true;
        doorServo.write(SERVO_DEBLOCAT);
        Serial.println("Servo: DEBLOCAT (90 grade)");
    }
 
    Serial.println("SISTEM DEZARMAT!");
    pornestePattern(PATTERN_DEZARMAT, PATTERN_DEZARMAT_LEN);
    actualizeazaOLED();
    sendStatusWeb();
}

void declansaAlarma(const char* motiv) 
{
    Serial.print("ALARMA! Motiv: ");
    Serial.println(motiv);
 
    // Nu suprascriem STARE_BLOCAT cu STARE_ALARMA
    if (stareCurenta != STARE_BLOCAT) {
        stareCurenta = STARE_ALARMA;
    }
 
    alarmaActiva    = true;
    timpStartAlarma = millis();
    pinIntrodusPanaAcum= "";
 
    pornestePattern(PATTERN_ALARMA, PATTERN_ALARMA_LEN);
    actualizeazaOLED();
    sendStatusWeb();
    sendIstoricAlarma(motiv);
}

void toggleServo() 
{
    usaDeblocata = !usaDeblocata;
    if (usaDeblocata) 
    {
        doorServo.write(SERVO_DEBLOCAT);
        Serial.println("Servo: DEBLOCAT (90 grade)");
    } 
    else 
    {
        doorServo.write(SERVO_BLOCAT);
        Serial.println("Servo: BLOCAT (0 grade)");
    }
    pornestePattern(PATTERN_PIR, PATTERN_PIR_LEN); // 2 beep-uri scurte
    actualizeazaOLED();
}
  

// ════════════════════════════════════════════════════════════
// FUNCTII BUZZER
// ════════════════════════════════════════════════════════════

// Porneste un pattern nou.
// Daca rulează deja un pattern, il intrerupe si incepe cel nou.
void pornestePattern(const int* pattern, int len) 
{
    patternCurent = pattern;
    patternLen    = len;
    patternIndex  = 0;
    buzzerUltimPas = millis();
    // Primul pas din orice pattern e mereu ON
    digitalWrite(PIN_BUZZER, HIGH);
}

// Ruleaza pasul curent din pattern — apelata in loop()
void ruleazaPattern() {
    // Daca nu e niciun pattern activ, nu facem nimic
    if (patternCurent == nullptr) return;

    // Verificam daca a trecut timpul pentru pasul curent
    if (millis() - buzzerUltimPas >= patternCurent[patternIndex]) 
    {

        // Trecem la pasul urmator
        patternIndex++;

        // Am terminat pattern-ul?
        if (patternIndex >= patternLen) {
            // Da — oprim buzzer-ul si resetam
            digitalWrite(PIN_BUZZER, LOW);
            patternCurent = nullptr;
            return;
        }

        // Nu — alternăm starea buzzer-ului
        // Pasii pari (0,2,4) = ON, pasii impari (1,3,5) = OFF
        bool esteOn = (patternIndex % 2 == 0);
        digitalWrite(PIN_BUZZER, esteOn ? HIGH : LOW);
        buzzerUltimPas = millis();
    }
}

// Functie simpla pentru un beep cu delay — DOAR pentru setup()
void pornesteBuzzer(int pin, int durata) {
    digitalWrite(pin, HIGH);
    delay(durata);
    digitalWrite(pin, LOW);
}

// ════════════════════════════════════════════════════════════
// OLED
// ════════════════════════════════════════════════════════════
void actualizeazaOLED() 
{
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
 
    switch (stareCurenta) 
    {
 
        // ── DEZARMAT ─────────────────────────────────────────
        case STARE_DEZARMAT:
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println("SISTEM SECURITATE");
            display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
 
            display.setTextSize(2);
            display.setCursor(0, 14);
            display.println("DEZARMAT");
 
            display.setTextSize(1);
            display.setCursor(0, 36);
            display.print("PIR: ");
            display.println(pirAfisatPeEcran ? "MISCARE!" : "liber");
            display.setCursor(0, 46);
            display.print("USA: ");
            display.println(reedStare ? "deschisa" : "inchisa");
            display.setCursor(0, 56);
            display.print(usaDeblocata ? "[A]Armeaza [#]Blocare"
                                       : "[A]Armeaza [#]Deschide");
            break;
 
        // ── ARMAT ─────────────────────────────────────────────
        case STARE_ARMAT:
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println("SISTEM SECURITATE");
            display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
 
            display.setTextSize(2);
            display.setCursor(25, 14);
            display.println("ARMAT");
 
            display.setTextSize(1);
            display.setCursor(0, 36);
            display.print("PIR: ");
            display.println(pirAfisatPeEcran ? "!MISCARE!" : "liber");
            display.setCursor(0, 46);
            display.print("USA: ");
            display.println(reedStare ? "!DESCHISA!" : "inchisa");
            display.setCursor(0, 56);
            display.println("Tasteaza PIN pt dezarmare");
            break;
 
        // ── ENTRY DELAY ───────────────────────────────────────
        case STARE_ENTRY_DELAY:
        case STARE_PIN_ENTRY: {
            unsigned long trecut   = millis() - timpStartEntryDelay;
            unsigned long ramas    = (trecut >= DURATA_ENTRY_DELAY)? 0 : (DURATA_ENTRY_DELAY - trecut) / 1000;

            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);

            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println("!! ENTRY DELAY !!");
 
            display.setTextSize(3);
            String countdown =String(ramas)+"s";
            int16_t x1,y1;
            uint16_t w,h;
            display.getTextBounds(countdown,0,0,&x1,&y1,&w,&h);
            display.setCursor((128-w)/2,14);
            display.print(countdown);
    
            display.setTextSize(1);
            display.setCursor(0,42);
            display.print("PIN: ");
            for(int i=0;i<(int)pinIntrodusPanaAcum.length();i++)
            {
                display.print("*");
            }
            
            display.setCursor(0,54);
            display.print("Greseli: ");
            display.print(greseliCountConsecutiv);
            display.print("/");
            display.print(MAX_GRESELI);
            
            display.display();
            break;
        }
 
        // ── ALARMA ────────────────────────────────────────────
        case STARE_ALARMA:
        case STARE_BLOCAT:
            // Text blink: afisam alternativ
            if (blinkStare) 
            {
                display.setTextSize(2);
                display.setCursor(10, 5);
                display.println("!! ALARMA");
                display.setCursor(10, 28);
                display.println("ACTIVA !!");
            } 
            else 
            {
                display.setTextSize(1);
                display.setCursor(0, 5);
                if (stareCurenta == STARE_BLOCAT) 
                {
                    display.println("SISTEM BLOCAT!");
                    display.setCursor(0, 18);
                    display.println("Oprire doar din app!");
                } 
                else 
                {
                    display.println("Introduceti PIN:");
                    display.setCursor(0, 18);
                    for (int i = 0; i < (int)pinIntrodusPanaAcum.length(); i++) 
                    {
                        display.print("* ");
                    }
                }
            }
 
            // Timp ramas pana la 112
            if (timpStartAlarma > 0) 
            {
                unsigned long trecut = millis() - timpStartAlarma;
                unsigned long ramas  = (trecut >= DURATA_ALARMA_112)? 0 : (DURATA_ALARMA_112 - trecut) / 1000;
                display.setTextSize(1);
                display.setCursor(0, 48);
                display.print("112 in: ");
                display.print(ramas);
                display.print("s");
            }
            break;
 
        // ── APEL 112 ─────────────────────────────────────────
        case STARE_APEL_112:
            display.setTextSize(2);
            display.setCursor(5, 5);
            display.println("APEL 112");
            display.setCursor(5, 28);
            display.println("IN CURS..");
            display.setTextSize(1);
            display.setCursor(0, 52);
            display.println("Oprire doar din aplicatie");
            break;
    }
 
    display.display();
}  

void afiseazaCalibrare(int secunde) {
    for (int sec = secunde; sec >= 0; sec--) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(10, 5);
        display.println("Calibrare PIR...");
        display.setTextSize(3);
        display.setCursor(50, 28);
        display.println(sec);
        display.display();
        delay(1000);
    }
}