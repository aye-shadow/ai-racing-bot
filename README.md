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
- Create a virtual environment and install dependencies:
  ```sh
  python -m venv venv
  venv\Scripts\activate
  pip install -r requirements.txt
  ```
- **Download required data files:**  
  - Download the Torcs-C.rar file from [this Google Drive link](https://drive.google.com/file/d/1rqi4CFeLxAcPJscZe4ROw5QER1a3epoc/view) and run `Torcs-C\torcs\torcs_1.3.7_setup.exe` to install the simulator.
  - Download `torcs.zip` from [this link](https://drive.google.com/file/d/1BFqITV0LWU_v0xcyliBqOqsVY999ujiC/view?usp=drive_link) and replace the `Torcs-C` folder with the contents of the zip file.

## How to Run

1. **Start the AI racing bot:**
   ```sh
   python src/pyclient.py
   ```
2. **Launch TORCS - The Open Racing Car Simulator** (from your Start menu or desktop shortcut).
3. In TORCS, follow these steps:
   - Click **"Race"** > **"Quick Race"** > **"Configure Race"**
   - Pick a track type and track, then click **"Accept"**
   - Select any drivers from the right (not "Player") as your opponents by clicking one and clicking **"de(select)"**
   - Click **"Accept"**
   - Change display to **"Normal"**, then click **"Accept"**
   - Click **"New Race"** to start

You are now ready to play. The results will be logged in `telemetry_log.csv`.

## Output
- The bot will output control commands (steering, throttle, brake) to the console or to a connected simulator.
- Telemetry data and results are logged in `telemetry_log.csv`.