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
void Camera::readFrame(void *buffer, bool fUseDMA) {
  sensor->readFrame(buffer, fUseDMA);
}

//normal Read mode
void Camera::readFrameGPIO(void *buffer) {
  return sensor->readFrameGPIO(buffer);
}

void Camera::readFrame4BitGPIO(void *buffer) {
  return sensor->readFrame4BitGPIO(buffer);
}

bool Camera::readContinuous(bool (*callback)(void *frame_buffer), void *fb1, void *fb2) {
  return sensor->readContinuous(callback, fb1, fb2);
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

void Camera::readFrameFlexIO(void *buffer, bool fUseDMA) {
  return sensor->readFrameFlexIO(buffer, fUseDMA);
}

bool Camera::startReadFlexIO(bool (*callback)(void *frame_buffer), void *fb1, void *fb2) {
  return sensor->startReadFlexIO(callback, fb1, fb1);
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
    sensor->setExposure(enable);
}

bool Camera::begin_omnivision(framesize_t resolution, pixformat_t format, int fps, bool use_gpio) { // Supported FPS: 1, 5, 10, 15, 30
    return sensor->begin_omnivision(resolution, format, fps, use_gpio);
}