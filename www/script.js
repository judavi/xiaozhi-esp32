let alarms = [];
const days = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

// Volume control
function setVolume(volume) {
    document.getElementById('volumeValue').textContent = volume;
    
    // Send volume to ESP32
    if (!config.isLocalDevelopment) {
        fetch('/api/volume', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ volume: parseInt(volume) })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                console.log(`Volume set to ${volume}%`);
            }
        })
        .catch(error => console.error('Error setting volume:', error));
    } else {
        console.log(`Mock: Volume set to ${volume}%`);
    }
}

// Configuration for local development vs ESP32
const config = {
    // Set to true when testing locally, false when running on ESP32
    isLocalDevelopment: window.location.hostname === '127.0.0.1' || 
                       window.location.hostname === 'localhost' ||
                       window.location.protocol === 'file:',
    
    // Mock data for local development
    mockAlarms: [
        {
            id: 0,
            name: "Morning Alarm",
            enabled: true,
            hour: 7,
            minute: 30,
            days: [false, true, true, true, true, true, false] // Weekdays
        },
        {
            id: 1,
            name: "Weekend Alarm",
            enabled: false,
            hour: 9,
            minute: 0,
            days: [true, false, false, false, false, false, true] // Weekends
        }
    ],
    
    mockBattery: {
        percentage: 85,
        voltage_mv: 3950,
        charging: false,
        status: "normal"
    },
    
    mockTime: new Date().toLocaleString()
};

function loadAlarms() {
    if (config.isLocalDevelopment) {
        // Mock data for local development
        setTimeout(() => {
            alarms = config.mockAlarms;
            document.getElementById('currentTime').textContent = config.mockTime;
            renderAlarms();
            updateBatteryStatus(config.mockBattery);
        }, 500); // Simulate network delay
        return;
    }
    
    fetch('/api/alarms')
        .then(response => response.json())
        .then(data => {
            alarms = data.alarms;
            document.getElementById('currentTime').textContent = data.current_time;
            renderAlarms();
        })
        .catch(error => {
            console.error('Error loading alarms:', error);
            showStatus('Failed to load alarms', 'error');
        });
}

function loadBatteryStatus() {
    if (config.isLocalDevelopment) {
        updateBatteryStatus(config.mockBattery);
        return;
    }
    
    fetch('/api/battery')
        .then(response => response.json())
        .then(data => {
            updateBatteryStatus(data);
        })
        .catch(error => {
            console.error('Error loading battery status:', error);
            // Hide battery status on error
            document.getElementById('batteryStatus').style.display = 'none';
        });
}

function updateBatteryStatus(batteryData) {
    const batteryStatus = document.getElementById('batteryStatus');
    const batteryIcon = document.getElementById('batteryIcon');
    const batteryPercentage = document.getElementById('batteryPercentage');
    
    batteryPercentage.textContent = batteryData.percentage;
    
    // Update battery icon based on percentage and charging status
    if (batteryData.charging) {
        batteryIcon.textContent = 'Battery';  // Use text instead of emoji
    } else if (batteryData.percentage > 75) {
        batteryIcon.textContent = 'Battery';
    } else if (batteryData.percentage > 50) {
        batteryIcon.textContent = 'Battery';
    } else if (batteryData.percentage > 25) {
        batteryIcon.textContent = 'Battery';
    } else {
        batteryIcon.textContent = 'Battery';
    }
    
    // Update status class
    batteryStatus.className = 'battery-status ' + batteryData.status;
}

function renderAlarms() {
    const container = document.getElementById('alarmsContainer');
    container.innerHTML = '';
    
    alarms.forEach((alarm, index) => {
        const alarmCard = document.createElement('div');
        alarmCard.className = 'alarm-card';
        alarmCard.innerHTML = `
            <div class="toggle">
                <h3>Alarm ${alarm.id + 1}</h3>
                <input type="checkbox" id="enabled${index}" ${alarm.enabled ? 'checked' : ''} onchange="toggleAlarm(${index})">
                <label for="enabled${index}">Enabled</label>
                ${alarms.length > 1 ? `<button class="remove-alarm-btn" onclick="removeAlarm(${index})">Remove</button>` : ''}
            </div>
            <div class="form-group">
                <label>Alarm Name</label>
                <input type="text" value="${alarm.name}" onchange="updateAlarmName(${index}, this.value)">
            </div>
            <div class="form-group">
                <label>Time</label>
                <div class="time-input">
                    <input type="number" min="0" max="23" value="${String(alarm.hour).padStart(2, '0')}" onchange="updateAlarmTime(${index}, 'hour', this.value)">
                    <span>:</span>
                    <input type="number" min="0" max="59" value="${String(alarm.minute).padStart(2, '0')}" onchange="updateAlarmTime(${index}, 'minute', this.value)">
                </div>
            </div>
            <div class="form-group">
                <label>Repeat Days</label>
                <div class="days">${days.map((day, dayIndex) => 
                    `<div class="day-btn ${alarm.days[dayIndex] ? 'active' : ''}" onclick="toggleDay(${index}, ${dayIndex})">${day}</div>`
                ).join('')}</div>
            </div>`;
        container.appendChild(alarmCard);
    });
}

function toggleAlarm(index) {
    alarms[index].enabled = !alarms[index].enabled;
    renderAlarms();
}

function updateAlarmName(index, name) {
    alarms[index].name = name;
}

function updateAlarmTime(index, field, value) {
    const numValue = parseInt(value) || 0;
    if (field === 'hour' && (numValue < 0 || numValue > 23)) return;
    if (field === 'minute' && (numValue < 0 || numValue > 59)) return;
    alarms[index][field] = numValue;
}

function toggleDay(index, dayIndex) {
    alarms[index].days[dayIndex] = !alarms[index].days[dayIndex];
    renderAlarms();
}

function addAlarm() {
    const newId = alarms.length > 0 ? Math.max(...alarms.map(a => a.id)) + 1 : 0;
    const newAlarm = {
        id: newId,
        name: `Alarm ${newId + 1}`,
        enabled: false,
        hour: 8,
        minute: 0,
        days: [false, false, false, false, false, false, false] // No days selected initially
    };
    
    alarms.push(newAlarm);
    renderAlarms();
    showStatus('New alarm added!', 'success');
}

function removeAlarm(index) {
    if (alarms.length > 1) {
        alarms.splice(index, 1);
        renderAlarms();
        showStatus('Alarm removed!', 'success');
    }
}

function playSound(soundType) {
    if (config.isLocalDevelopment) {
        showStatus(`Testing ${soundType} sound (Local mode)`, 'success');
        return;
    }
    
    fetch('/api/sound/test', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ sound_type: soundType })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showStatus(`Playing ${soundType} sound...`, 'success');
        } else {
            showStatus(`Failed to play ${soundType} sound`, 'error');
        }
    })
    .catch(error => {
        console.error('Error playing sound:', error);
        showStatus(`Error playing ${soundType} sound`, 'error');
    });
}

function saveAlarms() {
    if (config.isLocalDevelopment) {
        showStatus('Alarms saved successfully! (Local mode)', 'success');
        return;
    }
    
    fetch('/api/alarms', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ alarms: alarms })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showStatus('Alarms saved successfully!', 'success');
        } else {
            showStatus('Failed to save alarms', 'error');
        }
    })
    .catch(error => {
        console.error('Error saving alarms:', error);
        showStatus('Failed to save alarms', 'error');
    });
}

function showStatus(message, type) {
    const status = document.getElementById('status');
    status.textContent = message;
    status.className = 'status ' + type;
    status.style.display = 'block';
    setTimeout(() => {
        status.style.display = 'none';
    }, 3000);
}

function updateTime() {
    if (config.isLocalDevelopment) {
        document.getElementById('currentTime').textContent = new Date().toLocaleString();
        return;
    }
    
    if (document.getElementById('currentTime').textContent !== 'Loading...') {
        fetch('/api/time')
            .then(response => response.json())
            .then(data => {
                document.getElementById('currentTime').textContent = data.current_time;
            })
            .catch(error => {
                console.error('Error updating time:', error);
            });
    }
}

// Initialize the application
function init() {
    loadAlarms();
    loadBatteryStatus();
    
    // Update time every second
    setInterval(updateTime, 1000);
    
    // Update battery status every 30 seconds
    if (!config.isLocalDevelopment) {
        setInterval(loadBatteryStatus, 30000);
    }
}

// Start the application when page loads
document.addEventListener('DOMContentLoaded', init);