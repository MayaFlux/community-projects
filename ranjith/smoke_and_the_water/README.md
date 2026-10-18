# Smoke and the Water

A hand disturbs a body of smoke and water. The medium swirls, thins, and begins to come apart; five voices gather around it, changing with its movement and adding new matter as it changes. The image and sound share one living volume, but neither simply illustrates the other.

[![▶ Smoke and the Water](https://markdown-videos-api.jorgenkh.no/youtube/q3tGICMhUCg?width=1920&height=1080)](https://youtu.be/q3tGICMhUCg)

## The changing medium

The piece begins with a sculpted volume, seen through a slowly shifting palette of dark, glowing, and reflective material. A pen tablet gives the work its touch: strokes push and curl the medium, leaving trails that return later as a reversed gesture.

As the volume changes, its once-coherent form gradually loosens. Smoke and flame gather differently around it, and the image lets that loss arrive as a transition rather than a sudden cut. Four states offer distinct forms of matter: shell, knot, combustion, and liquid.

## The sound

Five voices draw on recorded sounds, looping and changing their direction, pitch, and stereo placement. They enter gradually, and the character of the volume helps shape how they move. Their sound also feeds back into the shared material, so the image continues to respond to the music.

The pace and intensity are affected by both the medium's changing state and the movement of the pen. Together they form a performance that is responsive, but not a sequence of fixed visual cues.

## Running it

Include `src/Piece.hpp` and call `Attrition::attrition()` from `user_project.hpp`.

**A connected pen tablet is required. The piece will not start without one.** Stereo output is recommended.

### Your own sounds

The recordings are not included. Before running, set the five paths in `k_noise` (in `src/Attrition.hpp`) to audio files of your own, one per voice. Longer, textured recordings such as wind, voices, or field recordings work well; each is looped and read at several speeds, so the piece takes its character from what you give it. A missing file is skipped and that voice stays silent.

### Controls

- **N1–N4:** Choose shell, knot, combustion, or liquid.
- **N0:** Restart the current state.
- **F1–F5:** Bring a voice in or out.
- **F6:** Toggle automatic voice arrivals.
- **Up / Down:** Adjust performance intensity.
- **H:** Hush or release the sound.
- **B:** Toggle the slow breathing pulse.
- **F:** Freeze or resume the voices.
- **Enter:** Start or stop recording the volume.
