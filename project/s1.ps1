# Start Flask app and v6.7.py in parallel and wait for both to finish

# Ensure database and tables are created
python -c "import serve.db_config" -WorkingDirectory "$PSScriptRoot\serve"

# Start Flask app (serve/app.py) in background
$proc1 = Start-Process -FilePath "python" -ArgumentList "app.py" -WorkingDirectory "$PSScriptRoot\serve" -NoNewWindow -PassThru

# Start v6.7.py (serve/v6.7.py) in background - will retry MQTT connection
$proc2 = Start-Process -FilePath "python" -ArgumentList "v6.7.py" -WorkingDirectory "$PSScriptRoot\serve" -NoNewWindow -PassThru

# Wait for both processes to finish
Wait-Process -Id $proc1.Id, $proc2.Id