# Threshold calibration

The firmware ships with `IMPACT_ROUGH_G = 0.40` and `IMPACT_SEVERE_G = 1.20` in `Config.h`.

**Those are starting points, not measurements.** This document is where they become measurements. The difference matters: "we picked some numbers that felt right" and "we calibrated it against a drop test" are two very different answers to the same Q&A question.

## Method

1. Upload `firmware/tools/AdxlDropTest`. Serial Monitor at 9600.
2. Let it calibrate — hold the board perfectly still for two seconds.
3. **Put the board inside the parcel you will actually demo with.** Padding, box stiffness and where the unit sits all change the numbers. Calibrating a bare PCB and then demoing it inside a padded box will give you thresholds that never fire.
4. For each test below, do **five** repetitions and take the **median** — single drops scatter badly.
5. Send `r` between drops to reset the peak.

## Record your results

### Normal handling — these must stay BELOW your rough threshold

| Action | Peak Δg (5 reps) | Median |
|---|---|---|
| Carrying the parcel while walking | | |
| Setting it down normally on a desk | | |
| Firm shake | | |
| Sliding it across a table | | |
| Car / bus ride, if you can | | |

### Impacts — these set the tiers

| Drop height (onto a folded towel) | Peak Δg (5 reps) | Median |
|---|---|---|
| 10 cm | | |
| 30 cm | | |
| 60 cm | | |

## Choosing the thresholds

- **`IMPACT_ROUGH_G`** → just *above* the highest normal-handling peak. If a brisk walk trips it, you will log dozens of false events and the evidence becomes noise.
- **`IMPACT_SEVERE_G`** → just *below* your 30 cm median. A 30 cm drop onto a hard floor is a genuine mishandling event; a 10 cm set-down is not.

Leave a margin. Thresholds sitting exactly on a measured median will fire about half the time, which is the worst of both worlds.

## Final values used

| Constant | Default | **Our measured value** | Date | Who |
|---|---|---|---|---|
| `IMPACT_ROUGH_G` | 0.40 | | | |
| `IMPACT_SEVERE_G` | 1.20 | | | |
| `MOTION_G` | 0.12 | | | |
| `IMPACT_DEBOUNCE_MS` | 2000 | | | |

Update `Config.h` once these are filled in, then re-run the normal-handling tests to confirm you get **zero** false positives.

## A note on what we can claim

This procedure validates **impact detection**: the device reliably distinguishes a drop from normal handling, with thresholds derived from measurement.

It does **not** validate fall detection, free-fall detection, or damage prediction. We do not know that 1.2 g breaks a given item — only that the parcel was subjected to it. Say exactly that, and no more.
