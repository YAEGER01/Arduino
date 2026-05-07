from flask import Flask, request, jsonify, render_template
import mysql.connector
from db_config import db_config

app = Flask(__name__)


@app.route("/data", methods=["POST"])
def receive_data():
    data = request.get_json()
    conn = mysql.connector.connect(**db_config)
    cursor = conn.cursor()
    cursor.execute(
        "INSERT INTO logs (timestamp, state, distance, pir, presence, last_change_time, last_change_dist, trend_buffer, loiter_count, last_pir_trigger, lcd_on) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)",
        (
            data["timestamp"],
            data["state"],
            data["distance"],
            data["pir"],
            data["presence"],
            data["last_change_time"],
            data["last_change_dist"],
            data["trend_buffer"],
            data["loiter_count"],
            data["last_pir_trigger"],
            data["lcd_on"],
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
    conn = mysql.connector.connect(**db_config)
    cursor = conn.cursor(dictionary=True)
    cursor.execute("SELECT * FROM logs ORDER BY timestamp DESC LIMIT 100")
    rows = cursor.fetchall()
    cursor.close()
    conn.close()
    return jsonify(rows)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
