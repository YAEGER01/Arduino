import os
import pymysql
from dotenv import load_dotenv

# Load environment variables from .env file
load_dotenv()

db_config = {
    "host": os.getenv("DB_HOST"),
    "port": int(os.getenv("DB_PORT")),
    "user": os.getenv("DB_USER"),
    "password": os.getenv("DB_PASSWORD"),
    "database": "hallway_db",
    "charset": "utf8",
}

# Setup database and tables
def setup_database():
    # Connect without database to create it
    temp_config = db_config.copy()
    del temp_config["database"]
    del temp_config["charset"]
    conn = pymysql.connect(**temp_config)
    cursor = conn.cursor()
    
    # Set timezone to Philippine Time
    cursor.execute("SET time_zone = '+08:00'")
    
    # Create database if not exists
    cursor.execute("CREATE DATABASE IF NOT EXISTS hallway_db DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci")
    
    # Use the database
    cursor.execute("USE hallway_db")
    
    # Create sensor_logs table for MQTT data
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS sensor_logs (
            id INT AUTO_INCREMENT PRIMARY KEY,
            set_id VARCHAR(10),
            pir VARCHAR(5),
            distance INT,
            sound_detected VARCHAR(5),
            sound_vol INT,
            current_state VARCHAR(15),
            buzzer_active VARCHAR(5),
            timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci
    """)
    
    # Create logs table for Flask app data
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS logs (
            id INT AUTO_INCREMENT PRIMARY KEY,
            timestamp BIGINT NOT NULL,
            set_id TINYINT NOT NULL,
            state VARCHAR(20) NOT NULL,
            distance INT NOT NULL,
            pir BOOLEAN NOT NULL,
            presence BOOLEAN NOT NULL,
            last_change_time BIGINT NOT NULL,
            last_change_dist INT NOT NULL,
            trend_buffer INT NOT NULL,
            loiter_count INT NOT NULL,
            last_pir_trigger BIGINT NOT NULL,
            sound_level INT NOT NULL,
            anchor_dist INT NOT NULL
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci
    """)
    
    conn.commit()
    cursor.close()
    conn.close()

# Run setup on import
if __name__ != "__main__":
    setup_database()
    import pymysql
    pymysql.converters.encoders[pymysql.constants.FIELD_TYPE.TIMESTAMP] = lambda x: x
