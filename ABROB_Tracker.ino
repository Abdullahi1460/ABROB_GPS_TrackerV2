#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ========== Configuration ==========
// SIM800L Configuration
#define SIM800L_RX 16
#define SIM800L_TX 17
HardwareSerial SIM800L(1);
String PHONE_NUMBER = "";  // Will be set via web interface

// GPS Configuration
#define GPS_RX 4
#define GPS_TX 5
HardwareSerial GPS(2);
TinyGPSPlus gps;

// Button Configuration
#define BUTTON_PIN 13
#define CONTROL_PIN 45
const char* DEFAULT_SSID = "mr-robot";
const char* DEFAULT_PASSWORD = "123456789";

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI 42
#define OLED_CLK 18
#define OLED_DC 20
#define OLED_CS 21
#define OLED_RESET 14
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
 OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// Web Server Configuration (only used in WEB_CONFIG mode)
WebServer server(80);
Preferences preferences;

// Security Configuration
#define SECURITY_CODE_LENGTH 6
String securityCode = ""; // Global variable to store security code
bool securityEnabled = true; // Flag to enable/disable security feature

// ========== Global Variables ==========
// Operation Mode
enum OperationMode { NORMAL, WEB_CONFIG };
OperationMode currentMode = NORMAL;

// GPS Data Variables
float latitude = 0.0;
float longitude = 0.0;
String lastValidGPSData = "";
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 1000;
unsigned long lastSMSCheck = 0;
const unsigned long smsCheckInterval = 5000; // Check for SMS every 5 seconds
String commandKeyword = "location"; // Base command (now requxires security code)

// Button Press Tracking
unsigned long buttonPressStartTime = 0;
const unsigned long longPressDuration = 3000; // 3 seconds for long press

// WiFi Credentials
String wifi_SSID;
String wifi_Password;
String serial_no;
String reg_no;
String full_name;

// Login Page
const char login_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Login</title>
    <style>
        :root {
            --primary: #2c3e50;
            --accent: #e74c3c;
            --success: #27ae60;
            --warning: #f39c12;
            --background: #ecf0f1;
        }
        body {
            font-family: Arial, sans-serif;
            background-color: var(--background);
            color: #333;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
        }
        .login-container {
            background-color: white;
            padding: 40px;
            border-radius: 8px;
            box-shadow: 0 4px 8px rgba(0,0,0,0.1);
            width: 100%;
            max-width: 400px;
        }
        h1 {
            color: var(--primary);
            text-align: center;
            margin-bottom: 30px;
        }
        input {
            width: 100%;
            padding: 12px;
            margin-bottom: 15px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 16px;
        }
        button {
            width: 100%;
            padding: 12px;
            background-color: var(--accent);
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 16px;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        button:hover {
            background-color: #c0392b;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: var(--primary);
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="login-container">
        <h1>GPS Tracker Login</h1>
        <form action="/submit" method="get">
            <div class="form-group">
                <label for="username">Username:</label>
                <input type="text" id="username" name="username" required>
            </div>
            <div class="form-group">
                <label for="password">Password:</label>
                <input type="password" id="password" name="password" required>
            </div>
            <button type="submit">Login</button>
        </form>
    </div>
</body>
</html>
)rawliteral";

// Dashboard Page
const char dashboard_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dashboard</title>
    <style>
        :root {
            --primary: #2c3e50;
            --accent: #e74c3c;
            --success: #27ae60;
            --warning: #f39c12;
            --background: #ecf0f1;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: Arial, sans-serif;
            background-color: var(--background);
            color: #333;
        }
        header {
            background-color: var(--primary);
            color: white;
            padding: 15px 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .menu-toggle {
            cursor: pointer;
            font-size: 20px;
        }
        aside {
            background-color: white;
            width: 250px;
            height: 100vh;
            position: fixed;
            top: 0;
            left: -250px;
            transition: left 0.3s;
            box-shadow: 2px 0 5px rgba(0,0,0,0.1);
            padding-top: 60px;
        }
        aside.open {
            left: 0;
        }
        nav ul {
            list-style: none;
        }
        nav ul li {
            padding: 15px 20px;
            border-bottom: 1px solid #eee;
        }
        nav ul li a {
            color: var(--primary);
            text-decoration: none;
            display: block;
        }
        nav ul li.active {
            background-color: var(--accent);
        }
        nav ul li.active a {
            color: white;
        }
        main {
            padding: 20px;
            margin-left: 0;
            transition: margin-left 0.3s;
        }
        aside.open + main {
            margin-left: 250px;
        }
        .status-cards {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }
        .card {
            background-color: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .card h3 {
            color: var(--primary);
            margin-bottom: 15px;
        }
        .status-item {
            display: flex;
            justify-content: space-between;
            margin-bottom: 10px;
        }
        .status-value {
            font-weight: bold;
        }
        .status-online {
            color: var(--success);
        }
        .status-offline {
            color: var(--accent);
        }
        .status-warning {
            color: var(--warning);
        }
        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
            gap: 20px;
            margin-top: 30px;
        }
        .dashboard-card {
            background-color: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            transition: transform 0.3s;
            cursor: pointer;
        }
        .dashboard-card:hover {
            transform: translateY(-5px);
        }
        .dashboard-card h3 {
            color: var(--primary);
            margin-bottom: 10px;
        }
        @media (max-width: 768px) {
            aside {
                width: 100%;
                left: -100%;
            }
            aside.open + main {
                margin-left: 0;
            }
        }
    </style>
</head>
<body>
    <header>
        <h2>GPS Tracker Dashboard</h2>
        <div class="menu-toggle">☰</div>
    </header>
    <aside>
        <nav>
            <ul>
                <li class="active"><a href="/dashboard">Dashboard</a></li>
                <li><a href="/map">Location Map</a></li>
                <li><a href="/settings">Settings</a></li>
                <li><a href="/user-info">User Info</a></li>
                <li><a href="/wifi-config">WiFi Config</a></li>
            </ul>
        </nav>
    </aside>
    <main>
        <h1>Welcome to GPS Tracker</h1>
        <p>Monitor and manage your GPS tracking device</p>
        
        <div class="status-cards">
            <div class="card">
                <h3>Device Status</h3>
                <div class="status-item">
                    <span>GPS:</span>
                    <span class="status-value status-online">Active</span>
                </div>
                <div class="status-item">
                    <span>WiFi:</span>
                    <span class="status-value status-online">Connected</span>
                </div>
                <div class="status-item">
                    <span>Battery:</span>
                    <span class="status-value status-warning">78%</span>
                </div>
            </div>
            
            <div class="card">
                <h3>Current Location</h3>
                <div class="status-item">
                    <span>Latitude:</span>
                    <span class="status-value">40.7128° N</span>
                </div>
                <div class="status-item">
                    <span>Longitude:</span>
                    <span class="status-value">74.0060° W</span>
                </div>
                <div class="status-item">
                    <span>Last Update:</span>
                    <span class="status-value">2 min ago</span>
                </div>
            </div>
        </div>
        
        <div class="dashboard-grid">
            <a href="/map" class="dashboard-card">
                <h3>Location Map</h3>
                <p>View current GPS location and tracking history</p>
            </a>
            
            <a href="/settings" class="dashboard-card">
                <h3>Device Settings</h3>
                <p>Configure GPS and device parameters</p>
            </a>
            
            <a href="/user-info" class="dashboard-card">
                <h3>User Information</h3>
                <p>View and update your profile</p>
            </a>
            
            <a href="/wifi-config" class="dashboard-card">
                <h3>WiFi Config</h3>
                <p>Configure network settings</p>
            </a>
        </div>
    </main>
    <script>
        document.querySelector('.menu-toggle').addEventListener('click', function() {
            document.querySelector('aside').classList.toggle('open');
        });
    </script>
</body>
</html>
)rawliteral";

// Map Page
const char map_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Location Map</title>
    <style>
        :root {
            --primary: #2c3e50;
            --accent: #e74c3c;
            --success: #27ae60;
            --warning: #f39c12;
            --background: #ecf0f1;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: Arial, sans-serif;
            background-color: var(--background);
            color: #333;
        }
        header {
            background-color: var(--primary);
            color: white;
            padding: 15px 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .menu-toggle {
            cursor: pointer;
            font-size: 20px;
        }
        aside {
            background-color: white;
            width: 250px;
            height: 100vh;
            position: fixed;
            top: 0;
            left: -250px;
            transition: left 0.3s;
            box-shadow: 2px 0 5px rgba(0,0,0,0.1);
            padding-top: 60px;
        }
        aside.open {
            left: 0;
        }
        nav ul {
            list-style: none;
        }
        nav ul li {
            padding: 15px 20px;
            border-bottom: 1px solid #eee;
        }
        nav ul li a {
            color: var(--primary);
            text-decoration: none;
            display: block;
        }
        nav ul li.active {
            background-color: var(--accent);
        }
        nav ul li.active a {
            color: white;
        }
        main {
            padding: 20px;
            margin-left: 0;
            transition: margin-left 0.3s;
        }
        aside.open + main {
            margin-left: 250px;
        }
        .map-container {
            background-color: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            margin-top: 20px;
            height: 500px;
            display: flex;
            justify-content: center;
            align-items: center;
            background-color: #f9f9f9;
        }
        .map-placeholder {
            text-align: center;
            color: #666;
        }
        .map-controls {
            display: flex;
            gap: 10px;
            margin-top: 15px;
        }
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
        }
        .btn-primary {
            background-color: var(--accent);
            color: white;
        }
        .btn-secondary {
            background-color: var(--primary);
            color: white;
        }
        @media (max-width: 768px) {
            aside {
                width: 100%;
                left: -100%;
            }
            aside.open + main {
                margin-left: 0;
            }
        }
    </style>
</head>
<body>
    <header>
        <h2>Location Map</h2>
        <div class="menu-toggle">☰</div>
    </header>
    <aside>
        <nav>
            <ul>
                <li><a href="/dashboard">Dashboard</a></li>
                <li class="active"><a href="/map">Location Map</a></li>
                <li><a href="/settings">Settings</a></li>
                <li><a href="/user-info">User Info</a></li>
                <li><a href="/wifi-config">WiFi Config</a></li>
            </ul>
        </nav>
    </aside>
    <main>
        <h1>GPS Location Tracking</h1>
        <p>View your device's current location and tracking history</p>
        
        <div class="map-container">
            <div class="map-placeholder">
                <h3>Map View</h3>
                <p>Map functionality would be displayed here</p>
                <p>Current location: 40.7128° N, 74.0060° W</p>
            </div>
        </div>
        
        <div class="map-controls">
            <button class="btn btn-primary" id="refresh-btn">Refresh Location</button>
            <button class="btn btn-secondary" id="history-btn">View History</button>
            <a href="/dashboard" class="btn btn-secondary">Back to Dashboard</a>
        </div>
    </main>
    <script>
        document.querySelector('.menu-toggle').addEventListener('click', function() {
            document.querySelector('aside').classList.toggle('open');
        });
        
        document.getElementById('refresh-btn').addEventListener('click', function() {
            alert('Refreshing location data...');
        });
        
        document.getElementById('history-btn').addEventListener('click', function() {
            alert('Showing tracking history...');
        });
    </script>
</body>
</html>
)rawliteral";

// Updated Settings Page with Security Tab
const char settings_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Device Settings</title>
    <style>
        :root {
            --primary: #2c3e50;
            --accent: #e74c3c;
            --success: #27ae60;
            --warning: #f39c12;
            --background: #ecf0f1;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: Arial, sans-serif;
            background-color: var(--background);
            color: #333;
        }
        header {
            background-color: var(--primary);
            color: white;
            padding: 15px 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .menu-toggle {
            cursor: pointer;
            font-size: 20px;
        }
        aside {
            background-color: white;
            width: 250px;
            height: 100vh;
            position: fixed;
            top: 0;
            left: -250px;
            transition: left 0.3s;
            box-shadow: 2px 0 5px rgba(0,0,0,0.1);
            padding-top: 60px;
        }
        aside.open {
            left: 0;
        }
        nav ul {
            list-style: none;
        }
        nav ul li {
            padding: 15px 20px;
            border-bottom: 1px solid #eee;
        }
        nav ul li a {
            color: var(--primary);
            text-decoration: none;
            display: block;
        }
        nav ul li.active {
            background-color: var(--accent);
        }
        nav ul li.active a {
            color: white;
        }
        main {
            padding: 20px;
            margin-left: 0;
            transition: margin-left 0.3s;
        }
        aside.open + main {
            margin-left: 250px;
        }
        .settings-container {
            background-color: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            margin-top: 20px;
        }
        .tabs {
            display: flex;
            border-bottom: 1px solid #ddd;
            margin-bottom: 20px;
        }
        .tab {
            padding: 10px 20px;
            cursor: pointer;
            border-bottom: 3px solid transparent;
        }
        .tab.active {
            border-bottom-color: var(--accent);
            color: var(--accent);
            font-weight: bold;
        }
        .tab-content {
            display: none;
        }
        .tab-content.active {
            display: block;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: var(--primary);
        }
        input, select {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 16px;
        }
        .btn {
            padding: 10px 20px;
            background-color: var(--accent);
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            margin-top: 10px;
        }
        .security-code-display {
            font-size: 24px;
            font-weight: bold;
            text-align: center;
            margin: 20px 0;
            padding: 15px;
            background-color: #f5f5f5;
            border-radius: 4px;
            letter-spacing: 3px;
        }
        .toggle-switch {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
        }
        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 34px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        input:checked + .slider {
            background-color: var(--accent);
        }
        input:checked + .slider:before {
            transform: translateX(26px);
        }
        .toggle-label {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        @media (max-width: 768px) {
            aside {
                width: 100%;
                left: -100%;
            }
            aside.open + main {
                margin-left: 0;
            }
        }
    </style>
</head>
<body>
    <header>
        <h2>Device Settings</h2>
        <div class="menu-toggle">☰</div>
    </header>
    <aside>
        <nav>
            <ul>
                <li><a href="/dashboard">Dashboard</a></li>
                <li><a href="/map">Location Map</a></li>
                <li class="active"><a href="/settings">Settings</a></li>
                <li><a href="/user-info">User Info</a></li>
                <li><a href="/wifi-config">WiFi Config</a></li>
            </ul>
        </nav>
    </aside>
    <main>
        <h1>Device Configuration</h1>
        <p>Adjust your tracker's settings and preferences</p>
        
        <div class="settings-container">
            <div class="tabs">
                <div class="tab active" data-tab="general">General</div>
                <div class="tab" data-tab="gps">GPS</div>
                <div class="tab" data-tab="power">Power</div>
                <div class="tab" data-tab="security">Security</div>
            </div>
            
            <div id="general" class="tab-content active">
                <form>
                    <div class="form-group">
                        <label for="device-name">Device Name</label>
                        <input type="text" id="device-name" value="GPS Tracker 01">
                    </div>
                    <div class="form-group">
                        <label for="update-interval">Update Interval (seconds)</label>
                        <input type="number" id="update-interval" value="30" min="5" max="300">
                    </div>
                    <button type="button" class="btn">Save General Settings</button>
                </form>
            </div>
            
            <div id="gps" class="tab-content">
                <form>
                    <div class="form-group">
                        <label for="gps-mode">GPS Mode</label>
                        <select id="gps-mode">
                            <option value="normal">Normal</option>
                            <option value="powersave">Power Save</option>
                            <option value="performance">Performance</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label for="gps-update">GPS Update Rate (Hz)</label>
                        <input type="number" id="gps-update" value="1" min="1" max="10">
                    </div>
                    <button type="button" class="btn">Save GPS Settings</button>
                </form>
            </div>
            
            <div id="power" class="tab-content">
                <form>
                    <div class="form-group">
                        <label for="sleep-mode">Sleep Mode</label>
                        <select id="sleep-mode">
                            <option value="none">Disabled</option>
                            <option value="light">Light Sleep</option>
                            <option value="deep">Deep Sleep</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label for="battery-mode">Battery Mode</label>
                        <select id="battery-mode">
                            <option value="balanced">Balanced</option>
                            <option value="max-battery">Maximum Battery</option>
                            <option value="max-performance">Maximum Performance</option>
                        </select>
                    </div>
                    <button type="button" class="btn">Save Power Settings</button>
                </form>
            </div>
            
            <div id="security" class="tab-content">
                <form id="security-form" action="/security-settings" method="post">
                    <div class="form-group">
                        <label class="toggle-label">
                            <span>Security Feature:</span>
                            <label class="toggle-switch">
                                <input type="checkbox" id="security-enabled" name="security_enabled" %SECURITY_CHECKED%>
                                <span class="slider"></span>
                            </label>
                        </label>
                    </div>
                    
                    <div class="form-group">
                        <label>Current Security Code:</label>
                        <div class="security-code-display" id="current-code">%SECURITY_CODE%</div>
                    </div>
                    
                    <div class="form-group">
                        <button type="button" class="btn" id="generate-btn">Generate New Code</button>
                    </div>
                    
                    <div class="form-group">
                        <label for="custom-code">Or set custom code:</label>
                        <input type="text" id="custom-code" name="custom_code" maxlength="6" pattern="[A-Z0-9]{6}" 
                               title="6-character alphanumeric code" placeholder="Enter 6-character code">
                    </div>
                    
                    <button type="submit" class="btn">Save Security Settings</button>
                </form>
            </div>
        </div>
    </main>
    <script>
        document.querySelector('.menu-toggle').addEventListener('click', function() {
            document.querySelector('aside').classList.toggle('open');
        });
        
        document.querySelectorAll('.tab').forEach(tab => {
            tab.addEventListener('click', function() {
                // Remove active class from all tabs and content
                document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
                document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
                
                // Add active class to clicked tab
                this.classList.add('active');
                
                // Show corresponding content
                const tabId = this.getAttribute('data-tab');
                document.getElementById(tabId).classList.add('active');
            });
        });
        
        // Generate random security code
        document.getElementById('generate-btn').addEventListener('click', function() {
            const chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ';
            let result = '';
            for (let i = 0; i < 6; i++) {
                result += chars.charAt(Math.floor(Math.random() * chars.length));
            }
            document.getElementById('custom-code').value = result;
            document.getElementById('current-code').textContent = result;
        });
        
        // Form submission
        document.getElementById('security-form').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const formData = new FormData(this);
            fetch('/security-settings', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (response.ok) {
                    alert('Security settings saved successfully!');
                    // Update the displayed code
                    const customCode = document.getElementById('custom-code').value;
                    if (customCode) {
                        document.getElementById('current-code').textContent = customCode;
                    }
                } else {
                    alert('Error saving settings. Please try again.');
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Error saving settings. Please try again.');
            });
        });
    </script>
</body>
</html>
)rawliteral";
// User Info Page
const char user_info_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>User Information</title>
    <style>
        :root {
            --primary: #2c3e50;
            --accent: #e74c3c;
            --success: #27ae60;
            --warning: #f39c12;
            --background: #ecf0f1;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: Arial, sans-serif;
            background-color: var(--background);
            color: #333;
        }
        header {
            background-color: var(--primary);
            color: white;
            padding: 15px 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .menu-toggle {
            cursor: pointer;
            font-size: 20px;
        }
        aside {
            background-color: white;
            width: 250px;
            height: 100vh;
            position: fixed;
            top: 0;
            left: -250px;
            transition: left 0.3s;
            box-shadow: 2px 0 5px rgba(0,0,0,0.1);
            padding-top: 60px;
        }
        aside.open {
            left: 0;
        }
        nav ul {
            list-style: none;
        }
        nav ul li {
            padding: 15px 20px;
            border-bottom: 1px solid #eee;
        }
        nav ul li a {
            color: var(--primary);
            text-decoration: none;
            display: block;
        }
        nav ul li.active {
            background-color: var(--accent);
        }
        nav ul li.active a {
            color: white;
        }
        main {
            padding: 20px;
            margin-left: 0;
            transition: margin-left 0.3s;
        }
        aside.open + main {
            margin-left: 250px;
        }
        .user-container {
            background-color: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            margin-top: 20px;
            max-width: 600px;
        }
        .user-avatar {
            width: 100px;
            height: 100px;
            border-radius: 50%;
            background-color: #ddd;
            display: flex;
            justify-content: center;
            align-items: center;
            margin: 0 auto 20px;
            font-size: 36px;
            color: #666;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: var(--primary);
        }
        input {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 16px;
        }
        .btn {
            padding: 10px 20px;
            background-color: var(--accent);
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            margin-top: 10px;
        }
        .section-title {
            color: var(--primary);
            margin: 20px 0 10px;
            padding-bottom: 5px;
            border-bottom: 1px solid #eee;
        }
        .status-message {
            padding: 10px;
            margin: 10px 0;
            border-radius: 4px;
            display: none;
        }
        .status-success {
            background-color: #d4edda;
            color: #155724;
        }
        .status-error {
            background-color: #f8d7da;
            color: #721c24;
        }
        @media (max-width: 768px) {
            aside {
                width: 100%;
                left: -100%;
            }
            aside.open + main {
                margin-left: 0;
            }
        }
    </style>
</head>
<body>
    <header>
        <h2>User Information</h2>
        <div class="menu-toggle">☰</div>
    </header>
    <aside>
        <nav>
            <ul>
                <li><a href="/dashboard">Dashboard</a></li>
                <li><a href="/map">Location Map</a></li>
                <li><a href="/settings">Settings</a></li>
                <li class="active"><a href="/user-info">User Info</a></li>
                <li><a href="/wifi-config">WiFi Config</a></li>
            </ul>
        </nav>
    </aside>
    <main>
        <h1>Your Profile</h1>
        <p>Update your account information and emergency contacts</p>
        
        <div id="status-message" class="status-message"></div>
        
        <div class="user-container">
            <div class="user-avatar">%INITIALS%</div>
            
            <form id="user-form" action="/user-info-submit" method="post">
                <h3 class="section-title">Account Information</h3>
                <div class="form-group">
                    <label for="username">Username</label>
                    <input type="text" id="username" name="username" value="%USERNAME%" readonly>
                </div>
                <div class="form-group">
                    <label for="fullname">Full Name</label>
                    <input type="text" id="fullname" name="fullname" value="%FULLNAME%" required>
                </div>
                <div class="form-group">
                    <label for="email">Email Address</label>
                    <input type="email" id="email" name="email" value="%EMAIL%" required>
                </div>
                
                <h3 class="section-title">Contact Numbers</h3>
                <div class="form-group">
                    <label for="user-phone">Your Phone Number</label>
                    <input type="tel" id="user-phone" name="user_phone" value="%USER_PHONE%" placeholder="+1234567890" required>
                </div>
                <div class="form-group">
                    <label for="kin1-phone">Next of Kin #1 Phone</label>
                    <input type="tel" id="kin1-phone" name="kin1_phone" value="%KIN1_PHONE%" placeholder="+1234567890" required>
                </div>
                <div class="form-group">
                    <label for="kin2-phone">Next of Kin #2 Phone</label>
                    <input type="tel" id="kin2-phone" name="kin2_phone" value="%KIN2_PHONE%" placeholder="+1234567890">
                </div>
                
                <h3 class="section-title">Password Change</h3>
                <div class="form-group">
                    <label for="current-password">Current Password</label>
                    <input type="password" id="current-password" name="current_password" placeholder="Leave blank to keep current">
                </div>
                <div class="form-group">
                    <label for="new-password">New Password</label>
                    <input type="password" id="new-password" name="new_password" placeholder="Leave blank to keep current">
                </div>
                <div class="form-group">
                    <label for="confirm-password">Confirm Password</label>
                    <input type="password" id="confirm-password" name="confirm_password" placeholder="Leave blank to keep current">
                </div>
                
                <button type="submit" class="btn">Update Profile</button>
            </form>
        </div>
    </main>
    <script>
        document.querySelector('.menu-toggle').addEventListener('click', function() {
            document.querySelector('aside').classList.toggle('open');
        });
        
        // Show status message if present in URL
        const urlParams = new URLSearchParams(window.location.search);
        const status = urlParams.get('status');
        const message = urlParams.get('message');
        
        if (status && message) {
            const statusElement = document.getElementById('status-message');
            statusElement.textContent = decodeURIComponent(message);
            statusElement.classList.add(`status-${status}`);
            statusElement.style.display = 'block';
            
            // Remove message after 5 seconds
            setTimeout(() => {
                statusElement.style.display = 'none';
            }, 5000);
        }
        
        // Form submission
        document.getElementById('user-form').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const newPassword = document.getElementById('new-password').value;
            const confirmPassword = document.getElementById('confirm-password').value;
            
            if (newPassword !== confirmPassword) {
                alert('New passwords do not match!');
                return;
            }
            
            const formData = new FormData(this);
            fetch('/user-info-submit', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (response.redirected) {
                    window.location.href = response.url;
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Error updating profile. Please try again.');
            });
        });
        
        // Generate initials from full name
        function getInitials(name) {
            return name.split(' ').map(n => n[0]).join('').toUpperCase();
        }
        
        // Update avatar initials when name changes
        document.getElementById('fullname').addEventListener('input', function() {
            const initials = getInitials(this.value) || 'AD';
            document.querySelector('.user-avatar').textContent = initials;
        });
    </script>
</body>
</html>
)rawliteral";

// WiFi Config Page
const char wifi_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Configuration</title>
    <style>
        :root {
            --primary: #2c3e50;
            --accent: #e74c3c;
            --success: #27ae60;
            --warning: #f39c12;
            --background: #ecf0f1;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: Arial, sans-serif;
            background-color: var(--background);
            color: #333;
        }
        header {
            background-color: var(--primary);
            color: white;
            padding: 15px 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .menu-toggle {
            cursor: pointer;
            font-size: 20px;
        }
        aside {
            background-color: white;
            width: 250px;
            height: 100vh;
            position: fixed;
            top: 0;
            left: -250px;
            transition: left 0.3s;
            box-shadow: 2px 0 5px rgba(0,0,0,0.1);
            padding-top: 60px;
        }
        aside.open {
            left: 0;
        }
        nav ul {
            list-style: none;
        }
        nav ul li {
            padding: 15px 20px;
            border-bottom: 1px solid #eee;
        }
        nav ul li a {
            color: var(--primary);
            text-decoration: none;
            display: block;
        }
        nav ul li.active {
            background-color: var(--accent);
        }
        nav ul li.active a {
            color: white;
        }
        main {
            padding: 20px;
            margin-left: 0;
            transition: margin-left 0.3s;
        }
        aside.open + main {
            margin-left: 250px;
        }
        .wifi-container {
            background-color: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            margin-top: 20px;
            max-width: 600px;
        }
        .wifi-status {
            background-color: #f9f9f9;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
        }
        .wifi-status h3 {
            color: var(--primary);
            margin-bottom: 10px;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: var(--primary);
        }
        input, select {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 16px;
        }
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            margin-top: 10px;
        }
        .btn-primary {
            background-color: var(--accent);
            color: white;
        }
        .btn-danger {
            background-color: #dc3545;
            color: white;
            float: right;
        }
        .networks-list {
            margin-top: 15px;
            display: none;
        }
        .network-item {
            padding: 10px;
            border-bottom: 1px solid #eee;
            cursor: pointer;
        }
        .network-item:hover {
            background-color: #f5f5f5;
        }
        .network-strength {
            height: 10px;
            background-color: #ddd;
            margin-top: 5px;
            position: relative;
        }
        .network-strength-bar {
            height: 100%;
            background-color: var(--success);
            position: absolute;
            top: 0;
            left: 0;
        }
        .status-message {
            padding: 10px;
            margin: 10px 0;
            border-radius: 4px;
            display: none;
        }
        .status-success {
            background-color: #d4edda;
            color: #155724;
        }
        .status-error {
            background-color: #f8d7da;
            color: #721c24;
        }
        @media (max-width: 768px) {
            aside {
                width: 100%;
                left: -100%;
            }
            aside.open + main {
                margin-left: 0;
            }
        }
    </style>
</head>
<body>
    <header>
        <h2>WiFi Configuration</h2>
        <div class="menu-toggle">☰</div>
    </header>
    <aside>
        <nav>
            <ul>
                <li><a href="/dashboard">Dashboard</a></li>
                <li><a href="/map">Location Map</a></li>
                <li><a href="/settings">Settings</a></li>
                <li><a href="/user-info">User Info</a></li>
                <li class="active"><a href="/wifi-config">WiFi Config</a></li>
            </ul>
        </nav>
    </aside>
    <main>
        <h1>Network Settings</h1>
        <p>Configure your device's WiFi connection</p>
        
        <div id="status-message" class="status-message"></div>
        
        <div class="wifi-container">
            <div class="wifi-status">
                <h3>Current Connection</h3>
                <p><strong>SSID:</strong> %CURRENT_SSID%</p>
                <p><strong>IP Address:</strong> %IP_ADDRESS%</p>
                <p><strong>Signal Strength:</strong> %SIGNAL_STRENGTH%</p>
            </div>
            
            <form id="wifi-form" action="/wifi-config-submit" method="post">
                <div class="form-group">
                    <label for="ssid">Network SSID</label>
                    <input type="text" id="ssid" name="ssid" placeholder="Enter WiFi network name" value="%CURRENT_SSID%" required>
                </div>
                <div class="form-group">
                    <label for="password">Password</label>
                    <input type="password" id="password" name="password" placeholder="Enter WiFi password">
                </div>
                
                <button type="button" class="btn btn-primary" id="scan-btn">Scan Networks</button>
                
                <div class="networks-list" id="networks-list">
                    <h3>Available Networks</h3>
                    <!-- Networks will be populated by JavaScript -->
                </div>
                
                <div style="margin-top: 20px;">
                    <button type="submit" class="btn btn-primary">Save Configuration</button>
                    <button type="button" class="btn btn-danger" id="forget-btn">Forget Network</button>
                </div>
            </form>
        </div>
    </main>
    <script>
        document.querySelector('.menu-toggle').addEventListener('click', function() {
            document.querySelector('aside').classList.toggle('open');
        });
        
        // Show status message if present in URL
        const urlParams = new URLSearchParams(window.location.search);
        const status = urlParams.get('status');
        const message = urlParams.get('message');
        
        if (status && message) {
            const statusElement = document.getElementById('status-message');
            statusElement.textContent = decodeURIComponent(message);
            statusElement.classList.add(`status-${status}`);
            statusElement.style.display = 'block';
            
            // Remove message after 5 seconds
            setTimeout(() => {
                statusElement.style.display = 'none';
            }, 5000);
        }
        
        // Network scanning functionality
        document.getElementById('scan-btn').addEventListener('click', function() {
            const networksList = document.getElementById('networks-list');
            networksList.style.display = networksList.style.display === 'none' ? 'block' : 'none';
            
            if (networksList.style.display === 'block') {
                fetch('/scan-wifi')
                    .then(response => response.json())
                    .then(networks => {
                        networksList.innerHTML = '<h3>Available Networks</h3>';
                        networks.forEach(network => {
                            const networkItem = document.createElement('div');
                            networkItem.className = 'network-item';
                            networkItem.setAttribute('data-ssid', network.ssid);
                            
                            const strength = Math.min(100, Math.max(0, (network.rssi + 100) * 2));
                            
                            networkItem.innerHTML = `
                                <div>${network.ssid} (${network.rssi} dBm)</div>
                                <div class="network-strength">
                                    <div class="network-strength-bar" style="width: ${strength}%"></div>
                                </div>
                            `;
                            
                            networkItem.addEventListener('click', function() {
                                document.getElementById('ssid').value = network.ssid;
                                document.getElementById('password').focus();
                            });
                            
                            networksList.appendChild(networkItem);
                        });
                    })
                    .catch(error => {
                        console.error('Error scanning networks:', error);
                        alert('Error scanning networks. Please try again.');
                    });
            }
        });
        
        // Form submission
        document.getElementById('wifi-form').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const formData = new FormData(this);
            fetch('/wifi-config-submit', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (response.redirected) {
                    window.location.href = response.url;
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Error saving configuration. Please try again.');
            });
        });
        
        // Forget network
        document.getElementById('forget-btn').addEventListener('click', function() {
            if (confirm('Are you sure you want to forget the current network?')) {
                fetch('/forget-wifi', { method: 'POST' })
                    .then(response => {
                        if (response.ok) {
                            alert('Network forgotten. Device will restart in AP mode.');
                            setTimeout(() => {
                                window.location.href = '/wifi-config';
                            }, 2000);
                        }
                    });
            }
        });
    </script>
</body>
</html>
)rawliteral";

void handleRoot();
void handleSubmit();
void handleDashboard();
void handleMap();
void handleSettings();
void handleUserInfo();
void handleUserInfoSubmit();
void handleWifiConfig();
void handleWifiConfigSubmit();
void handleForgetWifi();
String replacePlaceholders(String input);
String urlEncode(const String &str);
void saveDataToNVS(const String& key, const String& data);
String readDataFromNVS(const char* key);
void checkForSMS();
void sendLocationViaSMS(String number);
void updateDisplay();
void displayMessage(const char* message, uint8_t line);
void sendATCommand(const char* command, bool waitForResponse);
void waitForNetwork();
void sendSMS(const char* number, const char* message);
void handleSecuritySettings();
String generateSecurityCode();
void saveSecuritySettings(bool enabled, const String& code);
void connectToWiFi();
void runWebConfigOperation();
void initWebServer();
void deinitWebConfigMode();
void initWebConfigMode();
void handleButtonPress();



// ========== Setup Function ==========
void setup() {
  Serial.begin(115200);
  
  // Initialize hardware components
  SIM800L.begin(9600, SERIAL_8N1, SIM800L_RX, SIM800L_TX);
  GPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(CONTROL_PIN, OUTPUT);
  digitalWrite(CONTROL_PIN, LOW);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.setRotation(2);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  // Initialize preferences
  preferences.begin("storage", false);

  // Read saved data from NVS
  serial_no = readDataFromNVS("serial_no");
  reg_no = readDataFromNVS("reg_no");
  full_name = readDataFromNVS("full_name");
  PHONE_NUMBER = readDataFromNVS("user_phone");

  // Read security settings
  securityEnabled = preferences.getBool("security_enabled", true);
  securityCode = preferences.getString("security_code", "");
  
  // Generate initial security code if none exists
  if (securityCode == "") {
    securityCode = generateSecurityCode();
    preferences.putString("security_code", securityCode);
    preferences.putBool("security_enabled", securityEnabled);
  }

  // Initialize SIM800L
  Serial.println("Initializing SIM800L...");
  displayMessage("Initializing...", 1);
  delay(1000);
  sendATCommand("AT", true);
  waitForNetwork();
  sendATCommand("AT+CMGF=1", true);
  sendATCommand("AT+CNMI=2,2,0,0,0", true);

  delay(2000);
  display.clearDisplay();
  displayMessage("System Ready!", 1);
  displayMessage("Press button", 2);
  displayMessage("to send location", 3);
  Serial.println("System Ready! Press button to send location.");
}

// ========== Main Loop ==========
void loop() {
  // Handle button press for mode switching
  handleButtonPress();

  // Depending on current mode, run appropriate functions
  if (currentMode == NORMAL) {
    runNormalOperation();
  } else {
    runWebConfigOperation();
  }
}

void handleButtonPress() {
  static bool buttonPressed = false;
  
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonPressed) {
      // Button just pressed
      buttonPressed = true;
      buttonPressStartTime = millis();
    } else {
      // Button still pressed - check for long press
      if (millis() - buttonPressStartTime >= longPressDuration) {
        // Long press detected - switch mode
        if (currentMode == NORMAL) {
          // Switching to WEB_CONFIG mode - initialize WiFi and web server
          currentMode = WEB_CONFIG;
          initWebConfigMode();
          displayMessage("Web Config Mode", 1);
          displayMessage(WiFi.localIP().toString().c_str(), 2);
          Serial.println("Switched to Web Config Mode");
        } else {
          // Switching to NORMAL mode - deinitialize WiFi and web server
          currentMode = NORMAL;
          deinitWebConfigMode();
          display.clearDisplay();
          displayMessage("Normal Mode", 1);
          displayMessage("Press to send", 2);
          Serial.println("Switched to Normal Mode");
        }
        // Reset button state to prevent multiple triggers
        buttonPressed = false;
        delay(1000); // Debounce
      }
    }
  } else {
    buttonPressed = false;
    
    // Short press in normal mode - send location
    if (currentMode == NORMAL && millis() - buttonPressStartTime > 50 && 
        millis() - buttonPressStartTime < longPressDuration && gps.location.isValid()) {
      sendLocationViaSMS(PHONE_NUMBER);
      delay(1000); // Prevent multiple sends
    }
  }
}


void initWebConfigMode() {
  // Read WiFi credentials
  wifi_SSID = preferences.getString("wifi_ssid", DEFAULT_SSID);
  wifi_Password = preferences.getString("wifi_password", DEFAULT_PASSWORD);
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_SSID.c_str(), wifi_Password.c_str());
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(wifi_SSID);
  display.clearDisplay();
  displayMessage("Connecting WiFi", 1);
  displayMessage(wifi_SSID.c_str(), 2);
  display.display();
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    display.clearDisplay();
    displayMessage("WiFi Connected", 1);
    displayMessage(WiFi.localIP().toString().c_str(), 2);
    display.display();

    // Initialize web server
    initWebServer();
    Serial.println("HTTP server started");
  } else {
    Serial.println("\nFailed to connect to WiFi");
    display.clearDisplay();
    displayMessage("WiFi Failed", 1);
    displayMessage("Using AP Mode", 2);
    display.display();
    
    // Start access point if can't connect
    WiFi.mode(WIFI_AP);
    WiFi.softAP(DEFAULT_SSID, DEFAULT_PASSWORD);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
    
    display.clearDisplay();
    displayMessage("AP Mode", 1);
    displayMessage(WiFi.softAPIP().toString().c_str(), 2);
    display.display();
    
    // Initialize web server
    initWebServer();
  }
}

void deinitWebConfigMode() {
  // Stop web server
  server.stop();
  
  // Disconnect WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  Serial.println("WiFi and web server stopped");
}

void initWebServer() {
  // Set up server routes
  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.on("/dashboard", handleDashboard);
  server.on("/map", handleMap);
  server.on("/settings", handleSettings);
  server.on("/user-info", HTTP_GET, handleUserInfo);
  server.on("/user-info-submit", HTTP_POST, handleUserInfoSubmit);
  server.on("/wifi-config", handleWifiConfig);
  server.on("/wifi-config-submit", handleWifiConfigSubmit);
  server.on("/forget-wifi", handleForgetWifi);
  server.on("/security-settings", HTTP_POST, handleSecuritySettings);

  server.begin();
}

void runNormalOperation() {
  // Read GPS data
  while (GPS.available() > 0) {
    char c = GPS.read();
    gps.encode(c);
  }

  // Process GPS data
  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();

    String gpsData = "Current Location:\n";
    gpsData += "Lat: " + String(latitude, 6) + "\n";
    gpsData += "Lng: " + String(longitude, 6) + "\n";
    String googleMapsLink = "https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
    gpsData += "Map: " + googleMapsLink + "\n";

    if (gps.date.isValid()) {
      gpsData += "Date: " + String(gps.date.day()) + "/" + 
                 String(gps.date.month()) + "/" + 
                 String(gps.date.year()) + "\n";
    }

    if (gps.time.isValid()) {
      gpsData += "Time: " + String(gps.time.hour()) + ":" + 
                 String(gps.time.minute()) + ":" + 
                 String(gps.time.second()) + "\n";
    }

    lastValidGPSData = gpsData;
  }

  // Update display periodically
  if (millis() - lastDisplayUpdate >= displayUpdateInterval) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  // Check for incoming SMS periodically
  if (millis() - lastSMSCheck >= smsCheckInterval) {
    checkForSMS();
    lastSMSCheck = millis();
  }

  delay(50);
}

void runWebConfigOperation() {
  // Handle web server clients
  server.handleClient();
  
  // Update display with IP address
  if (millis() - lastDisplayUpdate >= displayUpdateInterval) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Web Config Mode");
    display.println("IP:");
    if (WiFi.getMode() == WIFI_AP) {
      display.println(WiFi.softAPIP());
    } else {
      display.println(WiFi.localIP());
    }
    display.println("Long press button");
    display.println("to exit");
    display.display();
    lastDisplayUpdate = millis();
  }
}
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  
  if (!gps.location.isValid()) {
    display.setCursor(0, 0);
    display.println("GPS: No signal");
    display.setCursor(0, 10);
    display.println("Waiting...");
  } else {
    // Display basic GPS info
    display.setCursor(0, 0);
    display.print("Lat:");
    display.println(latitude, 6);
    
    display.setCursor(0, 10);
    display.print("Lng:");
    display.println(longitude, 6);
    
    // Display date and time if available
    if (gps.date.isValid()) {
      display.setCursor(0, 20);
      display.print("Date:");
      display.print(gps.date.day());
      display.print("/");
      display.print(gps.date.month());
      display.print("/");
      display.println(gps.date.year());
    }
    
    if (gps.time.isValid()) {
      display.setCursor(0, 30);
      display.print("Time:");
      display.print(gps.time.hour());
      display.print(":");
      display.print(gps.time.minute());
      display.print(":");
      display.println(gps.time.second());
    }
    
    display.setCursor(0, 50);
    display.println("Press to send SMS");
  }
  
  display.display();
}

void displayMessage(const char* message, uint8_t line) {
  display.setTextSize(1);
  display.setCursor(0, line * 10); // Each line is 10 pixels apart
  display.println(message);
  display.display();
}

void handleRoot() {
  server.send_P(200, "text/html", login_html);
}

// Handle form submission
void handleSubmit() {
  String username = server.arg("username");
  String password = server.arg("password");

  if (username == "admin" && password == "admin") {
    server.sendHeader("Location", "/dashboard");
    server.send(302, "text/plain", "Redirecting to dashboard...");
  } else {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Redirecting to login...");
  }
}

// Handle dashboard page
void handleDashboard() {
  server.send_P(200, "text/html", dashboard_html);
}

// Handle map page
void handleMap() {
  server.send_P(200, "text/html", map_html);
}

// Handle settings page
void handleSettings() {
  server.send_P(200, "text/html", settings_html);
}

// Handle user info page
void handleUserInfo() {
    String html = FPSTR(user_info_html);
    
    // Replace placeholders with actual values from NVS
    html.replace("%USERNAME%", preferences.getString("username", "admin"));
    html.replace("%FULLNAME%", preferences.getString("fullname", "Administrator"));
    html.replace("%EMAIL%", preferences.getString("email", "admin@example.com"));
    html.replace("%USER_PHONE%", preferences.getString("user_phone", ""));
    html.replace("%KIN1_PHONE%", preferences.getString("kin1_phone", ""));
    html.replace("%KIN2_PHONE%", preferences.getString("kin2_phone", ""));
    
    // Generate initials from full name
    String fullname = preferences.getString("fullname", "Administrator");
    String initials;
    if (fullname.length() > 0) {
        initials = String(fullname[0]);
        for (int i = 1; i < fullname.length(); i++) {
            if (fullname[i-1] == ' ') {
                initials += fullname[i];
            }
        }
        initials.toUpperCase();
    } else {
        initials = "AD";
    }
    html.replace("%INITIALS%", initials);
    
    server.send(200, "text/html", html);
}

// Handle user info form submission
void handleUserInfoSubmit() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    // Get form values
    String fullname = server.arg("fullname");
    String email = server.arg("email");
    String userPhone = server.arg("user_phone");
    String kin1Phone = server.arg("kin1_phone");
    String kin2Phone = server.arg("kin2_phone");
    String currentPassword = server.arg("current_password");
    String newPassword = server.arg("new_password");
    String confirmPassword = server.arg("confirm_password");

    // Print to serial monitor
    Serial.println("\n=== User Info Update ===");
    Serial.print("Full Name: ");
    Serial.println(fullname);
    Serial.print("Email: ");
    Serial.println(email);
    Serial.print("User Phone: ");
    Serial.println(userPhone);
    Serial.print("Next of Kin 1: ");
    Serial.println(kin1Phone);
    Serial.print("Next of Kin 2: ");
    Serial.println(kin2Phone);
    
    if (newPassword.length() > 0) {
        Serial.println("Password: [changed]"); // Don't log actual passwords
    }
    Serial.println("=======================");

    // Save to NVS
    preferences.putString("fullname", fullname);
    preferences.putString("email", email);
    preferences.putString("user_phone", userPhone);
    preferences.putString("kin1_phone", kin1Phone);
    preferences.putString("kin2_phone", kin2Phone);

    // Handle password change if requested
    if (newPassword.length() > 0 && newPassword == confirmPassword) {
        // In a real application, verify current password first
        preferences.putString("password", newPassword);
    }

    // Redirect with success message
    String redirectUrl = "/user-info?status=success&message=" + urlEncode("Profile updated successfully!");
    server.sendHeader("Location", redirectUrl);
    server.send(302, "text/plain", "Profile updated");
}

// Handle WiFi config page
void handleWifiConfig() {
    String html = FPSTR(wifi_config_html);
    server.send(200, "text/html", replacePlaceholders(html));
}
void handleWifiConfigSubmit() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    String ssid = server.arg("ssid");
    String password = server.arg("password");

    // Print to serial monitor
    Serial.println("\n=== WiFi Configuration Received ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password); // Note: Be careful logging passwords in production
    Serial.println("================================");

    // Save to NVS
    preferences.putString("wifi_ssid", ssid);
    if (password.length() > 0) {
        preferences.putString("wifi_password", password);
    }

    server.sendHeader("Location", "/wifi-config?success=true");
    server.send(302, "text/plain", "WiFi configuration saved");
}

void handleForgetWifi() {
    preferences.remove("wifi_ssid");
    preferences.remove("wifi_password");
    
    Serial.println("\n=== WiFi Credentials Forgotten ===");
    Serial.println("Device will restart in AP mode");
    Serial.println("================================");
    
    server.send(200, "text/plain", "WiFi credentials forgotten");
    delay(1000);
    ESP.restart();
}

// ========== Security Code Functions ==========
String generateSecurityCode() {
  String code = "";
  const char* alphanumeric = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  
  for (int i = 0; i < SECURITY_CODE_LENGTH; i++) {
    int randomIndex = random(36); // 26 letters + 10 digits
    code += alphanumeric[randomIndex];
  }

  Serial.println("Generated new security code: " + code);
  return code;
}

void saveSecuritySettings(bool enabled, const String& code) {
  securityEnabled = enabled;
  if (code.length() == SECURITY_CODE_LENGTH) {
    securityCode = code;
  }
  
  preferences.putBool("security_enabled", securityEnabled);
  preferences.putString("security_code", securityCode);
  
  Serial.println("Security settings saved:");
  Serial.print("Enabled: "); Serial.println(securityEnabled ? "YES" : "NO");
  Serial.print("Code: "); Serial.println(securityCode);
}

void handleSecuritySettings() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  bool enabled = server.arg("security_enabled") == "on";
  String customCode = server.arg("custom_code");
  
  // Validate custom code if provided
  if (customCode.length() > 0) {
    if (customCode.length() != SECURITY_CODE_LENGTH) {
      server.send(400, "text/plain", "Code must be exactly 6 characters");
      return;
    }
    
    // Convert to uppercase and validate characters
    customCode.toUpperCase();
    for (char c : customCode) {
      if (!isalnum(c)) {
        server.send(400, "text/plain", "Code must be alphanumeric");
        return;
      }
    }
    
    saveSecuritySettings(enabled, customCode);
  } else {
    // Keep current code if no new one provided
    saveSecuritySettings(enabled, securityCode);
  }
  
  server.send(200, "text/plain", "Security settings saved");
}

// ========== SMS Command Processing ==========
void checkForSMS() {
  while (SIM800L.available()) {
    String response = SIM800L.readString();
    Serial.println("Received from SIM800L: " + response);
    
    // Check if response contains an SMS with our command
    if (response.indexOf("+CMT:") != -1) {
      // Extract sender's phone number
      int startIndex = response.indexOf('"') + 1;
      int endIndex = response.indexOf('"', startIndex);
      String senderNumber = response.substring(startIndex, endIndex);
      
      // Check for location command with security code
      if (response.indexOf(commandKeyword) != -1) {
        // Check if security is enabled and code matches
        bool authorized = !securityEnabled; // Default to authorized if security is off
        
        if (securityEnabled) {
          // Look for the security code in the message
          for (int i = 0; i < response.length() - SECURITY_CODE_LENGTH; i++) {
            String possibleCode = response.substring(i, i + SECURITY_CODE_LENGTH);
            if (possibleCode == securityCode) {
              authorized = true;
              break;
            }
          }
        }
        
        if (authorized) {
          Serial.println("Authorized command received from: " + senderNumber);
          display.clearDisplay();
          displayMessage("Cmd received!", 1);
          displayMessage("Sending location", 2);
          
          if (gps.location.isValid()) {
            sendLocationViaSMS(senderNumber);
          } else {
            String message = "GPS location not available yet. Please try again later.";
            sendSMS(senderNumber.c_str(), message.c_str());
            displayMessage("No GPS signal", 3);
          }
        } else {
          Serial.println("Unauthorized command received");
          String message = "Invalid security code. Command rejected.";
          sendSMS(senderNumber.c_str(), message.c_str());
          display.clearDisplay();
          displayMessage("Invalid code", 1);
          displayMessage("Cmd rejected", 2);
        }
      }
      // Check for ON command with security code
      else if (response.indexOf("ON") != -1) {
        bool authorized = !securityEnabled;
        
        if (securityEnabled) {
          for (int i = 0; i < response.length() - SECURITY_CODE_LENGTH; i++) {
            String possibleCode = response.substring(i, i + SECURITY_CODE_LENGTH);
            if (possibleCode == securityCode) {
              authorized = true;
              break;
            }
          }
        }
        
        if (authorized) {
          digitalWrite(CONTROL_PIN, HIGH);
          Serial.println("Turning engine ON");
          display.clearDisplay();
          displayMessage("engine ON", 1);
          sendSMS(senderNumber.c_str(), "Engine has been turned ON");
        } else {
          sendSMS(senderNumber.c_str(), "Invalid security code. Command rejected.");
        }
      }
      // Check for OFF command with security code
      else if (response.indexOf("OFF") != -1) {
        bool authorized = !securityEnabled;
        
        if (securityEnabled) {
          for (int i = 0; i < response.length() - SECURITY_CODE_LENGTH; i++) {
            String possibleCode = response.substring(i, i + SECURITY_CODE_LENGTH);
            if (possibleCode == securityCode) {
              authorized = true;
              break;
            }
          }
        }
        
        if (authorized) {
          digitalWrite(CONTROL_PIN, LOW);
          Serial.println("Turning engine OFF");
          display.clearDisplay();
          displayMessage("engine OFF", 1);
          sendSMS(senderNumber.c_str(), "engine has been turned OFF");
        } else {
          sendSMS(senderNumber.c_str(), "Invalid security code. Command rejected.");
        }
      }
    }
  }
}

void sendLocationViaSMS(String number) {
  if (gps.location.isValid()) {
    String googleMapsLink = "https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
    String smsMessage = "My current location:\n";
    smsMessage += "Latitude: " + String(latitude, 6) + "\n";
    smsMessage += "Longitude: " + String(longitude, 6) + "\n";
    smsMessage += "Google Maps: " + googleMapsLink;

    sendSMS(number.c_str(), smsMessage.c_str());
    display.clearDisplay();
    displayMessage("Location sent!", 1);
    displayMessage("via SMS", 2);
    Serial.println("Location sent via SMS with Google Maps link!");
  } else {
    display.clearDisplay();
    displayMessage("No GPS signal", 1);
    displayMessage("Can't send location", 2);
  }
}

void sendATCommand(const char* command, bool waitForResponse) {
  Serial.print("Sending: ");
  Serial.println(command);
  SIM800L.println(command);

  if (waitForResponse) {
    delay(100);
    while (SIM800L.available()) {
      Serial.write(SIM800L.read());
    }
  }
}

void waitForNetwork() {
  Serial.println("Waiting for network...");
  while (true) {
    SIM800L.println("AT+CREG?");
    delay(500);
    String response = "";

    while (SIM800L.available()) {
      response += (char)SIM800L.read();
    }

    if (response.indexOf("+CREG: 0,1") != -1 || response.indexOf("+CREG: 0,5") != -1) {
      Serial.println("Registered on home network");
      break;
    }

    delay(1000);
  }
}

void sendSMS(const char* number, const char* message) {
  Serial.println("Sending SMS with GPS data...");
  sendATCommand("AT+CMGF=1", true);
  delay(500);

  SIM800L.print("AT+CMGS=\"");
  SIM800L.print(number);
  SIM800L.println("\"");
  delay(500);

  SIM800L.print(message);
  SIM800L.write(26);
  delay(500);

  Serial.println("SMS Sent!");
}

// ========== Helper Functions ==========
String replacePlaceholders(String input) {
    input.replace("%CURRENT_SSID%", WiFi.SSID());
    input.replace("%IP_ADDRESS%", WiFi.localIP().toString());
    
    // Simple signal strength indicator
    String signalStrength;
    int32_t rssi = WiFi.RSSI();
    if (rssi >= -50) signalStrength = "Excellent";
    else if (rssi >= -60) signalStrength = "Good";
    else if (rssi >= -70) signalStrength = "Fair";
    else signalStrength = "Weak";
    
    input.replace("%SIGNAL_STRENGTH%", signalStrength);
    return input;
}

String urlEncode(const String &str) {
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (size_t i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encodedString += '+';
        } else if (isalnum(c)) {
            encodedString += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
    }
    return encodedString;
}

void saveDataToNVS(const String& key, const String& data) {
  preferences.putString(key.c_str(), data);
}

String readDataFromNVS(const char* key) {
  preferences.begin("storage", true);
  if (preferences.isKey(key)) {
    String value = preferences.getString(key, "");
    return value;
  } else {
    Serial.println("no data saved");
    return "";
  }
}
