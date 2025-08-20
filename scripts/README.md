# Audio Processing Scripts

This directory contains scripts for preparing audio files for the ESP32 audio codec (ES8311).

## Scripts

### `mp3_to_ogg.sh`
Simple shell script to convert MP3 files to OGG format using Opus codec.

**Usage:**
```bash
./mp3_to_ogg.sh input.mp3 output.ogg
```

### `convert_audio.py`
Comprehensive Python script for converting various audio formats to ESP32-compatible formats.

**Usage:**
```bash
# Convert to Opus (default)
python3 convert_audio.py input.mp3

# Convert to raw PCM
python3 convert_audio.py input.mp3 -f pcm

# Specify custom bitrate and sample rate
python3 convert_audio.py input.wav -b 24k -r 22050 -o output.ogg
```

**Options:**
- `-f, --format`: Output format (opus/pcm)
- `-b, --bitrate`: Bitrate for Opus encoding (default: 16k)
- `-r, --sample-rate`: Sample rate (default: 16000)
- `-o, --output`: Output filename (auto-generated if not specified)

## Audio Format Specifications

### For ES8311 Codec:
- **Sample Rate**: 16kHz (recommended for voice/alarms)
- **Channels**: Mono (1 channel)
- **Bit Depth**: 16-bit
- **Format**: Opus codec in OGG container or raw PCM

### Requirements:
- FFmpeg must be installed on your system
- Python 3.6+ (for convert_audio.py)

## Examples

```bash
# Convert alarm sound
./mp3_to_ogg.sh alarm_beep.mp3 beep.ogg

# Convert notification sound with custom settings
python3 convert_audio.py notification.wav -b 24k -r 22050

# Convert to raw PCM for direct codec use
python3 convert_audio.py alert.mp3 -f pcm -o alert_sound.pcm
```