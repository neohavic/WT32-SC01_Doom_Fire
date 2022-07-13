/************************************************************************************
 * Doom Fire graphics demo by Austin Everman
 * Port of Haroldo Amaral's STM32 implentation to a WT32-SC01 ESP32 based LCD board
 * 
 * Based on: https://github.com/filipedeschamps/doom-fire-algorithm (original) 
 * and https://github.com/agaelema/doom-fire-algorithm (Amaral's)
 ***********************************************************************************
 *TO-DO:
 *
 * - Performance enhancements
 * - Add multitask support
 * - Add touch to control wind direction
 *
 ************************************************************************************/

#include <stdint.h>
#include <Wire.h>
//#include "FT62XXTouchScreen.h" // Eventually add touch control for wind direction
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
//#include <Esp.h>

#define IWIDTH  480
#define IHEIGHT 320
#define PALETTESIZE (sizeof(ColorPalette)/sizeof(uint32_t))
#define ROWS 80 // Number of lines
#define COLS 120 // Number of columns
#define MATRIXSIZE (ROWS*COLS) // Number os vector (cells)
#define PIXELSIZE 4 // Size of pixel

uint8_t FlameMatrix[ROWS*COLS];

uint32_t ColorPalette[] = 
{
    0xFF070707, //{"r":7,"g":7,"b":7}
    0xFF1F0707, //{"r":31,"g":7,"b":7}
    0xFF2F0F07, //{"r":47,"g":15,"b":7}
    0xFF470F07, //{"r":71,"g":15,"b":7}
    0xFF571707, //{"r":87,"g":23,"b":7}
    0xFF671F07, //{"r":103,"g":31,"b":7}
    0xFF771F07, //{"r":119,"g":31,"b":7}
    0xFF8F2707, //{"r":143,"g":39,"b":7}
    0xFF9F2F07, //{"r":159,"g":47,"b":7}
    0xFFAF3F07, //{"r":175,"g":63,"b":7}
    0xFFBF4707, //{"r":191,"g":71,"b":7}
    0xFFC74707, //{"r":199,"g":71,"b":7}
    0xFFDF4707, //{"r":223,"g":79,"b":7}
    0xFFDF5707, //{"r":223,"g":87,"b":7}
    0xFFDF5707, //{"r":223,"g":87,"b":7}
    0xFFD75F07, //{"r":215,"g":95,"b":7}
    0xFFD75F07, //{"r":215,"g":95,"b":7}
    0xFFD7670F, //{"r":215,"g":103,"b":15}
    0xFFCF6F0F, //{"r":207,"g":111,"b":15}
    0xFFCF770F, //{"r":207,"g":119,"b":15}
    0xFFCF7F0F, //{"r":207,"g":127,"b":15}
    0xFFCF8717, //{"r":207,"g":135,"b":23}
    0xFFC78717, //{"r":199,"g":135,"b":23}
    0xFFC78F17, //{"r":199,"g":143,"b":23}
    0xFFC7971F, //{"r":199,"g":151,"b":31}
    0xFFBF9F1F, //{"r":191,"g":159,"b":31}
    0xFFBF9F1F, //{"r":191,"g":159,"b":31}
    0xFFBFA727, //{"r":191,"g":167,"b":39}
    0xFFBFA727, //{"r":191,"g":167,"b":39}
    0xFFBFAF2F, //{"r":191,"g":175,"b":47}
    0xFFB7AF2F, //{"r":183,"g":175,"b":47}
    0xFFB7B72F, //{"r":183,"g":183,"b":47}
    0xFFB7B737, //{"r":183,"g":183,"b":55}
    0xFFCFCF6F, //{"r":207,"g":207,"b":111}
    0xFFDFDF9F, //{"r":223,"g":223,"b":159}
    0xFFEFEFC7, //{"r":239,"g":239,"b":199}
    0xFFFFFFFF, //{"r":255,"g":255,"b":255}
};

/*********************************************************************
 * Prototype of functions
 *********************************************************************/
void fillWithZeros(uint8_t *ptrMatrix, uint32_t size);
void createFireSource(uint8_t *ptrMatrix, uint32_t rows, uint32_t cols, uint32_t fireIntensity);
void drawFlames(uint8_t *ptrMatrix, uint32_t rows, uint32_t cols, uint32_t pixelSize);
void calculateFirePropagation(uint8_t *ptrMatrix, uint32_t rows, uint32_t cols, int32_t randAtt, int32_t wind);

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
TFT_eSprite img = TFT_eSprite(&tft);

uint32_t color;

float fps = 0;
int fpscount;
unsigned long lastfps = millis();

static unsigned int g_seed;

// Used to seed the generator.           
inline void fast_srand(int seed) {
    g_seed = seed;
}

// Compute a pseudorandom integer.
// Output value in range [0, 32767]
inline int fast_rand(void) {
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}

void setup() 
{
  fast_srand(500);
  tft.init();
  // Backlight hack...
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, 128);
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  Serial.begin(115200);
}

void loop() 
{ 
  uint8_t *matrixPtr;                             // pointer to matrix
  matrixPtr = FlameMatrix;
    
  fillWithZeros(matrixPtr, MATRIXSIZE);           // fill matrix with zeros

  /*
   * create firesource 
   * - matrix     -> pointer to fire matrix
    * - rows       -> number of lines
    * - cols       -> number of columns
    * - fireIntensity -> intensity of source
    */
    createFireSource(matrixPtr, ROWS, COLS, 36);
    
    // Set colour depth of Sprite to 8 (or 16) bits
    img.setColorDepth(16);
    
    while(1)
    {
      /*
       * merge functions "calculateFirePropagation" and "updateFireIntensityPerPixel"
       * - matrix     -> pointer to fire matrix
       * - rows       -> number of lines
       * - cols       -> number of columns
       * - randAtt    -> attenuation of randomness
       * - wind       -> negative number (left), positive (right), zero (no wind)
       */

      calculateFirePropagation(matrixPtr, ROWS, COLS, 3 , -3);
      drawFlames(matrixPtr, ROWS, COLS, PIXELSIZE);
    }
}

void fillWithZeros(uint8_t *ptrMatrix, uint32_t size)
{
  uint32_t counter;
    
  for (counter = 0; counter < size; counter++)
  {
    *(ptrMatrix+counter) = 0;
  }
}

void createFireSource(uint8_t *ptrMatrix, uint32_t rows, uint32_t cols, uint32_t fireIntensity)
{
  int32_t lasRow = rows*cols - cols;
  uint32_t counter;
    
  if (fireIntensity > 24) fireIntensity = 24;
    
  for (counter = 0; counter < cols; counter++) // In each cell
  {
    ptrMatrix[lasRow + counter] = fireIntensity;
  }
}

void drawFlames(uint8_t *ptrMatrix, uint32_t rows, uint32_t cols, uint32_t pixelSize)
{
  uint32_t rowCounter = 0;
  uint32_t colCounter = 0;

  // Create the sprite and clear background to black
  img.createSprite(IWIDTH, IHEIGHT);
    
  for (rowCounter = 0; rowCounter < rows; rowCounter++) // In each line
  {
    for (colCounter = 0; colCounter < cols; colCounter++) // In each column
    {
      color = tft.color24to16(ColorPalette[*(ptrMatrix + rowCounter*cols + colCounter)]); // Convert palette colors to 16bit RGB
      img.fillRect(pixelSize*colCounter, pixelSize*rowCounter, pixelSize, pixelSize, color);
    }
  }

  // Display FPS
    img.setCursor(10, 10);
    img.print("FPS: ");
    img.printf("%.4f", fps);

    // Calculate FPS
    fpscount++;
    unsigned long fpsdelay = millis() - lastfps;

    if(fpsdelay >= 1000) 
    {
      fps = float(fpscount*1000)/float(fpsdelay);
      lastfps = millis();
      fpscount = 0;
    }

    /**** BEGIN DEBUG CODE ****/
    // Check free heap memory before deleting sprite...
/*
    Serial.print(" Free memory: ");
    Serial.print (esp_get_free_heap_size()); 
    Serial.print("\n");
*/
    img.pushSprite(0 ,0);
    img.deleteSprite();

    /**** BEGIN DEBUG CODE ****/
    // Check free heap memory AFTER deleting sprite to compare; used to debug random display corruption and restarts
/*    
    Serial.print(" Free memory: ");
    Serial.print (esp_get_free_heap_size()); 
    Serial.print("\n");
*/
}

void calculateFirePropagation(uint8_t *ptrMatrix, uint32_t rows, uint32_t cols, int32_t randAtt, int32_t wind)
{
  uint32_t rowCounter = 0;
  uint32_t colCounter = 0;
  int32_t currentIndex;
    
  for (colCounter = 0; colCounter < cols; colCounter++) // In each column
  {
    for (rowCounter = 0; rowCounter < rows - 1; rowCounter++) // In each line
    {
      float random = (float)rand()/0xFFFFFFFF; // Calculate a random number  between 0-
      int32_t decay = (int32_t)(random * randAtt); // Calculate decay

      // Calculate the current index considering the randomness 
      currentIndex = rowCounter*cols + colCounter + (int32_t)(random * wind);

      // Avoid exiting the matrix
      if (currentIndex < 0) 
      {
        currentIndex = 0;
      }
                       
      int32_t temp = *(ptrMatrix + (rowCounter+1)*cols + colCounter) - decay;
      *(ptrMatrix + currentIndex) = (temp <=0) ? 0 : temp; // Avoid nonexistent palette color
    }
  }
}