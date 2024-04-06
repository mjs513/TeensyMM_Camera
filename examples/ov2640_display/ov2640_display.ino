#include <stdint.h>
#include <SD.h>
#include <SPI.h>

#include "Camera.h"

//#define USE_MMOD_ATP_ADAPTER
#define USE_SDCARD

#define ARDUCAM_CAMERA_OV2640
#include "TMM_OV2640/OV2640.h"
OV2640 omni;
Camera camera(omni);
#define CameraID OV2640a
#define MIRROR_FLIP_CAMERA

//set cam configuration - need to remember when saving jpeg
framesize_t camera_framesize = FRAMESIZE_QVGA;
pixformat_t camera_format = RGB565;

#define skipFrames 1

File file;

/************** Set up MTP Disk *************/
#if defined(USE_SDCARD)
  #include <MTP_Teensy.h>
  #include <LittleFS.h>

  //Used a store of index file
  LittleFS_Program lfsProg; // Used to create FS on the Flash memory of the chip
  FS *myfs = &lfsProg; // current default FS...
  static const uint32_t file_system_size = 1024 * 512;
  uint8_t current_store = 0;
  uint8_t storage_index = '0';

  #define SPI_SPEED SD_SCK_MHZ(50)  // adjust to sd card 
  elapsedMillis elapsed_millis_since_last_sd_check = 0;
  #define TIME_BETWEEN_SD_CHECKS_MS 1000
  bool sdio_previously_present;

  const char *sd_str[]={"SD1"}; // edit to reflect your configuration
  #if MMOD_ML == 1
  const int cs[] = { 10}; // edit to reflect your configuration
  #else
  //const char *sd_str[]={"BUILTIN"}; // edit to reflect your configuration
  const int cs[] = {BUILTIN_SDCARD}; // edit to reflect your configuration
  #endif
  const int cdPin[] = {0xff};
  const int nsd = sizeof(sd_str)/sizeof(const char *);
  bool sd_media_present_prev[nsd];
    
  SDClass sdx[nsd];
#endif

/*****************************************************
 * If using the HM01B0 Arduino breakout 8bit mode    *
 * does not work.  Arduino breakout only brings out  *
 * the lower 4 bits.                                 *
 ****************************************************/
#define _hmConfig 0  // select mode string below

PROGMEM const char hmConfig[][48] = {
  "FLEXIO_CUSTOM_LIKE_8_BIT",
  "FLEXIO_CUSTOM_LIKE_4_BIT"
};


#ifdef ARDUINO_TEENSY_DEVBRD4
//Set up ILI9341
#undef USE_MMOD_ATP_ADAPTER

#define TFT_CS 10  // AD_B0_02
#define TFT_DC 25  // AD_B0_03
#define TFT_RST 24

#elif defined(USE_MMOD_ATP_ADAPTER)
#define TFT_DC 4   //0   // "TX1" on left side of Sparkfun ML Carrier
#define TFT_CS 5   //4   // "CS" on left side of Sparkfun ML Carrier
#define TFT_RST 2  //1  // "RX1" on left side of Sparkfun ML Carrier
#else
#define TFT_DC 0   //20   // "TX1" on left side of Sparkfun ML Carrier
#define TFT_CS 4   //5, 4   // "CS" on left side of Sparkfun ML Carrier
#define TFT_RST 1  //2, 1  // "RX1" on left side of Sparkfun ML Carrier
#endif

#include "ILI9341_t3n.h"  // https://github.com/KurtE/ILI9341_t3n
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);
#define TFT_BLACK ILI9341_BLACK
#define TFT_YELLOW ILI9341_YELLOW
#define TFT_RED ILI9341_RED
#define TFT_GREEN ILI9341_GREEN
#define TFT_BLUE ILI9341_BLUE
#define CENTER ILI9341_t3n::CENTER


// Setup framebuffers
DMAMEM uint16_t FRAME_WIDTH, FRAME_HEIGHT;
#ifdef ARDUINO_TEENSY_DEVBRD4
//#include "SDRAM_t4.h"
//SDRAM_t4 sdram;
uint16_t *frameBuffer = nullptr;
uint16_t *frameBuffer2 = nullptr;
#else
uint16_t DMAMEM frameBuffer[(480) * 320] __attribute__((aligned(32)));
uint16_t DMAMEM frameBuffer2[(320) * 240] __attribute__((aligned(32)));
#endif

// Setup display modes frame / video
bool g_continuous_flex_mode = false;
void *volatile g_new_flexio_data = nullptr;
uint32_t g_flexio_capture_count = 0;
uint32_t g_flexio_redraw_count = 0;
elapsedMillis g_flexio_runtime;
bool g_dma_mode = false;


void setup() {
  Serial.begin(921600);
  while (!Serial && millis() < 5000) {}
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
  SerialUSB1.begin(921600);
#endif

  if (CrashReport) {
    Serial.print(CrashReport);
    while (1)
      ;
  }

  //This is mandatory to begin the d session.
  #if defined(USE_SDCARD)
    storage_configure();
    MTP.begin();
  #endif

  tft.begin(15000000);
  test_display();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.println("Waiting for Arduino Serial Monitor...");
/*
#if defined(USE_SDCARD)
  Serial.println("Using SDCARD - Initializing");
#if MMOD_ML == 1
  if (!SD.begin(10)) {
#else
  if (!SD.begin(BUILTIN_SDCARD)) {
#endif
  }
  Serial.println("initialization failed!");
  //while (1){
  //    LEDON; delay(100);
  //    LEDOFF; delay(100);
  //  }
  Serial.println("initialization done.");
  delay(100);
#endif
*/

  while (!Serial)
    ;
  Serial.println("hm0360 Camera Test");
  Serial.println(hmConfig[_hmConfig]);
  Serial.println("------------------");

  delay(500);

  tft.fillScreen(TFT_BLACK);

/***************************************************************/
//    setPins(uint8_t mclk_pin, uint8_t pclk_pin, uint8_t vsync_pin, uint8_t hsync_pin, en_pin,
//    uint8_t g0, uint8_t g1,uint8_t g2, uint8_t g3,
//    uint8_t g4=0xff, uint8_t g5=0xff,uint8_t g6=0xff,uint8_t g7=0xff);
#ifdef USE_MMOD_ATP_ADAPTER
  pinMode(30, INPUT);
  pinMode(31, INPUT_PULLUP);

  if ((_hmConfig == 0) || (_hmConfig == 2)) {
    camera.setPins(29, 10, 33, 32, 31, 40, 41, 42, 43, 44, 45, 6, 9);
  } else if (_hmConfig == 1) {
    //camera.setPins(7, 8, 33, 32, 17, 40, 41, 42, 43);
    camera.setPins(29, 10, 33, 32, 31, 40, 41, 42, 43);
  }
#elif defined(ARDUINO_TEENSY_DEVBRD4)
  if ((_hmConfig == 0) || (_hmConfig == 2)) {
    camera.setPins(7, 8, 21, 46, 23, 40, 41, 42, 43, 44, 45, 6, 9);
  } else if (_hmConfig == 1) {
    //camera.setPins(7, 8, 33, 32, 17, 40, 41, 42, 43);
    camera.setPins(7, 8, 21, 46, 31, 40, 41, 42, 43);
  }

#else
  if (_hmConfig == 0) {
    //camera.setPins(29, 10, 33, 32, 31, 40, 41, 42, 43, 44, 45, 6, 9);
    camera.setPins(7, 8, 33, 32, 17, 40, 41, 42, 43, 44, 45, 6, 9);
  } else if (_hmConfig == 1) {
    camera.setPins(7, 8, 33, 32, 17, 40, 41, 42, 43);
  }
#endif

  uint8_t status = 0;
  status = camera.begin(camera_framesize, camera_format, 15, CameraID, false);
  camera.useDMA(true);

#ifdef MIRROR_FLIP_CAMERA
  camera.setHmirror(true);
  camera.setVflip(true);
#endif


Serial.printf("Begin status: %d\n", status);
if(!status) {
  Serial.println("Camera failed to start!!!");
  while(1){}
}

#if defined(ARDUINO_TEENSY_DEVBRD4)
  // we need to allocate bufers
  //if (!sdram.begin()) {
  //  Serial.printf("SDRAM Init Failed!!!\n");
  //  while (1)
  //    ;
  //};
  frameBuffer = (uint16_t *)((((uint32_t)(sdram_malloc(camera.width() * camera.height() * 2 + 32)) + 32) & 0xffffffe0));
  frameBuffer2 = (uint16_t *)((((uint32_t)(sdram_malloc(camera.width() * camera.height() * 2 + 32)) + 32) & 0xffffffe0));
  Serial.printf("Camera Buffers: %p %p\n", frameBuffer, frameBuffer2);
#endif

  //camera.setBrightness(0);          // -2 to +2
  //camera.setContrast(0);            // -2 to +2
  //camera.setSaturation(0);          // -2 to +2
  //omni.setSpecialEffect(RETRO);  // NOEFFECT, NEGATIVE, BW, REDDISH, GREEISH, BLUEISH, RETRO
  //omni.setWBmode(0);                  // AWB ON, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home

  Serial.println("Camera settings:");
  Serial.print("\twidth = ");
  Serial.println(camera.width());
  Serial.print("\theight = ");
  Serial.println(camera.height());
  //Serial.print("\tbits per pixel = NA");
  //Serial.println(camera.bitsPerPixel());
  Serial.println();
  Serial.printf("TFT Width = %u Height = %u\n\n", tft.width(), tft.height());

  FRAME_HEIGHT = camera.height();
  FRAME_WIDTH = camera.width();
  Serial.printf("ImageSize (w,h): %d, %d\n", FRAME_WIDTH, FRAME_HEIGHT);

  // Lets setup camera interrupt priorities:
  //camera.setVSyncISRPriority(102); // higher priority than default
  camera.setDMACompleteISRPriority(192);  // lower than default

  showCommandList();

}

bool hm0360_flexio_callback(void *pfb) {
  //Serial.println("Flexio callback");
  g_new_flexio_data = pfb;
  return true;
}


// Quick and Dirty
#define UPDATE_ON_CAMERA_FRAMES

inline uint16_t HTONS(uint16_t x) {
  #if defined(ARDUCAM_CAMERA_OV2640)
  return x;
  #else  //byte reverse
  return ((x >> 8) & 0x00FF) | ((x << 8) & 0xFF00);
  #endif
}

volatile uint16_t *pfb_last_frame_returned = nullptr;

bool camera_flexio_callback_video(void *pfb) {
  pfb_last_frame_returned = (uint16_t *)pfb;
#ifdef UPDATE_ON_CAMERA_FRAMES
  if ((uint32_t)pfb_last_frame_returned >= 0x20200000u)
    arm_dcache_delete((void *)pfb_last_frame_returned, FRAME_WIDTH * FRAME_HEIGHT * 2);
  int numPixels = camera.width() * camera.height();

  for (int i = 0; i < numPixels; i++) pfb_last_frame_returned[i] = HTONS(pfb_last_frame_returned[i]);

  tft.writeRect(0, 0, FRAME_WIDTH, FRAME_HEIGHT, (uint16_t *)pfb_last_frame_returned);
  pfb_last_frame_returned = nullptr;
  tft.setOrigin(0, 0);
  uint16_t *pframebuf = tft.getFrameBuffer();
  if ((uint32_t)pframebuf >= 0x20200000u) arm_dcache_flush(pframebuf, FRAME_WIDTH * FRAME_HEIGHT);
#endif
  //Serial.print("#");
  return true;
}

void frame_complete_cb() {
  //Serial.print("@");
#ifndef UPDATE_ON_CAMERA_FRAMES
  if (!pfb_last_frame_returned) return;
  if ((uint32_t)pfb_last_frame_returned >= 0x20200000u)
    arm_dcache_delete(pfb_last_frame_returned, FRAME_WIDTH * FRAME_HEIGHT * 2);
  tft.writeSubImageRectBytesReversed(0, 0, FRAME_WIDTH, FRAME_HEIGHT, 0, 0, FRAME_WIDTH, FRAME_HEIGHT, pfb_last_frame_returned);
  pfb_last_frame_returned = nullptr;
  uint16_t *pfb = tft.getFrameBuffer();
  if ((uint32_t)pfb >= 0x20200000u) arm_dcache_flush(pfb, FRAME_WIDTH * FRAME_HEIGHT);
#endif
}


void loop() {
  int ch;
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
  while (SerialUSB1.available()) {
    ch = SerialUSB1.read();
    Serial.println(ch, HEX);
    switch (ch) {
      case 0x30:
        {
        Serial.print(F("ACK CMD CAM start single shoot ... "));
        send_image(&SerialUSB1);
        Serial.println(F("READY. END"));
        }
        break;
      case 0x40:
        omni.setWBmode(0);
        break;
      case 0x41:
        omni.setWBmode(1);
        break;
      case 0x42:
        omni.setWBmode(2);
        break;        
      case 0x43:
        omni.setWBmode(3);
        break;        
      case 0x44:
        omni.setWBmode(4);
        break;
      case 0x87:  //Normal
        omni.setSpecialEffect(NOEFFECT);
        break;
      case 0x85:
        omni.setSpecialEffect(NEGATIVE);
        break;
      case 0x84:
        omni.setSpecialEffect(BW);
        break;
      case 0x83:
        omni.setSpecialEffect(REDDISH);
        break;
      case 0x82:
        omni.setSpecialEffect(GREENISH);
        break;
      case 0x81:
        omni.setSpecialEffect(BLUEISH);
        break;
      case 0x80:
        omni.setSpecialEffect(RETRO);
        break;
      case 0x50:
        camera.setSaturation(2);
        break;    
      case 0x51:
        camera.setSaturation(1);
        break;   
      case 0x52:
        camera.setSaturation(0);
        break;   
      case 0x53:
        camera.setSaturation(-1);
        break;   
      case 0x54:
        camera.setSaturation(-2);
        break;   
      case 0x60:
        camera.setBrightness(2);
        break;    
      case 0x61:
        camera.setBrightness(1);
        break;   
      case 0x62:
        camera.setBrightness(0);
        break;   
      case 0x63:
        camera.setBrightness(-1);
        break;   
      case 0x64:
        camera.setBrightness(-2);
        break;
      case 0x70:
        camera.setContrast(2);
        break;    
      case 0x71:
        camera.setContrast(1);
        break;   
      case 0x72:
        camera.setContrast(0);
        break;   
      case 0x73:
        camera.setContrast(-1);
        break;   
      case 0x74:
        camera.setContrast(-2);
        break;      
      default:
        break;      
    }
  }
#endif
  if (Serial.available()) {
    uint8_t command = Serial.read();
    ch = Serial.read();
    #if defined(USE_SDCARD)
      if ('2'==command) storage_index = CommandLineReadNextNumber(ch, 0);
    #endif
    switch (command) {
      case 'p':
        {
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
          send_raw();
          Serial.println("Image Sent!");
          ch = ' ';
#else
          Serial.println("*** Only works in USB Dual or Triple Serial Mode ***");
#endif
          break;
        }
      case 'z':
        {
#if defined(USE_SDCARD)
          save_image_SD();
#endif
          break;
        }
      case 'j':
        {
#if (defined(USE_SDCARD) && defined(ARDUCAM_CAMERA_OV2640))
          bool error = false;
          error = save_jpg_SD();
          if(!error) Serial.println("ERROR reading JPEG.  Try again....");
#endif
          break;
        }
      case 'b':
        {
#if defined(USE_SDCARD)
          memset((uint8_t *)frameBuffer, 0, sizeof(frameBuffer));
          camera.setMode(HIMAX_MODE_STREAMING_NFRAMES, 1);
          camera.readFrame(frameBuffer, sizeof(frameBuffer));
          int numPixels = camera.width() * camera.height();
          for (int i = 0; i < numPixels; i++) frameBuffer[i] = HTONS(frameBuffer[i]);

          save_image_SD();
          ch = ' ';
#endif
          break;
        }
      case 'm':
        read_display_multiple_frames(false);
        break;

      case 'M':
        read_display_multiple_frames(true);
        break;
      case 't':
        test_display();
        break;
      case 'd':
        camera.debug(!camera.debug());
        if (camera.debug()) Serial.println("Camera Debug turned on");
        else Serial.println("Camera debug turned off");
        break;

      case 'f':
        {
          memset((uint8_t *)frameBuffer, 0, sizeof(frameBuffer));
          camera.setMode(HIMAX_MODE_STREAMING_NFRAMES, 1);
          tft.useFrameBuffer(false);
          tft.fillScreen(TFT_BLACK);
          Serial.println("Reading frame");
          Serial.printf("Buffer: %p halfway: %p end:%p\n", frameBuffer, &frameBuffer[camera.width() * camera.height() / 2], &frameBuffer[camera.width() * camera.height()]);
          memset((uint8_t *)frameBuffer, 0, sizeof(frameBuffer));
          for(uint8_t i = 0; i < skipFrames; i++) {
              camera.readFrame(frameBuffer, sizeof(frameBuffer));
              delay(100);
          }

          Serial.println("Finished reading frame");
          Serial.flush();

          dump_partial_FB();

          //int numPixels = camera.width() * camera.height();
          Serial.printf("TFT(%u, %u) Camera(%u, %u)\n", tft.width(), tft.height(), camera.width(), camera.height());
          //int camera_width = Camera.width();

          tft.setOrigin(-2, -2);
          int numPixels = camera.width() * camera.height();
          Serial.printf("TFT(%u, %u) Camera(%u, %u)\n", tft.width(), tft.height(), camera.width(), camera.height());


//int camera_width = Camera.width();
#if 1
          //byte swap
          //for (int i = 0; i < numPixels; i++) frameBuffer[i] = (frameBuffer[i] >> 8) | (((frameBuffer[i] & 0xff) << 8));
          for (int i = 0; i < numPixels; i++) frameBuffer[i] = HTONS(frameBuffer[i]);

          if ((camera.width() <= tft.width()) && (camera.height() <= tft.height())) {
            if ((camera.width() != tft.width()) || (camera.height() != tft.height())) tft.fillScreen(TFT_BLACK);
            tft.writeRect(CENTER, CENTER, camera.width(), camera.height(), frameBuffer);
          } else {
            Serial.println("sub image");
            tft.writeSubImageRect(0, 0, tft.width(), tft.height(), (camera.width() - tft.width()) / 2, (camera.height() - tft.height()),
                                  camera.width(), camera.height(), frameBuffer);
          }
#else
          Serial.println("sub image1");
          tft.writeSubImageRect(0, 0, tft.width(), tft.height(), 0, 0, camera.width(), camera.height(), pixels);
#endif
          tft.setOrigin(0, 0);
          ch = ' ';
          g_continuous_flex_mode = false;
          break;
        }
      case 'F':
        {
          if (!g_continuous_flex_mode) {
            if (camera.readContinuous(&hm0360_flexio_callback, frameBuffer, sizeof(frameBuffer), frameBuffer2, sizeof(frameBuffer2))) {
              Serial.println("* continuous mode started");
              g_flexio_capture_count = 0;
              g_flexio_redraw_count = 0;
              g_continuous_flex_mode = true;
            } else {
              Serial.println("* error, could not start continuous mode");
            }
          } else {
            camera.stopReadContinuous();
            g_continuous_flex_mode = false;
            Serial.println("* continuous mode stopped");
          }
          break;
        }
      case 'V':
        {
          if (!g_continuous_flex_mode) {
            if (camera.readContinuous(&camera_flexio_callback_video, frameBuffer, sizeof(frameBuffer), frameBuffer2, sizeof(frameBuffer2))) {

              Serial.println("Before Set frame complete CB");
              if (!tft.useFrameBuffer(true)) Serial.println("Failed call to useFrameBuffer");
              tft.setFrameCompleteCB(&frame_complete_cb, false);
              Serial.println("Before UPdateScreen Async");
              tft.updateScreenAsync(true);
              Serial.println("* continuous mode (Video) started");
              g_flexio_capture_count = 0;
              g_flexio_redraw_count = 0;
              g_continuous_flex_mode = 2;
            } else {
              Serial.println("* error, could not start continuous mode");
            }
          } else {
            camera.stopReadContinuous();
            tft.endUpdateAsync();
            tft.useFrameBuffer(false);
            g_continuous_flex_mode = 0;
            Serial.println("* continuous mode stopped");
          }
          ch = ' ';
          break;
        }
      case 'c':
        {
          tft.fillScreen(TFT_BLACK);
          break;
        }
      case 0x30:
        {
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
          SerialUSB1.println(F("ACK CMD CAM start single shoot. END"));
          send_image(&SerialUSB1);
          SerialUSB1.println(F("READY. END"));
#else
          Serial.println("*** Only works in USB Dual or Triple Serial Mode ***");
#endif
          break;
        }
    #if defined(USE_SDCARD)
    uint32_t fsCount;
    case '1':
      // first dump list of storages:
      fsCount = MTP.getFilesystemCount();
      Serial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        Serial.printf("store:%u storage:%x name:%s fs:%x pn:", ii,
                         MTP.Store2Storage(ii), MTP.getFilesystemNameByIndex(ii),
                         (uint32_t)MTP.getFilesystemByIndex(ii));
    /*    char dest[12];     // Destination string
        char key[] = "MSC";
        strncpy(dest, MTP.getFilesystemNameByIndex(ii), 3);
        if(strcmp(key, dest) == 0) {
          DBGSerial.println(getFSPN(ii));
          DBGSerial.println("USB Storage");
        } else {
          DBGSerial.println(getFSPN(ii));
        }
     */
          //Serial.println(getFSPN(ii));
      } 
      //DBGSerial.println("\nDump Index List");
      //MTP.storage()->dumpIndexList();
      break;
    case '2':
      if (storage_index < MTP.getFilesystemCount()) {
        Serial.printf("Storage Index %u Name: %s Selected\n", storage_index,
        MTP.getFilesystemNameByIndex(storage_index));
        myfs = MTP.getFilesystemByIndex(storage_index);
        current_store = storage_index;
      } else {
        Serial.printf("Storage Index %u out of range\n", storage_index);
      }
      break;
    case 'l': listFiles(); break;
    case 'e': eraseFiles(); break;
    #endif

    case '?':
      {
        showCommandList();
        ch = ' ';
        break;
      }
    default:
      break;
    }
    while (Serial.read() != -1)
      ;  // lets strip the rest out
  }


  if (g_continuous_flex_mode) {
    if (g_new_flexio_data) {
      //Serial.println("new FlexIO data");
#ifndef CAMERA_USES_MONO_PALETTE
      uint16_t *pframe = (uint16_t*)g_new_flexio_data;
      for (int i = 0; i < (FRAME_WIDTH * FRAME_HEIGHT); i++) pframe[i] = HTONS(pframe[i]);
      tft.writeRect(CENTER, CENTER, camera.width(), camera.height(), pframe);

#else
      tft.setOrigin(-2, -2);
      tft.writeRect8BPP(0, 0, FRAME_WIDTH, FRAME_HEIGHT, (uint8_t *)g_new_flexio_data, mono_palette);
      tft.setOrigin(0, 0);
#endif
      tft.updateScreenAsync();
      g_new_flexio_data = nullptr;
      g_flexio_redraw_count++;
      if (g_flexio_runtime > 10000) {
        // print some stats on actual speed, but not too much
        // printing too quickly to be considered "spew"
        float redraw_rate = (float)g_flexio_redraw_count / (float)g_flexio_runtime * 1000.0f;
        g_flexio_runtime = 0;
        g_flexio_redraw_count = 0;
        Serial.printf("redraw rate = %.2f Hz\n", redraw_rate);
      }
    }
  }
  #if defined(USE_SDCARD)  
  MTP.loop(); 
  #endif
}

// Pass 8-bit (each) R,G,B, get back 16-bit packed color
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
void send_image(Stream *imgSerial) {
  memset((uint8_t *)frameBuffer, 0, sizeof(frameBuffer));
  camera.setHmirror(1);
  camera.readFrame(frameBuffer, sizeof(frameBuffer));

  imgSerial->write(0xFF);
  imgSerial->write(0xAA);

  // BUGBUG:: maybe combine with the save to SD card code
  unsigned char bmpFileHeader[14] = { 'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0 };
  unsigned char bmpInfoHeader[40] = { 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0 };

  int rowSize = 4 * ((3 * FRAME_WIDTH + 3) / 4);  // how many bytes in the row (used to create padding)
  int fileSize = 54 + FRAME_HEIGHT * rowSize;      // headers (54 bytes) + pixel data

  bmpFileHeader[2] = (unsigned char)(fileSize);
  bmpFileHeader[3] = (unsigned char)(fileSize >> 8);
  bmpFileHeader[4] = (unsigned char)(fileSize >> 16);
  bmpFileHeader[5] = (unsigned char)(fileSize >> 24);

  bmpInfoHeader[4] = (unsigned char)(FRAME_WIDTH);
  bmpInfoHeader[5] = (unsigned char)(FRAME_WIDTH >> 8);
  bmpInfoHeader[6] = (unsigned char)(FRAME_WIDTH >> 16);
  bmpInfoHeader[7] = (unsigned char)(FRAME_WIDTH >> 24);
  bmpInfoHeader[8] = (unsigned char)(FRAME_HEIGHT);
  bmpInfoHeader[9] = (unsigned char)(FRAME_HEIGHT >> 8);
  bmpInfoHeader[10] = (unsigned char)(FRAME_HEIGHT >> 16);
  bmpInfoHeader[11] = (unsigned char)(FRAME_HEIGHT >> 24);


  imgSerial->write(bmpFileHeader, sizeof(bmpFileHeader));  // write file header
  imgSerial->write(bmpInfoHeader, sizeof(bmpInfoHeader));  // " info header

  unsigned char bmpPad[rowSize - 3 * FRAME_WIDTH];
  for (int i = 0; i < (int)(sizeof(bmpPad)); i++) {  // fill with 0s
    bmpPad[i] = 0;
  }
  
  uint32_t idx = 0;

  uint16_t *pfb = frameBuffer;
  uint8_t img[3];
  uint32_t count_y_first_buffer = sizeof(frameBuffer) / (FRAME_WIDTH * 2);
  for (int y = FRAME_HEIGHT - 1; y >= 0; y--) {  // iterate image array
    if (y < (int)count_y_first_buffer) pfb = &frameBuffer[y * FRAME_WIDTH];
    for (int x = 0; x < FRAME_WIDTH; x++) {
      //r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3
      uint16_t pixel = HTONS(*pfb++);
      img[2] = (pixel >> 8) & 0xf8;  // r
      img[1] = (pixel >> 3) & 0xfc;  // g
      img[0] = (pixel << 3);         // b
      imgSerial->write(img, 3);
      delayMicroseconds(8);
      }
      imgSerial->write(bmpPad, (4 - (FRAME_WIDTH * 3) % 4) % 4);  // and padding as needed
     }

  imgSerial->write(0xBB);
  imgSerial->write(0xCC);

  imgSerial->println(F("ACK CMD CAM Capture Done. END"));
  camera.setHmirror(0);

  delay(50);
}

void send_raw() {
  memset((uint8_t *)frameBuffer, 0, sizeof(frameBuffer));
  camera.readFrame(frameBuffer, sizeof(frameBuffer));

  for (int i = 0; i < FRAME_HEIGHT * FRAME_WIDTH; i++) {
    SerialUSB1.write((frameBuffer[i] >> 8) & 0xFF);
    SerialUSB1.write((frameBuffer[i]) & 0xFF);
  }
}

#endif


#if defined(USE_SDCARD)
char name[] = "9px_0000.bmp";  // filename convention (will auto-increment)
  // can probably reuse framebuffer2...

//DMAMEM unsigned char img[3 * 320*240];
void save_image_SD() {
  //uint8_t r, g, b;
  //uint32_t x, y;

  Serial.print("Writing BMP to SD CARD File: ");

  // if name exists, create new filename, SD.exists(filename)
  for (int i = 0; i < 10000; i++) {
    name[4] = (i / 1000) % 10 + '0';  // thousands place
    name[5] = (i / 100) % 10 + '0';   // hundreds
    name[6] = (i / 10) % 10 + '0';    // tens
    name[7] = i % 10 + '0';           // ones
    if (!myfs->exists(name)) {
      Serial.println(name);
      file = myfs->open(name, FILE_WRITE);
      break;
    }
  }

  uint16_t w = FRAME_WIDTH;
  uint16_t h = FRAME_HEIGHT;

  //unsigned char *img = NULL;
  // set fileSize (used in bmp header)
  int rowSize = 4 * ((3 * w + 3) / 4);  // how many bytes in the row (used to create padding)
  int fileSize = 54 + h * rowSize;      // headers (54 bytes) + pixel data

  //  img = (unsigned char *)malloc(3 * w * h);

  // create padding (based on the number of pixels in a row
  unsigned char bmpPad[rowSize - 3 * w];
  for (int i = 0; i < (int)(sizeof(bmpPad)); i++) {  // fill with 0s
    bmpPad[i] = 0;
  }

  unsigned char bmpFileHeader[14] = { 'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0 };
  unsigned char bmpInfoHeader[40] = { 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0 };

  bmpFileHeader[2] = (unsigned char)(fileSize);
  bmpFileHeader[3] = (unsigned char)(fileSize >> 8);
  bmpFileHeader[4] = (unsigned char)(fileSize >> 16);
  bmpFileHeader[5] = (unsigned char)(fileSize >> 24);

  bmpInfoHeader[4] = (unsigned char)(w);
  bmpInfoHeader[5] = (unsigned char)(w >> 8);
  bmpInfoHeader[6] = (unsigned char)(w >> 16);
  bmpInfoHeader[7] = (unsigned char)(w >> 24);
  bmpInfoHeader[8] = (unsigned char)(h);
  bmpInfoHeader[9] = (unsigned char)(h >> 8);
  bmpInfoHeader[10] = (unsigned char)(h >> 16);
  bmpInfoHeader[11] = (unsigned char)(h >> 24);

  // write the file (thanks forum!)
  file.write(bmpFileHeader, sizeof(bmpFileHeader));  // write file header
  file.write(bmpInfoHeader, sizeof(bmpInfoHeader));  // " info header

// try to compute and output one row at a time.
  uint16_t *pfb = frameBuffer;
  uint8_t img[3];
  for (int y = h - 1; y >= 0; y--) {  // iterate image array
    pfb = &frameBuffer[y * w];
    for (int x = 0; x < w; x++) {
      //r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3
      img[2] = (*pfb >> 8) & 0xf8;  // r
      img[1] = (*pfb >> 3) & 0xfc;  // g
      img[0] = (*pfb << 3);         // b
      file.write(img, 3);
      pfb++;
    }
    file.write(bmpPad, (4 - (FRAME_WIDTH * 3) % 4) % 4);  // and padding as needed
  }
  file.close();  // close file when done writing
  Serial.println("Done Writing BMP");
}

char name_jpg[] = "9px_0000.jpg";  // filename convention (will auto-increment)
  // can probably reuse framebuffer2...

bool save_jpg_SD() {

  camera.setPixformat(JPEG);
  delay(50);
  //omni.setQuality(11);
  camera.useDMA(false);

  memset((uint8_t *)frameBuffer, 0, sizeof(frameBuffer));
  if(camera.usingGPIO()) {
    omni.readFrameGPIO_JPEG(frameBuffer, sizeof(frameBuffer));
    delay(100);
    omni.readFrameGPIO_JPEG(frameBuffer, sizeof(frameBuffer));
  } else {
    camera.readFrame(frameBuffer, sizeof(frameBuffer));
    delay(100);
    camera.readFrame(frameBuffer, sizeof(frameBuffer));
  }

  uint16_t w = FRAME_WIDTH;
  uint16_t h = FRAME_HEIGHT;
  uint16_t eop = 0;
  uint8_t eoi = 0;

  uint16_t *pfb = frameBuffer;
  Serial.printf("jpeg size: %d\n", (w*h/5));

  for(uint32_t i = 0; i < (w*h/5); i++) {
    //Serial.println(pfb[i], HEX);
    if((i == 0) && (pfb[0] == 0xD8FF)) {
      eoi = 1;
      Serial.printf("Found begining of frame at position %d\n", i);
    }

    if(pfb[i] == 0xD9FF){
      eop = i;
      Serial.printf("Found ending of frame at position %d\n", i);
    } 
  }

  if(eop == 0 || eoi == 0) return false;

  Serial.print("Writing jpg to SD CARD File: ");

  // if name exists, create new filename, SD.exists(filename)
  for (int i = 0; i < 10000; i++) {
    name_jpg[4] = (i / 1000) % 10 + '0';  // thousands place
    name_jpg[5] = (i / 100) % 10 + '0';   // hundreds
    name_jpg[6] = (i / 10) % 10 + '0';    // tens
    name_jpg[7] = i % 10 + '0';           // ones
    if (!myfs->exists(name_jpg)) {
      Serial.println(name_jpg);
      file = myfs->open(name_jpg, FILE_WRITE);
      break;
    }
  }


  Serial.println(" Writing to SD");
  for(uint32_t i = 0; i < (w*h/5); i++){
    //file.write(pfb[i]);
    file.write(pfb[i] & 0xFF);
    file.write((pfb[i] >> 8) & 0xFF);
  }


  file.close();  // close file when done writing
  Serial.println("Done Writing JPG");

  Serial.printf("%x, %x\n", pfb[0], pfb[eop]);
  camera.setPixformat(camera_format);
  camera.useDMA(true);
  delay(50);
  return true;

}
#endif

void showCommandList() {
  if (camera.usingGPIO()) {
    Serial.println("Send the 'f' character to read a frame using GPIO");
    Serial.println("Send the 'F' to start/stop continuous using GPIO");
  } else {
    Serial.println("Send the 'f' character to read a frame using FlexIO (changes hardware setup!)");
    Serial.println("Send the 'F' to start/stop continuous using FlexIO (changes hardware setup!)");
  }
  Serial.println("Send the 'm' character to read and display multiple frames");
  Serial.println("Send the 'M' character to read and display multiple frames use Frame buffer");
  Serial.println("Send the 'V' character DMA to TFT async continueous  ...");
  Serial.println("Send the 'p' character to snapshot to PC on USB1");
  Serial.println("Send the 'b' character to save snapshot (BMP) to SD Card");
  Serial.println("Send the 'j' character to save snapshot (JPEG) to SD Card");
  Serial.println("Send the 'c' character to blank the display");
  Serial.println("Send the 'z' character to send current screen BMP to SD");
  Serial.println("Send the 't' character to send Check the display");
  Serial.println("Send the 'd' character to toggle camera debug on and off");
  
  #if defined(USE_SDCARD)
  Serial.println("MTP FUNCTIONS:");
  Serial.println("\t1 - List Drives (Step 1)");
  Serial.println("\t2# - Select Drive # for Logging (Step 2)");
  Serial.println("\tl - List files on disk");
  Serial.println("\te - Erase files on disk with Format");
  Serial.println();
  #endif
}

//=============================================================================
void read_display_multiple_frames(bool use_frame_buffer) {
  if (use_frame_buffer) {
    Serial.println("\n*** Read and display multiple frames (using async screen updates), press any key to stop ***");
    tft.useFrameBuffer(true);
  } else {
    Serial.println("\n*** Read and display multiple frames, press any key to stop ***");
  }

  while (Serial.read() != -1) {}

  elapsedMicros em = 0;
  int frame_count = 0;

  for (;;) {

    for(uint8_t i = 0; i < skipFrames; i++) camera.readFrame(frameBuffer, sizeof(frameBuffer));

    int numPixels = camera.width() * camera.height();

//byte swap
//for (int i = 0; i < numPixels; i++) frameBuffer[i] = (frameBuffer[i] >> 8) | (((frameBuffer[i] & 0xff) << 8));
    for (int i = 0; i < numPixels; i++) frameBuffer[i] = HTONS(frameBuffer[i]);

    if (use_frame_buffer) tft.waitUpdateAsyncComplete();

    if ((camera.width() <= tft.width()) && (camera.height() <= tft.height())) {
      if ((camera.width() != tft.width()) || (camera.height() != tft.height())) tft.fillScreen(TFT_BLACK);
      tft.writeRect(CENTER, CENTER, camera.width(), camera.height(), frameBuffer);
    } else {
      tft.writeSubImageRect(0, 0, tft.width(), tft.height(), (camera.width() - tft.width()) / 2, (camera.height() - tft.height()),
                            camera.width(), camera.height(), frameBuffer);
    }

    if (use_frame_buffer) tft.updateScreenAsync();

    frame_count++;
    if ((frame_count & 0x7) == 0) {
      Serial.printf("Elapsed: %u frames: %d fps: %.2f\n", (uint32_t)em, frame_count, (float)(1000000.0 / em) * (float)frame_count);
    }
    if (Serial.available()) break;
  }
  // turn off frame buffer
  tft.useFrameBuffer(false);
}
/******************************************************************/

/***********************************************************/

void test_display() {
  tft.setRotation(3);
  tft.fillScreen(TFT_RED);
  delay(500);
  tft.fillScreen(TFT_GREEN);
  delay(500);
  tft.fillScreen(TFT_BLUE);
  delay(500);
  tft.fillScreen(TFT_BLACK);
  delay(500);
}

void dump_partial_FB() {
  for (volatile uint16_t *pfb = frameBuffer; pfb < (frameBuffer + 4 * camera.width()); pfb += camera.width()) {
    Serial.printf("\n%08x: ", (uint32_t)pfb);
    for (uint16_t i = 0; i < 8; i++) Serial.printf("%02x ", pfb[i]);
    Serial.print("..");
    Serial.print("..");
    for (uint16_t i = camera.width() - 8; i < camera.width(); i++) Serial.printf("%04x ", pfb[i]);
  }
  Serial.println("\n");

  // Lets dump out some of center of image.
  Serial.println("Show Center pixels\n");
  for (volatile uint16_t *pfb = frameBuffer + camera.width() * ((camera.height() / 2) - 8); pfb < (frameBuffer + camera.width() * (camera.height() / 2 + 8)); pfb += camera.width()) {
    Serial.printf("\n%08x: ", (uint32_t)pfb);
    for (uint16_t i = 0; i < 8; i++) Serial.printf("%02x ", pfb[i]);
    Serial.print("..");
    for (uint16_t i = (camera.width() / 2) - 4; i < (camera.width() / 2) + 4; i++) Serial.printf("%02x ", pfb[i]);
    Serial.print("..");
    for (uint16_t i = camera.width() - 8; i < camera.width(); i++) Serial.printf("%02x ", pfb[i]);
  }
  Serial.println("\n...");
}