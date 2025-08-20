#!/usr/bin/env python3
"""
Generate simple test PCM audio samples without external dependencies
"""

import math
import struct

def generate_tone(frequency, duration_ms, sample_rate=16000, amplitude=0.3):
    """Generate a sine wave tone"""
    samples = []
    num_samples = int(sample_rate * duration_ms / 1000)
    
    for i in range(num_samples):
        t = i / sample_rate
        value = amplitude * math.sin(2 * math.pi * frequency * t)
        sample = int(value * 32767)  # Convert to 16-bit signed integer
        samples.append(max(-32768, min(32767, sample)))  # Clamp to 16-bit range
    
    return samples

def save_as_c_header(samples, filename, array_name):
    """Save samples as C header file"""
    with open(filename, 'w') as f:
        f.write(f"#ifndef {array_name.upper()}_H\n")
        f.write(f"#define {array_name.upper()}_H\n\n")
        f.write(f"#include <stdint.h>\n\n")
        f.write(f"const uint16_t {array_name}_length = {len(samples)};\n")
        f.write(f"const int16_t {array_name}[] = {{\n")
        
        # Write data in rows of 8 values
        for i in range(0, len(samples), 8):
            row = samples[i:i+8]
            values = ", ".join(f"{val:6d}" for val in row)
            f.write(f"    {values},\n")
        
        f.write("};\n\n")
        f.write(f"#endif // {array_name.upper()}_H\n")

def save_as_pcm(samples, filename):
    """Save samples as raw PCM file"""
    with open(filename, 'wb') as f:
        for sample in samples:
            f.write(struct.pack('<h', sample))  # Little-endian signed 16-bit

def main():
    import os
    output_dir = "../main/assets"
    os.makedirs(output_dir, exist_ok=True)
    
    print("🎵 Generating simple test audio samples...")
    
    # Generate a short beep (1kHz, 300ms)
    beep_samples = generate_tone(1000, 300, amplitude=0.4)
    
    # Generate a two-tone chime
    chime_samples = []
    chime_samples.extend(generate_tone(800, 250, amplitude=0.3))   # First tone
    chime_samples.extend([0] * int(16000 * 0.05))                  # 50ms silence  
    chime_samples.extend(generate_tone(1200, 250, amplitude=0.3))  # Second tone
    
    # Generate a simple melody (C major scale)
    melody_samples = []
    notes = [523, 587, 659, 698, 784, 880, 988]  # C D E F G A B
    for freq in notes:
        melody_samples.extend(generate_tone(freq, 150, amplitude=0.25))
        melody_samples.extend([0] * int(16000 * 0.02))  # 20ms silence between notes
    
    # Save as C header files
    save_as_c_header(beep_samples, f"{output_dir}/audio_beep.h", "audio_beep")
    save_as_c_header(chime_samples, f"{output_dir}/audio_chime.h", "audio_chime")
    save_as_c_header(melody_samples, f"{output_dir}/audio_melody.h", "audio_melody")
    
    # Also save as PCM files for reference
    save_as_pcm(beep_samples, f"{output_dir}/beep.pcm")
    save_as_pcm(chime_samples, f"{output_dir}/chime.pcm") 
    save_as_pcm(melody_samples, f"{output_dir}/melody.pcm")
    
    print(f"✅ Generated audio samples:")
    print(f"   - Beep: {len(beep_samples)} samples ({len(beep_samples)/16000:.2f}s)")
    print(f"   - Chime: {len(chime_samples)} samples ({len(chime_samples)/16000:.2f}s)")
    print(f"   - Melody: {len(melody_samples)} samples ({len(melody_samples)/16000:.2f}s)")
    print(f"📁 Files saved to: {output_dir}/")

if __name__ == "__main__":
    main()