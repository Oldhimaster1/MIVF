#!/usr/bin/env python3
import sys
import time
import subprocess
import struct
import os
from multiprocessing import cpu_count
from pathlib import Path

def wr_u32le(b, offset, val):
    struct.pack_into('<I', b, offset, val)

def wr_u64le(b, offset, val):
    struct.pack_into('<Q', b, offset, val)

def r_u32le(b, offset):
    return struct.unpack_from('<I', b, offset)[0]

def fmt_time(secs):
    m, s = divmod(int(secs), 60)
    h, m = divmod(m, 60)
    if h > 0:
        return f"{h:02d}:{m:02d}:{s:02d}"
    return f"{m:02d}:{s:02d}"

def main():
    yuv_master_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    w = int(sys.argv[3])
    h = int(sys.argv[4])
    fps = int(sys.argv[5])
    encoder_args = sys.argv[6:]

    frame_size = w * h + (w // 2) * (h // 2) * 2
    total_bytes = yuv_master_path.stat().st_size
    total_frames = total_bytes // frame_size

    cores = cpu_count()
    print(f"Parallel Engine: Slicing raw data across {cores} CPU Core Clusters...")

    frames_per_core = total_frames // cores
    if frames_per_core == 0:
        cores = 1
        frames_per_core = total_frames

    chunk_files = []
    chunk_outputs = []
    processes = []

    # Segment the master file into isolated physical frame chunks
    with yuv_master_path.open('rb') as f_in:
        for i in range(cores):
            num_frames = frames_per_core if i < cores - 1 else total_frames - (frames_per_core * i)
            if num_frames <= 0:
                break
                
            c_yuv = Path(f"temp_slice_{i}.yuv")
            c_mivf = Path(f"temp_slice_{i}.mivf")
            chunk_files.append(c_yuv)
            chunk_outputs.append(c_mivf)

            # High-speed write sequence segment pass
            c_yuv.write_bytes(f_in.read(num_frames * frame_size))

    # Wiping out the master file immediately to free up hard drive space before encoding begins
    print("Parallel Engine: Master raw cache split successfully. Flushing temporary disk space...")
    yuv_master_path.unlink(missing_ok=True)

    print(f"Parallel Engine: Launching {len(chunk_files)} concurrent compression instances at full throttle...")
    start_time = time.time()
    
    for i in range(len(chunk_files)):
        cmd = [
            "./miv2y_moflex_tier.exe",
            "--input", str(chunk_files[i]),
            "--output", str(chunk_outputs[i]),
            "--width", str(w),
            "--height", str(h),
            "--fps", str(fps)
        ] + encoder_args
        
        p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        processes.append(p)

    # Monitor thread pools with an active telemetry dashboard clock
    while True:
        active_p = [p for p in processes if p.poll() is None]
        active_count = len(active_p)
        done_count = len(processes) - active_count
        elapsed = time.time() - start_time
        
        fps_speed = (total_frames * (done_count / len(processes))) / elapsed if done_count > 0 else 0
        eta_str = fmt_time((len(processes) - done_count) * (elapsed / done_count)) if done_count > 0 else "Calculating..."

        sys.stdout.write(
            f"\r[Time: {fmt_time(elapsed)}] | Cores Active: {active_count}/{len(processes)} | Chunks Done: {done_count} | Predicted ETA: {eta_str}"
        )
        sys.stdout.flush()

        if active_count == 0:
            break
        time.sleep(1)

    final_time = time.time() - start_time
    print(f"\n============================================================")
    print(f"🏆 TOTAL ENCODER RUNTIME: {fmt_time(final_time)} | Speed: {total_frames/final_time:.1f} fps")
    print(f"============================================================")

    print("Parallel Engine: Patching headers and reconstructing container streams...")
    with out_path.open('wb') as f_out:
        first_chunk = chunk_outputs[0].read_bytes()
        header = bytearray(first_chunk[:96])
        
        # Binary search-and-replace to correct global file length pointers
        chunk_duration = (total_frames // cores) * 30000 // fps
        total_duration = total_frames * 30000 // fps
        
        chunk_dur_bytes = struct.pack('<Q', chunk_duration)
        total_dur_bytes = struct.pack('<Q', total_duration)
        idx = header.find(chunk_dur_bytes)
        if idx != -1:
            header[idx:idx+8] = total_dur_bytes
        f_out.write(header)

        running_frame_idx = 0
        for c_mivf in chunk_outputs:
            b = c_mivf.read_bytes()
            offset = 96
            file_len = len(b)

            while offset < file_len:
                h_page = bytearray(b[offset:offset+32])
                payload_size = r_u32le(h_page, 16)

                wr_u32le(h_page, 4, running_frame_idx)
                wr_u64le(h_page, 8, running_frame_idx * 30000 // fps)

                f_out.write(h_page)
                f_out.write(b[offset+32:offset+32+payload_size])

                offset += 32 + payload_size
                running_frame_idx += 1

    # Clear lingering temporary disk segments
    for f1, f2 in zip(chunk_files, chunk_outputs):
        f1.unlink(missing_ok=True)
        f2.unlink(missing_ok=True)

    print(f"Parallel Engine: Master container unified with {running_frame_idx} sequential frames.")

if __name__ == '__main__':
    main()
