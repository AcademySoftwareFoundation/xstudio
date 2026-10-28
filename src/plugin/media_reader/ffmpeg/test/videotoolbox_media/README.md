# VideoToolbox decode test media

Media used to test the VideoToolbox decode backend of the FFmpeg reader
(`openspec/changes/add-videotoolbox-decode-backend`). The files aren't
committed. Generate them with:

```sh
./generate.sh [path/to/ffmpeg] [output dir]

# also write the 12 bit / 10 bit 4:4:4 HEVC variants, which need a libx265 built
# for high bit depth (the vcpkg one is 8 bit only)
FFMPEG_HIGHBIT="rez-env ffmpeg-9.0 -- ffmpeg" ./generate.sh
```

By default the script uses the vcpkg-built FFmpeg at
`build/vcpkg_installed/arm-osx/tools/ffmpeg/ffmpeg` and writes to
`build/test_media/videotoolbox/`. It needs an FFmpeg with libx264, libx265 and
the VideoToolbox encoders, so it has to run on macOS. Generation takes about
a minute on an M2 Max, and the output is about 1.7 GB, most of it the 4K
ProRes benchmark files.

Every clip is `testsrc2` at 24 fps, so each frame shows a running counter for
checking frame accuracy by eye. The HD clips are 1920x1080 and 2 s long
(48 frames). The `bench_` clips are 3840x2160 and 5 s long (120 frames). Every
clip is tagged BT.709 limited range unless listed otherwise.

| File | Codec / profile | Depth, subsampling | GOP / structure | Tests |
|---|---|---|---|---|
| `h264_8bit_bframes.mov` | H.264 High (libx264), avcC | 8-bit 4:2:0 | 3 B-frames, closed GOP of 24 | Reorder (D8), random access, bit-exact parity |
| `hevc_main10_bframes.mov` | HEVC Main 10 (VideoToolbox), hvcC | 10-bit 4:2:0 | 2 B-frames, GOP 24 | `x420`/P010 output path, reorder |
| `hevc_422_10bit_bframes.mov` | HEVC Main 4:2:2 10 (VideoToolbox), hvcC | 10-bit 4:2:2 | 2 B-frames, GOP 24 | `x422`/P210 output path |
| `hevc_8bit_opengop.mov` | HEVC Main (libx265), hvcC | 8-bit 4:2:0 | 4 B-frames, **open GOP**, keyint 24 | Leading pictures after a seek |
| `hevc_main10_bt2020_pq.mov` | HEVC Main 10 (VideoToolbox), hvcC | 10-bit 4:2:0 | 2 B-frames, GOP 24 | Colour metadata: BT.2020 non-constant-luminance matrix and primaries, PQ transfer |
| `prores_422_proxy.mov` | ProRes 422 Proxy (`apco`) | 10-bit 4:2:2 | Intra | ProRes 422 decode |
| `prores_422_lt.mov` | ProRes 422 LT (`apcs`) | 10-bit 4:2:2 | Intra | " |
| `prores_422_standard.mov` | ProRes 422 (`apcn`) | 10-bit 4:2:2 | Intra | " |
| `prores_422_hq.mov` | ProRes 422 HQ (`apch`) | 10-bit 4:2:2 | Intra | " |
| `prores_4444_alpha.mov` | ProRes 4444 (`ap4h`) | 12-bit 4:4:4 + alpha | Intra | Alpha preserved. Alpha is a horizontal ramp from 0 (left) to 1 (right) |
| `prores_4444xq.mov` | ProRes 4444 XQ (`ap4x`) | 12-bit 4:4:4 | Intra | 4444 XQ decode, 12-bit precision |
| `h264_annexb.h264` | H.264 High, raw Annex-B elementary stream | 8-bit 4:2:0 | 3 B-frames | Not eligible for VideoToolbox; must fall back to software. No container, so FFmpeg guesses 25 fps |
| `h264_aac.mov` | H.264 High + AAC-LC mono 48 kHz, rising chirp | 8-bit 4:2:0 | 3 B-frames | AAC through `aac_at` (D10), A/V sync. The chirp never repeats, so any sample offset between AAC decoders shows up |
| `dnxhr_fullrange.mov`, `dnxhr_fullrange.dneg.mov` | DNxHR HQ tagged full range (the same file twice) | 8-bit 4:2:2 | Intra | Not VideoToolbox. Regression test for the `.dneg.mov` video-range override |
| `variant_h264_{10bit_420,8bit_422,10bit_422,8bit_444,10bit_444}.mov` | H.264 High 10 / High 4:2:2 / High 4:4:4 Predictive | as named | 3 B-frames | Spike 1.8 variants |
| `variant_hevc_{8bit_422,8bit_444}.mov` | HEVC Rext (libx265) | as named | 4 B-frames, open GOP | Spike 1.8 variants |
| `variant_hevc_{10bit_444,12bit_420,12bit_422,12bit_444}.mov` | HEVC Rext, only with `FFMPEG_HIGHBIT` | as named | 4 B-frames, open GOP | Spike 1.8 variants. 12-bit 4:2:0 must stay on FFmpeg |
| `bench_uhd_hevc_main10.mov` | HEVC Main 10 (VideoToolbox), 40 Mb/s | 10-bit 4:2:0 | 2 B-frames, GOP 24 | Throughput benchmark (task 1.5) |
| `bench_uhd_prores_4444.mov` | ProRes 4444 (`ap4h`) | 12-bit 4:4:4 | Intra | Throughput benchmark (task 1.5) |
| `bench_uhd_{hevc_main10,h264,prores_422hq,prores_4444}_grain.mov` | As above, with temporal grain | | | Realistic decode cost for benchmarks: `testsrc2` alone compresses unrealistically well |

Notes:

- The vcpkg `libx265` is an 8-bit build, so the 10-bit HEVC clips come from
  Apple's `hevc_videotoolbox` encoder. `libx265` is still used for the open-GOP
  clip, because `hevc_videotoolbox` has no open-GOP control.
- FFmpeg 8 takes colour tags from the frames, so the script tags them with a
  `setparams` filter as well as with the `-color_*` output options.
