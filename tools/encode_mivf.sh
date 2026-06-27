#!/usr/bin/env bash
set -e

if [ "$#" -ne 2 ]; then
    echo "Usage: ./encode_mivf.sh <input_video.mp4> <output_video.mivf>"
    exit 1
fi

INPUT="$1"
OUTPUT="$2"
TEMP_MASTER_YUV="temp_master_raw.yuv"
TEMP_VIDEO_ONLY="temp_video_only.mivf"
SD_CARD="/d"

echo "============================================================"
echo "1. Extracting Raw Master Frame Buffer (Clean Demux)"
echo "============================================================"
# Standard continuous extraction pass to avoid container decoding freezes
ffmpeg -y -v quiet -stats -i "$INPUT" -vf "scale=400:240,format=yuv420p" -c:v rawvideo "$TEMP_MASTER_YUV"

echo
echo "============================================================"
echo "2. Splitting and Compressing Video Streams (Multi-Core Cluster Engine)"
echo "============================================================"
/ucrt64/bin/python.exe mivf_parallel_engine.py \
  "$TEMP_MASTER_YUV" \
  "$TEMP_VIDEO_ONLY" \
  400 240 30 \
  --keyint 240 \
  --qp 42 \
  --c-qp-offset 9 \
  --lambda 35.0 \
  --y-skip 36 \
  --c-skip 44 \
  --y-delta 40 \
  --c-delta 56 \
  --mv-range 4

echo
echo "============================================================"
echo "3. Multiplexing Compressed 4-bit Audio (mivf_ia4m_mux.py)"
echo "============================================================"
/ucrt64/bin/python.exe mivf_ia4m_mux.py \
  "$TEMP_VIDEO_ONLY" \
  "$INPUT" \
  "$OUTPUT" \
  --rate 44100 \
  --channels 1

rm -f "$TEMP_VIDEO_ONLY"

echo
echo "============================================================"
echo "4. Deploying Package to SD Card"
echo "============================================================"
if [ ! -d "$SD_CARD" ]; then
    echo "⚠️ WARNING: SD Card volume '$SD_CARD' not found!"
    echo "Your completed file is safe locally at: /c/dev/MIVF/$OUTPUT"
    exit 0
fi

mkdir -p "$SD_CARD/3ds/mivf_player_3ds"
cp /c/dev/MIVF/MIVF_3DS_Player_Phase4B_v1_0_Audio/mivf_player_3ds.3dsx "$SD_CARD/3ds/mivf_player_3ds/mivf_player_3ds.3dsx"
cp "$OUTPUT" "$SD_CARD/$OUTPUT"
echo "DEPLOY SUCCESSFUL!"
