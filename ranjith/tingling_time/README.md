# Tingling Time

A pen on a tablet strikes bells and drones, and every strike pulls a moment from the last seconds
of a camera back into the picture. Here the camera looks half at the tablet and half at the screen,
so the piece watches itself being played.

[![▶ Tingling Time](https://markdown-videos-api.jorgenkh.no/youtube/8GmkjOVUtTA?width=1920&height=1080)](https://youtu.be/8GmkjOVUtTA)

Run it by including `src/Piece.hpp` and calling `TinglingTime::tingling_time()` from
`user_project.hpp`. **It needs a pen tablet and a camera, and it will not start without a
tablet.** Stereo output is recommended, because the voices travel between the two ears.

When it starts, a window opens and two pickers follow: first the camera (and its mode), then the
pen.

## What you hear

Eight voices share the piece, alternating between two kinds.

**Drones.** Four sustained voices, each a stack of nine slightly detuned sine partials. They swell
in slowly when they enter and drift in pitch, detune and balance, so they never sit still. Every
drone crosses from one ear to the other and back, each on a journey of its own length.

**Bells.** Four struck voices, each a ten mode inharmonic body whose modes are coupled, so a
strike rings as a cluster rather than a single pitch. Each bell has a pitch of its own, between
about 400 and 1400 Hz, and a decay of one to four seconds. A bell that has just been struck rests
for a moment before it can be struck again.

**The written drone.** A fourteen partial body that is never struck. Its partials rise and fall at
unrelated slow rates, its upper partials fade at a drifting speed, and a second slow motion
detunes them. It joins only in the two fuller modes below.

**Rules.** In the fuller modes, three very slow sines strike the bells by simple rules. Two of the
three high strikes the first bell. An odd number high strikes the second bell and climbs a five
note scale. A window between two of them strikes the third, and the first two compared strike the
fourth. Another slow sine sends the first and third bells to one ear and the others to the other,
and back.

The piece opens sparse and fills as it goes: bells rest several seconds at first and less later,
and bursts of strikes become denser. The arc is written in time and sped up by a constant, see
Tweaking.

## What you see

The camera is recorded into a ring of its most recent frames, about twenty seconds at thirty
frames per second. The live picture is always there. Each bell owns one more layer of the picture.

When a bell is struck, its layer cuts back into the recording, between about half a second for a
light touch and five seconds for a hard one. A region of the picture, shaped by noise and bent
by the live brightness, turns into that moment, with a darkened seam along its edge. The region
grows with the strike and then recedes over a couple of seconds.

The pen decides how fast the past plays. When your hand is still, the recalled moments move
slowly; when it moves, they speed up.

## Playing

The pen is the whole instrument. A touch strikes the next bell. A faster or harder hand lifts
the drones, and a hard press accents them. Every fourth touch lets another voice in, so the
piece grows as you play. Voices arrive one at a time, and the first drone and the first bell are
already there when you begin.

**Keys N1 to N4** choose how the piece answers the pen.

| Key | Mode       | What changes                                                                                                           |
| --- | ---------- | ---------------------------------------------------------------------------------------------------------------------- |
| N1  | TAP        | The pen strikes the bells alone. Drones hold a fixed level. A bell struck by the pen is nudged to a new pitch.         |
| N2  | TAP_MOTION | The pen's motion also shapes the strikes and lifts the drones. A moving pen strikes bells on its own.                  |
| N3  | RESONANT   | The bells are also set ringing by a drone. The drones wander, the rules strike the bells, and the written drone joins. |
| N4  | FULL       | Everything in RESONANT, and every touch sets off a burst of strikes at falling pitch.                                  |

The window is named `pen`.

## Files

| File                | What is in it                                                                        |
| ------------------- | ------------------------------------------------------------------------------------ |
| `Piece.hpp/.cpp`    | `TinglingTime::tingling_time()`, what a touch, a frame of pen motion and the keys do |
| `TinglingTime.hpp`  | The constants that shape the piece, and the voice and mode types                     |
| `Voices.hpp/.cpp`   | The drones, the bells, the rules that strike them, and the written drone             |
| `Senses.hpp/.cpp`   | The pen, smoothed, and the camera picture                                            |
| `shaders/echo.frag` | The picture: how recalled moments replace regions of the live camera                 |

## Tweaking

Everything below is a constant at the top of `TinglingTime.hpp` unless noted. Change it and
rebuild.

### Time

| Constant                             | Effect                                                                                                    |
| ------------------------------------ | --------------------------------------------------------------------------------------------------------- |
| `k_pace`                             | How much faster the piece unfolds than it is written. 4 is a short take, 1 is the full length performance |
| `k_open_seconds`, `k_settle_seconds` | When the opening ends and when the piece has fully settled, in written time before `k_pace` is applied    |

### The voices

| Constant           | Effect                                                                   |
| ------------------ | ------------------------------------------------------------------------ |
| `k_bank_size`      | Number of voices. Half are drones and half are bells                     |
| `k_modes`          | Number of coupled modes in each bell                                     |
| `k_energy_cap`     | How loud the bells may be together before they are quieted               |
| `k_hard_press`     | Pen pressure, 0 to 1, that counts as a hard press and accents the drones |
| `k_swell_interval` | Seconds between steps of a drone's slow swell on entering                |

In `Voices.cpp`, the bell pitch range is the `exp_map` of 400 to 1400 Hz in `Bank::build`, the
drone range is 200 to 500 Hz, and each voice's journey length is `journey_seconds`. The three
rule sines are the first three `slow(...)` calls in `Garden::build`, and the five note scale is
`scale` there. The written drone's pitch and mode count are at the top of `Written::build`.

### Reading the pen

| Constant                                         | Effect                                                                                      |
| ------------------------------------------------ | ------------------------------------------------------------------------------------------- |
| `k_pen_event_rate`                               | The rate, in events per second, the tablet is expected to report. Pen speed is scaled by it |
| `k_motion_travel_gain`, `k_motion_pressure_gain` | How much distance travelled and pressure each add to the pen's motion                       |
| `k_motion_follow`                                | How quickly the motion follows the pen, 0 to 1 per frame. Lower is smoother                 |

### The picture

| Constant                                  | Effect                                                             |
| ----------------------------------------- | ------------------------------------------------------------------ |
| `k_eye_width`, `k_eye_height`             | Size of the recorded camera frames                                 |
| `k_eye_ring`                              | Frames kept in the recording. More means a longer past             |
| `k_eye_step`                              | Seconds between updates of the picture's parameters                |
| `k_cut_near`, `k_cut_far`                 | Seconds back that a light and a hard strike cut into the recording |
| `k_strike_tau`                            | Seconds for a recalled moment to fade                              |
| `k_lag_speed_still`, `k_lag_speed_moving` | How fast the past plays when the pen is still and when it moves    |
| `k_motion_ref`                            | The pen motion at which the past plays at its moving speed         |

The look of the regions, the noise that shapes them, the darkened seam and how recalled moments
are pushed along the noise are all in `shaders/echo.frag`.
