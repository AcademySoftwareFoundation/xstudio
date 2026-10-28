#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Generates the media set used to test the VideoToolbox decode backend.
# See README.md in this directory for what each file exercises.
#
# usage: generate.sh [ffmpeg binary] [output dir]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../../.." && pwd)"

FFMPEG="${1:-${REPO_ROOT}/build/vcpkg_installed/arm-osx/tools/ffmpeg/ffmpeg}"
OUT="${2:-${REPO_ROOT}/build/test_media/videotoolbox}"

mkdir -p "${OUT}"

FF=("${FFMPEG}" -hide_banner -loglevel error -y)

# 2 seconds at 24fps; frame number burnt in so frame accuracy can be checked by eye
HD="testsrc2=size=1920x1080:rate=24:duration=2"
UHD="testsrc2=size=3840x2160:rate=24:duration=5"
# FFmpeg 8 takes colour tags from the frames, so set them with setparams rather than
# -color_* output options alone.
P709="setparams=colorspace=bt709:color_primaries=bt709:color_trc=bt709:range=tv"
P2020PQ="setparams=colorspace=bt2020nc:color_primaries=bt2020:color_trc=smpte2084:range=tv"
TAGS_709=(-colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range tv)

echo "Writing test media to ${OUT}"

# H.264 8-bit 4:2:0, 3 B-frames, closed GOP of 24
"${FF[@]}" -f lavfi -i "${HD}" -c:v libx264 -pix_fmt yuv420p -bf 3 -g 24 \
    -x264-params open-gop=0 -vf "${P709}" "${TAGS_709[@]}" "${OUT}/h264_8bit_bframes.mov"

# The vcpkg libx265 is an 8 bit build, so 10 bit HEVC comes from Apple's encoder.
# libx265 is still used where we need GOP control that hevc_videotoolbox doesn't expose.

# HEVC Main10 4:2:0, B-frames
"${FF[@]}" -f lavfi -i "${HD}" -c:v hevc_videotoolbox -profile:v main10 -pix_fmt p010le \
    -bf 2 -g 24 -b:v 8M -tag:v hvc1 -vf "${P709}" "${TAGS_709[@]}" "${OUT}/hevc_main10_bframes.mov"

# HEVC Main 4:2:2 10, B-frames
"${FF[@]}" -f lavfi -i "${HD}" -c:v hevc_videotoolbox -profile:v main42210 -pix_fmt p210le \
    -bf 2 -g 24 -b:v 10M -tag:v hvc1 -vf "${P709}" "${TAGS_709[@]}" "${OUT}/hevc_422_10bit_bframes.mov"

# HEVC Main (8 bit), open GOP with B-frames - leading pictures after each I frame
"${FF[@]}" -f lavfi -i "${HD}" -c:v libx265 -pix_fmt yuv420p -tag:v hvc1 \
    -x265-params "bframes=4:keyint=24:open-gop=1:log-level=error" -vf "${P709}" "${TAGS_709[@]}" \
    "${OUT}/hevc_8bit_opengop.mov"

# HEVC Main10 tagged BT.2020 / PQ
"${FF[@]}" -f lavfi -i "${HD}" -c:v hevc_videotoolbox -profile:v main10 -pix_fmt p010le \
    -bf 2 -g 24 -b:v 8M -tag:v hvc1 -vf "${P2020PQ}" \
    -colorspace bt2020nc -color_primaries bt2020 -color_trc smpte2084 -color_range tv \
    "${OUT}/hevc_main10_bt2020_pq.mov"

# ProRes 422 family, via Apple's encoder
for profile in proxy lt standard hq; do
    "${FF[@]}" -f lavfi -i "${HD}" -c:v prores_videotoolbox -profile:v "${profile}" \
        -pix_fmt p210le -vf "${P709}" "${TAGS_709[@]}" "${OUT}/prores_422_${profile}.mov"
done

# ProRes 4444 with an alpha gradient, and 4444 XQ
"${FF[@]}" -f lavfi -i "${HD}" -vf "format=yuva444p10le,geq=lum='lum(X,Y)':cb='cb(X,Y)':cr='cr(X,Y)':a='X*1023/W',${P709}" \
    -c:v prores_videotoolbox -profile:v 4444 -pix_fmt ayuv64le "${TAGS_709[@]}" \
    "${OUT}/prores_4444_alpha.mov"
"${FF[@]}" -f lavfi -i "${HD}" -c:v prores_videotoolbox -profile:v xq -pix_fmt ayuv64le \
    -vf "${P709}" "${TAGS_709[@]}" "${OUT}/prores_4444xq.mov"

# Raw Annex-B H.264 elementary stream (no avcC) - must fall back to software
"${FF[@]}" -f lavfi -i "${HD}" -c:v libx264 -pix_fmt yuv420p -bf 3 -g 24 \
    -vf "${P709}" "${TAGS_709[@]}" -f h264 "${OUT}/h264_annexb.h264"

# H.264 with an AAC audio track: a rising chirp, so that any offset between AAC
# decoders shows up (a steady tone would hide one that is a whole number of cycles,
# e.g. the usual 2112 sample priming at 1kHz/48kHz)
"${FF[@]}" -f lavfi -i "${HD}" -f lavfi -i "aevalsrc='0.5*sin(2*PI*(200+1500*t)*t)':s=48000:d=2" \
    -c:v libx264 -pix_fmt yuv420p -bf 3 -g 24 -vf "${P709}" "${TAGS_709[@]}" -c:a aac -b:a 192k \
    -shortest "${OUT}/h264_aac.mov"

# DNxHR HQ tagged full range, and the same file named .dneg.mov - the reader forces
# DNxHD/HR in .dneg.mov files to video range. Not VideoToolbox media, but the
# regression test for that override lives with these tests.
"${FF[@]}" -f lavfi -i "${HD}" -c:v dnxhd -profile:v dnxhr_hq -pix_fmt yuv422p \
    -vf "setparams=colorspace=bt709:color_primaries=bt709:color_trc=bt709:range=pc" \
    -colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range pc \
    "${OUT}/dnxhr_fullrange.mov"
cp "${OUT}/dnxhr_fullrange.mov" "${OUT}/dnxhr_fullrange.dneg.mov"

# Variants VideoToolbox isn't used for yet (design D4), to see what it does with them.
# libx265 here is 8 bit only, so there is no 12 bit or 10 bit 4:4:4 HEVC.
for v in "yuv420p10le h264_10bit_420" "yuv422p h264_8bit_422" "yuv422p10le h264_10bit_422" \
         "yuv444p h264_8bit_444" "yuv444p10le h264_10bit_444"; do
    set -- $v
    "${FF[@]}" -f lavfi -i "${HD}" -c:v libx264 -pix_fmt "$1" -bf 3 -g 24 \
        -vf "${P709}" "${TAGS_709[@]}" "${OUT}/variant_$2.mov"
done
for v in "yuv422p hevc_8bit_422" "yuv444p hevc_8bit_444"; do
    set -- $v
    "${FF[@]}" -f lavfi -i "${HD}" -c:v libx265 -pix_fmt "$1" -tag:v hvc1 \
        -x265-params "bframes=4:keyint=24:log-level=error" -vf "${P709}" "${TAGS_709[@]}" \
        "${OUT}/variant_$2.mov"
done

# High bit depth HEVC needs a libx265 built for it, which the vcpkg one isn't. Point
# FFMPEG_HIGHBIT at an ffmpeg that has one (e.g. "rez-env ffmpeg-9.0 -- ffmpeg") to
# also write these.
if [ -n "${FFMPEG_HIGHBIT:-}" ]; then
    for v in "yuv420p12le hevc_12bit_420" "yuv422p12le hevc_12bit_422" \
             "yuv444p12le hevc_12bit_444" "yuv444p10le hevc_10bit_444"; do
        set -- $v
        ${FFMPEG_HIGHBIT} -hide_banner -loglevel error -y -f lavfi -i "${HD}" -c:v libx265 \
            -pix_fmt "$1" -tag:v hvc1 -x265-params "bframes=4:keyint=24:log-level=error" \
            -vf "${P709}" "${TAGS_709[@]}" "${OUT}/variant_$2.mov"
    done
fi

# 4K benchmark material
"${FF[@]}" -f lavfi -i "${UHD}" -c:v hevc_videotoolbox -profile:v main10 -pix_fmt p010le \
    -bf 2 -g 24 -b:v 40M -tag:v hvc1 -vf "${P709}" "${TAGS_709[@]}" "${OUT}/bench_uhd_hevc_main10.mov"
"${FF[@]}" -f lavfi -i "${UHD}" -c:v prores_videotoolbox -profile:v 4444 -pix_fmt ayuv64le \
    -vf "${P709}" "${TAGS_709[@]}" "${OUT}/bench_uhd_prores_4444.mov"

# testsrc2 compresses unrealistically well, so these add temporal grain to get bitrates and
# decode cost closer to real camera footage.
GRAIN="noise=alls=12:allf=t+u"
"${FF[@]}" -f lavfi -i "${UHD}" -c:v hevc_videotoolbox -profile:v main10 -pix_fmt p010le \
    -bf 2 -g 24 -b:v 80M -tag:v hvc1 -vf "${GRAIN},${P709}" "${TAGS_709[@]}" \
    "${OUT}/bench_uhd_hevc_main10_grain.mov"
"${FF[@]}" -f lavfi -i "${UHD}" -c:v prores_videotoolbox -profile:v hq -pix_fmt p210le \
    -vf "${GRAIN},${P709}" "${TAGS_709[@]}" "${OUT}/bench_uhd_prores_422hq_grain.mov"
"${FF[@]}" -f lavfi -i "${UHD}" -c:v prores_videotoolbox -profile:v 4444 -pix_fmt ayuv64le \
    -vf "${GRAIN},${P709}" "${TAGS_709[@]}" "${OUT}/bench_uhd_prores_4444_grain.mov"
"${FF[@]}" -f lavfi -i "${UHD}" -c:v libx264 -pix_fmt yuv420p -bf 3 -g 24 -b:v 60M \
    -vf "${GRAIN},${P709}" "${TAGS_709[@]}" "${OUT}/bench_uhd_h264_grain.mov"

echo "Done."
