import paho.mqtt.client as mqtt
import os
import time
import json
import logging
from datetime import datetime, timedelta
from threading import Thread, Lock
from flask import Flask, jsonify, request, render_template_string
from flask_cors import CORS
from supabase import create_client, Client
from dotenv import load_dotenv
from email_handler import EmailHandler


load_dotenv()
MQTT_BROKER = os.getenv("MQTT_BROKER", "192.168.1.70")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
MQTT_USER = os.getenv("MQTT_USER", "python_app")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "your_password")
MQTT_TOPIC_DATA = "irrigation/data"
MQTT_TOPIC_CMD = "irrigation/cmd"
MQTT_TOPIC_STATUS = "irrigation/status"

# Supabase Config
SUPABASE_URL = os.getenv("SUPABASE_URL", "https://kiewjweubjlzmemnslor.supabase.co")
SUPABASE_KEY = os.getenv("SUPABASE_SERVICE_KEY", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImtpZXdqd2V1Ympsem1lbW5zbG9yIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTk0NTYzNzEsImV4cCI6MjA3NTAzMjM3MX0.m_c1dW_MkF6V7KmY9RHDfPGwjPmVYYV8F30rCjkxU5A")

# Flask Config
PORT = int(os.getenv("PORT", 5000))

# EMAIL CONFIG - Dùng cho EmailHandler
EMAIL_ENABLED = os.getenv("EMAIL_ENABLED", "True").lower() == "true"

# Alert thresholds
CRITICAL_THRESHOLD = 3800
ALERT_COOLDOWN = 300  # Chỉ gửi email mỗi 5 phút

# =============================================
# SETUP LOGGING
# =============================================

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# =============================================
# INITIALIZE SERVICES
# =============================================

# Flask App
app = Flask(__name__)
CORS(app, resources={r"/api/*": {"origins": "*"}}) 
@app.after_request
def after_request(response):
    response.headers.add('Access-Control-Allow-Origin', '*')
    response.headers.add('Access-Control-Allow-Headers', 'Content-Type,Authorization,ngrok-skip-browser-warning')
    response.headers.add('Access-Control-Allow-Methods', 'GET,PUT,POST,DELETE,OPTIONS')
    return response

# Supabase Client
supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)

# MQTT Client
mqtt_client = mqtt.Client(client_id=f"PythonWebApp_{int(time.time())}")
mqtt_client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

# =============================================
# EMAIL HANDLER SETUP
# =============================================

class SystemConfig:
    """Config class cho EmailHandler"""
    def __init__(self):
        self.config = {
            "email_sender": os.getenv("EMAIL_SENDER", "fullday2k5@gmail.com"),
            "email_password": os.getenv("EMAIL_PASSWORD", "iejx czjz tlki wcwy"),
            "email_recipient": os.getenv("EMAIL_RECEIVER", "ngochieuvan123@gmail.com")
        }

# Khởi tạo EmailHandler
email_handler = EmailHandler(SystemConfig())

# Global state
latest_data = {
    "moisture": 0,
    "status": "unknown",
    "autoMode": True,
    "pumpOn": False,
    "timestamp": None,
    "connected": False
}

# Email alert tracking
last_alert_sent = 0
alert_lock = Lock()

# =============================================
# EMAIL FUNCTIONS - Sử dụng EmailHandler
# =============================================

def send_email_alert(moisture_value, status):
    """Gửi email cảnh báo dùng EmailHandler"""
    global last_alert_sent
    
    # Kiểm tra cooldown
    with alert_lock:
        current_time = time.time()
        if current_time - last_alert_sent < ALERT_COOLDOWN:
            logger.info(f"Email cooldown active. {int(ALERT_COOLDOWN - (current_time - last_alert_sent))}s remaining")
            return False
        
        last_alert_sent = current_time
    
    if not EMAIL_ENABLED:
        logger.warning("Email alerts disabled in config")
        return False
    
    try:
        current_time = email_handler.get_vn_time()
        
        subject = "⚠️ CẢNH BÁO: Hệ Thống Tưới - Đất Quá Ướt"
        body = f"""
        <html>
            <body style="margin: 0; padding: 0; font-family: Arial, sans-serif; background-color: white;">
                <div style="max-width: 600px; margin: 40px auto; background-color: white; border-radius: 10px; overflow: hidden; box-shadow: 0 4px 6px rgba(0,0,0,0.1);">
                    
                    <div style="background: linear-gradient(135deg, #3498db 0%, #2980b9 100%); color: white; padding: 40px 20px; text-align: center;">
                        <h1 style="margin: 0; font-size: 28px; font-weight: bold;">🚨 CẢNH BÁO KHẨN CẤP 🚨</h1>
                        <p style="margin: 10px 0 0 0; font-size: 16px; opacity: 0.95;">Đất quá ướt - có thể ngập úng</p>
                    </div>
                    
                    <div style="padding: 40px 30px; text-align: center;">
                        <h2 style="color: #333; font-size: 20px; margin: 0 0 30px 0;">TÌNH TRẠNG HỆ THỐNG TƯỚI</h2>
                        
                        <table style="width: 100%; max-width: 400px; margin: 0 auto; border-collapse: collapse;">
                            <tr>
                                <td style="padding: 15px; text-align: left; border-bottom: 1px solid #eee;">
                                    <strong style="color: #666;">Thời gian:</strong>
                                </td>
                                <td style="padding: 15px; text-align: right; border-bottom: 1px solid #eee;">
                                    <span style="color: #333;">{current_time}</span>
                                </td>
                            </tr>
                            <tr>
                                <td style="padding: 15px; text-align: left; border-bottom: 1px solid #eee;">
                                    <strong style="color: #666;">Độ ẩm đất:</strong>
                                </td>
                                <td style="padding: 15px; text-align: right; border-bottom: 1px solid #eee;">
                                    <span style="color: #3498db; font-weight: bold; font-size: 1.5em;">{moisture_value}</span>
                                </td>
                            </tr>
                            <tr>
                                <td style="padding: 15px; text-align: left; border-bottom: 1px solid #eee;">
                                    <strong style="color: #666;">Ngưỡng chuẩn:</strong>
                                </td>
                                <td style="padding: 15px; text-align: right; border-bottom: 1px solid #eee;">
                                    <span style="color: #333;">&gt; 3000</span>
                                </td>
                            </tr>
                            <tr>
                                <td style="padding: 15px; text-align: left;">
                                    <strong style="color: #666;">Trạng thái:</strong>
                                </td>
                                <td style="padding: 15px; text-align: right;">
                                    <span style="color: #3498db; font-weight: bold;">{status.upper()}</span>
                                </td>
                            </tr>
                        </table>
                        
                        <div style="background-color: #e8f4f8; border-left: 4px solid #3498db; padding: 20px; margin: 30px 0; text-align: left;">
                            <p style="margin: 0; color: #2980b9; font-weight: bold; line-height: 1.6;">
                                ⚠️ Đất quá ướt! Có thể do mưa lớn hoặc tưới quá nhiều. Hệ thống đã tắt máy bơm tự động.
                            </p>
                        </div>
                    </div>
                    <hr>
                    
                    <div style="background-color: #f9f9fa; padding: 20px; border-top: 1px solid #eee;">
                        <p style="margin: 0; font-size: 12px; color: #999;">
                            Email tự động này được gửi đi bởi Hệ Thống Tưới Cây Tự Động.
                            <br>
                            Thời gian gửi: {current_time}
                        </p>
                    </div>
                    
                </div>
            </body>
        </html>
        """
        
        success = email_handler.send_email(subject, body)
        
        if success:
            logger.info(f"✓ Email alert sent: moisture={moisture_value}, status={status}")
            
            try:
                supabase.table("email_logs").insert({
                    "recipient": SystemConfig().config['email_recipient'],
                    "moisture_value": moisture_value,
                    "status": status,
                    "sent_at": datetime.now().isoformat()
                }).execute()
            except Exception as e:
                logger.error(f"Failed to log email: {e}")
        
        return success
        
    except Exception as e:
        logger.error(f"✗ Email send failed: {e}")
        return False

def check_alert_condition(moisture_value, status):
    """Kiểm tra xem có cần gửi cảnh báo không"""
    config = get_system_config()
    email_alerts_enabled = config.get("email_alerts_enabled", True)
    manual_threshold = config.get("manual_threshold", 3000)
    
    if not email_alerts_enabled:
        return
    
    # GỬI CẢNH BÁO KHI ĐẤT QUÁ ƯỚT (độ ẩm thấp < 3000)
    if moisture_value < manual_threshold:
        logger.warning(f"Alert: Đất quá ướt - moisture={moisture_value} < threshold={manual_threshold}")
        send_email_alert(moisture_value, status)

# =============================================
# MQTT HANDLERS
# =============================================

def on_connect(client, userdata, flags, rc):
    """MQTT kết nối thành công"""
    if rc == 0:
        logger.info("✓ Connected to MQTT Broker")
        client.subscribe(MQTT_TOPIC_DATA)
        client.subscribe(MQTT_TOPIC_STATUS)
        latest_data["connected"] = True
    else:
        logger.error(f"✗ MQTT Connection failed with code {rc}")
        latest_data["connected"] = False

def on_disconnect(client, userdata, rc):
    """MQTT mất kết nối"""
    logger.warning(f"Disconnected from MQTT Broker (code: {rc})")
    latest_data["connected"] = False
    
    if rc != 0:
        logger.info("Attempting to reconnect...")
        try:
            client.reconnect()
        except Exception as e:
            logger.error(f"Reconnect failed: {e}")

def on_message(client, userdata, msg):
    """Nhận dữ liệu từ ESP32"""
    try:
        topic = msg.topic
        payload = json.loads(msg.payload.decode())
        
        if topic == MQTT_TOPIC_DATA:
            # Cập nhật latest data
            latest_data.update({
                "moisture": payload.get("moisture", 0),
                "status": payload.get("status", "unknown"),
                "autoMode": payload.get("autoMode", True),
                "pumpOn": payload.get("pumpOn", False),
                "timestamp": datetime.now().isoformat(),
                "connected": True
            })
            
            # Lưu vào Supabase
            save_to_supabase(payload)
            
            # KIỂM TRA ĐIỀU KIỆN CẢNH BÁO
            check_alert_condition(payload.get("moisture", 0), payload.get("status", "unknown"))
            
            logger.info(f"📊 Data: moisture={payload['moisture']}, status={payload['status']}")
        
        elif topic == MQTT_TOPIC_STATUS:
            device_ip = payload.get("ip", "unknown")
            logger.info(f"📱 Device online: {device_ip}")
            update_device_status(device_ip, True)
            
    except json.JSONDecodeError:
        logger.error(f"Invalid JSON from MQTT: {msg.payload}")
    except Exception as e:
        logger.error(f"Error processing MQTT message: {e}")

mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect
mqtt_client.on_message = on_message

# =============================================
# SUPABASE FUNCTIONS
# =============================================

def save_to_supabase(data):
    """Lưu dữ liệu sensor vào Supabase"""
    try:
        result = supabase.table("sensor_data_raw").insert({
            "moisture": data.get("moisture", 0),
            "status": data.get("status", "unknown"),
            "auto_mode": data.get("autoMode", True),
            "pump_on": data.get("pumpOn", False)
        }).execute()
        
        logger.debug(f"✓ Saved to Supabase")
        return True
        
    except Exception as e:
        logger.error(f"✗ Supabase insert failed: {e}")
        return False

def log_control_action(command, source="web", user_id=None):
    """Ghi log điều khiển"""
    try:
        supabase.table("control_logs").insert({
            "command": command,
            "source": source,
            "user_id": user_id
        }).execute()
        logger.info(f"✓ Logged control: {command} from {source}")
    except Exception as e:
        logger.error(f"✗ Log control failed: {e}")

def update_device_status(ip_address, is_online):
    """Cập nhật trạng thái thiết bị"""
    try:
        supabase.table("device_status").update({
            "is_online": is_online,
            "ip_address": ip_address,
            "last_seen": datetime.now().isoformat()
        }).eq("id", 1).execute()
    except Exception as e:
        logger.error(f"✗ Update device status failed: {e}")

def get_system_config():
    """Lấy cấu hình hệ thống"""
    try:
        result = supabase.table("system_config").select("*").eq("id", 1).execute()
        return result.data[0] if result.data else {}
    except Exception as e:
        logger.error(f"✗ Get config failed: {e}")
        return {}

# =============================================
# FLASK API ROUTES
# =============================================

@app.route('/')
def index():
    """Trang chủ đơn giản"""
    return render_template_string('''
    <!DOCTYPE html>
    <html>
    <head>
        <title>Irrigation System API</title>
        <style>
            body { font-family: Arial; max-width: 800px; margin: 50px auto; padding: 20px; }
            h1 { color: #2c3e50; }
            .endpoint { background: #ecf0f1; padding: 10px; margin: 10px 0; border-radius: 5px; }
            code { background: #34495e; color: #ecf0f1; padding: 2px 6px; border-radius: 3px; }
            .email-status { padding: 15px; background: #d4edda; color: #155724; border-radius: 5px; margin: 20px 0; }
        </style>
    </head>
    <body>
        <h1>🌱 Irrigation System API</h1>
        <div class="email-status">
            <strong>📧 Email Alerts:</strong> {{ 'ENABLED' if email_enabled else 'DISABLED' }}
        </div>
        <p>Server is running! Access these endpoints:</p>
        
        <div class="endpoint">
            <strong>GET</strong> <code>/api/status</code> - Trạng thái hiện tại
        </div>
        <div class="endpoint">
            <strong>GET</strong> <code>/api/history?hours=24</code> - Lịch sử dữ liệu
        </div>
        <div class="endpoint">
            <strong>POST</strong> <code>/api/control</code> - Điều khiển
        </div>
        <div class="endpoint">
            <strong>POST</strong> <code>/api/test-email</code> - Test gửi email
        </div>
        <div class="endpoint">
            <strong>GET</strong> <code>/api/email-logs</code> - Lịch sử email đã gửi
        </div>
    </body>
    </html>
    ''', email_enabled=EMAIL_ENABLED)

@app.route('/api/status')
def get_status():
    """Lấy trạng thái hiện tại của hệ thống"""
    return jsonify({
        "success": True,
        "data": latest_data
    })

@app.route('/api/test-email', methods=['POST'])
def test_email():
    """Test gửi email cảnh báo"""
    try:
        moisture = request.json.get('moisture', 3900) if request.json else 3900
        status = request.json.get('status', 'critical') if request.json else 'critical'
        
        success = send_email_alert(moisture, status)
        
        return jsonify({
            "success": success,
            "message": "Email sent successfully" if success else "Email send failed"
        })
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/email-logs')
def get_email_logs():
    """Lấy lịch sử email đã gửi"""
    try:
        result = supabase.table("email_logs") \
            .select("*") \
            .order("sent_at", desc=True) \
            .limit(50) \
            .execute()
        
        return jsonify({
            "success": True,
            "data": result.data
        })
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/history')
def get_history():
    """Lấy lịch sử dữ liệu"""
    hours = request.args.get('hours', default=24, type=int)
    limit = request.args.get('limit', default=1000, type=int)
    
    try:
        time_threshold = (datetime.now() - timedelta(hours=hours)).isoformat()
        
        result = supabase.table("sensor_data_raw") \
            .select("*") \
            .gte("created_at", time_threshold) \
            .order("created_at", desc=True) \
            .limit(limit) \
            .execute()
        
        return jsonify({
            "success": True,
            "count": len(result.data),
            "data": result.data
        })
    except Exception as e:
        logger.error(f"Get history error: {e}")
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/control', methods=['POST'])
def control():
    """Điều khiển hệ thống"""
    try:
        data = request.get_json()
        command = data.get('command', '').upper()
        
        valid_commands = ['PUMP_ON', 'PUMP_OFF', 'AUTO_ON', 'AUTO_OFF','RELOAD_CONFIG'] 
        
        if command not in valid_commands:
            return jsonify({
                "success": False,
                "error": f"Invalid command. Valid: {valid_commands}"
            }), 400
            
        mqtt_client.publish(MQTT_TOPIC_CMD, command) 
        log_control_action(command, source="web") 
        
        logger.info(f"✓ Command sent: {command}")
        
        return jsonify({
            "success": True,
            "command": command,
            "timestamp": datetime.now().isoformat()
        })
        
    except Exception as e:
        logger.error(f"Control error: {e}")
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/config', methods=['GET', 'PUT'])
def config():
    """Lấy hoặc cập nhật cấu hình hệ thống"""
    if request.method == 'GET':
        config_data = get_system_config()
        return jsonify({"success": True, "data": config_data})
    
    elif request.method == 'PUT':
        try:
            data = request.get_json()
            
            result = supabase.table("system_config") \
                .update(data) \
                .eq("id", 1) \
                .execute()
            
            return jsonify({
                "success": True,
                "data": result.data[0] if result.data else {}
            })
        except Exception as e:
            logger.error(f"Update config error: {e}")
            return jsonify({"success": False, "error": str(e)}), 500

# =============================================
# MQTT THREAD
# =============================================

def start_mqtt():
    """Chạy MQTT client trong thread riêng"""
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        logger.error(f"MQTT connection error: {e}")

# =============================================
# MAIN
# =============================================

if __name__ == '__main__':
    logger.info("🚀 Starting Irrigation System Web App")
    logger.info(f"📧 Email Alerts: {'ENABLED' if EMAIL_ENABLED else 'DISABLED'}")
    
    # Start MQTT trong thread riêng
    mqtt_thread = Thread(target=start_mqtt, daemon=True)
    mqtt_thread.start()
    
    # Start Flask
    logger.info(f"🌐 Flask server starting on port {PORT}")
    app.run(host='0.0.0.0', port=PORT, debug=False)