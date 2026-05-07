-- SQL script to create database and table for ESP data logger
-- Run this script in MySQL to set up the logging database

CREATE DATABASE IF NOT EXISTS esp_logger;

USE esp_logger;

CREATE TABLE IF NOT EXISTS logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    state VARCHAR(20) NOT NULL,
    distance INT NOT NULL,
    pir BOOLEAN NOT NULL,
    presence BOOLEAN DEFAULT FALSE,
    last_change_time BIGINT DEFAULT 0
);