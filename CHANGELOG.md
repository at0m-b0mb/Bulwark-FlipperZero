# Changelog

## v1.0 — 2026-08-12

First release.

### The detector
- Listens on the three Bluetooth LE advertising channels (2402, 2426 and
  2480 MHz) and on the three Wi-Fi channel centres between them, using the
  Flipper's own radio in RF test mode. No extra hardware.
- Seven signals across four independent families — occupancy, shape, level and
  persistence — scored to a verdict of CLEAR, BUSY BAND, SUSPECT or
  SPAM LIKELY.
- Nine ceilings, each one a refusal to overclaim. SPAM LIKELY requires the
  tri-channel signature specifically; nothing ever reaches 100, because
  Bulwark cannot read a packet.
- Wi-Fi is handled with two ceilings rather than one, because "as busy as" and
  "twice as busy as" are different amounts of blindness. Interference is
  reported separately from the score.
- A stored baseline, so Bulwark knows what your own places sound like. Without
  one every verdict is capped at 88 and says why.

### The app
- Watch screen with two pages: the band at its real frequency spacing, and the
  same story over time, one column per sweep.
- Breakdown screen that leads with the ceiling that held the score down, then
  every signal and what it was worth, then the raw numbers, then a list of
  what was *not* measured.
- Seven animated panels on how the attack works and how to turn the popups off.
- Alerts for a Flipper you are not looking at, CSV sweep logs, and seven
  synthetic bands for use without hardware — including a beacon farm that
  Bulwark gets wrong on purpose, because that is the honest limit.

### Verified
- 8.3 million host checks over the measurement and scoring engine, including
  every ceiling as an invariant over randomised inputs.
- Builds clean on the release (API 87.1) and dev (API 88.0) SDK channels.
