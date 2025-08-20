#!/usr/bin/env python3
"""
Audio Conversion Script for ESP32 Audio Codec
Converts various audio formats to formats suitable for ES8311 codec
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path

def convert_to_opus(input_file, output_file, bitrate="16k", sample_rate="16000"):
    """Convert audio file to Opus format optimized for ESP32"""
    cmd = [
        "ffmpeg", "-i", input_file,
        "-c:a", "libopus",
        "-b:a", bitrate,
        "-ac", "1",  # Mono
        "-ar", sample_rate,
        "-frame_duration", "60",
        "-y",  # Overwrite output files
        output_file
    ]
    
    try:
        subprocess.run(cmd, check=True, capture_output=True)
        print(f"✅ Successfully converted {input_file} to {output_file}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ Error converting {input_file}: {e.stderr.decode()}")
        return False
    except FileNotFoundError:
        print("❌ FFmpeg not found. Please install FFmpeg first.")
        return False

def convert_to_pcm(input_file, output_file, sample_rate="16000"):
    """Convert audio file to raw PCM format"""
    cmd = [
        "ffmpeg", "-i", input_file,
        "-f", "s16le",  # 16-bit little-endian PCM
        "-ac", "1",     # Mono
        "-ar", sample_rate,
        "-y",
        output_file
    ]
    
    try:
        subprocess.run(cmd, check=True, capture_output=True)
        print(f"✅ Successfully converted {input_file} to PCM: {output_file}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ Error converting {input_file}: {e.stderr.decode()}")
        return False

def main():
    parser = argparse.ArgumentParser(description="Convert audio files for ESP32 audio codec")
    parser.add_argument("input", help="Input audio file")
    parser.add_argument("-o", "--output", help="Output file (default: auto-generate)")
    parser.add_argument("-f", "--format", choices=["opus", "pcm"], default="opus",
                       help="Output format (default: opus)")
    parser.add_argument("-b", "--bitrate", default="16k", help="Bitrate for Opus (default: 16k)")
    parser.add_argument("-r", "--sample-rate", default="16000", help="Sample rate (default: 16000)")
    
    args = parser.parse_args()
    
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"❌ Input file not found: {args.input}")
        return 1
    
    # Generate output filename if not provided
    if args.output:
        output_path = Path(args.output)
    else:
        if args.format == "opus":
            output_path = input_path.with_suffix(".ogg")
        else:  # pcm
            output_path = input_path.with_suffix(".pcm")
    
    print(f"🔄 Converting {input_path} to {output_path} ({args.format} format)")
    
    if args.format == "opus":
        success = convert_to_opus(str(input_path), str(output_path), 
                                args.bitrate, args.sample_rate)
    else:
        success = convert_to_pcm(str(input_path), str(output_path), args.sample_rate)
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())