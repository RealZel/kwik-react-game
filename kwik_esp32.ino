#define U8X8_USE_CYRILLIC

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ===== Пины I2C (ESP32) =====
static const int I2C_SDA = 23;
static const int I2C_SCL = 19;

// ===== OLED дисплей =====
U8G2_SSD1306_128X64_NONAME_F_HW_I2C disp(
  U8G2_R0, U8X8_PIN_NONE, I2C_SCL, I2C_SDA
);

// ===== Пины энкодера и кнопок =====
#define PIN_ENC_A    35
#define PIN_ENC_B    34
#define PIN_ENC_BTN  25
#define PIN_BTN_L    26
#define PIN_BTN_R    27
#define PIN_VIBRO_L  14
#define PIN_VIBRO_R  12
#define PIN_LED_L     4
#define PIN_LED_R    16
#define PIN_BUZZER   17

// ===== Шрифты =====
#define FONT_CYR   u8g2_font_unifont_t_cyrillic
#define FONT_NUM   u8g2_font_logisoso22_tf

// ===== Глобальные =====
int  punktMenu              = 0;
bool inMenu                 = true;
int  lastEncA               = LOW;
unsigned long lastInputTime = 0;
unsigned long signalTime;   // для точного замера

// ===== Вибро и сигнал =====
void vibrate(int pin) {
  digitalWrite(pin, HIGH);
  delay(180);
  digitalWrite(pin, LOW);
}
void signalBoth() {
  digitalWrite(PIN_LED_L, HIGH);
  digitalWrite(PIN_LED_R, HIGH);
  tone(PIN_BUZZER, 1000, 150);
  delay(150);
  noTone(PIN_BUZZER);
  signalTime = micros();
  digitalWrite(PIN_LED_L, LOW);
  digitalWrite(PIN_LED_R, LOW);
}

// ===== Рисовалки =====
void drawCentered(int y, const char* txt, const uint8_t* font) {
  disp.setFont(font);
  int w = disp.getUTF8Width(txt);
  int x = (128 - w) / 2;
  disp.setCursor(x, y);
  disp.print(txt);
}
void drawBoxed(int x,int y,int w,int h,const char* txt,const uint8_t* f){
  disp.setFont(f);
  int tw=disp.getUTF8Width(txt);
  int asc=disp.getAscent(), des=disp.getDescent();
  int cx=x+(w-tw)/2, cy=y+(h+asc-des)/2-1;
  disp.setCursor(cx, cy);
  disp.print(txt);
}
void drawNumberMs(int x,int y,int w,int h,long val){
  char buf[10];
  if(val>0) snprintf(buf,sizeof(buf),"%ld",val);
  else      snprintf(buf,sizeof(buf),"--");
  int hh=h/2;
  drawBoxed(x,y,      w,hh, buf,     FONT_NUM);
  drawBoxed(x,y+hh,   w,h-hh,"мс",   FONT_CYR);
}

// ===== Вспомогательые паузы =====
void waitEnterHint(){
  drawCentered(10, "ENTER = МЕНЮ", FONT_CYR);
  disp.sendBuffer();
  while(digitalRead(PIN_ENC_BTN)) delay(10);
  delay(200);
  while(!digitalRead(PIN_ENC_BTN)) delay(10);
  delay(200);
}
void waitEnterNoHint(){
  while(digitalRead(PIN_ENC_BTN)) delay(10);
  delay(200);
  while(!digitalRead(PIN_ENC_BTN)) delay(10);
  delay(200);
}

// ===== Отсчет =====
void countdown(int sec){
  for(int i=sec;i>0;i--){
    disp.clearBuffer();
    drawCentered(26, "Старт через", FONT_CYR);
    char num[4]; snprintf(num,sizeof(num),"%d",i);
    drawCentered(62, num, FONT_NUM);
    disp.sendBuffer();
    delay(1000);
  }
}

// ===== Экран «Готов» =====
void showReady(const char* title,int round,bool dual){
  disp.clearBuffer();
  drawCentered(12, title, FONT_CYR);
  if(dual){
    disp.drawFrame(0,16,64,48);
    disp.drawFrame(64,16,64,48);
  } else {
    drawCentered(40,"Готовьтесь",FONT_CYR);
  }
  char buf[20];
  snprintf(buf,sizeof(buf),"Раунд %d/10",round);
  drawCentered(62, buf, FONT_CYR);
  disp.sendBuffer();
}

// ===== Игра 1: Реакция =====
void reactionMode(){
  const int R = 10;                   // число раундов
  const long PENALTY_MS = 500;        // штраф за раннее нажатие
  long sumL = 0, sumR = 0;

  for(int i = 0; i < R; i++){
    showReady("Реакция", i + 1, true);

    // 1) случайная задержка + ловим «РАНО»
    bool lEarly = false, rEarly = false;
    unsigned long delayMs = random(500, 3000), t0 = millis();
    while(millis() - t0 < delayMs){
      if(!lEarly && digitalRead(PIN_BTN_L) == LOW){
        lEarly = true;
        disp.clearBuffer();
        disp.drawFrame(0,16,64,48);
        disp.drawFrame(64,16,64,48);
        drawBoxed(0,16,64,48,"РАНО!",FONT_CYR);
        disp.sendBuffer();
      }
      if(!rEarly && digitalRead(PIN_BTN_R) == LOW){
        rEarly = true;
        disp.clearBuffer();
        disp.drawFrame(0,16,64,48);
        disp.drawFrame(64,16,64,48);
        drawBoxed(64,16,64,48,"РАНО!",FONT_CYR);
        disp.sendBuffer();
      }
      if(lEarly && rEarly) break;
    }

    // 2) общий сигнал
    signalBoth();

    // 3) измеряем реакцию (если не «РАНО»)
    long lm = 0, rm = 0;
    bool gotL = lEarly, gotR = rEarly;
    unsigned long start = micros();
    while(((micros() - start) / 1000) < 5000 && !(gotL && gotR)){
      if(!gotL && digitalRead(PIN_BTN_L) == LOW){
        lm = (micros() - signalTime) / 1000;
        vibrate(PIN_VIBRO_L);
        gotL = true;
      }
      if(!gotR && digitalRead(PIN_BTN_R) == LOW){
        rm = (micros() - signalTime) / 1000;
        vibrate(PIN_VIBRO_R);
        gotR = true;
      }
    }

    // 4) ставим штраф, если было «РАНО!»
    if(lEarly) lm = PENALTY_MS;
    if(rEarly) rm = PENALTY_MS;

    sumL += lm;
    sumR += rm;

    // 5) вывод результатов текущего раунда
    disp.clearBuffer();
    char buf[16];
    snprintf(buf, sizeof(buf), "Раунд %d", i + 1);
    drawCentered(12, buf, FONT_CYR);
    disp.drawFrame(0,16,64,48);
    disp.drawFrame(64,16,64,48);
    if(lEarly)       drawBoxed(0,16,64,48,"РАНО!",FONT_CYR);
    else             drawNumberMs(0,16,64,48,lm);
    if(rEarly)       drawBoxed(64,16,64,48,"РАНО!",FONT_CYR);
    else             drawNumberMs(64,16,64,48,rm);
    disp.sendBuffer();
    delay(900);
  }

  // 6) итоговый экран победы/ничьи
  disp.clearBuffer();
  disp.drawFrame(0,16,64,48);
  disp.drawFrame(64,16,64,48);
  if(sumL < sumR)      drawBoxed(0,16,64,48,"ПОБЕДА!",FONT_CYR);
  else if(sumR < sumL) drawBoxed(64,16,64,48,"ПОБЕДА!",FONT_CYR);
  else {
    drawBoxed(0,16,64,48,"НИЧЬЯ",FONT_CYR);
    drawBoxed(64,16,64,48,"НИЧЬЯ",FONT_CYR);
  }
  disp.sendBuffer();
  waitEnterNoHint();

  // 7) показываем среднее время по всем раундам
  long avgL = sumL / R;
  long avgR = sumR / R;

  disp.clearBuffer();
  disp.drawFrame(0,16,64,48);
  disp.drawFrame(64,16,64,48);
  char avgBufL[12], avgBufR[12];
  snprintf(avgBufL, sizeof(avgBufL), "%ld", avgL);
  snprintf(avgBufR, sizeof(avgBufR), "%ld", avgR);
  drawBoxed(0,16,64,24,avgBufL, FONT_NUM);
  drawBoxed(64,16,64,24,avgBufR, FONT_NUM);
  drawBoxed(0,40,64,24,"мс", FONT_CYR);
  drawBoxed(64,40,64,24,"мс", FONT_CYR);
  disp.sendBuffer();
  waitEnterHint();
}


// ===== Игра 2: Задержка =====
void timedMode(){
  const int R = 10;
  long sumL=0, sumR=0;

  for(int i=0;i<R;i++){
    showReady("Задержка", i+1, true);

    // 1) ловим преждевременные, показываем «РАНО!»
    bool lEarly=false, rEarly=false;
    unsigned long delayMs = random(500,3000), t0=millis();
    while(millis()-t0<delayMs){
      if(!lEarly && digitalRead(PIN_BTN_L)==LOW){
        lEarly=true;
        disp.clearBuffer();
        disp.drawFrame(0,16,64,48);
        disp.drawFrame(64,16,64,48);
        drawBoxed(0,16,64,48,"РАНО!",FONT_CYR);
        disp.sendBuffer(); delay(900);
      }
      if(!rEarly && digitalRead(PIN_BTN_R)==LOW){
        rEarly=true;
        disp.clearBuffer();
        disp.drawFrame(0,16,64,48);
        disp.drawFrame(64,16,64,48);
        drawBoxed(64,16,64,48,"РАНО!",FONT_CYR);
        disp.sendBuffer(); delay(900);
      }
      if(lEarly && rEarly) break;
    }
    // если оба «рано» — следующий раунд
    if(lEarly && rEarly) continue;

    // 2) общий сигнал
    signalBoth();

    // 3) измеряем в течение 3 сек
    unsigned long start=micros();
    bool gotL=lEarly, gotR=rEarly;
    long lm=0, rm=0;
    while(((micros()-start)/1000)<3000 && !(gotL&&gotR)){
      if(!gotL && digitalRead(PIN_BTN_L)==LOW){
        lm=(micros()-signalTime)/1000;
        vibrate(PIN_VIBRO_L);
        gotL=true;
      }
      if(!gotR && digitalRead(PIN_BTN_R)==LOW){
        rm=(micros()-signalTime)/1000;
        vibrate(PIN_VIBRO_R);
        gotR=true;
      }
    }
    // поздние
    if(!gotL) lm=0;
    if(!gotR) rm=0;

    sumL+=lm;
    sumR+=rm;

    // результат раунда
    disp.clearBuffer();
    char buf[16]; snprintf(buf,sizeof(buf),"Раунд %d",i+1);
    drawCentered(12, buf, FONT_CYR);
    disp.drawFrame(0,16,64,48);
    disp.drawFrame(64,16,64,48);
    if(lEarly) drawBoxed(0,16,64,48,"РАНО!",FONT_CYR);
    else if(lm==0) drawBoxed(0,16,64,48,"ПОЗДНО!",FONT_CYR);
    else          drawNumberMs(0,16,64,48,lm);
    if(rEarly) drawBoxed(64,16,64,48,"РАНО!",FONT_CYR);
    else if(rm==0) drawBoxed(64,16,64,48,"ПОЗДНО!",FONT_CYR);
    else           drawNumberMs(64,16,64,48,rm);
    disp.sendBuffer(); delay(900);
  }

  // итоги
  disp.clearBuffer();
  disp.drawFrame(0,16,64,48);
  disp.drawFrame(64,16,64,48);
  if(sumL>sumR)      drawBoxed(0,16,64,48,"ПОБЕДА!",FONT_CYR);
  else if(sumR>sumL) drawBoxed(64,16,64,48,"ПОБЕДА!",FONT_CYR);
  else {
    drawBoxed(0,16,64,48,"НИЧЬЯ",FONT_CYR);
    drawBoxed(64,16,64,48,"НИЧЬЯ",FONT_CYR);
  }
  disp.sendBuffer(); waitEnterNoHint();

  disp.clearBuffer();
  disp.drawFrame(0,16,64,48);
  disp.drawFrame(64,16,64,48);
  char bL[12],bR[12];
  snprintf(bL,sizeof(bL),"%ld",sumL);
  snprintf(bR,sizeof(bR),"%ld",sumR);
  drawBoxed(0,16,64,24,bL,FONT_NUM);
  drawBoxed(64,16,64,24,bR,FONT_NUM);
  drawBoxed(0,40,64,24,"мс",FONT_CYR);
  drawBoxed(64,40,64,24,"мс",FONT_CYR);
  disp.sendBuffer(); waitEnterHint();
}

// ===== МЕНЮ =====
void showMenu() {
  disp.clearBuffer();

  // 1) «ВЫБЕРИТЕ РЕЖИМ» в маленькой рамке по ширине текста + отступы
  disp.setFont(FONT_CYR);
  const char* title = "ВЫБЕРИТЕ ИГРУ";
  int tw = disp.getUTF8Width(title);
  int fw = tw + 22;                      // ширина рамки: текст + 8px полей
  int fx = (128 - fw) / 2;              // центрируем рамку
  disp.drawFrame(fx, 0, fw, 16);        // высота рамки 16px
  drawCentered(12, title, FONT_CYR);    // текст на y=12 (центриров.)

  const char* items[2] = { "Реакция", "Задержка" };
  for (int i = 0; i < 2; i++) {
    int y = 32 + i * 20;                 // y=32 и y=52
    int by = y - 12;                     // рамка с 16px высоты, текст по центру
    if (i == punktMenu) {
      disp.drawBox(0, by, 128, 16);
      disp.setDrawColor(0);
      drawCentered(y, items[i], FONT_CYR);
      disp.setDrawColor(1);
    } else {
      drawCentered(y, items[i], FONT_CYR);
    }
  }

  disp.sendBuffer();
}

void handleMenu(){
  int a = digitalRead(PIN_ENC_A);
  if(a!=lastEncA && millis()-lastInputTime>100){
    lastInputTime=millis();
    punktMenu=(punktMenu+1)%2;
    showMenu();
  }
  lastEncA=a;
  if(digitalRead(PIN_ENC_BTN)==LOW && millis()-lastInputTime>200){
    lastInputTime=millis();
    inMenu=false;
    countdown(5);
    if(punktMenu==0) reactionMode();
    else             timedMode();
    inMenu=true;
    showMenu();
  }
}

void setup(){
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(800000);

  disp.begin();
  disp.enableUTF8Print();
  disp.setFont(FONT_CYR);

  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_BTN, INPUT_PULLUP);
  lastEncA = digitalRead(PIN_ENC_A);

  pinMode(PIN_BTN_L, INPUT_PULLUP);
  pinMode(PIN_BTN_R, INPUT_PULLUP);
  pinMode(PIN_VIBRO_L, OUTPUT);
  pinMode(PIN_VIBRO_R, OUTPUT);
  pinMode(PIN_LED_L, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  randomSeed(esp_random());
  showMenu();
}

void loop(){
  if(inMenu) handleMenu();
}
