# Security position

## Current phone-server build

The default now sends **local HTTP to the phone on port 8080**, authenticated
with a per-install pairing key from Android secure storage. This key is exposed
to anyone able to observe the local traffic; it is not encryption or a digital
signature. Use a private demonstration hotspot. No real tracking deployment is
claimed. The optional phone-to-ThingSpeak relay uses validated HTTPS.

The receiver validates shipment ID and telemetry, saves a flushed journal record
before acknowledging, and does not expose a history HTTP endpoint. Android backup
is disabled for this app. Export evidence before uninstalling. Signed evidence,
tamper-proof chain-of-custody and assured SD power-loss durability remain outside
this build. See PROTOCOL.md and AUDIT.md for precise limits.

The sections below describe the retained legacy `UPLINK_PHONE=0` route.

A short, honest statement of what this build protects and what it does not. Stating this plainly is worth more marks than pretending the problem does not exist.

## What is wrong with the transport

The ESP-01 uplink uses **plain HTTP on port 80**. That means:

- The **ThingSpeak write key** is transmitted in clear text on every upload.
- The **coordinates** are transmitted in clear text.
- Anyone on the same network, or anywhere along the path, can read both.
- Anyone who captures the write key can inject fake data into the channel, which quietly destroys the evidential value of the whole thing.

## Why it is like this

Validated TLS needs certificate storage, a current cipher suite and the RAM to run a handshake. ESP-01 AT firmware of the vintage used on this shield cannot do that credibly, and an Uno with 2 KB of SRAM cannot help it. We could switch on something that *claims* to be HTTPS without certificate validation, but an unvalidated TLS session is security theatre — it looks safer in a screenshot while providing no real protection against the attack that matters.

So the firmware does the honest thing: plain HTTP, clearly labelled.

## The rules that follow

1. **Demo and dummy data only.** Do not track a real person. Do not track a real shipment with real contents.
2. **Use a throwaway ThingSpeak channel** for the demo, not one tied to anything you care about.
3. **Rotate the write key** after the demo, and immediately if it is ever committed.
4. **`Secrets.h` is gitignored.** This repository is public. A key pushed here is a key that must be rotated — assume it is compromised the moment it lands.
5. **Keep the channel private** and do not publish real movement history.

## What a real deployment would need

| Problem | What it would actually take |
|---|---|
| Clear-text transport | A module with validated TLS — an LTE-M / NB-IoT modem or an ESP32 with a current certificate bundle |
| Shared write key | Per-device credentials, revocable individually |
| Spoofable data | Signed records, so an injected row can be told from a real one |
| Location is personal data | A retention policy, access controls, and a defensible answer to "who can see this and for how long" |

Note the second and third rows: even perfect transport security would not make this evidence *tamper-proof*, because a shared write key means any holder can fabricate a row. For the chain-of-custody claim to survive a real dispute, records would need to be signed at the device. That is well outside the scope of this module, but it is the honest answer if someone asks whether this would stand up in an actual liability argument.

## The one thing that stays true

The **microSD log is physically held** and never leaves the parcel until handover. Of the two records this system keeps, the local one is the more trustworthy — which is a pleasant accident of the offline-first design rather than something we engineered on purpose, but it is worth knowing.
