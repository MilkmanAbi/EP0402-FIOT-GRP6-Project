/*
 * SdLogger.h — the black box.
 *
 * This card is the SYSTEM OF RECORD. The cloud is a cache of whatever
 * happened to get through. A parcel spends almost its whole journey out
 * of WiFi range, so if the cloud were authoritative the product would
 * fail to observe the exact window it exists to observe.
 *
 * Every row is opened, written and closed immediately. That costs a few
 * milliseconds per row but means a power cut loses at most the row in
 * flight, which matters because power cuts and dropped parcels tend to
 * happen at the same moment.
 *
 * Rows are streamed field by field straight into the File object rather
 * than composed in a RAM buffer first — on a 2 KB part a 96-byte line
 * buffer is a luxury we would rather spend elsewhere.
 */
#ifndef PG_SD_LOGGER_H
#define PG_SD_LOGGER_H

#include <Arduino.h>

bool sdBegin();
bool sdOk();

/* Append one record. Returns false if the card is absent or the write
 * failed; the caller carries on regardless — a missing card degrades the
 * product, it does not stop it. */
bool sdLogRow(const char *utc, const char *state, const char *event,
              float deltaG, float peakG, uint32_t eventCount,
              bool hasFix, double lat, double lon, uint8_t sats,
              bool uploaded);

/* Write a session banner so one file can hold several journeys. */
bool sdLogBoot(uint16_t bootCount, uint16_t shipmentId);

/* Stream the whole log to a Stream (the serial console) — this is the
 * handover read-out the pitch deck calls the chain-of-custody report. */
bool sdDump(Stream &out);

/* Delete the log. Asked for explicitly from the console only. */
bool sdWipe();

uint32_t sdRowsWritten();
uint32_t sdWriteErrors();

#endif /* PG_SD_LOGGER_H */
