/*
 * ImpactDetector.h — turns a stream of accelerometer samples into
 * timestamped mishandling events.
 *
 * The algorithm, in full:
 *
 *   1. Sample x, y, z from the ADXL345 at 20 Hz.
 *   2. magnitude = sqrt(x^2 + y^2 + z^2)          -- orientation independent
 *   3. delta     = | magnitude - baseline |       -- baseline is ~1 g at rest
 *   4. Classify delta against the severity tiers in Config.h.
 *   5. Hold a debounce window before re-arming, so one drop logs once
 *      instead of forty times as the box settles.
 *
 * Why magnitude and not per-axis: a parcel's orientation in a truck is
 * unknown and changes constantly, so a per-axis threshold would miss a
 * drop that lands on the "wrong" face. The vector magnitude does not care
 * which way up the box is.
 *
 * Thresholds are unvalidated defaults until measured with the actual
 * parcel. This is impact thresholding, not validated fall/damage detection.
 */
#ifndef PG_IMPACT_DETECTOR_H
#define PG_IMPACT_DETECTOR_H

#include <Arduino.h>
#include "Adxl345.h"

enum ImpactLevel : uint8_t {
  IMPACT_NONE   = 0,
  IMPACT_ROUGH  = 1,
  IMPACT_SEVERE = 2
};

class ImpactDetector {
public:
  ImpactDetector();

  void begin(Adxl345 *accel);

  /* Average N samples while the box is stationary to establish the
   * at-rest magnitude. Call this only when the unit is genuinely still. */
  bool calibrate(uint16_t samples);

  /* Call as often as you like; it self-limits to ACCEL_PERIOD_MS.
   * Returns the level of a NEWLY DETECTED event, or IMPACT_NONE. */
  ImpactLevel update();

  float    lastDeltaG()  const { return lastDelta_; }
  float    peakDeltaG()  const { return peakDelta_; }
  void     clearPeak()         { peakDelta_ = 0.0f; }
  float    baseline()    const { return baseline_; }
  bool     calibrated()  const { return calibrated_; }

  /* True while the parcel is being handled at all (above MOTION_G but
   * not necessarily an impact). Used to promote PREP -> TRANSIT. */
  bool     moving()      const { return moving_; }

  uint32_t eventCount()  const { return eventCount_; }
  void     setEventCount(uint32_t n) { eventCount_ = n; }

  uint32_t roughCount()  const { return roughCount_; }
  uint32_t severeCount() const { return severeCount_; }

private:
  Adxl345 *accel_;
  float    baseline_;
  float    lastDelta_;
  float    peakDelta_;
  uint32_t eventCount_;
  uint32_t roughCount_;
  uint32_t severeCount_;
  uint32_t lastSampleMs_;
  uint32_t debounceUntilMs_;
  bool     calibrated_;
  bool     moving_;
};

#endif /* PG_IMPACT_DETECTOR_H */
