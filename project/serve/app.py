from flask import Flask, request, jsonify, render_template
import pymysql
import os
from db_config import db_config

template_dir = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "templates"))
app = Flask(__name__, template_folder=template_dir)


@app.route("/data", methods=["POST"])
def receive_data():
    data = request.get_json()
    conn = pymysql.connect(**db_config)
    cursor = conn.cursor()
    cursor.execute(
        "INSERT INTO logs (timestamp, set_id, state, distance, pir, presence, last_change_time, last_change_dist, trend_buffer, loiter_count, last_pir_trigger, sound_level, anchor_dist) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)",
        (
            data["timestamp"],
            int(data["set"]),
            data["state"],
            data["distance"],
            data["pir"] == "true",
            data["presence"] == "true",
            data["last_change_time"],
            data["last_change_dist"],
            data["trend_buffer"],
            data["loiter_count"],
            data["last_pir_trigger"],
            data["sound_level"],
            data.get("anchor_dist", 0),
        ),
    )
    conn.commit()
    cursor.close()
    conn.close()
    return jsonify({"status": "success"}), 200


@app.route("/dashboard")
def dashboard():
    return render_template("index.html")


@app.route("/api/data")
def get_data():
    conn = pymysql.connect(**db_config)
    cursor = conn.cursor(pymysql.cursors.DictCursor)
    cursor.execute("SET time_zone = '+08:00'")
    cursor.execute("""
        SELECT id, timestamp as timestamp, 
               CASE set_id WHEN 'SET1' THEN 1 WHEN 'SET2' THEN 2 ELSE 0 END as `set`,
               distance, pir, current_state as state, 
               sound_vol as sound_level, buzzer_active,
               0 as presence, 0 as loiter_count, 0 as last_change_time,
               0 as last_change_dist, 0 as trend_buffer, 0 as last_pir_trigger
        FROM sensor_logs 
        ORDER BY timestamp DESC LIMIT 200
    """)
    rows = cursor.fetchall()
    cursor.close()
    conn.close()
    return jsonify(rows)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
