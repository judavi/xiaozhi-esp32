#include "http_server.h"
#include "alarm_manager.h"
#include "time_sync.h"
#include "battery_monitor.h"
#include "sound_manager.h"
#include <esp_log.h>
#include <esp_http_server.h>
#include <string.h>
#include <cJSON.h>
#include <sys/param.h>

static const char *TAG = "HTTP_SERVER";
static httpd_handle_t server = NULL;

// Include static files
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[]   asm("_binary_script_js_end");

// Fallback HTML page (in case static files are not embedded)
static const char* html_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<title>ESP32 Alarm Clock</title>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<style>"
"body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }"
".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }"
".header { text-align: center; color: #333; margin-bottom: 30px; }"
".alarm-card { background: #f8f9fa; padding: 20px; margin: 20px 0; border-radius: 10px; border: 1px solid #ddd; }"
".form-group { margin: 15px 0; }"
".form-group label { display: block; margin-bottom: 5px; font-weight: bold; }"
".form-group input { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 5px; }"
".time-input { display: flex; gap: 10px; align-items: center; }"
".time-input input { width: 60px; text-align: center; }"
".days { display: flex; gap: 5px; flex-wrap: wrap; }"
".day-btn { padding: 5px 10px; border: 1px solid #ddd; background: white; cursor: pointer; border-radius: 5px; }"
".day-btn.active { background: #007bff; color: white; }"
".toggle { display: flex; align-items: center; gap: 10px; }"
".save-btn { background: #28a745; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 20px 0; }"
".status { padding: 10px; border-radius: 5px; margin: 10px 0; display: none; }"
".success { background: #d4edda; color: #155724; }"
".error { background: #f8d7da; color: #721c24; }"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<div class='header'>"
"<h1>Alarm Clock Configuration</h1>"
"<div id='currentTime'>Loading...</div>"
"</div>"
"<div id='alarmsContainer'></div>"
"<button class='save-btn' onclick='saveAlarms()'>Save All Alarms</button>"
"<div id='status' class='status'></div>"
"</div>"
"<script>"
"let alarms = [];"
"const days = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];"
"function loadAlarms() {"
"  fetch('/api/alarms')"
"    .then(response => response.json())"
"    .then(data => {"
"      alarms = data.alarms;"
"      document.getElementById('currentTime').textContent = data.current_time;"
"      renderAlarms();"
"    })"
"    .catch(error => {"
"      console.error('Error loading alarms:', error);"
"      showStatus('Failed to load alarms', 'error');"
"    });"
"}"
"function renderAlarms() {"
"  const container = document.getElementById('alarmsContainer');"
"  container.innerHTML = '';"
"  alarms.forEach((alarm, index) => {"
"    const alarmCard = document.createElement('div');"
"    alarmCard.className = 'alarm-card';"
"    alarmCard.innerHTML = '"
"      <div class=\"toggle\">"
"        <h3>' + alarm.name + '</h3>"
"        <input type=\"checkbox\" id=\"enabled' + index + '\" ' + (alarm.enabled ? 'checked' : '') + ' onchange=\"toggleAlarm(' + index + ')\">"
"        <label for=\"enabled' + index + '\">Enabled</label>"
"      </div>"
"      <div class=\"form-group\">"
"        <label>Alarm Name</label>"
"        <input type=\"text\" value=\"' + alarm.name + '\" onchange=\"updateAlarmName(' + index + ', this.value)\">"
"      </div>"
"      <div class=\"form-group\">"
"        <label>Time</label>"
"        <div class=\"time-input\">"
"          <input type=\"number\" min=\"0\" max=\"23\" value=\"' + String(alarm.hour).padStart(2, '0') + '\" onchange=\"updateAlarmTime(' + index + ', 'hour', this.value)\">"
"          <span>:</span>"
"          <input type=\"number\" min=\"0\" max=\"59\" value=\"' + String(alarm.minute).padStart(2, '0') + '\" onchange=\"updateAlarmTime(' + index + ', 'minute', this.value)\">"
"        </div>"
"      </div>"
"      <div class=\"form-group\">"
"        <label>Repeat Days</label>"
"        <div class=\"days\">' + days.map((day, dayIndex) => "
"          '<div class=\"day-btn ' + (alarm.days[dayIndex] ? 'active' : '') + '\" onclick=\"toggleDay(' + index + ', ' + dayIndex + ')\">' + day + '</div>'"
"        ).join('') + '</div>"
"      </div>';"
"    container.appendChild(alarmCard);"
"  });"
"}"
"function toggleAlarm(index) {"
"  alarms[index].enabled = !alarms[index].enabled;"
"  renderAlarms();"
"}"
"function updateAlarmName(index, name) {"
"  alarms[index].name = name;"
"}"
"function updateAlarmTime(index, field, value) {"
"  const numValue = parseInt(value) || 0;"
"  if (field === 'hour' && (numValue < 0 || numValue > 23)) return;"
"  if (field === 'minute' && (numValue < 0 || numValue > 59)) return;"
"  alarms[index][field] = numValue;"
"}"
"function toggleDay(index, dayIndex) {"
"  alarms[index].days[dayIndex] = !alarms[index].days[dayIndex];"
"  renderAlarms();"
"}"
"function saveAlarms() {"
"  fetch('/api/alarms', {"
"    method: 'POST',"
"    headers: {"
"      'Content-Type': 'application/json',"
"    },"
"    body: JSON.stringify({ alarms: alarms })"
"  })"
"  .then(response => response.json())"
"  .then(data => {"
"    if (data.success) {"
"      showStatus('Alarms saved successfully!', 'success');"
"    } else {"
"      showStatus('Failed to save alarms', 'error');"
"    }"
"  })"
"  .catch(error => {"
"    console.error('Error saving alarms:', error);"
"    showStatus('Failed to save alarms', 'error');"
"  });"
"}"
"function showStatus(message, type) {"
"  const status = document.getElementById('status');"
"  status.textContent = message;"
"  status.className = 'status ' + type;"
"  status.style.display = 'block';"
"  setTimeout(() => {"
"    status.style.display = 'none';"
"  }, 3000);"
"}"
"setInterval(() => {"
"  if (document.getElementById('currentTime').textContent !== 'Loading...') {"
"    fetch('/api/time')"
"      .then(response => response.json())"
"      .then(data => {"
"        document.getElementById('currentTime').textContent = data.current_time;"
"      });"
"  }"
"}, 1000);"
"loadAlarms();"
"</script>"
"</body>"
"</html>";

// HTTP handler for the main page
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    
    // Try to serve embedded static file first
    const char* data = (const char*)index_html_start;
    size_t data_len = index_html_end - index_html_start;
    
    if (data_len > 0) {
        httpd_resp_send(req, data, data_len);
    } else {
        // Fallback to inline HTML
        httpd_resp_send(req, html_page, strlen(html_page));
    }
    return ESP_OK;
}

// HTTP handler for CSS file
static esp_err_t css_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    
    const char* data = (const char*)style_css_start;
    size_t data_len = style_css_end - style_css_start;
    
    if (data_len > 0) {
        httpd_resp_send(req, data, data_len);
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "CSS file not found");
    }
    return ESP_OK;
}

// HTTP handler for JavaScript file
static esp_err_t js_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    
    const char* data = (const char*)script_js_start;
    size_t data_len = script_js_end - script_js_start;
    
    if (data_len > 0) {
        httpd_resp_send(req, data, data_len);
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "JavaScript file not found");
    }
    return ESP_OK;
}

// HTTP handler for getting alarms JSON
static esp_err_t get_alarms_handler(httpd_req_t *req) {
    char* json_data = alarm_manager_get_json();
    if (json_data) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, json_data, strlen(json_data));
        free(json_data);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get alarms");
    }
    return ESP_OK;
}

// HTTP handler for setting alarms JSON
static esp_err_t set_alarms_handler(httpd_req_t *req) {
    char content[1024];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
    
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    
    content[ret] = '\0';
    
    bool success = alarm_manager_set_from_json(content);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    if (success) {
        httpd_resp_send(req, "{\"success\":true}", 16);
    } else {
        httpd_resp_send(req, "{\"success\":false}", 17);
    }
    
    return ESP_OK;
}

// HTTP handler for getting current time
static esp_err_t get_time_handler(httpd_req_t *req) {
    char time_str[64];
    time_sync_get_time_string(time_str, sizeof(time_str));
    
    char response[128];
    snprintf(response, sizeof(response), "{\"current_time\":\"%s\",\"time_synced\":%s}", 
             time_str, time_sync_is_ready() ? "true" : "false");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

// HTTP handler for getting battery status
static esp_err_t get_battery_handler(httpd_req_t *req) {
    char* json_data = battery_monitor_get_json();
    if (json_data) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, json_data, strlen(json_data));
        free(json_data);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get battery status");
    }
    return ESP_OK;
}

// HTTP handler for testing sounds
static esp_err_t test_sound_handler(httpd_req_t *req) {
    char content[256];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
    
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    
    content[ret] = '\0';
    
    // Parse JSON to get sound type
    cJSON *root = cJSON_Parse(content);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *sound_type = cJSON_GetObjectItem(root, "sound_type");
    if (!cJSON_IsString(sound_type)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing sound_type");
        return ESP_FAIL;
    }
    
    // Test the sound
    bool success = sound_manager_test_sound(sound_type->valuestring);
    char* json_response = sound_manager_get_test_result_json(success, sound_type->valuestring);
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    if (json_response) {
        httpd_resp_send(req, json_response, strlen(json_response));
        free(json_response);
    } else {
        httpd_resp_send(req, "{\"success\":false,\"message\":\"Failed to create response\"}", 59);
    }
    
    return ESP_OK;
}

// HTTP handler for volume control
static esp_err_t volume_handler(httpd_req_t *req) {
    char content[256];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
    
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    
    content[ret] = '\0';
    
    // Parse JSON to get volume level
    cJSON *root = cJSON_Parse(content);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *volume = cJSON_GetObjectItem(root, "volume");
    if (!cJSON_IsNumber(volume)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing volume");
        return ESP_FAIL;
    }
    
    int volume_level = volume->valueint;
    if (volume_level < 0 || volume_level > 100) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Volume must be 0-100");
        return ESP_FAIL;
    }
    
    // Set volume using audio codec
    bool success = false;
    audio_codec_handle_t codec_handle = sound_manager_get_codec_handle();
    if (codec_handle) {
        esp_err_t err = audio_codec_set_volume(codec_handle, volume_level);
        success = (err == ESP_OK);
    }
    
    cJSON_Delete(root);
    
    // Create response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", success);
    cJSON_AddNumberToObject(response, "volume", volume_level);
    cJSON_AddStringToObject(response, "message", 
        success ? "Volume set successfully" : "Failed to set volume");
    
    char *json_response = cJSON_Print(response);
    cJSON_Delete(response);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    if (json_response) {
        httpd_resp_send(req, json_response, strlen(json_response));
        free(json_response);
    } else {
        httpd_resp_send(req, "{\"success\":false,\"message\":\"Failed to create response\"}", 59);
    }
    
    ESP_LOGI(TAG, "🔊 Volume set to %d%%", volume_level);
    return ESP_OK;
}

void http_server_start(void) {
    ESP_LOGI(TAG, "🌐 Starting HTTP Server...");
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 15;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        
        httpd_uri_t get_alarms_uri = {
            .uri = "/api/alarms",
            .method = HTTP_GET,
            .handler = get_alarms_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &get_alarms_uri);
        
        httpd_uri_t set_alarms_uri = {
            .uri = "/api/alarms",
            .method = HTTP_POST,
            .handler = set_alarms_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &set_alarms_uri);
        
        httpd_uri_t get_time_uri = {
            .uri = "/api/time",
            .method = HTTP_GET,
            .handler = get_time_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &get_time_uri);
        
        httpd_uri_t get_battery_uri = {
            .uri = "/api/battery",
            .method = HTTP_GET,
            .handler = get_battery_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &get_battery_uri);
        
        httpd_uri_t css_uri = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = css_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &css_uri);
        
        httpd_uri_t js_uri = {
            .uri = "/script.js",
            .method = HTTP_GET,
            .handler = js_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &js_uri);
        
        httpd_uri_t test_sound_uri = {
            .uri = "/api/sound/test",
            .method = HTTP_POST,
            .handler = test_sound_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &test_sound_uri);
        
        httpd_uri_t volume_uri = {
            .uri = "/api/volume",
            .method = HTTP_POST,
            .handler = volume_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &volume_uri);
        
        ESP_LOGI(TAG, "✅ HTTP Server started successfully on port 80");
        ESP_LOGI(TAG, "🔗 Access the alarm interface at: http://<your-esp32-ip>/");
    } else {
        ESP_LOGE(TAG, "❌ Failed to start HTTP Server");
    }
}

void http_server_stop(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "🛑 HTTP Server stopped");
    }
} 