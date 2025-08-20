#!/usr/bin/env python3
"""
Generate test PCM audio samples for ESP32 testing
"""

import numpy as np
import struct
import os

def generate_beep(frequency=1000, duration=0.5, sample_rate=16000, amplitude=0.3):
    """Generate a simple beep tone"""
    t = np.linspace(0, duration, int(sample_rate * duration), False)
    wave = amplitude * np.sin(2 * np.pi * frequency * t)
    # Convert to 16-bit signed integers
    pcm_data = (wave * 32767).astype(np.int16)
    return pcm_data

def generate_chime(sample_rate=16000, amplitude=0.3):
    """Generate a pleasant two-tone chime"""
    # First tone: 800Hz for 0.3s
    tone1 = generate_beep(800, 0.3, sample_rate, amplitude)
    # Short silence
    silence = np.zeros(int(sample_rate * 0.1), dtype=np.int16)
    # Second tone: 1200Hz for 0.3s
    tone2 = generate_beep(1200, 0.3, sample_rate, amplitude)
    
    return np.concatenate([tone1, silence, tone2])

def generate_melody(sample_rate=16000, amplitude=0.3):
    """Generate a simple melody"""
    # C major scale frequencies
    notes = [523, 587, 659, 698, 784, 880, 988]  # C D E F G A B
    melody = []
    
    for freq in notes:
        tone = generate_beep(freq, 0.2, sample_rate, amplitude)
        silence = np.zeros(int(sample_rate * 0.05), dtype=np.int16)
        melody.extend(tone)
        melody.extend(silence)
    
    return np.array(melody)

def save_pcm_as_c_array(pcm_data, filename, array_name):
    """Save PCM data as C array in header file"""
    with open(filename, 'w') as f:
        f.write(f"#ifndef {array_name.upper()}_H\n")
        f.write(f"#define {array_name.upper()}_H\n\n")
        f.write(f"#include <stdint.h>\n\n")
        f.write(f"const uint16_t {array_name}_length = {len(pcm_data)};\n")
        f.write(f"const int16_t {array_name}[] = {{\n")
        
        # Write data in rows of 8 values
        for i in range(0, len(pcm_data), 8):
            row = pcm_data[i:i+8]
            values = ", ".join(f"{val:6d}" for val in row)
            f.write(f"    {values},\n")
        
        f.write("};\n\n")
        f.write(f"#endif // {array_name.upper()}_H\n")

def main():
    output_dir = "../main/assets"
    os.makedirs(output_dir, exist_ok=True)
    
    print("🎵 Generating test audio samples...")
    
    # Generate different audio samples
    beep_data = generate_beep(2000, 0.2, amplitude=0.4)  # Short beep
    chime_data = generate_chime(amplitude=0.3)            # Pleasant chime
    melody_data = generate_melody(amplitude=0.2)          # Musical scale
    
    # Save as C header files
    save_pcm_as_c_array(beep_data, f"{output_dir}/audio_beep.h", "audio_beep")
    save_pcm_as_c_array(chime_data, f"{output_dir}/audio_chime.h", "audio_chime") 
    save_pcm_as_c_array(melody_data, f"{output_dir}/audio_melody.h", "audio_melody")
    
    # Also save as raw PCM files for testing
    beep_data.tofile(f"{output_dir}/beep.pcm")
    chime_data.tofile(f"{output_dir}/chime.pcm")
    melody_data.tofile(f"{output_dir}/melody.pcm")
    
    print(f"✅ Generated audio samples:")
    print(f"   - Beep: {len(beep_data)} samples ({len(beep_data)/16000:.1f}s)")
    print(f"   - Chime: {len(chime_data)} samples ({len(chime_data)/16000:.1f}s)")
    print(f"   - Melody: {len(melody_data)} samples ({len(melody_data)/16000:.1f}s)")
    print(f"📁 Files saved to: {output_dir}/")

if __name__ == "__main__":
    main()