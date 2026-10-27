# Prototype: monotonic overall gain (issue #110)

Branch: `gain-monotonic-110`. Prototype for discussion — not for merge as-is.

## What it does

Issue #110: `SoapySDRDevice_setGain(dev, dir, chan, value)` (the aggregate
"overall amplification" overload) is not monotonically non-decreasing on this
driver. The two gain elements are gain **reductions** — `IFGR` (`gRdB`, 20–59 dB)
and `RFGR` (`LNAstate`, an index into a per-band RF gain-reduction table) — so
the inherited base-class aggregate runs backwards: a larger overall value
produces *less* hardware gain.

This prototype overrides the three aggregate overloads (`setGain`, `getGain`,
`getGainRange` — the ones **without** a gain-element name) so overall gain is a
single value in dB:

```
overall_gain(dB) = -(gRdB + rfGR[LNAstate])
```

Larger value ⇒ more hardware gain, always. `setGain` snaps to the nearest
achievable setting and clamps to `getGainRange` (issue #110 bonus points 1 & 2).

The RF table is **not** monotonic in `LNAstate` (e.g. RSP2 1–2 GHz
`{0,5,21,15,15,34}`), so the `LNAstate` for a requested gain is chosen by a
linear nearest search, not bisection — mirroring gr-sdrplay3.

## Legacy behavior is preserved

- The named `IFGR` / `RFGR` overloads are **unchanged**: they still set/return
  `gRdB` and the `LNAstate` index exactly as before.
- The new aggregate behavior is **opt-in**. Default is the old (base-class)
  aggregate, so nothing changes unless you ask for it.

Enable via the device string or `writeSetting`:

```
SoapySDRDevice(driver=sdrplay, monotonic_gain=true)
# or: dev->writeSetting("monotonic_gain", "true")
```

`readSetting("monotonic_gain")` and `getSettingInfo` expose it too.

## Open design questions (for Oliver / for review)

1. **Default.** Shipped here as opt-in (default = legacy) to guarantee no
   regression. If the goal is "sdrplay shouldn't need special-casing," the
   default should flip to monotonic — that's a one-line change, and the real
   decision to make.
2. **IF/RF split policy.** For a requested overall gain, the prototype picks the
   `(LNAstate, gRdB)` nearest the target, tie-breaking toward a mid-range `gRdB`.
   A different allocation (e.g. maximize front-end LNA gain for noise figure, or
   follow SDRplay's own AGC schedule) may be preferable; the monotonic contract
   holds regardless.
3. **AGC on.** With AGC enabled, `gRdB` is hardware-controlled; the aggregate
   `setGain` then only moves the LNA and logs a warning.

## Data source

RF gain-reduction tables are ported from gr-sdrplay3 (SDRplay API spec v3.15,
section 5). RSPdx-R2 reuses the RSPdx tables as a prototype approximation.

## Validation

Builds clean (no warnings) against SDRplay API 3.15. A standalone replay of the
selection math confirms `setGain(x)` → actual gain is monotonically
non-decreasing across a full sweep for every ported table, including the
non-monotonic RSP2 1–2 GHz band. Not yet exercised on hardware — needs a run on
a real RSP (RSPduo available for testing).
