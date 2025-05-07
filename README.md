# ai-racing-bot

This project is an AI-powered racing bot that controls a simulated car using computer vision and control algorithms. The bot processes telemetry data, makes driving decisions, and outputs control commands to navigate a virtual track.

## Project Structure

- `carControl.py`: Implements car control logic.
- `carState.py`: Maintains the state of the car.
- `driver.py`: Main entry point for running the bot.
- `msgParser.py`: Parses telemetry and control messages.
- `pyclient.py`: Python client for communication.
- `input.txt`: Example input data for testing.
- `telemetry_log.csv`: Output log of telemetry data.

## Requirements

- Python 3.8+
- CUDA toolkit (if using `canny.cu`)
- Required Python packages (install with pip):
  ```sh
  pip install numpy
    ```

## How to Run

1. Run the main driver
```sh
python src/driver.py
```
This will start the AI racing bot, which will read telemetry data, process it, and output control commands.

2. (Optional) Run with sample input:  You can provide telemetry data via `input.txt` for testing:
```sh
python src/driver.py < src/input.txt
```

## Output
- The bot will output control commands (steering, throttle, brake) to the console or to a connected simulator.
- Telemetry data and results are logged in `telemetry_log.csv`.