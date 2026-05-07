CREATE DATABASE IF NOT EXISTS esp_logger;

USE esp_logger;

CREATE TABLE IF NOT EXISTS logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    state VARCHAR(20) NOT NULL,
    distance INT NOT NULL,
    pir BOOLEAN NOT NULL,
    presence BOOLEAN NOT NULL,
    last_change_time BIGINT NOT NULL,
    last_change_dist INT NOT NULL,
    trend_buffer INT NOT NULL,
    loiter_count INT NOT NULL,
    last_pir_trigger BIGINT NOT NULL,
    lcd_on BOOLEAN NOT NULL
);