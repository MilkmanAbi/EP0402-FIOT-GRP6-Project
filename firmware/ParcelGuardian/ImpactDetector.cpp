#include "ImpactDetector.h"
#include "Config.h"
#include <math.h>

ImpactDetector::ImpactDetector()
  : accel_(NULL), baseline_(1.0f), lastDelta_(0.0f), peakDelta_(0.0f),
    eventCount_(0), roughCount_(0), severeCount_(0),
    lastSampleMs_(0), debounceUntilMs_(0),
    calibrated_(false), moving_(false) {}

void ImpactDetector::begin(Adxl345 *accel) {
  accel_ = accel;
}

bool ImpactDetector::calibrate(uint16_t samples) {
  if (!accel_ || !accel_->present() || samples == 0) return false;

  float sum = 0.0f;
  uint16_t good = 0;

  for (uint16_t i = 0; i < samples; i++) {
    float mag;
    if (accel_->readMagnitude(mag)) { sum += mag; good++; }
    delay(5);
  }

  /* Demand most of the samples — a handful of I2C reads succeeding is not
   * a baseline worth trusting. */
  if (good < (samples / 2)) return false;

  baseline_   = sum / good;
  calibrated_ = true;
  lastDelta_  = 0.0f;
  peakDelta_  = 0.0f;
  return true;
}

ImpactLevel ImpactDetector::update() {
  if (!accel_ || !accel_->present()) return IMPACT_NONE;

  uint32_t now = millis();
  if ((uint32_t)(now - lastSampleMs_) < ACCEL_PERIOD_MS) return IMPACT_NONE;
  lastSampleMs_ = now;

  float mag;
  if (!accel_->readMagnitude(mag)) return IMPACT_NONE;

  float delta = fabs(mag - baseline_);
  lastDelta_  = delta;
  if (delta > peakDelta_) peakDelta_ = delta;

  moving_ = (delta >= MOTION_G);

  /* Still settling from the previous hit — measure, but do not re-fire. */
  if ((int32_t)(now - debounceUntilMs_) < 0) return IMPACT_NONE;

  ImpactLevel level = IMPACT_NONE;
  if      (delta >= IMPACT_SEVERE_G) level = IMPACT_SEVERE;
  else if (delta >= IMPACT_ROUGH_G)  level = IMPACT_ROUGH;

  if (level != IMPACT_NONE) {
    eventCount_++;
    if (level == IMPACT_SEVERE) severeCount_++; else roughCount_++;
    debounceUntilMs_ = now + IMPACT_DEBOUNCE_MS;
  }
  return level;
}
