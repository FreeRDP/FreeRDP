# VAAPI H.264 X11 Direct Rendering Checkpoint

Date: 2026-05-07
Checkpoint: new17

## CPU Comparison

Observed client CPU usage in AVC420/H.264 mode at 3440x1600:

| Build | CPU usage |
| --- | ---: |
| original | ~84% |
| new17 | ~51% |

This is a reduction of about 33 percentage points, or roughly 39% relative to the
original measurement.

## Image Quality

The new17 rendering path improves readability in the observed test case. Red text
on a black background is clearly more legible than with the original path.
