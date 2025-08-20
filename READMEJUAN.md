# ESP32 Alarm Clock - Simplified Version

This project has been simplified from the original voice assistant/MCP device into a basic alarm clock with the following features:

## Features

✅ **WiFi Connectivity** - Connects to your WiFi network  
✅ **NTP Time Sync** - Gets accurate time from internet time servers  
✅ **Web Interface** - Configure alarms through a simple web browser interface  
✅ **Alarm Management** - Set multiple alarms with custom labels  
✅ **Display** - Shows current time and alarm status on the device display  
✅ **Audio Output** - Basic audio capabilities for alarm sounds (TODO: implement alarm sound)

## Changes Made

### Removed Components
- ❌ MCP (Model Context Protocol) server functionality
- ❌ OTA (Over-The-Air) firmware updates  
- ❌ WebSocket and MQTT communication protocols
- ❌ Voice recognition and wake word detection
- ❌ Audio streaming and processing pipeline
- ❌ Complex audio service with Opus encoding/decoding
- ❌ Text-to-speech and speech-to-text capabilities
- ❌ AI chatbot integration

### Simplified Components
- ✅ Basic audio codec (for alarm sounds)
- ✅ WiFi board functionality (connection only)
- ✅ Display system (time and status only)
- ✅ Device state management (simplified states)
- ✅ Settings storage system

### New Components
- ✅ NTP time synchronization
- ✅ HTTP web server for alarm configuration
- ✅ Alarm storage and management system
- ✅ Web-based user interface

## Usage

1. **Setup WiFi**: Device will enter WiFi configuration mode if no network is configured
2. **Access Web Interface**: Connect to the device's IP address in your browser
3. **Set Alarms**: Use the web interface to create alarms with time and label
4. **View Time**: Device display shows current time when idle
5. **Alarm Notification**: When alarm time arrives, device shows alarm message on display

## Web Interface

The device hosts a simple web server at port 80 that provides:
- Real-time clock display
- Alarm creation form (time + label)
- List of active alarms
- Alarm deletion functionality

## Device States

Simplified to only essential states:
- `Starting` - Device initializing
- `WiFi Configuring` - Setting up network connection  
- `Idle` - Normal operation, showing time
- `Alarm Ringing` - Active alarm notification
- `Fatal Error` - System error state

## File Structure

```
main/
├── application.cc/h     # Main application logic with alarm functionality
├── main.cc             # Entry point
├── settings.cc/h       # Configuration storage
├── system_info.cc/h    # System information utilities
├── device_state*.cc/h  # Device state management
├── audio/
│   ├── audio_codec.*   # Basic audio hardware interface
│   └── codecs/         # Hardware-specific audio codec drivers
├── display/            # Display management (time, status)
├── boards/             # Hardware board configurations
└── led/               # LED indicator support
```

## Building

1. Set up ESP-IDF environment
2. Configure your board type in menuconfig
3. Build with `idf.py build`
4. Flash with `idf.py flash`

## TODO

- [ ] Implement actual alarm sound playback
- [ ] Add snooze functionality
- [ ] Add timezone configuration
- [ ] Add alarm repeat options (daily, weekly, etc.)
- [ ] Improve web interface styling
- [ ] Add buzzer/speaker support for boards without audio codec