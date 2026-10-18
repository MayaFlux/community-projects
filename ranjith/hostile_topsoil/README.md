# Topsoil

A machine of pure tones, clicks and noise whose score is written by cellular automata, over a
ringing drone, with a living picture that follows it. Sound and picture share one set of rules
and one set of triggers, and the picture answers them late and in its own way.

[![▶ A Hostile Topsoil](https://markdown-videos-api.jorgenkh.no/youtube/LdvbbOaLIfA?width=1920&height=1080)](https://youtu.be/LdvbbOaLIfA)

Run it by including `src/Piece.hpp` and calling `Topsoil::topsoil()` from
`user_project.hpp`. It needs a window and stereo output. No tablet, no camera.

## What you hear

The piece arrives in layers, each on a clock of its own.

**The page.** Four parts play a 40 second phrase that repeats: a sub pulse (62 Hz), a tick (6 kHz
click), a gate (2 kHz blip) and a burst of noise. The rows are not written by hand. Most bars are
one generation of an elementary cellular automaton, and the generation count grows from bar to
bar, so the pattern slowly unfolds, then runs backwards near the end. The chapters change the
rule: rule 90 for the ticks, then rule 30, then rule 90 reversed; rule 110 and rule 90 for the
gate; rule 22 for the noise. Four hard cuts silence everything from a fixed step of the fourth, eighth and
twelfth bars, and the whole of the last bar, so the phrase breaks off and restarts. Every part starts from the
same instant and nothing is random, so the page is the same every time.

**The drone.** Five voices (A3, E4, A4, E5, E6) are click trains pushed through resonators. Each
click rings as a short damped sine, so the voice is a stream of tight grains. The click rates
share no common divisor, so the grains of the five voices interlock and never line up, and each
rate wanders slowly. The voices enter late, one after another (about 6, 9.5, 15, 21 and 27
seconds in at the default pace), swell in and out on prime length periods, and the whole bank
glides through a repeating chord sequence. The drone ignores the score and its cuts. It is not
rhythmic.

**The tape.** The drone is recorded into a 60 second ring, and 36 taps replay it at inharmonic
ratios. Three groups of six layers enter at 8, 16 and 26 seconds. Two groups morph slowly between
the harmonic series and the partials of a free bar, and one sits on a golden ratio ladder. Each
layer fades in and out with where it sits in its window of the past, so nothing clicks. Together
the layers make a slowly shifting cloud of partials made of tape.

## What you see

A grid of 3840 by 2160 cells, drawn as one point per cell, on a torus. Each cell holds a height, an
age, a trail and a species.

- The plane is divided into drifting territories about 240 cells across, around 140 of them.
- Each territory runs one of twelve Life-like rules (Conway, HighLife, Day and Night, Seeds, Maze,
  Coral, Anneal, Replicator, 2x2, Move, Diamoeba, Amoeba), so colonies of many kinds grow, meet
  and cross borders. The borders drift, so a colony can find itself under a different rule.
- Each species also has an altitude band it can be born in, and builds or erodes the ground under
  it by its own amount. The ground relaxes: it blends toward its neighbours and slides off slopes
  steeper than a limit.
- The look is a stained specimen on a dark field. Each species has its own stain, newborn cells
  are white, a dim halo marks where life has been, and the edge falls into a lens vignette.

## How sound and picture meet

The picture reads the score through `Cues`, which reports each hit as it comes due without
touching the audio. A hit does not appear on screen when it sounds:

- Each part has its own delay (sub 1.4 s, tick 0.35 s, gate 2.2 s, noise 4 s), and each delay
  wanders by up to 30 percent on its own slow period, so the picture follows the rhythm without
  moving in step with it.
- A hit lands across the grid by its place in the bar, scrambled, and down the grid by the rule
  number of that bar. The same automaton always plays on the same part of the ground.
- The sub heaves the ground over a wide area. Ticks, the gate and noise seed colonies of growing
  size. The footprints are turned, squashed, lobed and ragged, never circles.

## Terrain

Press **Space** to raise the picture into terrain and press it again to lower it. The plane eases
into a tilted perspective over about a second, and **scroll** changes the tilt.

In terrain, five features, one per drone voice, drift slowly over the plane. Voices 1, 3 and 5
raise mountains and voices 2 and 4 cut depths. Each feature morphs now and then between a broad
swell and a narrow tall spire. Their heights follow each voice's energy, smoothed over seconds,
so the ground grows as each voice enters and sinks slowly when it fades. A small plane-wide
rumble of the raw drone blocks runs underneath.

## Files

| File | What is in it |
| --- | --- |
| `Piece.hpp/.cpp` | `Topsoil::topsoil()`, the machine that starts everything, and the three sine gates |
| `Score.hpp/.cpp` | The page, the automata table, `Cues` (reading the score), and the noise renderer |
| `Drone.hpp/.cpp` | The drone bank and the tape |
| `Grid.hpp/.cpp` | The picture, its link to the score, the terrain controls |
| `shaders/relax_life_rule.comp` | The Life rules, territories, ground relaxation and score events |
| `shaders/relax_life_emit.comp` | The stained look and the vignette |
| `shaders/topsoil_terrain.vert` | The terrain: mountains, depths and spires from the drone |

## Tweaking

Everything below is a constant at the top of a file. Change it and rebuild.

### Time and balance

| Constant | File | Effect |
| --- | --- | --- |
| `k_drone_pace` | `Drone.hpp` | Speed of the drone's whole arc. 4 is a short take, 1 is the full length performance |
| `k_master` | `Score.hpp` | Gain of the gates, noise and drone together. Coinciding hits can clip |
| `k_tape_gain`, `k_drone_master` | `Drone.hpp` | Level of the tape and of the drone alone |
| `k_tape_morph` | `Drone.hpp` | Seconds for the tape to move between harmonic and bar partials |

### The rhythm

- `k_strands` in `Score.hpp` is one row per automaton bar: the rule number, the starting cell and
  the generation count. Change a rule or a count and that bar changes.
- `k_cuts` in `Score.hpp` lists the hard cuts as (bar, from step), bars counted from zero.
- `k_gates` and `k_burst` set each part's pitch, envelope, level and cell length.
- The bars written by hand (the sub row and the noise hits of bars 6 to 9) are in `make_page` in
  `Score.cpp`.

### The drone and tape

- `k_drone_voices` sets each voice's pitch, click rate and level.
- `k_drone_evolution` sets when each voice enters, how it swells and how its rate wanders.
- `k_drone_chord` is the chord sequence, with the hold time of each step.
- `k_drone_ring` sets the ring time of a grain. `k_drone_lift` sets how dense the voices are.
- `k_tape_groups` sets the root, entry time and level of each tape group.

### The ecology

All in `Grid.cpp`, in `k_specimen` (see `LifeRule` in `Grid.hpp` for each field):

- `territory` is the size of a territory. Smaller means more, smaller species.
- `drift` is how far the borders wander. `spore` is how often an empty cell is born anyway, which
  keeps every territory seeded.
- `deposit`, `diffuse`, `talus`, `slide` and `subside` shape how the ground builds and relaxes.
- `trail_decay` sets how long the halo of a dead cell lasts.

The twelve rules, and each species' altitude band and build or erode factor, are in
`relax_life_rule.comp`.

### The link to the score

`k_reach` in `Grid.cpp` has one row per part (sub, tick, gate, noise):

| Field | Effect |
| --- | --- |
| `lag` | How late the picture answers the part, in seconds |
| `wander_period` | Period of the slow change of that lag |
| `radius` | Size of the footprint in cells |
| `strength` | Positive seeds living cells with that chance, negative heaves the ground |

`k_wander` sets how far each lag varies. `k_event_slots` (in `Grid.hpp`) limits events per step.

### The terrain

- `k_tilt`, `k_tilt_step`, `k_tilt_limit`, `k_view_fov` and `k_overscan` in `Grid.cpp` set the view.
  Raise `k_overscan` if the far edge shows a gap when tilted.
- `k_lift_tau` is how long the Space toggle takes to ease in or out.
- `k_energy_attack` and `k_energy_release` set how gradually the terrain rises and sinks. Longer is
  slower.
- At the top of `topsoil_terrain.vert`, `RELIEF` is the overall height, `LIMIT` caps it (keep the
  product under the camera distance of about 3.7), `DOME` is the height each voice gives its
  feature, `BASE` is the plane-wide rumble, `RIPPLE` is the audio-rate ripple on the features, and
  `LEVEL` is how strongly the voice energies are scaled before use.
- Each feature is one `feature(...)` call in `main`. Its arguments are the drift rates and phases
  of its place, the rate and phase of its swell-to-spire morph, its wide and narrow radius, its
  energy, and whether it is a depth. Lower radii make narrower, spikier features, higher radii
  make broader, lower Q ones. Inside `feature`, the exponent and gain mixes set how sharp and how
  tall the spire state becomes.

### The look

- `k_grid_width`, `k_grid_height` and `k_grid_point` in `Grid.hpp` set the resolution and the size
  of each point. Keep the grid and window aspect ratios equal. `k_window_aspect` in `Grid.cpp`
  must match the window.
- The stain palette, the trail halo and the vignette are in `relax_life_emit.comp`.
