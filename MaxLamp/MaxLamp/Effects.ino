// -------------------------------------- АУДІО АНАЛІЗАТОР ---------------------------------------------
#define VOL_THR 20

void audioAnalyzerEffect() {
  byte audioAnalyzerValue = 0;
  int thisMax = 0;
  int thisSignal = 0;
  for (int i = 0; i < 200; i++) {
    thisSignal = analogRead(ADC_PIN);
    if (thisSignal > thisMax) thisMax = thisSignal;
  }

  static float filMax = 1000, filMin = 1000;
  static float filLength;
  static int maxMax = 0, minMin = 1023;
  static byte counter;

  counter++;
  if (counter == 50) {
    counter = 0;
    maxMax = 0;
    minMin = 1023;
  }
  if (thisMax > maxMax) maxMax = thisMax;
  if (thisMax < minMin) minMin = thisMax;

  filMax += (float)(maxMax - filMax) * 0.01;
  filMin += (float)(minMin - filMin) * 0.01;

  int thisLenght = map(thisMax - filMin, VOL_THR, filMax - filMin, 0, HEIGHT - 1);
  thisLenght = constrain(thisLenght, 0, HEIGHT - 1);

  filLength += (float)(thisLenght - filLength) * 0.2;
  if (thisMax > filMax) filLength = HEIGHT - 1;
  
  if (filMax - filMin > VOL_THR) audioAnalyzerValue = filLength;
  else audioAnalyzerValue = 0;

  //FastLED.clear();
  int scale = effects[currentEffect].getScale();
  for(byte i = 0; i < HEIGHT; i++){
    if(i <= audioAnalyzerValue){
      drawRow(i, CHSV(255 - (scale + i * 7), 255, 255));
      /*if(i < 4) drawRow(i, CRGB::Green);
      else if(i >= 4 && i < 8) drawRow(i, CRGB::Yellow);
      else if(i >= 8 && i < 12) drawRow(i, CRGB::OrangeRed);
      else if(i >= 12 && i < 16) drawRow(i, CRGB::Red);*/
    }
    else drawRow(i, CRGB::Black);
  }
}

// -------------------------------------- СТАТИЧНИЙ КОЛІР ---------------------------------------------
void coloredLightEffect(){
  int scale = effects[currentEffect].getScale() * 2;
  if(scale < 255){
    effects[currentEffect].scaleStep = 2;
    fillAll(CHSV(scale, 255, 255));
  }
  else if(scale >= 255 && scale <= effects[currentEffect].getMaxScale() * 2){
    effects[currentEffect].scaleStep = 4;
    fillAll(CHSV(20, (scale - 255) / 1.2f, 255));
  }
}

// -------------------------------------- ВОГОНЬ ---------------------------------------------
#define SPARKLES 1        // вилітаючі жаринки on | off
unsigned char line[WIDTH];
int pcnt = 0;
unsigned char matrixValue[8][16];

//these values are substracetd from the generated values to give a shape to the animation
const unsigned char valueMask[8][16] PROGMEM = {
  {0  , 0  , 0  , 32 , 32 , 0  , 0  , 0  , 0  , 0  , 0  , 32 , 32 , 0  , 0  , 0  },
  {0  , 0  , 0  , 64 , 64 , 0  , 0  , 0  , 0  , 0  , 0  , 64 , 64 , 0  , 0  , 0  },
  {0  , 0  , 32 , 96 , 96 , 32 , 0  , 0  , 0  , 0  , 32 , 96 , 96 , 32 , 0  , 0  },
  {0  , 32 , 64 , 128, 128, 64 , 32 , 0  , 0  , 32 , 64 , 128, 128, 64 , 32 , 0  },
  {32 , 64 , 96 , 160, 160, 96 , 64 , 32 , 32 , 64 , 96 , 160, 160, 96 , 64 , 32 },
  {64 , 96 , 128, 192, 192, 128, 96 , 64 , 64 , 96 , 128, 192, 192, 128, 96 , 64 },
  {96 , 128, 160, 255, 255, 160, 128, 96 , 96 , 128, 160, 255, 255, 160, 128, 96 },
  {128, 160, 192, 255, 255, 192, 160, 128, 128, 160, 192, 255 , 255, 192, 160, 128}
};

//these are the hues for the fire,
//should be between 0 (red) to about 25 (yellow)
const unsigned char hueMask[8][16] PROGMEM = {
  {25, 22, 11, 1 , 1 , 11, 19, 25, 25, 22, 11, 1 , 1 , 11, 19, 25},
  {25, 19, 8 , 1 , 1 , 8 , 13, 19, 25, 19, 8 , 1 , 1 , 8 , 13, 19},
  {19, 16, 8 , 1 , 1 , 8 , 13, 16, 19, 16, 8 , 1 , 1 , 8 , 13, 16},
  {13, 13, 5 , 1 , 1 , 5 , 11, 13, 13, 13, 5 , 1 , 1 , 5 , 11, 13},
  {11, 11, 5 , 1 , 1 , 5 , 11, 11, 11, 11, 5 , 1 , 1 , 5 , 11, 11},
  {8 , 5 , 1 , 0 , 0 , 1 , 5 , 8 , 8 , 5 , 1 , 0 , 0 , 1 , 5 , 8 },
  {5 , 1 , 0 , 0 , 0 , 0 , 1 , 5 , 5 , 1 , 0 , 0 , 0 , 0 , 1 , 5 },
  {1 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 0 , 0 , 0 , 0 , 0 , 0 , 1 }
};

void fireEffect(){
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  if (loadingFlag){
    loadingFlag = false;
    generateLine();
  }
 
  if (pcnt >= 100){
    shiftUp();
    generateLine();
    pcnt = 0;
  }
  
  drawFrame(pcnt);
  pcnt += 30;
}

// Randomly generate the next line (matrix row)
void generateLine() {
  for (uint8_t x = 0; x < WIDTH; x++){
    line[x] = random(64, 255);
  }
}

void shiftUp() {
  for (uint8_t y = HEIGHT - 1; y > 0; y--){
    for (uint8_t x = 0; x < WIDTH; x++){
      uint8_t newX = x;
      if (x > 15) newX = x - 15;
      if (y > 7) continue;
      matrixValue[y][newX] = matrixValue[y - 1][newX];
    }
  }

  for (uint8_t x = 0; x < WIDTH; x++){
    uint8_t newX = x;
    if (x > 15) newX = x - 15;
    matrixValue[0][newX] = line[newX];
  }
}

// draw a frame, interpolating between 2 "key frames"
// @param pcnt percentage of interpolation
void drawFrame(int pcnt){
  int nextv;

  //each row interpolates with the one before it
  for (unsigned char y = HEIGHT - 1; y > 0; y--){
    for (unsigned char x = 0; x < WIDTH; x++){
      uint8_t newX = x;
      if (x > 15) newX = x - 15;
      if (y < 8){
        nextv = (((100.0 - pcnt) * matrixValue[y][newX]
                + pcnt * matrixValue[y - 1][newX]) / 100.0)
                - pgm_read_byte(&(valueMask[y][newX]));

        CRGB color = CHSV(effects[currentEffect].getScale() + pgm_read_byte(&(hueMask[y][newX])), // H
                          255, // S
                          (uint8_t)max(0, nextv) // V
                          );

        leds[getPixelNumber(x, y)] = color;
      } 
      else if (y == 8 && SPARKLES){
        if (random(0, 20) == 0 && getPixColorXY(x, y - 1) != 0) drawPixelXY(x, y, getPixColorXY(x, y - 1));
        else drawPixelXY(x, y, 0);
      } 
      else if (SPARKLES){
        // яскравість
        if (getPixColorXY(x, y - 1) > 0)
          drawPixelXY(x, y, getPixColorXY(x, y - 1));
        else drawPixelXY(x, y, 0);
      }
    }
  }

  //first row interpolates with the "next" line
  for (unsigned char x = 0; x < WIDTH; x++){
    uint8_t newX = x;
    if (x > 15) newX = x - 15;
    CRGB color = CHSV(effects[currentEffect].getScale() + pgm_read_byte(&(hueMask[0][newX])), // H
                      255,           // S
                      (uint8_t)(((100.0 - pcnt) * matrixValue[0][newX] + pcnt * line[newX]) / 100.0) // V
                      );
    leds[getPixelNumber(newX, 0)] = color;
  }
}

byte hue;

// -------------------------------------- ВЕСЕЛКИ --------------------------------------
void rainbowVerticalEffect() {
  hue += 2;
  for (byte j = 0; j < HEIGHT; j++) {
    CHSV thisColor = CHSV((byte)(hue + j * effects[currentEffect].getScale()), 255, 255);
    for (byte i = 0; i < WIDTH; i++)
      drawPixelXY(i, j, thisColor);
  }
}
void rainbowHorizontalEffect() {
  hue += 2;
  for (byte i = 0; i < WIDTH; i++) {
    CHSV thisColor = CHSV((byte)(hue + i *effects[currentEffect].getScale()), 255, 255);
    for (byte j = 0; j < HEIGHT; j++)
      drawPixelXY(i, j, thisColor);   //leds[getPixelNumber(i, j)] = thisColor;
  }
}

// -------------------------------------- ДОЩ --------------------------------------
void rainEffect() {
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  for (byte x = 0; x < WIDTH; x++) {
    uint32_t thisColor = getPixColorXY(x, HEIGHT - 1);
    if (thisColor == 0)
      drawPixelXY(x, HEIGHT - 1, 0x0000FF * (random(0, 
        map(effects[currentEffect].getScale(), effects[currentEffect].getMinScale(), effects[currentEffect].getMaxScale(), effects[currentEffect].getMaxScale(), effects[currentEffect].getMinScale())) == 0));
    else if (thisColor < 0x000020)
      drawPixelXY(x, HEIGHT - 1, 0);
    else
      drawPixelXY(x, HEIGHT - 1, thisColor - 0x000020);
  }

  // сдвигаем всё вниз
  for (byte x = 0; x < WIDTH; x++) {
    for (byte y = 0; y < HEIGHT - 1; y++) {
      drawPixelXY(x, y, getPixColorXY(x, y + 1));
    }
  }
}

// -------------------------------------- СНІГОПАД --------------------------------------
void snowEffect() {
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  for (byte x = 0; x < WIDTH; x++) {
    uint32_t thisColor = getPixColorXY(x, HEIGHT - 1);
    if (thisColor == 0)
      drawPixelXY(x, HEIGHT - 1, 0xFFFFFF * (random(0, 
        map(effects[currentEffect].getScale(), effects[currentEffect].getMinScale(), effects[currentEffect].getMaxScale(), effects[currentEffect].getMaxScale(), effects[currentEffect].getMinScale())) == 0));
    else if (thisColor < 0x202020)
      drawPixelXY(x, HEIGHT - 1, 0);
    else
      drawPixelXY(x, HEIGHT - 1, thisColor - 0x202020);
  }

  // сдвигаем всё вниз
  for (byte x = 0; x < WIDTH; x++) {
    for (byte y = 0; y < HEIGHT - 1; y++) {
      drawPixelXY(x, y, getPixColorXY(x, y + 1));
    }
  }
 /*if(!isPlaying && isSound) mp3Play(currentEffect + 100);

  for (byte x = 0; x < WIDTH; x++) {
    for (byte y = 0; y < HEIGHT - 1; y++) {
      drawPixelXY(x, y, getPixColorXY(x, y + 1));
    }
  }

  for (byte x = 0; x < WIDTH; x++) {
    if (getPixColorXY(x, HEIGHT - 2) == 0 && (random(0, 
        map(effects[currentEffect].getScale(), effects[currentEffect].getMinScale(), effects[currentEffect].getMaxScale(), effects[currentEffect].getMaxScale(), effects[currentEffect].getMinScale())) == 0))
      drawPixelXY(x, HEIGHT - 1, 0xE0FFFF - 0x101010 * random(0, 4));
    else
      drawPixelXY(x, HEIGHT - 1, 0x000000);
  }*/
}

// -------------------------------------- МАТРИЦЯ --------------------------------------
void matrixEffect() {
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  for (byte x = 0; x < WIDTH; x++) {
    uint32_t thisColor = getPixColorXY(x, HEIGHT - 1);
    if (thisColor == 0)
      drawPixelXY(x, HEIGHT - 1, 0x00FF00 * (random(0, 
        map(effects[currentEffect].getScale(), effects[currentEffect].getMinScale(), effects[currentEffect].getMaxScale(), effects[currentEffect].getMaxScale(), effects[currentEffect].getMinScale())) == 0));
    else if (thisColor < 0x002000)
      drawPixelXY(x, HEIGHT - 1, 0);
    else
      drawPixelXY(x, HEIGHT - 1, thisColor - 0x002000);
  }

  // сдвигаем всё вниз
  for (byte x = 0; x < WIDTH; x++) {
    for (byte y = 0; y < HEIGHT - 1; y++) {
      drawPixelXY(x, y, getPixColorXY(x, y + 1));
    }
  }
}

// -------------------------------------- СВІТЛЯЧКИ --------------------------------------
#define LIGHTERS_AM 100
int lightersPos[2][LIGHTERS_AM];
int8_t lightersSpeed[2][LIGHTERS_AM];
CHSV lightersColor[LIGHTERS_AM];
byte loopCounter;

int angle[LIGHTERS_AM];
int speedV[LIGHTERS_AM];
int8_t angleSpeed[LIGHTERS_AM];

void firefliesEffect() {
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  if (loadingFlag) {
    loadingFlag = false;
    randomSeed(millis());
    for (byte i = 0; i < LIGHTERS_AM; i++) {
      lightersPos[0][i] = random(0, WIDTH * 10);
      lightersPos[1][i] = random(0, HEIGHT * 10);
      lightersSpeed[0][i] = random(-10, 10);
      lightersSpeed[1][i] = random(-10, 10);
      lightersColor[i] = CHSV(random(0, 255), 255, 255);
    }
  }
  FastLED.clear();
  if (++loopCounter > 20) loopCounter = 0;
  for (byte i = 0; i < effects[currentEffect].getScale(); i++) {
    if (loopCounter == 0) {     // змінюємо швидкість кожні 255 відображень
      lightersSpeed[0][i] += random(-3, 4);
      lightersSpeed[1][i] += random(-3, 4);
      lightersSpeed[0][i] = constrain(lightersSpeed[0][i], -20, 20);
      lightersSpeed[1][i] = constrain(lightersSpeed[1][i], -20, 20);
    }

    lightersPos[0][i] += lightersSpeed[0][i];
    lightersPos[1][i] += lightersSpeed[1][i];

    if (lightersPos[0][i] < 0) lightersPos[0][i] = (WIDTH - 1) * 10;
    if (lightersPos[0][i] >= WIDTH * 10) lightersPos[0][i] = 0;

    if (lightersPos[1][i] < 0) {
      lightersPos[1][i] = 0;
      lightersSpeed[1][i] = -lightersSpeed[1][i];
    }
    if (lightersPos[1][i] >= (HEIGHT - 1) * 10) {
      lightersPos[1][i] = (HEIGHT - 1) * 10;
      lightersSpeed[1][i] = -lightersSpeed[1][i];
    }
    drawPixelXY(lightersPos[0][i] / 10, lightersPos[1][i] / 10, lightersColor[i]);
  }
}

// --------------------------------- БЛИСКІТКИ ------------------------------------
void sparklesEffect() {
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  for (byte i = 0; i < effects[currentEffect].getScale(); i++) {
    byte x = random(0, WIDTH);
    byte y = random(0, HEIGHT);
    if (getPixColorXY(x, y) == 0)
      leds[getPixelNumber(x, y)] = CHSV(random(0, 255), 255, 255);
  }
  fader(70);
}

void fader(byte step) {
  for (byte i = 0; i < WIDTH; i++) {
    for (byte j = 0; j < HEIGHT; j++) {
      fadePixel(i, j, step);
    }
  }
}
void fadePixel(byte i, byte j, byte step) {
  int pixelNum = getPixelNumber(i, j);
  if (getPixColor(pixelNum) == 0) return;

  if (leds[pixelNum].r >= 30 ||
      leds[pixelNum].g >= 30 ||
      leds[pixelNum].b >= 30) {
    leds[pixelNum].fadeToBlackBy(step);
  } else {
    leds[pixelNum] = 0;
  }
}

// ********************************************* NOISE EFFECTS *********************************************

// The 16 bit version of our coordinates
static uint16_t x;
static uint16_t y;
static uint16_t z;

uint16_t speed = 20; // speed is set dynamically once we've started up
uint16_t scale = 30; // scale is set dynamically once we've started up

// This is the array that we keep our computed noise values in
#define MAX_DIMENSION (max(WIDTH, HEIGHT))
#if (WIDTH > HEIGHT)
uint8_t noise[WIDTH][WIDTH];
#else
uint8_t noise[HEIGHT][HEIGHT];
#endif

CRGBPalette16 currentPalette( PartyColors_p );
uint8_t colorLoop = 1;
uint8_t ihue = 0;

void fillNoiseLED(){
  uint8_t dataSmoothing = 0;
  if (speed < 50) dataSmoothing = 200 - (speed * 4);

  for (int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = scale * i;
    for (int j = 0; j < MAX_DIMENSION; j++) {
      int joffset = scale * j;

      uint8_t data = inoise8(x + ioffset, y + joffset, z);
      data = qsub8(data, 16);
      data = qadd8(data, scale8(data, 39));

      if (dataSmoothing){
        uint8_t olddata = noise[i][j];
        uint8_t newdata = scale8(olddata, dataSmoothing) + scale8(data, 256 - dataSmoothing);
        data = newdata;
      }

      noise[i][j] = data;
    }
  }
  
  z += speed;
  // apply slow drift to X and Y, just for visual variation.
  x += speed / 8;
  y -= speed / 16;

  for (int i = 0; i < WIDTH; i++) {
    for (int j = 0; j < HEIGHT; j++) {
      uint8_t index = noise[j][i];
      uint8_t bri = noise[i][j];
      
      // if this palette is a 'loop', add a slowly-changing base value
      if (colorLoop) index += ihue;
      
      // brighten up, as the color palette itself often contains the
      // light/dark dynamic range desired
      if (bri > 127) bri = 255; 
      else bri = dim8_raw(bri * 2);
      
      CRGB color = ColorFromPalette(currentPalette, index, bri);      
      drawPixelXY(i, j, color);   //leds[getPixelNumber(i, j)] = color;
    }
  }
  ihue += 1;
}

void fillnoise8() {
  for (int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = scale * i;
    for (int j = 0; j < MAX_DIMENSION; j++) {
      int joffset = scale * j;
      noise[i][j] = inoise8(x + ioffset, y + joffset, z);
    }
  }
  z += speed;
}

// ------------------------------------ ЛАВА ------------------------------------
void lava3DEffect() {
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  if (loadingFlag) {
    loadingFlag = false;
    currentPalette = LavaColors_p;
    colorLoop = 0;
  }
  
  scale = effects[currentEffect].getScale();
  speed = effects[currentEffect].getSpeed();
  fillNoiseLED();
}

// ------------------------------------ БОЖЕВІЛЛЯ КОЛЬОРІВ ------------------------------------
void madness3DEffect() {
  if (loadingFlag) {
    loadingFlag = false;
  }
  
  scale = effects[currentEffect].getScale();
  speed = effects[currentEffect].getSpeed();
  fillnoise8();
  
  for (int i = 0; i < WIDTH; i++) {
    for (int j = 0; j < HEIGHT; j++) {
      CRGB thisColor = CHSV(noise[j][i], 255, noise[i][j]);
      drawPixelXY(i, j, thisColor);   //leds[getPixelNumber(i, j)] = CHSV(noise[j][i], 255, noise[i][j]);
    }
  }
  ihue += 1;
}

// ------------------------------------ ВЕСЕЛКА 3D ------------------------------------
void rainbow3DEffect() {
  if (loadingFlag) {
    loadingFlag = false;
    currentPalette = RainbowColors_p;
    colorLoop = 1;
  }

  scale = effects[currentEffect].getScale();
  speed = effects[currentEffect].getSpeed();
  fillNoiseLED();
}

// ------------------------------------ ВЕСЕЛКОВА СТРІЧКА ------------------------------------
void rainbowStripe3DEffect() {
  if (loadingFlag) {
    loadingFlag = false;
    currentPalette = RainbowStripeColors_p;
    colorLoop = 1;
  }

  scale = effects[currentEffect].getScale();
  speed = effects[currentEffect].getSpeed();
  fillNoiseLED();
}

// ------------------------------------ ЗЕБРА ------------------------------------
void zebra3DEffect() {
  if (loadingFlag) {
    loadingFlag = false;
    // 'black out' all 16 palette entries...
    fill_solid( currentPalette, 16, CRGB::Black);
    // and set every fourth one to white.
    currentPalette[0] = CRGB::White;
    currentPalette[4] = CRGB::White;
    currentPalette[8] = CRGB::White;
    currentPalette[12] = CRGB::White;
    colorLoop = 1;
  }

  scale = effects[currentEffect].getScale();
  speed = effects[currentEffect].getSpeed();
  fillNoiseLED();
}

// ------------------------------------ ЛІС ------------------------------------
void forest3DEffect() {
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  if (loadingFlag) {
    loadingFlag = false;
    currentPalette = ForestColors_p;
    colorLoop = 0;
  }

  scale = effects[currentEffect].getScale();
  speed = effects[currentEffect].getSpeed();
  fillNoiseLED();
}

// ------------------------------------ ОКЕАН ------------------------------------
void ocean3DEffect() {
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  if (loadingFlag) {
    loadingFlag = false;
    currentPalette = OceanColors_p;
    colorLoop = 0;
  }

  scale = effects[currentEffect].getScale();
  speed = effects[currentEffect].getSpeed();
  fillNoiseLED();
}

// ------------------------------------ ПЛАЗМА ------------------------------------
void plasma3DEffect() {
  if (loadingFlag) {
    loadingFlag = false;
    currentPalette = PartyColors_p;
    colorLoop = 1;
  }

  scale = effects[currentEffect].getScale();
  speed = effects[currentEffect].getSpeed();
  fillNoiseLED();
}

// ------------------------------------ НЕБО ------------------------------------
void sky3DEffect() {
  if(!isPlaying && isSound) mp3Play(currentEffect + 100);
  
  if (loadingFlag) {
    loadingFlag = false;
    currentPalette = CloudColors_p;
    colorLoop = 0;
  }

  scale = effects[currentEffect].getScale();
  speed = effects[currentEffect].getSpeed();
  fillNoiseLED();
}
