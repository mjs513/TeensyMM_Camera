/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2013-2021 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2021 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * OV2640 driver.
 */


#include "OV2640.h"

#define debug     Serial

#define DEBUG_CAMERA
//#define DEBUG_CAMERA_VERBOSE
//#define DEBUG_FLEXIO
//#define USE_DEBUG_PINS
#define USE_VSYNC_PIN_INT

// if not defined in the variant
#ifndef digitalPinToBitMask
#define digitalPinToBitMask(P) (1 << (digitalPinToPinName(P) % 64))
#endif

#ifndef portInputRegister
#define portInputRegister(P) ((P == 0) ? &NRF_P0->IN : &NRF_P1->IN)
#endif

#define CNT_SHIFTERS 1

#define CIF_WIDTH      (400)
#define CIF_HEIGHT     (296)

#define SVGA_WIDTH     (800)
#define SVGA_HEIGHT    (600)

#define UXGA_WIDTH     (1600)
#define UXGA_HEIGHT    (1200)

/** ln(10) */
#ifndef LN10
#define LN10        2.30258509299404568402f
#endif /* !M_LN10 */

 /* log_e 2 */
#ifndef LN2
#define LN2 0.69314718055994530942
#endif /*!M_LN2 */


#define LOG2_2(x)             (((x) & 0x2ULL) ? (2) :             1)                                // NO ({ ... }) !
#define LOG2_4(x)             (((x) & 0xCULL) ? (2 + LOG2_2((x) >> 2)) :  LOG2_2(x))          // NO ({ ... }) !
#define LOG2_8(x)             (((x) & 0xF0ULL) ? (4 + LOG2_4((x) >> 4)) :  LOG2_4(x))         // NO ({ ... }) !
#define LOG2_16(x)         (((x) & 0xFF00ULL) ? (8 + LOG2_8((x) >> 8)) :  LOG2_8(x))       // NO ({ ... }) !
#define LOG2_32(x)            (((x) & 0xFFFF0000ULL) ? (16 + LOG2_16((x) >> 16)) : LOG2_16(x)) // NO ({ ... }) !
#define LOG2(x)               (((x) & 0xFFFFFFFF00000000ULL) ? (32 + LOG2_32((x) >> 32)) : LOG2_32(x)) // NO ({ ... }) !


// Sensor frame size/resolution table.
const int resolution[][2] = {
    {640,  480 },    /* VGA       */
    {160,  120 },    /* QQVGA     */
    {320,  240 },    /* QVGA      */
    {480,  320 },    /* ILI9488   */
    {320,  320 },    /* 320x320   */
    {320,  240 },    /* QVGA      */
    {176,  144 },    /* QCIF      */
    {352,  288 },    /* CIF       */
    {0,    0   },
};


static const uint8_t default_regs[][2] = {

// From Linux Driver.

    {BANK_SEL,      BANK_SEL_DSP},
    {0x2c,          0xff},
    {0x2e,          0xdf},
    {BANK_SEL,      BANK_SEL_SENSOR},
    {0x3c,          0x32},
    {CLKRC,         CLKRC_DOUBLE},
    {COM2,          COM2_OUT_DRIVE_3x},
    {REG04,         REG04_SET(REG04_HFLIP_IMG | REG04_VFLIP_IMG | REG04_VREF_EN | REG04_HREF_EN)},
    {COM8,          COM8_SET(COM8_BNDF_EN | COM8_AGC_EN | COM8_AEC_EN)},
    {COM9,          COM9_AGC_SET(COM9_AGC_GAIN_8x)},
    {0x2c,          0x0c},
    {0x33,          0x78},
    {0x3a,          0x33},
    {0x3b,          0xfb},
    {0x3e,          0x00},
    {0x43,          0x11},
    {0x16,          0x10},
    {0x39,          0x02},
    {0x35,          0x88},
    {0x22,          0x0a},
    {0x37,          0x40},
    {0x23,          0x00},
    {ARCOM2,        0xa0},
    {0x06,          0x02},
    {0x06,          0x88},
    {0x07,          0xc0},
    {0x0d,          0xb7},
    {0x0e,          0x01},
    {0x4c,          0x00},
    {0x4a,          0x81},
    {0x21,          0x99},
    {AEW,           0x40},
    {AEB,           0x38},
    {VV,            VV_AGC_TH_SET(0x08, 0x02)},
    {0x5c,          0x00},
    {0x63,          0x00},
    {FLL,           0x22},
    {COM3,          COM3_BAND_SET(COM3_BAND_AUTO)},
    {REG5D,         0x55},
    {REG5E,         0x7d},
    {REG5F,         0x7d},
    {REG60,         0x55},
    {HISTO_LOW,     0x70},
    {HISTO_HIGH,    0x80},
    {0x7c,          0x05},
    {0x20,          0x80},
    {0x28,          0x30},
    {0x6c,          0x00},
    {0x6d,          0x80},
    {0x6e,          0x00},
    {0x70,          0x02},
    {0x71,          0x94},
    {0x73,          0xc1},
    {0x3d,          0x34},
    {COM7,          COM7_RES_UXGA | COM7_ZOOM_EN},
    {0x5a,          0x57},
    {COM25,         0x00},
    {BD50,          0xbb},
    {BD60,          0x9c},
    {BANK_SEL,      BANK_SEL_DSP},
    {0xe5,          0x7f},
    {MC_BIST,       MC_BIST_RESET | MC_BIST_BOOT_ROM_SEL},
    {0x41,          0x24},
    {RESET,         RESET_JPEG | RESET_DVP},
    {0x76,          0xff},
    {0x33,          0xa0},
    {0x42,          0x20},
    {0x43,          0x18},
    {0x4c,          0x00},
    {CTRL3,         CTRL3_BPC_EN | CTRL3_WPC_EN | 0x10},
    {0x88,          0x3f},
    {0xd7,          0x03},
    {0xd9,          0x10},
    {R_DVP_SP,      R_DVP_SP_AUTO_MODE | 0x2},
    {0xc8,          0x08},
    {0xc9,          0x80},
    {BPADDR,        0x00},
    {BPDATA,        0x00},
    {BPADDR,        0x03},
    {BPDATA,        0x48},
    {BPDATA,        0x48},
    {BPADDR,        0x08},
    {BPDATA,        0x20},
    {BPDATA,        0x10},
    {BPDATA,        0x0e},
    {0x90,          0x00},
    {0x91,          0x0e},
    {0x91,          0x1a},
    {0x91,          0x31},
    {0x91,          0x5a},
    {0x91,          0x69},
    {0x91,          0x75},
    {0x91,          0x7e},
    {0x91,          0x88},
    {0x91,          0x8f},
    {0x91,          0x96},
    {0x91,          0xa3},
    {0x91,          0xaf},
    {0x91,          0xc4},
    {0x91,          0xd7},
    {0x91,          0xe8},
    {0x91,          0x20},
    {0x92,          0x00},
    {0x93,          0x06},
    {0x93,          0xe3},
    {0x93,          0x03},
    {0x93,          0x03},
    {0x93,          0x00},
    {0x93,          0x02},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x96,          0x00},
    {0x97,          0x08},
    {0x97,          0x19},
    {0x97,          0x02},
    {0x97,          0x0c},
    {0x97,          0x24},
    {0x97,          0x30},
    {0x97,          0x28},
    {0x97,          0x26},
    {0x97,          0x02},
    {0x97,          0x98},
    {0x97,          0x80},
    {0x97,          0x00},
    {0x97,          0x00},
    {0xa4,          0x00},
    {0xa8,          0x00},
    {0xc5,          0x11},
    {0xc6,          0x51},
    {0xbf,          0x80},
    {0xc7,          0x10},  /* simple AWB */
    {0xb6,          0x66},
    {0xb8,          0xA5},
    {0xb7,          0x64},
    {0xb9,          0x7C},
    {0xb3,          0xaf},
    {0xb4,          0x97},
    {0xb5,          0xFF},
    {0xb0,          0xC5},
    {0xb1,          0x94},
    {0xb2,          0x0f},
    {0xc4,          0x5c},
    {0xa6,          0x00},
    {0xa7,          0x20},
    {0xa7,          0xd8},
    {0xa7,          0x1b},
    {0xa7,          0x31},
    {0xa7,          0x00},
    {0xa7,          0x18},
    {0xa7,          0x20},
    {0xa7,          0xd8},
    {0xa7,          0x19},
    {0xa7,          0x31},
    {0xa7,          0x00},
    {0xa7,          0x18},
    {0xa7,          0x20},
    {0xa7,          0xd8},
    {0xa7,          0x19},
    {0xa7,          0x31},
    {0xa7,          0x00},
    {0xa7,          0x18},
    {0x7f,          0x00},
    {0xe5,          0x1f},
    {0xe1,          0x77},
    {0xdd,          0x7f},
    {CTRL0,         CTRL0_YUV422 | CTRL0_YUV_EN | CTRL0_RGB_EN},

// OpenMV Custom.

    {BANK_SEL,      BANK_SEL_SENSOR},
    {0x0f,          0x4b},
    {COM1,          0x8f},

// End.

    {0x00,          0x00},
};

// Looks really bad.
//static const uint8_t cif_regs[][2] = {
//    {BANK_SEL,  BANK_SEL_SENSOR},
//    {COM7,      COM7_RES_CIF},
//    {COM1,      0x06 | 0x80},
//    {HSTART,    0x11},
//    {HSTOP,     0x43},
//    {VSTART,    0x01}, // 0x01 fixes issue with garbage pixels in the image...
//    {VSTOP,     0x97},
//    {REG32,     0x09},
//    {BANK_SEL,  BANK_SEL_DSP},
//    {RESET,     RESET_DVP},
//    {SIZEL,     SIZEL_HSIZE8_11_SET(CIF_WIDTH) | SIZEL_HSIZE8_SET(CIF_WIDTH) | SIZEL_VSIZE8_SET(CIF_HEIGHT)},
//    {HSIZE8,    HSIZE8_SET(CIF_WIDTH)},
//    {VSIZE8,    VSIZE8_SET(CIF_HEIGHT)},
//    {CTRL2,     CTRL2_DCW_EN | CTRL2_SDE_EN | CTRL2_UV_AVG_EN | CTRL2_CMX_EN | CTRL2_UV_ADJ_EN},
//    {0,         0},
//};

static const uint8_t svga_regs[][2] = {
    {BANK_SEL,  BANK_SEL_SENSOR},
    {COM7,      COM7_RES_SVGA},
    {COM1,      0x0A | 0x80},
    {HSTART,    0x11},
    {HSTOP,     0x43},
    {VSTART,    0x01}, // 0x01 fixes issue with garbage pixels in the image...
    {VSTOP,     0x97},
    {REG32,     0x09},
    {BANK_SEL,  BANK_SEL_DSP},
    {RESET,     RESET_DVP},
    {SIZEL,     SIZEL_HSIZE8_11_SET(SVGA_WIDTH) | SIZEL_HSIZE8_SET(SVGA_WIDTH) | SIZEL_VSIZE8_SET(SVGA_HEIGHT)},
    {HSIZE8,    HSIZE8_SET(SVGA_WIDTH)},
    {VSIZE8,    VSIZE8_SET(SVGA_HEIGHT)},
    {CTRL2,     CTRL2_DCW_EN | CTRL2_SDE_EN | CTRL2_UV_AVG_EN | CTRL2_CMX_EN | CTRL2_UV_ADJ_EN},
    {0,         0},
};

static const uint8_t uxga_regs[][2] = {
    {BANK_SEL,  BANK_SEL_SENSOR},
    {COM7,      COM7_RES_UXGA},
    {COM1,      0x0F | 0x80},
    {HSTART,    0x11},
    {HSTOP,     0x75},
    {VSTART,    0x01},
    {VSTOP,     0x97},
    {REG32,     0x36},
    {BANK_SEL,  BANK_SEL_DSP},
    {RESET,     RESET_DVP},
    {SIZEL,     SIZEL_HSIZE8_11_SET(UXGA_WIDTH) | SIZEL_HSIZE8_SET(UXGA_WIDTH) | SIZEL_VSIZE8_SET(UXGA_HEIGHT)},
    {HSIZE8,    HSIZE8_SET(UXGA_WIDTH)},
    {VSIZE8,    VSIZE8_SET(UXGA_HEIGHT)},
    {CTRL2,     CTRL2_DCW_EN | CTRL2_SDE_EN | CTRL2_UV_AVG_EN | CTRL2_CMX_EN | CTRL2_UV_ADJ_EN},
    {0,         0},
};

static const uint8_t yuv422_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {IMAGE_MODE,    IMAGE_MODE_YUV422},
    {0xd7,          0x03},
    {0x33,          0xa0},
    {0xe5,          0x1f},
    {0xe1,          0x67},
    {RESET,         0x00},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {0,             0},
};

static const uint8_t rgb565_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {IMAGE_MODE,    IMAGE_MODE_RGB565},
    {0xd7,          0x03},
    {RESET,         0x00},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {0,             0},
};

static const uint8_t bayer_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {IMAGE_MODE,    IMAGE_MODE_RAW10},
    {0xd7,          0x03},
    {RESET,         0x00},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {0,             0},
};

static const uint8_t jpeg_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {IMAGE_MODE,    IMAGE_MODE_JPEG_EN},
    {0xd7,          0x03},
    {RESET,         0x00},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {0,             0},
};

#define NUM_BRIGHTNESS_LEVELS    (5)
static const uint8_t brightness_regs[NUM_BRIGHTNESS_LEVELS + 1][5] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA},
    {0x00, 0x04, 0x09, 0x00, 0x00}, /* -2 */
    {0x00, 0x04, 0x09, 0x10, 0x00}, /* -1 */
    {0x00, 0x04, 0x09, 0x20, 0x00}, /*  0 */
    {0x00, 0x04, 0x09, 0x30, 0x00}, /* +1 */
    {0x00, 0x04, 0x09, 0x40, 0x00}, /* +2 */
};

#define NUM_CONTRAST_LEVELS    (5)
static const uint8_t contrast_regs[NUM_CONTRAST_LEVELS + 1][7] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA, BPDATA, BPDATA},
    {0x00, 0x04, 0x07, 0x20, 0x18, 0x34, 0x06}, /* -2 */
    {0x00, 0x04, 0x07, 0x20, 0x1c, 0x2a, 0x06}, /* -1 */
    {0x00, 0x04, 0x07, 0x20, 0x20, 0x20, 0x06}, /*  0 */
    {0x00, 0x04, 0x07, 0x20, 0x24, 0x16, 0x06}, /* +1 */
    {0x00, 0x04, 0x07, 0x20, 0x28, 0x0c, 0x06}, /* +2 */
};

#define NUM_SATURATION_LEVELS    (5)
static const uint8_t saturation_regs[NUM_SATURATION_LEVELS + 1][5] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA},
    {0x00, 0x02, 0x03, 0x28, 0x28}, /* -2 */
    {0x00, 0x02, 0x03, 0x38, 0x38}, /* -1 */
    {0x00, 0x02, 0x03, 0x48, 0x48}, /*  0 */
    {0x00, 0x02, 0x03, 0x58, 0x58}, /* +1 */
    {0x00, 0x02, 0x03, 0x68, 0x68}, /* +2 */
};

const int OV2640_D[8] = {
  OV2640_D0, OV2640_D1, OV2640_D2, OV2640_D3, OV2640_D4, OV2640_D5, OV2640_D6, OV2640_D7
};

OV2640::OV2640() :
  _OV2640(NULL),
  _saturation(3),
  _hue(0),
  _frame_buffer_pointer(NULL)
{
  //setPins(OV2640_VSYNC, OV2640_HREF, OV2640_PLK, OV2640_XCLK, OV2640_RST, OV2640_D);
  setPins(OV2640_XCLK, OV2640_PLK, OV2640_VSYNC, OV2640_HREF, OV2640_RST,
                     OV2640_D0, OV2640_D1, OV2640_D2, OV2640_D3, OV2640_D4, OV2640_D5, OV2640_D6, OV2640_D7, Wire);

}

//void OV767X::setPins(int vsync, int href, int pclk, int xclk, int rst, const int dpins[8])
void OV2640::setPins(uint8_t mclk_pin, uint8_t pclk_pin, uint8_t vsync_pin, uint8_t hsync_pin, uint8_t en_pin,
                     uint8_t g0, uint8_t g1, uint8_t g2, uint8_t g3, uint8_t g4, uint8_t g5, uint8_t g6, uint8_t g7, TwoWire &wire)
{
  _vsyncPin = vsync_pin;
  _hrefPin = hsync_pin;
  _pclkPin = pclk_pin;
  _xclkPin = mclk_pin;
  _rst = en_pin;
  _dPins[0] = g0;
  _dPins[1] = g1;
  _dPins[2] = g2;
  _dPins[3] = g3;
  _dPins[4] = g4;
  _dPins[5] = g5;
  _dPins[6] = g6;
  _dPins[7] = g7;
  
  _wire = &wire;
              
  //memcpy(_dPins, dpins, sizeof(_dPins));
}


// Read a single uint8_t from address and return it as a uint8_t
uint8_t OV2640::cameraReadRegister(uint8_t reg) {
  _wire->beginTransmission(0x30);
  _wire->write(reg);
  if (_wire->endTransmission(false) != 0) {
    debug.println("error reading OV2640, address");
    return 0;
  }
  if (_wire->requestFrom(0x30, 1) < 1) {
    debug.println("error reading OV2640, data");
    return 0;
  }
  return _wire->read();
}


uint8_t OV2640::cameraWriteRegister(uint8_t reg, uint8_t data) {
  _wire->beginTransmission(0x30);
  _wire->write(reg);
  _wire->write(data);
  if (_wire->endTransmission() != 0) {
    debug.println("error writing to OV2640");
  }
  return 0;
}


uint16_t OV2640::getModelid()
{
    uint8_t Data;
    uint16_t MID = 0x0000;

    Data = cameraReadRegister(0x0A);
       MID = (Data << 8);

    Data = cameraReadRegister(0x0B);
        MID |= Data;

    return MID;
}


//int OV767X::begin(int resolution, int format, int fps,  int camera_name, bool use_gpio)
bool OV2640::begin_omnivision(framesize_t resolution, pixformat_t format, int fps, int camera_name, bool use_gpio)
{

  int _framesize = 0;
  int _format = 0;
  
  _use_gpio = use_gpio;
  // BUGBUG::: see where frame is
  #ifdef USE_DEBUG_PINS
  pinMode(49, OUTPUT);
  #endif
  
  //_wire = &Wire;
  _wire->begin();
  
  switch (resolution) {
  case FRAMESIZE_VGA:
    _width = 640;
    _height = 480;
    _framesize = 0;
    break;

  case FRAMESIZE_CIF:
    _width = 352;
    _height = 240;
    _framesize = 1;
    break;

  case FRAMESIZE_QVGA:
    _width = 320;
    _height = 240;
    _framesize = 2;
    break;

  case FRAMESIZE_QCIF:
    _width = 176;
    _height = 144;
    _framesize = 3;
    break;

  case FRAMESIZE_QQVGA:
    _width = 160;
    _height = 120;
    _framesize = 4;
    break;

  default:
    return 0;
  }

  _grayscale = false;
  switch (format) {
  case YUV422:
    _bytesPerPixel = 2;
    _format = 0;
    break;
  case BAYER:
    _bytesPerPixel = 2;
    _format = 1;
    break;
  case RGB565:
    _bytesPerPixel = 2;
    _format = 2;
    break;

  case GRAYSCALE:
    format = YUV422;    // We use YUV422 but discard U and V bytes
    _bytesPerPixel = 2; // 2 input bytes per pixel of which 1 is discarded
    _grayscale = true;
    _format = 4;
    break;

  default:
    return 0;
  }


  pinMode(_vsyncPin, INPUT_PULLDOWN);
//  const struct digital_pin_bitband_and_config_table_struct *p;
//  p = digital_pin_to_info_PGM + _vsyncPin;
//  *(p->pad) = IOMUXC_PAD_DSE(7) | IOMUXC_PAD_HYS;  // See if I turn on HYS...
  pinMode(_hrefPin, INPUT);
  pinMode(_pclkPin, INPUT_PULLDOWN);
  pinMode(_xclkPin, OUTPUT);
  
#ifdef DEBUG_CAMERA
  debug.printf("  VS=%d, HR=%d, PC=%d XC=%d\n", _vsyncPin, _hrefPin, _pclkPin, _xclkPin);

  for (int i = 0; i < 8; i++) {
    pinMode(_dPins[i], INPUT);
    debug.printf("  _dpins(%d)=%d\n", i, _dPins[i]);
  }
#endif



  _vsyncPort = portInputRegister(digitalPinToPort(_vsyncPin));
  _vsyncMask = digitalPinToBitMask(_vsyncPin);
  _hrefPort = portInputRegister(digitalPinToPort(_hrefPin));
  _hrefMask = digitalPinToBitMask(_hrefPin);
  _pclkPort = portInputRegister(digitalPinToPort(_pclkPin));
  _pclkMask = digitalPinToBitMask(_pclkPin);

/*
  if(camera_name == OV7670) {
      _xclk_freq = 14;  //was 16Mhz
  } else {
      if(fps <= 10){
       _xclk_freq = 14;
      } else {
      _xclk_freq = 16;
      }
  }
*/

  beginXClk();
  
  if(_rst != 0xFF){
    pinMode(_rst, OUTPUT);
    digitalWriteFast(_rst, LOW);      /* Reset */
    for(volatile uint32_t i=0; i<100000; i++)
    {}
    digitalWriteFast(_rst, HIGH);     /* Normal mode. */
    for(volatile uint32_t i=0; i<100000; i++)
    {}
  }
  
  _wire->begin();

  delay(1000);

  if (getModelid() != 0x2641 && getModelid() != 0x2642) {
    end();
    if(_debug) debug.println("Camera detect failed");
    return 0;
  }


  #ifdef DEBUG_CAMERA
  debug.printf("Calling ov7670_configure\n");
  debug.printf("Cam Name: %d, Format: %d, Resolution: %d, Clock: %d\n", camera_name, _format, _framesize, _xclk_freq);
  debug.printf("Frame rate: %d\n", fps);
  #endif
 
 
//flexIO/DMA
    if(!_use_gpio) {
        flexio_configure();
        setVSyncISRPriority(102);
        setDMACompleteISRPriority(192);
    } else {
        setVSyncISRPriority(102);
        setDMACompleteISRPriority(192);
    }

  setFramesize(resolution);
  setPixformat(format);
  //for now frame rate is fixed

  return 1;
}


int OV2640::reset() {
    int ret = 0;

    for (int i=0; default_regs[i][0] && ret == 0; i++) {
        ret |=  cameraWriteRegister(default_regs[i][0], default_regs[i][1]);
    }

    // Delay 10 ms
    delay(10);

    return ret;
}


void OV2640::beginXClk()
{
  // Generates 8 MHz signal using PWM... Will speed up.
  analogWriteFrequency(_xclkPin, _xclk_freq * 1000000);
  analogWrite(_xclkPin, 127); delay(100); // 9mhz works, but try to reduce to debug timings with logic analyzer    

}

void OV2640::endXClk()
{
#if defined(__IMXRT1062__)  // Teensy 4.x
  analogWrite(OV2640_XCLK, 0);
#else
  NRF_I2S->TASKS_STOP = 1;
#endif
}


void OV2640::end()
{
  endXClk();

  pinMode(_xclkPin, INPUT);

  _wire->end();

}

int16_t OV2640::width()
{
  return _width;
}

int16_t OV2640::height()
{
  return _height;
}


int OV2640::setPixformat(pixformat_t pixformat)
{
    //const uint8_t(*regs)[2];
    int ret = 0;

    switch (pixformat) {
      case RGB565:
          //regs = rgb565_regs;
          for (int i=0; rgb565_regs[i][0] && ret == 0; i++) {
              ret |=  cameraWriteRegister(rgb565_regs[i][0], rgb565_regs[i][1]);
          }
          break;
      case YUV422:
      case GRAYSCALE:
          //regs = yuv422_regs;
          for (int i=0; yuv422_regs[i][0] && ret == 0; i++) {
              ret |=  cameraWriteRegister(yuv422_regs[i][0], yuv422_regs[i][1]); 
          }
          break;
      case BAYER:
          //regs = bayer_regs;
          for (int i=0; bayer_regs[i][0] && ret == 0; i++) {
              ret |=  cameraWriteRegister(bayer_regs[i][0], bayer_regs[i][1]); 
          }
          break;
      case JPEG:
          //regs = jpeg_regs;
          for (int i=0; jpeg_regs[i][0] && ret == 0; i++) {
              ret |=  cameraWriteRegister(jpeg_regs[i][0], jpeg_regs[i][1]); 
          }
          break;
      default:
          return 0;
    }

    return ret;
}


uint8_t OV2640::setFramesize(framesize_t framesize) {

    uint16_t sensor_w = 0;
    uint16_t sensor_h = 0;
    int ret = 0;
    uint16_t w = resolution[framesize][0];
    uint16_t h = resolution[framesize][1];

    if ((w % 4) || (h % 4) || (w > UXGA_WIDTH) || (h > UXGA_HEIGHT)) {
        // w/h must be divisible by 4
        return 0;
    }

    // Looks really bad.
    /* if ((w <= CIF_WIDTH) && (h <= CIF_HEIGHT)) {
        regs = cif_regs;
        sensor_w = CIF_WIDTH;
        sensor_h = CIF_HEIGHT;
       } else */
      if ((w <= SVGA_WIDTH) && (h <= SVGA_HEIGHT)) {
        //regs = svga_regs;
      for (int i=0; svga_regs[i][0] && ret == 0; i++) {
          ret |=  cameraWriteRegister(svga_regs[i][0], svga_regs[i][1]); 
      }
      sensor_w = SVGA_WIDTH;
      sensor_h = SVGA_HEIGHT;
    } else {
      //regs = uxga_regs;
      for (int i=0; uxga_regs[i][0] && ret == 0; i++) {
          ret |=  cameraWriteRegister(uxga_regs[i][0], uxga_regs[i][1]); 
      }
      sensor_w = UXGA_WIDTH;
      sensor_h = UXGA_HEIGHT;
    }


    uint64_t tmp_div = min(sensor_w / w, sensor_h / h);
    uint16_t log_div = min(LOG2(tmp_div) - 1, 3);
    uint16_t div = 1 << log_div;
    uint16_t w_mul = w * div;
    uint16_t h_mul = h * div;
    uint16_t x_off = (sensor_w - w_mul) / 2;
    uint16_t y_off = (sensor_h - h_mul) / 2;

    ret |=
        cameraWriteRegister(  CTRLI,
                       CTRLI_LP_DP | CTRLI_V_DIV_SET(log_div) | CTRLI_H_DIV_SET(log_div));
    ret |= cameraWriteRegister( HSIZE, HSIZE_SET(w_mul));
    ret |= cameraWriteRegister(  VSIZE, VSIZE_SET(h_mul));
    ret |= cameraWriteRegister(  XOFFL, XOFFL_SET(x_off));
    ret |= cameraWriteRegister(  YOFFL, YOFFL_SET(y_off));
    ret |= cameraWriteRegister( 
                          VHYX,
                          VHYX_HSIZE_SET(w_mul) | VHYX_VSIZE_SET(h_mul) | VHYX_XOFF_SET(x_off) | VHYX_YOFF_SET(y_off));
    ret |= cameraWriteRegister(  TEST, TEST_HSIZE_SET(w_mul));
    ret |= cameraWriteRegister(  ZMOW, ZMOW_OUTW_SET(w));
    ret |= cameraWriteRegister(  ZMOH, ZMOH_OUTH_SET(h));
    ret |= cameraWriteRegister(  ZMHH, ZMHH_OUTW_SET(w) | ZMHH_OUTH_SET(h));
    ret |= cameraWriteRegister(  R_DVP_SP, div);
    ret |= cameraWriteRegister(  RESET, 0x00);

    return ret;
}

void OV2640::setContrast(int level) {
    int ret = 0;

    level += (NUM_CONTRAST_LEVELS / 2) + 1;
    if (level <= 0 || level > NUM_CONTRAST_LEVELS) {
        if(_debug) debug.println("ERROR: Contrast Levels Exceeded !!!");
        level = 5;
    }

    /* Switch to DSP register bank */
    ret |= cameraWriteRegister(  BANK_SEL, BANK_SEL_DSP);

    /* Write contrast registers */
    for (int i = 0; i < NUM_CONTRAST_LEVELS; i++) {
        ret |= cameraWriteRegister(  contrast_regs[0][i], contrast_regs[level][i]);
    }

    //return ret;
}

int OV2640::setBrightness(int level) {
    int ret = 0;

    level += (NUM_BRIGHTNESS_LEVELS / 2) + 1;
    if (level <= 0 || level > NUM_BRIGHTNESS_LEVELS) {
        return -1;
    }

    /* Switch to DSP register bank */
    ret |= cameraWriteRegister(  BANK_SEL, BANK_SEL_DSP);

    /* Write brightness registers */
    for (int i = 0; i < NUM_BRIGHTNESS_LEVELS; i++) {
        ret |= cameraWriteRegister(  brightness_regs[0][i], brightness_regs[level][i]);
    }

    return ret;
}

void OV2640::setSaturation( int level) {
    int ret = 0;

    level += (NUM_SATURATION_LEVELS / 2) + 1;
    if (level <= 0 || level > NUM_SATURATION_LEVELS) {
        //return 0;
        if(_debug) debug.println("ERROR: Saturation levels exceeded!!");
        level = 5;
    }

    /* Switch to DSP register bank */
    ret |= cameraWriteRegister(BANK_SEL, BANK_SEL_DSP);

    /* Write saturation registers */
    for (int i = 0; i < NUM_SATURATION_LEVELS; i++) {
        ret |= cameraWriteRegister(  saturation_regs[0][i], saturation_regs[level][i]);
    }

    //return ret;
}

int OV2640::setGainceiling(gainceiling_t gainceiling) {
    int ret = 0;

    /* Switch to SENSOR register bank */
    ret |= cameraWriteRegister(  BANK_SEL, BANK_SEL_SENSOR);

    /* Write gain ceiling register */
    ret |= cameraWriteRegister(  COM9, COM9_AGC_SET(gainceiling));

    return ret;
}

int OV2640::setQuality(int qs) {
    int ret = 0;

    /* Switch to DSP register bank */
    ret |= cameraWriteRegister(  BANK_SEL, BANK_SEL_DSP);

    /* Write QS register */
    ret |= cameraWriteRegister(  QS, qs);

    return ret;
}

int OV2640::setColorbar(int enable) {
    uint8_t reg;
    int ret = cameraWriteRegister(  BANK_SEL, BANK_SEL_SENSOR);
    reg = cameraReadRegister( COM7);

    if (enable) {
        reg |= COM7_COLOR_BAR;
    } else {
        reg &= ~COM7_COLOR_BAR;
    }

    return cameraWriteRegister(  COM7, reg) | ret;
}

int OV2640::setAutoGain(int enable, float gain_db, float gain_db_ceiling) {
    uint8_t reg;
    int ret = cameraWriteRegister(  BANK_SEL, BANK_SEL_SENSOR);
    reg = cameraReadRegister( COM8);
    ret |= cameraWriteRegister(  COM8, (reg & (~COM8_AGC_EN)) | ((enable != 0) ? COM8_AGC_EN : 0));

    if ((enable == 0) && (!isnanf(gain_db)) && (!isinff(gain_db))) {
        float gain = max(min(expf((gain_db / 20.0f) * LN10), 32.0f), 1.0f);

        int gain_temp = fast_ceilf(logf(max(gain / 2.0f, 1.0f)) / M_LN2);
        int gain_hi = 0xF >> (4 - gain_temp);
        int gain_lo = min(fast_roundf(((gain / (1 << gain_temp)) - 1.0f) * 16.0f), 15);

        ret |= cameraWriteRegister(  GAIN, (gain_hi << 4) | (gain_lo << 0));
    } else if ((enable != 0) && (!isnanf(gain_db_ceiling)) && (!isinff(gain_db_ceiling))) {
        float gain_ceiling = max(min(expf((gain_db_ceiling / 20.0f) * LN10), 128.0f), 2.0f);

        reg = cameraReadRegister(  COM9 );
        ret |=
            cameraWriteRegister(  COM9,
                           (reg & 0x1F) | ((fast_ceilf(logf(gain_ceiling) / M_LN2) - 1) << 5));
    }

    return ret;
}

int OV2640::getGain_db(float *gain_db) {
    uint8_t reg, gain;
    int ret = cameraWriteRegister(  BANK_SEL, BANK_SEL_SENSOR);
    reg = cameraReadRegister( COM8 );

    // DISABLED
    // if (reg & COM8_AGC_EN) {
    //     ret |= cameraWriteRegister(  COM8, reg & (~COM8_AGC_EN));
    // }
    // DISABLED

    gain = cameraReadRegister(  GAIN );

    // DISABLED
    // if (reg & COM8_AGC_EN) {
    //     ret |= cameraWriteRegister(  COM8, reg | COM8_AGC_EN);
    // }
    // DISABLED

    int hi_gain = 1 << (((gain >> 7) & 1) + ((gain >> 6) & 1) + ((gain >> 5) & 1) + ((gain >> 4) & 1));
    float lo_gain = 1.0f + (((gain >> 0) & 0xF) / 16.0f);
    *gain_db = 20.0f * log10f(hi_gain * lo_gain);

    return ret;
}

int OV2640::setAutoExposure(int enable, int exposure_us) {
    uint8_t reg;
    int ret = cameraWriteRegister(  BANK_SEL, BANK_SEL_SENSOR);
    reg = cameraReadRegister(  COM8 );
    ret |= cameraWriteRegister(  COM8, COM8_SET_AEC(reg, (enable != 0)));

    if ((enable == 0) && (exposure_us >= 0)) {
        reg = cameraReadRegister(  COM7 );
        int t_line = 0;

        if (COM7_GET_RES(reg) == COM7_RES_UXGA) {
            t_line = 1600 + 322;
        }
        if (COM7_GET_RES(reg) == COM7_RES_SVGA) {
            t_line = 800 + 390;
        }
        if (COM7_GET_RES(reg) == COM7_RES_CIF) {
            t_line = 400 + 195;
        }

        reg = cameraReadRegister(  CLKRC );
        int pll_mult = ((reg & CLKRC_DOUBLE) ? 2 : 1) * 3;
        int clk_rc = (reg & CLKRC_DIVIDER_MASK) + 2;

        ret |= cameraWriteRegister(  BANK_SEL, BANK_SEL_DSP);
        reg = cameraReadRegister(  IMAGE_MODE );
        int t_pclk = 0;

        if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_YUV422) {
            t_pclk = 2;
        }
        if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_RAW10) {
            t_pclk = 1;
        }
        if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_RGB565) {
            t_pclk = 2;
        }

        int exposure =
            max(min(((exposure_us * ((((_xclk_freq * 1000000)/ clk_rc) * pll_mult) / 1000000)) / t_pclk) / t_line,
                          0xFFFF), 0x0000);

        ret |= cameraWriteRegister(  BANK_SEL, BANK_SEL_SENSOR);

        reg = cameraReadRegister(  REG04 );
        ret |= cameraWriteRegister(  REG04, (reg & 0xFC) | ((exposure >> 0) & 0x3));

        reg = cameraReadRegister(  AEC );
        ret |= cameraWriteRegister(  AEC, (reg & 0x00) | ((exposure >> 2) & 0xFF));

        reg = cameraReadRegister(  REG45 );
        ret |= cameraWriteRegister(  REG45, (reg & 0xC0) | ((exposure >> 10) & 0x3F));
    }

    return ret;
}

int OV2640::getExposure_us(int *exposure_us) {
    uint8_t reg, aec_10, aec_92, aec_1510;
    int ret = cameraWriteRegister(  BANK_SEL, BANK_SEL_SENSOR);
    reg = cameraReadRegister(  COM8 );

    // DISABLED
    // if (reg & COM8_AEC_EN) {
    //     ret |= cameraWriteRegister(  COM8, reg & (~COM8_AEC_EN));
    // }
    // DISABLED

    aec_10 = cameraReadRegister(  REG04 );
    aec_92 = cameraReadRegister(  AEC );
    aec_1510 = cameraReadRegister(  REG45 );

    // DISABLED
    // if (reg & COM8_AEC_EN) {
    //     ret |= cameraWriteRegister(  COM8, reg | COM8_AEC_EN);
    // }
    // DISABLED

    reg = cameraReadRegister(  COM7 );
    int t_line = 0;

    if (COM7_GET_RES(reg) == COM7_RES_UXGA) {
        t_line = 1600 + 322;
    }
    if (COM7_GET_RES(reg) == COM7_RES_SVGA) {
        t_line = 800 + 390;
    }
    if (COM7_GET_RES(reg) == COM7_RES_CIF) {
        t_line = 400 + 195;
    }

    reg = cameraReadRegister(  CLKRC );
    int pll_mult = ((reg & CLKRC_DOUBLE) ? 2 : 1) * 3;
    int clk_rc = (reg & CLKRC_DIVIDER_MASK) + 2;

    ret |= cameraWriteRegister(  BANK_SEL, BANK_SEL_DSP);
    reg = cameraReadRegister(  IMAGE_MODE );
    int t_pclk = 0;

    if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_YUV422) {
        t_pclk = 2;
    }
    if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_RAW10) {
        t_pclk = 1;
    }
    if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_RGB565) {
        t_pclk = 2;
    }

    uint16_t exposure = ((aec_1510 & 0x3F) << 10) + ((aec_92 & 0xFF) << 2) + ((aec_10 & 0x3) << 0);
    *exposure_us = (exposure * t_line * t_pclk) / ((((_xclk_freq * 1000000)/ clk_rc) * pll_mult) / 1000000);

    return ret;
}

int OV2640::setAutoWhitebal(int enable, float r_gain_db, float g_gain_db, float b_gain_db) {
    uint8_t reg;
    int ret = cameraWriteRegister(  BANK_SEL, BANK_SEL_DSP);
    reg = cameraReadRegister(  CTRL1 );
    ret |= cameraWriteRegister(  CTRL1, (reg & (~CTRL1_AWB)) | ((enable != 0) ? CTRL1_AWB : 0));

    if ((enable == 0) && (!isnanf(r_gain_db)) && (!isnanf(g_gain_db)) && (!isnanf(b_gain_db))
        && (!isinff(r_gain_db)) && (!isinff(g_gain_db)) && (!isinff(b_gain_db))) {
    }

    return ret;
}

int OV2640::getRGB_Gain_db(float *r_gain_db, float *g_gain_db, float *b_gain_db) {
    uint8_t reg;
    int ret = cameraWriteRegister(  BANK_SEL, BANK_SEL_DSP);
    reg = cameraReadRegister(  CTRL1 );

    // DISABLED
    // if (reg & CTRL1_AWB) {
    //     ret |= cameraWriteRegister(  CTRL1, reg & (~CTRL1_AWB));
    // }
    // DISABLED

    // DISABLED
    // if (reg & CTRL1_AWB) {
    //     ret |= cameraWriteRegister(  CTRL1, reg | CTRL1_AWB);
    // }
    // DISABLED

    *r_gain_db = NAN;
    *g_gain_db = NAN;
    *b_gain_db = NAN;

    return ret;
}

int OV2640::setHmirror(int enable) {
    uint8_t reg;
    int ret = cameraWriteRegister(  BANK_SEL, BANK_SEL_SENSOR);
    reg = cameraReadRegister(  REG04 );

    if (!enable) {
        // Already mirrored.
        reg |= REG04_HFLIP_IMG;
    } else {
        reg &= ~REG04_HFLIP_IMG;
    }

    return cameraWriteRegister(  REG04, reg) | ret;
}

int OV2640::setVflip(int enable) {
    uint8_t reg;
    int ret = cameraWriteRegister(  BANK_SEL, BANK_SEL_SENSOR);
    reg = cameraReadRegister(  REG04 );

    if (!enable) {
        // Already flipped.
        reg |= REG04_VFLIP_IMG | REG04_VREF_EN;
    } else {
        reg &= ~(REG04_VFLIP_IMG | REG04_VREF_EN);
    }

    return cameraWriteRegister(  REG04, reg) | ret;
}

int OV2640::setSpecialEffect(sde_t sde) {
    int ret = 0;

    switch (sde) {
        case SDE_NEGATIVE:
            ret |= cameraWriteRegister(  BANK_SEL, BANK_SEL_DSP);
            ret |= cameraWriteRegister(  BPADDR, 0x00);
            ret |= cameraWriteRegister(  BPDATA, 0x40);
            ret |= cameraWriteRegister(  BPADDR, 0x05);
            ret |= cameraWriteRegister(  BPDATA, 0x80);
            ret |= cameraWriteRegister(  BPDATA, 0x80);
            break;
        case SDE_NORMAL:
            ret |= cameraWriteRegister(  BANK_SEL, BANK_SEL_DSP);
            ret |= cameraWriteRegister(  BPADDR, 0x00);
            ret |= cameraWriteRegister(  BPDATA, 0x00);
            ret |= cameraWriteRegister(  BPADDR, 0x05);
            ret |= cameraWriteRegister(  BPDATA, 0x80);
            ret |= cameraWriteRegister(  BPDATA, 0x80);
            break;
        default:
            return -1;
    }

    return ret;
}

/*******************************************************************/

#define FLEXIO_USE_DMA
bool OV2640::readFrame(void *buffer1, size_t cb1, void *buffer2, size_t cb2) {
    if(!_use_gpio) {
        return readFrameFlexIO(buffer1, cb1, buffer2, cb2);
    } else {
        return readFrameGPIO(buffer1, cb1, buffer2, cb2);
    }

}


bool OV2640::readContinuous(bool(*callback)(void *frame_buffer), void *fb1, size_t cb1, void *fb2, size_t cb2) {

	return startReadFlexIO(callback, fb1, cb1, fb2, cb2);

}

void OV2640::stopReadContinuous() {
	
  stopReadFlexIO();

}

bool OV2640::readFrameGPIO(void *buffer, size_t cb1, void *buffer2, size_t cb2)
{    
  debug.printf("$$readFrameGPIO(%p, %u, %p, %u)\n", buffer, cb1, buffer2, cb2);
  const uint32_t frame_size_bytes = _width*_height*_bytesPerPixel;
  if ((cb1+cb2) < frame_size_bytes) return false; // not enough to hold image

  uint8_t* b = (uint8_t*)buffer;
  uint32_t cb = (uint32_t)cb1;
//  bool _grayscale;  // ????  member variable ?????????????
  int bytesPerRow = _width * _bytesPerPixel;

  // Falling edge indicates start of frame
  //pinMode(PCLK_PIN, INPUT); // make sure back to input pin...
  // lets add our own glitch filter.  Say it must be hig for at least 100us
  elapsedMicros emHigh;
  do {
    while ((*_vsyncPort & _vsyncMask) == 0); // wait for HIGH
    emHigh = 0;
    while ((*_vsyncPort & _vsyncMask) != 0); // wait for LOW
  } while (emHigh < 2);

  for (int i = 0; i < _height; i++) {
    // rising edge indicates start of line
    while ((*_hrefPort & _hrefMask) == 0); // wait for HIGH
    while ((*_pclkPort & _pclkMask) != 0); // wait for LOW
    noInterrupts();

    for (int j = 0; j < bytesPerRow; j++) {
      // rising edges clock each data byte
      while ((*_pclkPort & _pclkMask) == 0); // wait for HIGH

      //uint32_t in = ((_frame_buffer_pointer)? GPIO1_DR : GPIO6_DR) >> 18; // read all bits in parallel
      uint32_t in =  (GPIO7_PSR >> 4); // read all bits in parallel  

	  //uint32_t in = mmBus;
      // bugbug what happens to the the data if grayscale?
      if (!(j & 1) || !_grayscale) {
        *b++ = in;
        if ( buffer2 && (--cb == 0) ) {
          if(_debug) debug.printf("\t$$ 2nd buffer: %u %u\n", i, j);
          b = (uint8_t *)buffer2;
          cb = (uint32_t)cb2;
          buffer2 = nullptr;
        }
      }
      while (((*_pclkPort & _pclkMask) != 0) && ((*_hrefPort & _hrefMask) != 0)) ; // wait for LOW bail if _href is lost
    }

    while ((*_hrefPort & _hrefMask) != 0) ;  // wait for LOW
    interrupts();
  }
  return true;
}

/*********************************************************************/

bool OV2640::flexio_configure()
{

    // Going to try this using my FlexIO library.

    // BUGBUG - starting off not going to worry about maybe first pin I choos is on multipl Flex IO controllers (yet)
    uint8_t tpclk_pin; 
    _pflex = FlexIOHandler::mapIOPinToFlexIOHandler(_pclkPin, tpclk_pin);
    if (!_pflex) {
        debug.printf("OV2640 PCLK(%u) is not a valid Flex IO pin\n", _pclkPin);
        return false;
    }
    _pflexio = &(_pflex->port());

    // Quick and dirty:
    uint8_t thsync_pin = _pflex->mapIOPinToFlexPin(_hrefPin);
    uint8_t tg0 = _pflex->mapIOPinToFlexPin(_dPins[0]);
    uint8_t tg1 = _pflex->mapIOPinToFlexPin(_dPins[1]);
    uint8_t tg2 = _pflex->mapIOPinToFlexPin(_dPins[2]);
    uint8_t tg3 = _pflex->mapIOPinToFlexPin(_dPins[3]);

    // make sure the minimum here is valid: 
    if ((thsync_pin == 0xff) || (tg0 == 0xff) || (tg1 == 0xff) || (tg2 == 0xff) || (tg3 == 0xff)) {
        debug.printf("OV2640 Some pins did not map to valid Flex IO pin\n");
        if(_debug) debug.printf("    HSYNC(%u %u) G0(%u %u) G1(%u %u) G2(%u %u) G3(%u %u)", 
            _hrefPin, thsync_pin, _dPins[0], tg0, _dPins[1], tg1, _dPins[2], tg2, _dPins[3], tg3 );
        return false;
    } 
    // Verify that the G numbers are consecutive... Should use arrays!
    if ((tg1 != (tg0+1)) || (tg2 != (tg0+2)) || (tg3 != (tg0+3))) {
        debug.printf("OV2640 Flex IO pins G0-G3 are not consective\n");
        if(_debug) debug.printf("    G0(%u %u) G1(%u %u) G2(%u %u) G3(%u %u)", 
            _dPins[0], tg0, _dPins[1], tg1, _dPins[2], tg2, _dPins[3], tg3 );
        return false;
    }
    if (_dPins[4] != 0xff) {
        uint8_t tg4 = _pflex->mapIOPinToFlexPin(_dPins[4]);
        uint8_t tg5 = _pflex->mapIOPinToFlexPin(_dPins[5]);
        uint8_t tg6 = _pflex->mapIOPinToFlexPin(_dPins[6]);
        uint8_t tg7 = _pflex->mapIOPinToFlexPin(_dPins[7]);
        if ((tg4 != (tg0+4)) || (tg5 != (tg0+5)) || (tg6 != (tg0+6)) || (tg7 != (tg0+7))) {
            debug.printf("OV2640 Flex IO pins G4-G7 are not consective with G0-3\n");
            if(_debug) debug.printf("    G0(%u %u) G4(%u %u) G5(%u %u) G6(%u %u) G7(%u %u)", 
                _dPins[0], tg0, _dPins[4], tg4, _dPins[5], tg5, _dPins[6], tg6, _dPins[7], tg7 );
            return false;
        }
        if(_debug) debug.println("Custom - Flexio is 8 bit mode");
    } else {
      // only 8 bit mode supported
      debug.println("Custom - Flexio 4 bit mode not supported");
      return false;
    }
#if (CNT_SHIFTERS == 1)
    // Needs Shifter 3 (maybe 7 would work as well?)
    if (_pflex->claimShifter(3)) _fshifter = 3;
    else if (_pflex->claimShifter(7)) _fshifter = 7;
    else {
      if(_debug) debug.printf("OV2640 Flex IO: Could not claim Shifter 3 or 7\n");
      return false;
    }
    _fshifter_mask = 1 << _fshifter;   // 4 channels.
    _dma_source = _pflex->shiftersDMAChannel(_fshifter); // looks like they use 

#elif (CNT_SHIFTERS == 4)
    // lets try to claim for shifters 0-3 or 4-7
    // Needs Shifter 3 (maybe 7 would work as well?)
    for (_fshifter = 0; _fshifter < 4; _fshifter++) {
      if (!_pflex->claimShifter(_fshifter)) break;
    }

    if (_fshifter < CNT_SHIFTERS) {
      // failed on 0-3 - released any we claimed
      if(_debug) debug.printf("Failed to claim 0-3(%u) shifters trying 4-7\n", _fshifter);
      while (_fshifter > 0) _pflex->freeShifter(--_fshifter);  // release any we grabbed

      for (_fshifter = 4; _fshifter < (4 + CNT_SHIFTERS); _fshifter++) {
        if (!_pflex->claimShifter(_fshifter)) {
          debug.printf("OV2640 Flex IO: Could not claim Shifter %u\n", _fshifter);
          while (_fshifter > 4) _pflex->freeShifter(--_fshifter);  // release any we grabbed
          return false;
        }
      }
      _fshifter = 4;
    } else {
      _fshifter = 0;
    }


    // ?????????? dma source... 
    _fshifter_mask = 1 << _fshifter;   // 4 channels.
    _dma_source = _pflex->shiftersDMAChannel(_fshifter); // looks like they use 
#else
    // all 8 shifters.
    for (_fshifter = 0; _fshifter < 8; _fshifter++) {
      if (!_pflex->claimShifter(_fshifter)) {
        if(_debug) debug.printf("OV2640 Flex IO: Could not claim Shifter %u\n", _fshifter);
        while (_fshifter > 4) _pflex->freeShifter(--_fshifter);  // release any we grabbed
        return false;
      }
    }
    _fshifter = 0;
    _fshifter_mask = 1 /*0xff */; // 8 channels << _fshifter;   // 4 channels.
    _dma_source = _pflex->shiftersDMAChannel(_fshifter); // looks like they use 
#endif    
    
    // Now request one timer
    uint8_t _ftimer = _pflex->requestTimers(); // request 1 timer. 
    if (_ftimer == 0xff) {
        if(_debug) debug.printf("OV2640 Flex IO: failed to request timer\n");
        return false;
    }

    _pflex->setIOPinToFlexMode(_hrefPin);
    _pflex->setIOPinToFlexMode(_pclkPin);
    _pflex->setIOPinToFlexMode(_dPins[0]);
    _pflex->setIOPinToFlexMode(_dPins[1]);
    _pflex->setIOPinToFlexMode(_dPins[2]);
    _pflex->setIOPinToFlexMode(_dPins[3]);
    _pflex->setIOPinToFlexMode(_dPins[4]);
    _pflex->setIOPinToFlexMode(_dPins[5]);
    _pflex->setIOPinToFlexMode(_dPins[6]);
    _pflex->setIOPinToFlexMode(_dPins[7]);



    // We already configured the clock to allow access.
    // Now sure yet aoub configuring the actual colock speed...

/*
    CCM_CSCMR2 |= CCM_CSCMR2__pflex->CLK_SEL(3); // 480 MHz from USB PLL

    CCM_CS1CDR = (CCM_CS1CDR
        & ~(CCM_CS1CDR__pflex->CLK_PRED(7) | CCM_CS1CDR__pflex->CLK_PODF(7)))
        | CCM_CS1CDR__pflex->CLK_PRED(1) | CCM_CS1CDR__pflex->CLK_PODF(1);


    CCM_CCGR3 |= CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
*/    
    // clksel(0-3PLL4, Pll3 PFD2 PLL5, *PLL3_sw)
    // clk_pred(0, 1, 2, 7) - divide (n+1)
    // clk_podf(0, *7) divide (n+1)
    // So default is 480mhz/16
    // Clock select, pred, podf:
    _pflex->setClockSettings(3, 1, 1);


#ifdef DEBUG_FLEXIO
    debug.println("FlexIO Configure");
    debug.printf(" CCM_CSCMR2 = %08X\n", CCM_CSCMR2);
    uint32_t div1 = ((CCM_CS1CDR >> 9) & 7) + 1;
    uint32_t div2 = ((CCM_CS1CDR >> 25) & 7) + 1;
    debug.printf(" div1 = %u, div2 = %u\n", div1, div2);
    debug.printf(" FlexIO Frequency = %.2f MHz\n", 480.0 / (float)div1 / (float)div2);
    debug.printf(" CCM_CCGR3 = %08X\n", CCM_CCGR3);
    debug.printf(" FlexIO CTRL = %08X\n", _pflexio->CTRL);
    debug.printf(" FlexIO Config, param=%08X\n", _pflexio->PARAM);
    
	  debug.println("8Bit FlexIO");
#endif
      // SHIFTCFG, page 2927
      //  PWIDTH: number of bits to be shifted on each Shift clock
      //          0 = 1 bit, 1-3 = 4 bit, 4-7 = 8 bit, 8-15 = 16 bit, 16-31 = 32 bit
      //  INSRC: Input Source, 0 = pin, 1 = Shifter N+1 Output
      //  SSTOP: Stop bit, 0 = disabled, 1 = match, 2 = use zero, 3 = use one
      //  SSTART: Start bit, 0 = disabled, 1 = disabled, 2 = use zero, 3 = use one
      // setup the for shifters
      #if (CNT_SHIFTERS == 1)
      _pflexio->SHIFTCFG[_fshifter] = FLEXIO_SHIFTCFG_PWIDTH(7);
      #else
      for (int i = 0; i < (CNT_SHIFTERS - 1); i++) {
        _pflexio->SHIFTCFG[_fshifter + i] = FLEXIO_SHIFTCFG_PWIDTH(7) | FLEXIO_SHIFTCFG_INSRC;
      }
      _pflexio->SHIFTCFG[_fshifter + CNT_SHIFTERS-1] = FLEXIO_SHIFTCFG_PWIDTH(7);
      #endif

      // Timer model, pages 2891-2893
      // TIMCMP, page 2937
      // using 4 shifters
      _pflexio->TIMCMP[_ftimer] = (8U * CNT_SHIFTERS) -1 ;
      
      // TIMCTL, page 2933
      //  TRGSEL: Trigger Select ....
      //          4*N - Pin 2*N input
      //          4*N+1 - Shifter N status flag
      //          4*N+2 - Pin 2*N+1 input
      //          4*N+3 - Timer N trigger output
      //  TRGPOL: 0 = active high, 1 = active low
      //  TRGSRC: 0 = external, 1 = internal
      //  PINCFG: timer pin, 0 = disable, 1 = open drain, 2 = bidir, 3 = output
      //  PINSEL: which pin is used by the Timer input or output
      //  PINPOL: 0 = active high, 1 = active low
      //  TIMOD: mode, 0 = disable, 1 = 8 bit baud rate, 2 = 8 bit PWM, 3 = 16 bit
      #define FLEXIO_TIMER_TRIGGER_SEL_PININPUT(x) ((uint32_t)(x) << 1U)
      _pflexio->TIMCTL[_ftimer] = FLEXIO_TIMCTL_TIMOD(3)
          | FLEXIO_TIMCTL_PINSEL(tpclk_pin) // "Pin" is 16 = PCLK
          //| FLEXIO_TIMCTL_TRGSEL(4 * (thsync_pin/2)) // "Trigger" is 12 = HSYNC
          | FLEXIO_TIMCTL_TRGSEL(FLEXIO_TIMER_TRIGGER_SEL_PININPUT(thsync_pin)) // "Trigger" is 12 = HSYNC
          | FLEXIO_TIMCTL_TRGSRC;
    #ifdef DEBUG_FLEXIO
      debug.printf("TIMCTL: %08X PINSEL: %x THSYNC: %x\n", _pflexio->TIMCTL[_ftimer], tpclk_pin, thsync_pin);
    #endif
    
    // SHIFTCTL, page 2926
    //  TIMSEL: which Timer is used for controlling the logic/shift register
    //  TIMPOL: 0 = shift of positive edge, 1 = shift on negative edge
    //  PINCFG: 0 = output disabled, 1 = open drain, 2 = bidir, 3 = output
    //  PINSEL: which pin is used by the Shifter input or output
    //  PINPOL: 0 = active high, 1 = active low
    //  SMOD: 0 = disable, 1 = receive, 2 = transmit, 4 = match store,
    //        5 = match continuous, 6 = state machine, 7 = logic
    // 4 shifters
    uint32_t shiftctl = FLEXIO_SHIFTCTL_TIMSEL(_ftimer) | FLEXIO_SHIFTCTL_SMOD(1)
        | FLEXIO_SHIFTCTL_PINSEL(tg0);    

    for (uint8_t i = 0; i < CNT_SHIFTERS; i++) {
      _pflexio->SHIFTCTL[_fshifter + i] = shiftctl; // 4 = D0
    }

    // TIMCFG, page 2935
    //  TIMOUT: Output
    //          0 = output is logic one when enabled and is not affected by timer reset
    //          1 = output is logic zero when enabled and is not affected by timer reset
    //          2 = output is logic one when enabled and on timer reset
    //          3 = output is logic zero when enabled and on timer reset
    //  TIMDEC: Decrement
    //          0 = on FlexIO clock, Shift clock equals Timer output
    //          1 = on Trigger input (both edges), Shift clock equals Timer output
    //          2 = on Pin input (both edges), Shift clock equals Pin input
    //          3 = on Trigger input (both edges), Shift clock equals Trigger input
    //  TIMRST: Reset
    //          0 = never reset
    //          2 = on Timer Pin equal to Timer Output
    //          3 = on Timer Trigger equal to Timer Output
    //          4 = on Timer Pin rising edge
    //          6 = on Trigger rising edge
    //          7 = on Trigger rising or falling edge
    //  TIMDIS: Disable
    //          0 = never disabled
    //          1 = disabled on Timer N-1 disable
    //          2 = disabled on Timer compare
    //          3 = on Timer compare and Trigger Low
    //          4 = on Pin rising or falling edge
    //          5 = on Pin rising or falling edge provided Trigger is high
    //          6 = on Trigger falling edge
    //  TIMENA
    //          0 = always enabled
    //          1 = enabled on Timer N-1 enable
    //          2 = enabled on Trigger high
    //          3 = enabled on Trigger high and Pin high
    //          4 = enabled on Pin rising edge
    //          5 = enabled on Pin rising edge and Trigger high
    //          6 = enabled on Trigger rising edge
    //          7 = enabled on Trigger rising or falling edge
    //  TSTOP Stop bit, 0 = disabled, 1 = on compare, 2 = on disable, 3 = on either
    //  TSTART: Start bit, 0 = disabled, 1 = enabled
    _pflexio->TIMCFG[_ftimer] = FLEXIO_TIMCFG_TIMOUT(1) | FLEXIO_TIMCFG_TIMDEC(2)
        | FLEXIO_TIMCFG_TIMENA(6) | FLEXIO_TIMCFG_TIMDIS(6);

    // CTRL, page 2916
    _pflexio->CTRL = FLEXIO_CTRL_FLEXEN; // enable after everything configured
    
#ifdef DEBUG_FLEXIO
    debug.printf(" FLEXIO:%u Shifter:%u Timer:%u\n", _pflex->FlexIOIndex(), _fshifter, _ftimer);
    debug.print("     SHIFTCFG = ");
    for (uint8_t i = 0; i < CNT_SHIFTERS; i++) debug.printf(" %08X", _pflexio->SHIFTCFG[_fshifter + i]);
    debug.print("\n     SHIFTCTL = ");
    for (uint8_t i = 0; i < CNT_SHIFTERS; i++) debug.printf(" %08X", _pflexio->SHIFTCTL[_fshifter + i]);
    debug.printf("\n     TIMCMP = %08X\n", _pflexio->TIMCMP[_ftimer]);
    debug.printf("     TIMCFG = %08X\n", _pflexio->TIMCFG[_ftimer]);
    debug.printf("     TIMCTL = %08X\n", _pflexio->TIMCTL[_ftimer]);
#endif
return true;
}


void dumpDMA_TCD_5(DMABaseClass *dmabc, const char *psz_title) {
  if (psz_title)
    debug.print(psz_title);
  debug.printf("%x %x: ", (uint32_t)dmabc, (uint32_t)dmabc->TCD);

  debug.printf(
      "SA:%x SO:%d AT:%x (SM:%x SS:%x DM:%x DS:%x) NB:%x SL:%d DA:%x DO: %d CI:%x DL:%x CS:%x BI:%x\n",
      (uint32_t)dmabc->TCD->SADDR, dmabc->TCD->SOFF, dmabc->TCD->ATTR,
      (dmabc->TCD->ATTR >> 11) & 0x1f, (dmabc->TCD->ATTR >> 8) & 0x7,
      (dmabc->TCD->ATTR >> 3) & 0x1f, (dmabc->TCD->ATTR >> 0) & 0x7,
      dmabc->TCD->NBYTES, dmabc->TCD->SLAST, (uint32_t)dmabc->TCD->DADDR,
      dmabc->TCD->DOFF, dmabc->TCD->CITER, dmabc->TCD->DLASTSGA,
      dmabc->TCD->CSR, dmabc->TCD->BITER);
}

bool OV2640::readFrameFlexIO(void *buffer, size_t cb1, void* buffer2, size_t cb2)
{
    if (_debug)debug.printf("$$OV2640::readFrameFlexIO(%p, %u, %p, %u, %u)\n", buffer, cb1, buffer2, cb2, _fuse_dma);
    const uint32_t frame_size_bytes = _width*_height*_bytesPerPixel;
    if ((cb1+cb2) < frame_size_bytes) return false; // not enough to hold image

    //flexio_configure(); // one-time hardware setup
    // wait for VSYNC to go high and then low with a sort of glitch filter
    elapsedMillis emWaitSOF;
    elapsedMicros emGlitch;
    for (;;) {
      if (emWaitSOF > 2000) {
        if(_debug) debug.println("Timeout waiting for Start of Frame");
        return false;
      }
      while ((*_vsyncPort & _vsyncMask) == 0);
      emGlitch = 0;
      while ((*_vsyncPort & _vsyncMask) != 0);
      if (emGlitch > 5) break;
    }

    _pflexio->SHIFTSTAT = _fshifter_mask; // clear any prior shift status
    _pflexio->SHIFTERR = _fshifter_mask;
    uint32_t *p = (uint32_t *)buffer;

    //----------------------------------------------------------------------
    // Polling FlexIO version
    //----------------------------------------------------------------------
    if (!_fuse_dma) {
      if (_debug)debug.println("\tNot DMA");
      #ifdef USE_DEBUG_PINS
      digitalWriteFast(2, HIGH);
      #endif
      // read FlexIO by polling
      //uint32_t *p_end = (uint32_t *)buffer + (_width*_height/4)*_bytesPerPixel;
      uint32_t count_items_left = (_width*_height/4)*_bytesPerPixel;
      uint32_t count_items_left_in_buffer = (uint32_t)cb1 / 4;

      while (count_items_left) {
          while ((_pflexio->SHIFTSTAT & _fshifter_mask) == 0) {
              // wait for FlexIO shifter data
          }
          // Lets try to load in multiple shifters
          for (uint8_t i = 0; i < CNT_SHIFTERS; i++) {
            *p++ = _pflexio->SHIFTBUF[_fshifter+i]; // should use DMA...
            count_items_left--;
            if (buffer2 && (--count_items_left_in_buffer == 0)) {
              p = (uint32_t*)buffer2;
              count_items_left_in_buffer = (uint32_t)cb2 / 4;
            }
          }
      }
      #ifdef USE_DEBUG_PINS
      digitalWriteFast(2, LOW);
      #endif
      return true;
    }

    //----------------------------------------------------------------------
    // Use DMA FlexIO version
    //----------------------------------------------------------------------

    _dmachannel.begin();
    _dmachannel.triggerAtHardwareEvent(_dma_source);
    active_dma_camera = this;
    _dmachannel.attachInterrupt(dmaInterruptFlexIO);
    /* Configure DMA MUX Source */
    //DMAMUX->CHCFG[FLEXIO_CAMERA_DMA_CHN] = DMAMUX->CHCFG[FLEXIO_CAMERA_DMA_CHN] &
    //                                        (~DMAMUX_CHCFG_SOURCE_MASK) | 
    //                                        DMAMUX_CHCFG_SOURCE(FLEXIO_CAMERA_DMA_MUX_SRC);
    /* Enable DMA channel. */
    // if only one buffer split over the one buffer assuming big enough.
    // Total length of bytes transfered
    // do it over 2 
    // first pass split into two
    uint8_t dmas_index = 0;
    // We will do like above with both buffers, maybe later try to merge the two sections.
    uint32_t cb_left = min(frame_size_bytes, cb1);
    uint8_t count_dma_settings = (cb_left / (32767 * 4)) + 1;
    uint32_t cb_per_setting = ((cb_left / count_dma_settings) + 3) & 0xfffffffc; // round up to next multiple of 4.
    if (_debug) debug.printf("frame size: %u, cb1:%u cnt dma: %u CB per: %u\n", frame_size_bytes, cb1, count_dma_settings, cb_per_setting);

    for (; dmas_index < count_dma_settings; dmas_index++) {
      _dmasettings[dmas_index].TCD->CSR = 0;
      _dmasettings[dmas_index].source(_pflexio->SHIFTBUF[_fshifter]);
      _dmasettings[dmas_index].destinationBuffer(p, cb_per_setting);
      _dmasettings[dmas_index].replaceSettingsOnCompletion(_dmasettings[dmas_index + 1]);
      p += (cb_per_setting / 4);
      cb_left -= cb_per_setting;
      if (cb_left < cb_per_setting) cb_per_setting = cb_left;
    }
    if (frame_size_bytes > cb1) {
      cb_left = frame_size_bytes - cb1;
      count_dma_settings = (cb_left / (32767 * 4)) + 1;
      cb_per_setting = ((cb_left / count_dma_settings) + 3) & 0xfffffffc; // round up to next multiple of 4.
      if (_debug) debug.printf("frame size left: %u, cb2:%u cnt dma: %u CB per: %u\n", cb_left, cb2, count_dma_settings, cb_per_setting);
      
      p = (uint32_t *)buffer2;

      for (uint8_t i=0; i < count_dma_settings; i++, dmas_index++) {
        _dmasettings[dmas_index].TCD->CSR = 0;
        _dmasettings[dmas_index].source(_pflexio->SHIFTBUF[_fshifter]);
        _dmasettings[dmas_index].destinationBuffer(p, cb_per_setting);
        _dmasettings[dmas_index].replaceSettingsOnCompletion(_dmasettings[dmas_index + 1]);
        p += (cb_per_setting / 4);
        cb_left -= cb_per_setting;
        if (cb_left < cb_per_setting) cb_per_setting = cb_left;
      }
    }  
    dmas_index--; // lets point back to the last one
    _dmasettings[dmas_index].replaceSettingsOnCompletion(_dmasettings[0]);
    _dmasettings[dmas_index].disableOnCompletion();
    _dmasettings[dmas_index].interruptAtCompletion();
    _dmachannel = _dmasettings[0];

    _dmachannel.clearComplete();
#ifdef DEBUG_FLEXIO
    if (_debug) {
      dumpDMA_TCD_5(&_dmachannel," CH: ");
      for (uint8_t i = 0; i <= dmas_index; i++) {
        debug.printf(" %u: ", i);
        dumpDMA_TCD_5(&_dmasettings[i], nullptr);
      }
    }
#endif


    _dma_state = DMA_STATE_ONE_FRAME;
    _pflexio->SHIFTSDEN = _fshifter_mask;
    _dmachannel.enable();
    
#ifdef DEBUG_FLEXIO
    if (_debug) debug.printf("Flexio DMA: length: %d\n", frame_size_bytes);
#endif
    
    elapsedMillis timeout = 0;
    //while (!_dmachannel.complete()) {
    while (_dma_state == DMA_STATE_ONE_FRAME) {
        // wait - we should not need to actually do anything during the DMA transfer
        if (_dmachannel.error()) {
            debug.println("DMA error");
            if (_pflexio->SHIFTSTAT) debug.printf(" SHIFTSTAT %08X\n", _pflexio->SHIFTSTAT);
            debug.flush();
            uint32_t i = _pflexio->SHIFTBUF[_fshifter];
            debug.printf("Result: %x\n", i);


            _dmachannel.clearError();
            break;
        }
        if (timeout > 500) {
            if (_debug) debug.println("Timeout waiting for DMA");
            if (_pflexio->SHIFTSTAT & _fshifter_mask) debug.printf(" SHIFTSTAT bit was set (%08X)\n", _pflexio->SHIFTSTAT);
            #ifdef DEBUG_CAMERA
            debug.printf(" DMA channel #%u\n", _dmachannel.channel);
            debug.printf(" DMAMUX = %08X\n", *(&DMAMUX_CHCFG0 + _dmachannel.channel));
            debug.printf(" _pflexio->SHIFTSDEN = %02X\n", _pflexio->SHIFTSDEN);
            debug.printf(" TCD CITER = %u\n", _dmachannel.TCD->CITER_ELINKNO);
            debug.printf(" TCD CSR = %08X\n", _dmachannel.TCD->CSR);
            #endif
            break;
        }
    }
    #ifdef USE_DEBUG_PINS
        digitalWriteFast(2, LOW);
    #endif
    //arm_dcache_delete(buffer, frame_size_bytes);
    if ((uint32_t)buffer >= 0x20200000u) arm_dcache_delete(buffer, min(cb1, frame_size_bytes));
    if (frame_size_bytes > cb1) {
      if ((uint32_t)buffer2 >= 0x20200000u) arm_dcache_delete(buffer2, frame_size_bytes - cb1);
    } 

#ifdef DEBUG_FLEXIO
    if (_debug) dumpDMA_TCD_5(&_dmachannel,"CM: ");
#endif
//    dumpDMA_TCD_5(&_dmasettings[0], " 0: ");
//    dumpDMA_TCD_5(&_dmasettings[1], " 1: ");
    return true;
}



bool OV2640::startReadFlexIO(bool(*callback)(void *frame_buffer), void *fb1, size_t cb1, void *fb2, size_t cb2)
{

#ifdef FLEXIO_USE_DMA
    const uint32_t frame_size_bytes = _width*_height*_bytesPerPixel;
    // lets handle a few cases.
    if (fb1 == nullptr) return false;
    if (cb1 < frame_size_bytes) {
      if ((fb2 == nullptr) || ((cb1+cb2) < frame_size_bytes)) return false;
    }

    _frame_buffer_1 = (uint8_t *)fb1;
    _frame_buffer_1_size = cb1;
    _frame_buffer_2 = (uint8_t *)fb2;
    _frame_buffer_2_size = cb2;
    _callback = callback;
    active_dma_camera = this;

    //flexio_configure(); // one-time hardware setup
    _pflexio->SHIFTSTAT = _fshifter_mask; // clear any prior shift status
    _pflexio->SHIFTERR = _fshifter_mask;
    uint32_t *p = (uint32_t *)fb1;

    //----------------------------------------------------------------------
    // Use DMA FlexIO version
    //----------------------------------------------------------------------
    // Currently lets setup for only one shifter
//    digitalWriteFast(2, HIGH);


    _dmachannel.begin();
    _dmachannel.triggerAtHardwareEvent(_dma_source);
    active_dma_camera = this;
    _dmachannel.attachInterrupt(dmaInterruptFlexIO);


    #if 1
    // Two versions.  If one buffer is large enough, we will use the two buffers to 
    // read in two frames.  If the combined in large enough for one frame, we will 
    // setup to read one frame but still interrupt on each buffer filled completion

    uint8_t dmas_index = 0;
    
    uint32_t cb_left = min(frame_size_bytes, cb1);
    uint8_t count_dma_settings = (cb_left / (32767 * 4)) + 1;
    uint32_t cb_per_setting = ((cb_left / count_dma_settings) + 3) & 0xfffffffc; // round up to next multiple of 4.
    if (_debug) debug.printf("frame size: %u, cb1:%u cnt dma: %u CB per: %u\n", frame_size_bytes, cb1, count_dma_settings, cb_per_setting);

    for (; dmas_index < count_dma_settings; dmas_index++) {
      _dmasettings[dmas_index].TCD->CSR = 0;
      _dmasettings[dmas_index].source(_pflexio->SHIFTBUF[_fshifter]);
      _dmasettings[dmas_index].destinationBuffer(p, cb_per_setting);
      _dmasettings[dmas_index].replaceSettingsOnCompletion(_dmasettings[dmas_index + 1]);
      p += (cb_per_setting / 4);
      cb_left -= cb_per_setting;
      if (cb_left < cb_per_setting) cb_per_setting = cb_left;
    }
    // Interrupt after each buffer is filled.
    _dmasettings[dmas_index-1].interruptAtCompletion();
    if (cb1 >= frame_size_bytes) {
      _dmasettings[dmas_index-1].disableOnCompletion();  // full frame
      if (fb2 && (cb2 >= frame_size_bytes)) cb_left = min(frame_size_bytes, cb2);
    } else cb_left = frame_size_bytes - cb1;  // need second buffer to complete one frame.

    if (cb_left) {
      count_dma_settings = (cb_left / (32767 * 4)) + 1;
      cb_per_setting = ((cb_left / count_dma_settings) + 3) & 0xfffffffc; // round up to next multiple of 4.
      if (_debug) debug.printf("frame size left: %u, cb2:%u cnt dma: %u CB per: %u\n", cb_left, cb2, count_dma_settings, cb_per_setting);
      
      p = (uint32_t *)fb2;

      for (uint8_t i=0; i < count_dma_settings; i++, dmas_index++) {
        _dmasettings[dmas_index].TCD->CSR = 0;
        _dmasettings[dmas_index].source(_pflexio->SHIFTBUF[_fshifter]);
        _dmasettings[dmas_index].destinationBuffer(p, cb_per_setting);
        _dmasettings[dmas_index].replaceSettingsOnCompletion(_dmasettings[dmas_index + 1]);
        p += (cb_per_setting / 4);
        cb_left -= cb_per_setting;
        if (cb_left < cb_per_setting) cb_per_setting = cb_left;
      }
      _dmasettings[dmas_index-1].disableOnCompletion();
      _dmasettings[dmas_index-1].interruptAtCompletion();
    }  
    dmas_index--; // lets point back to the last one
    _dmasettings[dmas_index].replaceSettingsOnCompletion(_dmasettings[0]);
    _dmachannel = _dmasettings[0];
    _dmachannel.clearComplete();

#ifdef DEBUG_FLEXIO
    if (_debug) {
      dumpDMA_TCD_5(&_dmachannel," CH: ");
      for (uint8_t i = 0; i <= dmas_index; i++) {
        debug.printf(" %u: ", i);
        dumpDMA_TCD_5(&_dmasettings[i], nullptr);
      }
    }
    debug.printf("Flexio DMA: length: %d\n", frame_size_bytes);

#endif

    #else
    // Total length of bytes transfered
    // do it over 2 
    // first pass split into two
    _dmasettings[0].source(_pflexio->SHIFTBUF[_fshifter]);
    _dmasettings[0].destinationBuffer(p, frame_size_bytes / 2);
    _dmasettings[0].replaceSettingsOnCompletion(_dmasettings[1]);

    _dmasettings[1].source(_pflexio->SHIFTBUF[_fshifter]);
    _dmasettings[1].destinationBuffer(&p[frame_size_bytes / 8], frame_size_bytes / 2);
    _dmasettings[1].replaceSettingsOnCompletion(_dmasettings[2]);
    _dmasettings[1].interruptAtCompletion();

    // lets preset up the dmasettings for second buffer
    p = (uint32_t *)fb2;
    _dmasettings[2].source(_pflexio->SHIFTBUF[_fshifter]);
    _dmasettings[2].destinationBuffer(p, frame_size_bytes / 2);
    _dmasettings[2].replaceSettingsOnCompletion(_dmasettings[3]);

    _dmasettings[3].source(_pflexio->SHIFTBUF[_fshifter]);
    _dmasettings[3].destinationBuffer(&p[frame_size_bytes / 8], frame_size_bytes / 2);
    _dmasettings[3].replaceSettingsOnCompletion(_dmasettings[0]);
    _dmasettings[3].interruptAtCompletion();


    #ifdef USE_VSYNC_PIN_INT
    // disable when we have received a full frame. 
    _dmasettings[1].disableOnCompletion();
    _dmasettings[3].disableOnCompletion();
    #else
    _dmasettings[1].TCD->CSR &= ~(DMA_TCD_CSR_DREQ); // Don't disable on this one
    _dmasettings[3].TCD->CSR &= ~(DMA_TCD_CSR_DREQ); // Don't disable on this one
    #endif

    _dmachannel = _dmasettings[0];

    _dmachannel.clearComplete();
#ifdef DEBUG_FLEXIO

    dumpDMA_TCD_5(&_dmachannel," CH: ");
    dumpDMA_TCD_5(&_dmasettings[0], " 0: ");
    dumpDMA_TCD_5(&_dmasettings[1], " 1: ");
    dumpDMA_TCD_5(&_dmasettings[2], " 2: ");
    dumpDMA_TCD_5(&_dmasettings[3], " 3: ");
    debug.printf("Flexio DMA: length: %d\n", frame_size_bytes);

#endif
#endif
    _pflexio->SHIFTSTAT = _fshifter_mask; // clear any prior shift status
    _pflexio->SHIFTERR = _fshifter_mask;


    _dma_last_completed_frame = nullptr;
    _dma_frame_count = 0;

    _dma_state = DMASTATE_RUNNING;

#ifdef USE_VSYNC_PIN_INT
    // Lets use interrupt on interrupt on VSYNC pin to start the capture of a frame
    _dma_active = false;
    _vsync_high_time = 0;
    NVIC_SET_PRIORITY(IRQ_GPIO6789, 102);
    //NVIC_SET_PRIORITY(dma_flexio.channel & 0xf, 102);
    attachInterrupt(_vsyncPin, &frameStartInterruptFlexIO, RISING);
    _pflexio->SHIFTSDEN = _fshifter_mask;
#else    
    // wait for VSYNC to go high and then low with a sort of glitch filter
    elapsedMillis emWaitSOF;
    elapsedMicros emGlitch;
    for (;;) {
      if (emWaitSOF > 2000) {
        if(_debug) debug.println("Timeout waiting for Start of Frame");
        return false;
      }
      while ((*_vsyncPort & _vsyncMask) == 0);
      emGlitch = 0;
      while ((*_vsyncPort & _vsyncMask) != 0);
      if (emGlitch > 2) break;
    }

    _pflexio->SHIFTSDEN = _fshifter_mask;
    _dmachannel.enable();
#endif    

    return true;
#else
    return false;
#endif
}

#ifdef USE_VSYNC_PIN_INT
void OV2640::frameStartInterruptFlexIO()
{
	active_dma_camera->processFrameStartInterruptFlexIO();
}

void OV2640::processFrameStartInterruptFlexIO()
{
  #ifdef USE_DEBUG_PINS
  digitalWriteFast(5, HIGH);
  #endif
  //debug.println("VSYNC");
  // See if we read the state of it a few times if the pin stays high...
  if (digitalReadFast(_vsyncPin) && digitalReadFast(_vsyncPin) && digitalReadFast(_vsyncPin) 
          && digitalReadFast(_vsyncPin) )  {
    // stop this interrupt.
    #ifdef USE_DEBUG_PINS
    //digitalToggleFast(2);
    digitalWriteFast(2, LOW);
    digitalWriteFast(2, HIGH);
    #endif
    detachInterrupt(_vsyncPin);

    // For this pass will leave in longer DMAChain with both buffers.
  	_pflexio->SHIFTSTAT = _fshifter_mask; // clear any prior shift status
  	_pflexio->SHIFTERR = _fshifter_mask;

    _vsync_high_time = 0; // clear out the time.
    _dmachannel.clearComplete();
    _dmachannel.enable();
  }
	asm("DSB");
  #ifdef USE_DEBUG_PINS
  digitalWriteFast(5, LOW);
  #endif
}

#endif

void OV2640::dmaInterruptFlexIO()
{
	active_dma_camera->processDMAInterruptFlexIO();
}

void OV2640::processDMAInterruptFlexIO()
{

  _dmachannel.clearInterrupt();
  #ifdef USE_DEBUG_PINS
//  digitalToggleFast(2);
  digitalWriteFast(2, HIGH);
  digitalWriteFast(2, LOW);
  #endif
  if (_dma_state == DMA_STATE_ONE_FRAME) {
    _dma_state = DMA_STATE_STOPPED;
    asm("DSB");
    return;

  } else if (_dma_state == DMASTATE_STOP_REQUESTED) {
    _dmachannel.disable();
    _frame_buffer_1 = nullptr;
    _frame_buffer_2 = nullptr;
    _callback = nullptr;
    _dma_state = DMA_STATE_STOPPED;
    asm("DSB");
    return;
  }

#if 0
  static uint8_t debug_print_count = 8;
  if (debug_print_count) {
    debug_print_count--;
    debug.printf("PDMAIF: %x\n", (uint32_t)_dmachannel.TCD->DADDR);
    dumpDMA_TCD_5(&_dmachannel," CH: ");

  }
#endif  
	_dmachannel.clearComplete();
  const uint32_t frame_size_bytes = _width*_height*_bytesPerPixel;
  if (((uint32_t)_dmachannel.TCD->DADDR) == (uint32_t)_frame_buffer_1) {
     _dma_last_completed_frame = _frame_buffer_2;
    if ((uint32_t)_frame_buffer_2 >= 0x20200000u) arm_dcache_delete(_frame_buffer_2, min(_frame_buffer_2_size, frame_size_bytes));
  } else {
     _dma_last_completed_frame = _frame_buffer_1;
    if ((uint32_t)_frame_buffer_1 >= 0x20200000u) arm_dcache_delete(_frame_buffer_1, min(_frame_buffer_1_size, frame_size_bytes));    
  }

	if (_callback) (*_callback)(_dma_last_completed_frame); // TODO: use EventResponder
  // if we disabled the DMA, then setup to wait for vsyncpin...
  if ((_dma_last_completed_frame == _frame_buffer_2) || (_frame_buffer_1_size >= frame_size_bytes)) {
    _dma_active = false;
    // start up interrupt to look for next start of interrupt.
    _vsync_high_time = 0; // remember the time we were called

    if (_dma_state == DMASTATE_RUNNING) attachInterrupt(_vsyncPin, &frameStartInterruptFlexIO, RISING);
  }

	asm("DSB");
}


bool OV2640::stopReadFlexIO()
{
  #ifdef USE_VSYNC_PIN_INT
  // first disable the vsync interrupt
  detachInterrupt(_vsyncPin);
  if (!_dma_active) {
    _dma_state = DMA_STATE_STOPPED;
  } else {
    cli();
    if (_dma_state != DMA_STATE_STOPPED) _dma_state = DMASTATE_STOP_REQUESTED;
    sei();
  }
  #else
  _dmasettings[1].disableOnCompletion();
  _dmasettings[3].disableOnCompletion();
  _dma_state = DMASTATE_STOP_REQUESTED;
  #endif
	return true;
}


//======================================== DMA JUNK
//================================================================================
// experiment with DMA
//================================================================================
// Define our DMA structure.
DMAChannel OV2640::_dmachannel;
DMASetting OV2640::_dmasettings[10];
uint32_t OV2640::_dmaBuffer1[DMABUFFER_SIZE] __attribute__ ((used, aligned(32)));
uint32_t OV2640::_dmaBuffer2[DMABUFFER_SIZE] __attribute__ ((used, aligned(32)));
extern "C" void xbar_connect(unsigned int input, unsigned int output); // in pwm.c

OV2640 *OV2640::active_dma_camera = nullptr;


//===================================================================
// Start a DMA operation -
//===================================================================
bool OV2640::startReadFrameDMA(bool(*callback)(void *frame_buffer), uint8_t *fb1, uint8_t *fb2)
{
  // First see if we need to allocate frame buffers.
  if (fb1) _frame_buffer_1 = fb1;
  else if (_frame_buffer_1 == nullptr) {
    _frame_buffer_1 = (uint8_t*)malloc(_width * _height );
    if (_frame_buffer_1 == nullptr) return false;
  }
  if (fb2) _frame_buffer_2 = fb2;
  else if (_frame_buffer_2 == nullptr) {
    _frame_buffer_2 = (uint8_t*)malloc(_width * _height);
    if (_frame_buffer_2 == nullptr) return false; // BUGBUG should we 32 byte align?
  }
  // remember the call back if passed in
  _callback = callback;
  active_dma_camera = this;

  if(_debug) debug.printf("startReadFrameDMA called buffers %x %x\n", (uint32_t)_frame_buffer_1, (uint32_t)_frame_buffer_2);

  //DebugDigitalToggle(OV7670_DEBUG_PIN_1);
  // lets figure out how many bytes we will tranfer per setting...
  //  _dmasettings[0].begin();
  _frame_row_buffer_pointer = _frame_buffer_pointer = (uint8_t *)_frame_buffer_1;

  // configure DMA channels
  _dmachannel.begin();
  _dmasettings[0].source(GPIO2_PSR); // setup source.
  _dmasettings[0].destinationBuffer(_dmaBuffer1, DMABUFFER_SIZE * 4);  // 32 bits per logical byte
  _dmasettings[0].replaceSettingsOnCompletion(_dmasettings[1]);
  _dmasettings[0].interruptAtCompletion();  // we will need an interrupt to process this.
  _dmasettings[0].TCD->CSR &= ~(DMA_TCD_CSR_DREQ); // Don't disable on this one
  //DebugDigitalToggle(OV7670_DEBUG_PIN_1);

  _dmasettings[1].source(GPIO2_PSR); // setup source.
  _dmasettings[1].destinationBuffer(_dmaBuffer2, DMABUFFER_SIZE * 4);  // 32 bits per logical byte
  _dmasettings[1].replaceSettingsOnCompletion(_dmasettings[0]);
  _dmasettings[1].interruptAtCompletion();  // we will need an interrupt to process this.
  _dmasettings[1].TCD->CSR &= ~(DMA_TCD_CSR_DREQ); // Don't disable on this one
  //DebugDigitalToggle(OV7670_DEBUG_PIN_1);

  GPIO2_GDIR = 0; // set all as input...
  GPIO2_DR = 0; // see if I can clear it out...

  _dmachannel = _dmasettings[0];  // setup the first on...
  _dmachannel.attachInterrupt(dmaInterrupt);
  _dmachannel.triggerAtHardwareEvent(DMAMUX_SOURCE_XBAR1_0);
  //DebugDigitalToggle(OV7670_DEBUG_PIN_1);

  // Lets try to setup the DMA setup...
  // first see if we can convert the _pclk to be an XBAR Input pin...
    // OV7670_PLK   4
  // OV7670_PLK   8    //8       B1_00   FlexIO2:16  XBAR IO14

  _save_pclkPin_portConfigRegister = *(portConfigRegister(_pclkPin));
  *(portConfigRegister(_pclkPin)) = 1; // set to XBAR mode 14

  // route the timer outputs through XBAR to edge trigger DMA request
  CCM_CCGR2 |= CCM_CCGR2_XBAR1(CCM_CCGR_ON);
  xbar_connect(XBARA1_IN_IOMUX_XBAR_INOUT14, XBARA1_OUT_DMA_CH_MUX_REQ30);
  //DebugDigitalToggle(OV7670_DEBUG_PIN_1);

  // Tell XBAR to dDMA on Rising
  XBARA1_CTRL0 = XBARA_CTRL_STS0 | XBARA_CTRL_EDGE0(1) | XBARA_CTRL_DEN0/* | XBARA_CTRL_IEN0 */ ;

  IOMUXC_GPR_GPR6 &= ~(IOMUXC_GPR_GPR6_IOMUXC_XBAR_DIR_SEL_14);  // Make sure it is input mode
  IOMUXC_XBAR1_IN14_SELECT_INPUT = 1; // Make sure this signal goes to this pin...


#if defined (ARDUINO_TEENSY_MICROMOD)
  // Need to switch the IO pins back to GPI1 from GPIO6
  _save_IOMUXC_GPR_GPR27 = IOMUXC_GPR_GPR27;  // save away the configuration before we change...
  IOMUXC_GPR_GPR27 &= ~(0x0ff0u);

  // lets also un map the _hrefPin to GPIO1
  IOMUXC_GPR_GPR27 &= ~_hrefMask; //
#else
  // Need to switch the IO pins back to GPI1 from GPIO6
  _save_IOMUXC_GPR_GPR26 = IOMUXC_GPR_GPR26;  // save away the configuration before we change...
  IOMUXC_GPR_GPR26 &= ~(0x0ff0u);

  // lets also un map the _hrefPin to GPIO1
  IOMUXC_GPR_GPR26 &= ~_hrefMask; //
#endif

  // Need to switch the IO pins back to GPI1 from GPIO6
  //_save_IOMUXC_GPR_GPR27 = IOMUXC_GPR_GPR27;  // save away the configuration before we change...
  //IOMUXC_GPR_GPR27 &= ~(0x0ff0u);

  // lets also un map the _hrefPin to GPIO1
  //IOMUXC_GPR_GPR27 &= ~_hrefMask; //


  //DebugDigitalToggle(OV7670_DEBUG_PIN_1);

  // Falling edge indicates start of frame
//  while ((*_vsyncPort & _vsyncMask) == 0); // wait for HIGH
//  while ((*_vsyncPort & _vsyncMask) != 0); // wait for LOW
//  DebugDigitalWrite(OV7670_DEBUG_PIN_2, HIGH);

// Debug stuff for now

  // We have the start of a frame, so lets start the dma.
#ifdef DEBUG_CAMERA
  dumpDMA_TCD_5(&_dmachannel," CH: ");
  dumpDMA_TCD_5(&_dmasettings[0], " 0: ");
  dumpDMA_TCD_5(&_dmasettings[1], " 1: ");

  debug.printf("pclk pin: %d config:%lx control:%lx\n", _pclkPin, *(portConfigRegister(_pclkPin)), *(portControlRegister(_pclkPin)));
  debug.printf("IOMUXC_GPR_GPR26-29:%lx %lx %lx %lx\n", IOMUXC_GPR_GPR26, IOMUXC_GPR_GPR27, IOMUXC_GPR_GPR28, IOMUXC_GPR_GPR29);
  debug.printf("GPIO1: %lx %lx, GPIO6: %lx %lx\n", GPIO1_DR, GPIO1_PSR, GPIO6_DR, GPIO6_PSR);
  debug.printf("XBAR CTRL0:%x CTRL1:%x\n\n", XBARA1_CTRL0, XBARA1_CTRL1);
#endif
  _dma_state = DMASTATE_RUNNING;
  _dma_last_completed_frame = nullptr;
  _dma_frame_count = 0;

  // Now start an interrupt for start of frame. 
//  attachInterrupt(_vsyncPin, &frameStartInterrupt, RISING);

  //DebugDigitalToggle(OV7670_DEBUG_PIN_1);
  return true;
}

//===================================================================
// stopReadFrameDMA - stop doing the reading and then exit.
//===================================================================
bool OV2640::stopReadFrameDMA()
{

  // hopefully it start here (fingers crossed)
  // for now will hang here to see if completes...
  #ifdef OV7670_USE_DEBUG_PINS
  //DebugDigitalWrite(OV7670_DEBUG_PIN_2, HIGH);
  #endif
  elapsedMillis em = 0;
  // tell the background stuff DMA stuff to exit.
  // Note: for now let it end on on, later could disable the DMA directly.
  _dma_state = DMASTATE_STOP_REQUESTED;

  while ((em < 1000) && (_dma_state == DMASTATE_STOP_REQUESTED)) ; // wait up to a second...
  if (_dma_state != DMA_STATE_STOPPED) {
    debug.println("*** stopReadFrameDMA DMA did not exit correctly...");
    debug.printf("  Bytes Left: %u frame buffer:%x Row:%u Col:%u\n", _bytes_left_dma, (uint32_t)_frame_buffer_pointer, _frame_row_index, _frame_col_index);
  }
  #ifdef OV7670_USE_DEBUG_PINS
  //DebugDigitalWrite(OV7670_DEBUG_PIN_2, LOW);
  #endif
#ifdef DEBUG_CAMERA
  dumpDMA_TCD_5(&_dmachannel, nullptr);
  dumpDMA_TCD_5(&_dmasettings[0], nullptr);
  dumpDMA_TCD_5(&_dmasettings[1], nullptr);
  debug.println();
#endif
  // Lets restore some hardware pieces back to the way we found them.
#if defined (ARDUINO_TEENSY_MICROMOD)
  IOMUXC_GPR_GPR27 = _save_IOMUXC_GPR_GPR27;  // Restore... away the configuration before we change...
#else
  IOMUXC_GPR_GPR26 = _save_IOMUXC_GPR_GPR26;  // Restore... away the configuration before we change...
#endif
  *(portConfigRegister(_pclkPin)) = _save_pclkPin_portConfigRegister;

  return (em < 1000); // did we stop...
}

//===================================================================
// Our Frame Start interrupt.
//===================================================================
#if 0
void  OV2640::frameStartInterrupt() {
  active_dma_camera->processFrameStartInterrupt();  // lets get back to the main object...
}

void  OV2640::processFrameStartInterrupt() {
  _bytes_left_dma = (_width + _frame_ignore_cols) * _height; // for now assuming color 565 image...
  _dma_index = 0;
  _frame_col_index = 0;  // which column we are in a row
  _frame_row_index = 0;  // which row
  _save_lsb = 0xffff;
  // make sure our DMA is setup properly again. 
  _dmasettings[0].transferCount(DMABUFFER_SIZE);
  _dmasettings[0].TCD->CSR &= ~(DMA_TCD_CSR_DREQ); // Don't disable on this one
  _dmasettings[1].transferCount(DMABUFFER_SIZE);
  _dmasettings[1].TCD->CSR &= ~(DMA_TCD_CSR_DREQ); // Don't disable on this one
  _dmachannel = _dmasettings[0];  // setup the first on...
  _dmachannel.enable();
  
  detachInterrupt(_vsyncPin);
}
#endif

//===================================================================
// Our DMA interrupt.
//===================================================================
void OV2640::dmaInterrupt() {
  active_dma_camera->processDMAInterrupt();  // lets get back to the main object...
}


// This version assumes only called when HREF...  as set pixclk to only fire
// when set.
void OV2640::processDMAInterrupt() {
  _dmachannel.clearInterrupt(); // tell system we processed it.
  asm("DSB");
  #ifdef USE_DEBUG_PINS
  //DebugDigitalWrite(OV7670_DEBUG_PIN_3, HIGH);
  #endif
  
  if (_dma_state == DMA_STATE_STOPPED) {
    debug.println("OV2640::dmaInterrupt called when DMA_STATE_STOPPED");
    return; //
  }


  // lets guess which buffer completed.
  uint32_t *buffer;
  uint16_t buffer_size;
  _dma_index++;
  if (_dma_index & 1) {
    buffer = _dmaBuffer1;
    buffer_size = _dmasettings[0].TCD->CITER;

  } else {
    buffer = _dmaBuffer2;
    buffer_size = _dmasettings[1].TCD->CITER;
  }
  // lets try dumping a little data on 1st 2nd and last buffer.
#ifdef DEBUG_CAMERA_VERBOSE
  if ((_dma_index < 3) || (buffer_size  < DMABUFFER_SIZE)) {
    debug.printf("D(%d, %d, %lu) %u : ", _dma_index, buffer_size, _bytes_left_dma, pixformat);
    for (uint16_t i = 0; i < 8; i++) {
      uint16_t b = buffer[i] >> 4;
      debug.printf(" %lx(%02x)", buffer[i], b);
    }
    debug.print("...");
    for (uint16_t i = buffer_size - 8; i < buffer_size; i++) {
      uint16_t b = buffer[i] >> 4;
      debug.printf(" %lx(%02x)", buffer[i], b);
    }
    debug.println();
  }
#endif

  for (uint16_t buffer_index = 0; buffer_index < buffer_size; buffer_index++) {
    if (!_bytes_left_dma || (_frame_row_index >= _height)) break;

    // only process if href high...
    uint16_t b = *buffer >> 4;
    *_frame_buffer_pointer++ = b;
    _frame_col_index++;
    if (_frame_col_index == _width) {
        // we just finished a row.
        _frame_row_index++;
        _frame_col_index = 0;
    }
    _bytes_left_dma--; // for now assuming color 565 image...
    buffer++;
  }

  if ((_frame_row_index == _height) || (_bytes_left_dma == 0)) { // We finished a frame lets bail
    _dmachannel.disable();  // disable the DMA now...
    #ifdef USE_DEBUG_PINS
    //DebugDigitalWrite(OV7670_DEBUG_PIN_2, LOW);
    #endif
#ifdef DEBUG_CAMERA_VERBOSE
    debug.println("EOF");
#endif
    _frame_row_index = 0;
    _dma_frame_count++;

    bool swap_buffers = true;

    //DebugDigitalToggle(OV7670_DEBUG_PIN_1);
    _dma_last_completed_frame = _frame_row_buffer_pointer;
    if (_callback) swap_buffers = (*_callback)(_dma_last_completed_frame);

    if (swap_buffers) {
        if (_frame_row_buffer_pointer != _frame_buffer_1) _frame_row_buffer_pointer = _frame_buffer_2;
        else _frame_row_buffer_pointer = _frame_buffer_2;    
    }

    _frame_buffer_pointer = _frame_row_buffer_pointer;

    //DebugDigitalToggle(OV7670_DEBUG_PIN_1);


    if (_dma_state == DMASTATE_STOP_REQUESTED) {
#ifdef DEBUG_CAMERA
      debug.println("OV2640::dmaInterrupt - Stop requested");
#endif
      _dma_state = DMA_STATE_STOPPED;
    } else {
      // We need to start up our ISR for the next frame. 
#if 1
  // bypass interrupt and just restart DMA... 
  _bytes_left_dma = (_width + _frame_ignore_cols) * _height; // for now assuming color 565 image...
  _dma_index = 0;
  _frame_col_index = 0;  // which column we are in a row
  _frame_row_index = 0;  // which row
  _save_lsb = 0xffff;
  // make sure our DMA is setup properly again. 
  _dmasettings[0].transferCount(DMABUFFER_SIZE);
  _dmasettings[0].TCD->CSR &= ~(DMA_TCD_CSR_DREQ); // Don't disable on this one
  _dmasettings[1].transferCount(DMABUFFER_SIZE);
  _dmasettings[1].TCD->CSR &= ~(DMA_TCD_CSR_DREQ); // Don't disable on this one
  _dmachannel = _dmasettings[0];  // setup the first on...
  _dmachannel.enable();

#else
      attachInterrupt(_vsyncPin, &frameStartInterrupt, RISING);
#endif
    }
  } else {

    if (_bytes_left_dma == (2 * DMABUFFER_SIZE)) {
      if (_dma_index & 1) _dmasettings[0].disableOnCompletion();
      else _dmasettings[1].disableOnCompletion();
    }

  }
  #ifdef OV7670_USE_DEBUG_PINS
  //DebugDigitalWrite(OV7670_DEBUG_PIN_3, LOW);
  #endif
}

typedef struct {
    uint32_t frameTimeMicros;
    uint16_t vsyncStartCycleCount;
    uint16_t vsyncEndCycleCount;
    uint16_t hrefCount;
    uint32_t cycleCount;
    uint16_t pclkCounts[350]; // room to spare.
    uint32_t hrefStartTime[350];
    uint16_t pclkNoHrefCount;
} frameStatics_t;

frameStatics_t fstatOmni1;

void OV2640::captureFrameStatistics()
{
   memset((void*)&fstatOmni1, 0, sizeof(fstatOmni1));

   // lets wait for the vsync to go high;
    while ((*_vsyncPort & _vsyncMask) != 0); // wait for HIGH
    // now lets wait for it to go low    
    while ((*_vsyncPort & _vsyncMask) == 0) fstatOmni1.vsyncStartCycleCount ++; // wait for LOW

    while ((*_hrefPort & _hrefMask) == 0); // wait for HIGH
    while ((*_pclkPort & _pclkMask) != 0); // wait for LOW

    uint32_t microsStart = micros();
    fstatOmni1.hrefStartTime[0] = microsStart;
    // now loop through until we get the next _vsynd
    // BUGBUG We know that HSYNC and PCLK on same GPIO VSYNC is not...
    uint32_t regs_prev = 0;
    //noInterrupts();
    while ((*_vsyncPort & _vsyncMask) != 0) {

        fstatOmni1.cycleCount++;
        uint32_t regs = (*_hrefPort & (_hrefMask | _pclkMask ));
        if (regs != regs_prev) {
            if ((regs & _hrefMask) && ((regs_prev & _hrefMask) ==0)) {
                fstatOmni1.hrefCount++;
                fstatOmni1.hrefStartTime[fstatOmni1.hrefCount] = micros();
            }
            if ((regs & _pclkMask) && ((regs_prev & _pclkMask) ==0)) fstatOmni1.pclkCounts[fstatOmni1.hrefCount]++;
            if ((regs & _pclkMask) && ((regs_prev & _hrefMask) ==0)) fstatOmni1.pclkNoHrefCount++;
            regs_prev = regs;
        }
    }
    while ((*_vsyncPort & _vsyncMask) == 0) fstatOmni1.vsyncEndCycleCount++; // wait for LOW
    //interrupts();
    fstatOmni1.frameTimeMicros = micros() - microsStart;

    // Maybe return data. print now
    debug.printf("*** Frame Capture Data: elapsed Micros: %u loops: %u\n", fstatOmni1.frameTimeMicros, fstatOmni1.cycleCount);
    debug.printf("   VSync Loops Start: %u end: %u\n", fstatOmni1.vsyncStartCycleCount, fstatOmni1.vsyncEndCycleCount);
    debug.printf("   href count: %u pclk ! href count: %u\n    ", fstatOmni1.hrefCount,  fstatOmni1.pclkNoHrefCount);
    for (uint16_t ii=0; ii < fstatOmni1.hrefCount + 1; ii++) {
        debug.printf("%3u(%u) ", fstatOmni1.pclkCounts[ii], (ii==0)? 0 : fstatOmni1.hrefStartTime[ii] - fstatOmni1.hrefStartTime[ii-1]);
        if (!(ii & 0x0f)) debug.print("\n    ");
    }
    debug.println();
}

/*****************************************************************/

