# Trig Crest (exact vs s16-safe)

Two programs from one source. They show why sin/cos return exact
±1.0 in s15.16 by default, and when to compile with `FR_TRIG_S16_SAFE`.

| Program | Compile flag | Crest (s15.16) | `(s16)(x >> 1)` at peak |
|---------|--------------|----------------|-------------------------|
| `trig_crest_exact` | *(none)* | ±65536 | wraps to −32768 |
| `trig_crest_s16_safe` | `-DFR_TRIG_S16_SAFE` | ±65534 | ±32767 |

Both still return **s15.16** — this is an s16-safe crest, not a q1.15 API.
Leave the flag off unless a caller blind-casts the shifted value into `s16`.

## Build / run

```bash
make          # both binaries
make run      # print both demos
make clean
```

From the repo root: `make examples` / `make run-examples` includes these.
