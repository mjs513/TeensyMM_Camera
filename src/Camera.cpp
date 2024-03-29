#include "camera.h"

Camera::Camera(ImageSensor &sensor)
  : sensor(&sensor) {
}

void Camera::setPins(uint8_t mclk_pin, uint8_t pclk_pin, uint8_t vsync_pin, uint8_t hsync_pin, uint8_t en_pin,
                     uint8_t g0, uint8_t g1, uint8_t g2, uint8_t g3, uint8_t g4, uint8_t g5, uint8_t g6, uint8_t g7, TwoWire &wire) {
  sensor->setPins(mclk_pin, pclk_pin, vsync_pin, hsync_pin, en_pin, g0, g1, g2, g3, g4, g5, g6, g7, wire);
}


bool Camera::begin(framesize_t framesize, int framerate, bool use_gpio) {

  return sensor->begin(framesize, framerate, use_gpio);
}

void Camera::end() {
    sensor->end();
}

void Camera::showRegisters(void) {
  sensor->showRegisters();
};

int Camera::setPixformat(pixformat_t pfmt) {
  return sensor->setPixformat(pfmt);
}

void Camera::debug(bool debug_on) {
  sensor->debug(debug_on);
}
bool Camera::debug() {
  return sensor->debug();
}

uint8_t Camera::readRegister(uint8_t reg) {
  return sensor->readRegister(reg);
}
bool Camera::writeRegister(uint8_t reg, uint8_t data) {
  return sensor->writeRegister(reg, data);
}

uint8_t Camera::setFramesize(framesize_t framesize) {
  return sensor->setFramesize(framesize);
}

int Camera::setFramerate(int framerate) {
  return sensor->setFramerate(framerate);
}

int Camera::setBrightness(int level) {
  return sensor->setBrightness(level);
}

int Camera::setGainceiling(gainceiling_t gainceiling) {
  return sensor->setGainceiling(gainceiling);
}

int Camera::setColorbar(int enable) {
  return sensor->setColorbar(enable);
}

int Camera::setAutoGain(int enable, float gain_db, float gain_db_ceiling) {
  return sensor->setAutoGain(enable, gain_db, gain_db_ceiling);
}

int Camera::get_vt_pix_clk(uint32_t *vt_pix_clk) {
  return sensor->get_vt_pix_clk(vt_pix_clk);
}

int Camera::getGain_db(float *gain_db) {
  return sensor->getGain_db(gain_db);
}

int Camera::getCameraClock(uint32_t *vt_pix_clk) {
  return sensor->getCameraClock(vt_pix_clk);
}

int Camera::setAutoExposure(int enable, int exposure_us) {
  return sensor->setAutoExposure(enable, exposure_us);
}

int Camera::getExposure_us(int *exposure_us) {
  return sensor->getExposure_us(exposure_us);
}

int Camera::setHmirror(int enable) {
  return sensor->setHmirror(enable);
}

int Camera::setVflip(int enable) {
  return sensor->setVflip(enable);
}

uint8_t Camera::setMode(uint8_t Mode, uint8_t FrameCnt) {
  return sensor->setMode(Mode, FrameCnt);
}

uint8_t Camera::cmdUpdate() {
  return sensor->cmdUpdate();
}

uint8_t Camera::loadSettings(camera_reg_settings_t settings) {
  return sensor->loadSettings(settings);
}


uint16_t Camera::getModelid() {
  return sensor->getModelid();
}

uint8_t Camera::getAE(ae_cfg_t *psAECfg) {
  return sensor->getAE(psAECfg);
}


uint8_t Camera::calAE(uint8_t CalFrames, uint8_t *Buffer, uint32_t ui32BufferLen, ae_cfg_t *pAECfg) {
  return sensor->calAE(CalFrames, Buffer, ui32BufferLen, pAECfg);
}

//-------------------------------------------------------
//Generic Read Frame base on _hw_config
bool Camera::readFrame(void *buffer1, size_t cb1, void *buffer2, size_t cb2) {
  return sensor->readFrame(buffer1, cb1, buffer2, cb2);
}


void Camera::useDMA(bool f) {
  sensor->useDMA(f);
}

bool Camera::useDMA() {
  return sensor->useDMA();
}


//normal Read mode
bool Camera::readFrameGPIO(void* buffer, size_t cb1, void* buffer2, size_t cb2) {
    return sensor->readFrameGPIO(buffer, cb1, buffer2, cb2);
}

void Camera::readFrame4BitGPIO(void *buffer) {
  return sensor->readFrame4BitGPIO(buffer);
}

bool Camera::readContinuous(bool (*callback)(void *frame_buffer), void *fb1, size_t cb1, void *fb2, size_t cb2) {
  return sensor->readContinuous(callback, fb1, cb1, fb2, cb2);
}

void Camera::stopReadContinuous() {
  return sensor->stopReadContinuous();
}

//FlexIO is default mode for the camera
/*
void Camera::readFrameFlexIO(void* buffer)
{
    return sensor->readFrameFlexIO(buffer);
}
*/

bool Camera::readFrameFlexIO(void *buffer, size_t cb1, void* buffer2, size_t cb2) {
  return sensor->readFrameFlexIO(buffer, cb1, buffer2, cb2);
}

bool Camera::startReadFlexIO(bool (*callback)(void *frame_buffer), void *fb1, size_t cb1, void *fb2, size_t cb2) {
  return sensor->startReadFlexIO(callback, fb1, cb1, fb1, cb2);
}

bool Camera::stopReadFlexIO() {
  return sensor->stopReadFlexIO();
}

// Lets try a dma version.  Doing one DMA that is synchronous does not gain anything
// So lets have a start, stop... Have it allocate 2 frame buffers and it's own DMA
// buffers, with the option of setting your own buffers if desired.

bool Camera::startReadFrameDMA(bool (*callback)(void *frame_buffer), uint8_t *fb1, uint8_t *fb2) {
  return sensor->startReadFrameDMA(callback, fb1, fb2);
}

bool Camera::stopReadFrameDMA() {
  return sensor->stopReadFrameDMA();
}

void Camera::captureFrameStatistics() {
  return sensor->captureFrameStatistics();
}

void Camera::setVSyncISRPriority(uint8_t priority) {
  sensor->setVSyncISRPriority(priority);
}

void Camera::setDMACompleteISRPriority(uint8_t priority) {
  sensor->setDMACompleteISRPriority(priority);
}

int16_t Camera::width(void) {
  return sensor->width();
}

int16_t Camera::height(void) {
  return sensor->height();
}

int16_t Camera::mode(void) {
  return sensor->_hw_config;
}

uint32_t Camera::frameCount()  //{return _dma_frame_count;}
{
  return sensor->frameCount();
}

/********* OV Supported cameras ********************/
void Camera::setSaturation(int saturation) { // 0 - 255
    sensor->setSaturation(saturation);
}

void Camera::setHue(int hue) {
    sensor->setHue(hue);
}

void Camera::setContrast(int contrast) {
    sensor->setContrast(contrast);
}

void Camera::setGain(int gain) {
    sensor->setGain(gain);
}

void Camera::autoGain(int enable, float gain_db, float gain_db_ceiling) {
    sensor->autoGain(enable, gain_db, gain_db_ceiling);
}

void Camera::setExposure(int exposure) {
    sensor->setExposure(exposure);
}

void Camera::autoExposure(int enable) {
    sensor->autoExposure(enable);
}

bool Camera::begin(framesize_t resolution, pixformat_t format, int fps, int camera_name, bool use_gpio) { // Supported FPS: 1, 5, 10, 15, 30
    return sensor->begin_omnivision(resolution, format, fps, camera_name, use_gpio);
}

int Camera::setAutoWhitebal(int enable, float r_gain_db, float g_gain_db, float b_gain_db) {
    return sensor->setAutoWhitebal(enable, r_gain_db, g_gain_db, b_gain_db);
}
