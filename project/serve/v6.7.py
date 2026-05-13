import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion
import pymysql
from datetime import datetime
from db_config import db_config  # Centralized DB config with auto-setup

# --- MQTT SETTINGS ---
MQTT_BROKER = "10.169.151.16"
MQTT_PORT = 1883
MQTT_USER = "admin"
MQTT_PASS = "admin123"


# --- DATABASE LOGIC ---
def save_to_db(set_id, parts):
    try:
        conn = pymysql.connect(**db_config)
        cursor = conn.cursor()
        cursor.execute("SET time_zone = '+08:00'")

        # Parts: [state, dist, pir, sound_detected, sound_vol, buzzer]
        sql = """INSERT INTO sensor_logs 
                 (set_id, pir, distance, sound_detected, sound_vol, current_state, buzzer_active) 
                 VALUES (%s, %s, %s, %s, %s, %s, %s)"""

        values = (
            set_id,
            parts[2],
            int(parts[1]),
            parts[3],
            int(parts[4]),
            parts[0],
            parts[5],
        )

        cursor.execute(sql, values)
        conn.commit()
        print(f"Successfully logged {set_id} | State: {parts[0]} | Dist: {parts[1]}cm")

    except pymysql.Error as err:
        print(f"Database Error: {err}")
    except Exception as e:
        print(f"Logic Error: {e}")
    finally:
        if 'conn' in locals() and conn:
            cursor.close()
            conn.close()


# --- MQTT CALLBACKS ---
def on_connect(client, userdata, connect_flags, reason_code, properties):
    if reason_code == 0:
        print("Connected to Mosquitto Successfully!")
        # Subscribes to hallway/SET1 and hallway/SET2
        client.subscribe("hallway/#")
    else:
        print(f"Connection failed with reason {reason_code}")


def on_message(client, userdata, msg):
    try:
        # Extract SET1 or SET2 from topic 'hallway/SET1'
        set_id = msg.topic.split("/")[-1]

        # Decode payload: WALK_IN|45|1|1|102|0
        payload = msg.payload.decode("utf-8")
        parts = payload.split("|")

        if len(parts) == 6:
            save_to_db(set_id, parts)
        else:
            print(f"Invalid payload format from {set_id}: {payload}")

    except Exception as e:
        print(f"Error parsing message: {e}")


# --- START BRIDGE ---
import time

client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2)
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.on_connect = on_connect
client.on_message = on_message

def run_bridge():
    reconnect_delay = 5
    while True:
        try:
            print(f"Connecting to MQTT broker at {MQTT_BROKER}...")
            client.connect(MQTT_BROKER, MQTT_PORT, 60)
            reconnect_delay = 5
            print("Starting MQTT loop...")
            client.loop_forever()
        except KeyboardInterrupt:
            print("\nBridge stopped by user.")
            break
        except Exception as e:
            print(f"MQTT error: {e}")
            print(f"Retrying in {reconnect_delay}s... (Ctrl+C to exit)")
            time.sleep(reconnect_delay)
            reconnect_delay = min(reconnect_delay * 2, 60)

if __name__ == "__main__":
    run_bridge()
