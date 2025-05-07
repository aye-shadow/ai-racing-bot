'''
Created on Apr 4, 2012

@author: lanquarden
'''

import msgParser
import carState
import csv
import os
from pynput import keyboard  # Replace 'keyboard' with 'pynput'
import carControl

class Driver:
    '''
    A driver object for the SCRC
    '''
    def __init__(self, stage, carmodel):
        self.WARM_UP = 0
        self.car_model = carmodel
        self.QUALIFYING = 1
        self.RACE = 2
        self.UNKNOWN = 3
        self.stage = stage
        
        self.parser = msgParser.MsgParser()
        
        self.state = carState.CarState()
        
        self.control = carControl.CarControl()
        
        self.steer_lock = 0.785398
        self.max_speed = 100
        self.prev_rpm = None
        self.log_file = "telemetry_log.csv"
        file_exists = os.path.isfile(self.log_file)

        with open(self.log_file, "a", newline='') as file:
            writer = csv.writer(file)
            if not file_exists:  # Write headers if file doesn't exist
                writer.writerow(["Car model", "curLapTime", "speedX", "speedY", "speedZ", 
                                 "trackPos", "steer", "gear", "rpm", "damage",
                                 "track", "opponents", "racePos", "acceleration", "brake"])
        self.manual_steer = True
        self.manual_throttle = True
        self.manual_gear = True
        self.setup_keyboard_listeners()
                
    def init(self):
        '''Return init string with rangefinder angles'''
        self.angles = [0] * 19
        
        for i in range(5):
            self.angles[i] = -90 + i * 15
            self.angles[18 - i] = 90 - i * 15
        
        for i in range(5, 9):
            self.angles[i] = -20 + (i-5) * 5
            self.angles[18 - i] = 20 - (i-5) * 5
        
        return self.parser.stringify({'init': self.angles})
    
    def drive(self, msg):
        self.state.setFromMsg(msg)

        # Reset manual flags (for next iteration)
        carmodel = self.car_model
        curLapTime = self.state.getCurLapTime()
        speedX = self.state.getSpeedX()
        speedY = self.state.getSpeedY()
        speedZ = self.state.getSpeedZ()
        trackPos = self.state.getTrackPos()
        racePos = self.state.getRacePos()
        rpm = self.state.getRpm()
        damage = self.state.getDamage()
        accel = self.control.getAccel()
        brake = self.control.getBrake()
        steer = self.control.getSteer()
        gear = self.control.getGear()
        track = ",".join(map(str, self.state.getTrack())) if self.state.getTrack() else ""
        opponents = ",".join(map(str, self.state.getOpponents())) if self.state.getOpponents() else ""

        with open(self.log_file, "a", newline='') as file:
            writer = csv.writer(file)
            writer.writerow([
                carmodel, curLapTime, speedX, speedY, speedZ,
                trackPos, steer, gear, rpm,
                damage, track, opponents, racePos, accel, brake
            ])
        
        self.manual_steer = True
        self.manual_throttle = True
        self.manual_gear = True

        return self.control.toMsg()
    
    def setup_keyboard_listeners(self):
        def on_press(key):
            try:
                if key == keyboard.Key.left:
                    self.set_manual_steer(0.5)
                elif key == keyboard.Key.right:
                    self.set_manual_steer(-0.5)
                elif key == keyboard.Key.up:
                    self.set_manual_throttle(0.7)
                elif key == keyboard.Key.down:
                    self.set_manual_throttle(-0.7)
                elif key.char == 'v':
                    self.set_manual_gear(1)  # Upshift
                elif key.char == 'b':
                    self.set_manual_gear(-1)  # Downshift
            except AttributeError:
                pass

        def on_release(key):
            try:
                if key == keyboard.Key.left or key == keyboard.Key.right:
                    self.set_manual_steer(0)
                elif key == keyboard.Key.up or key == keyboard.Key.down:
                    self.set_manual_throttle(0)
            except AttributeError:
                pass

        # Set up pynput listener
        listener = keyboard.Listener(on_press=on_press, on_release=on_release)
        listener.start()

    def set_manual_steer(self, value):
        self.manual_steer = True
        self.control.setSteer(value)

    def set_manual_throttle(self, value):
        self.manual_throttle = True
        if value > 0:
            self.control.setAccel(value)  # Throttle
            self.control.setBrake(0)
        else:
            self.control.setBrake(-value)  # Brake
            self.control.setAccel(0)

    def set_manual_gear(self, delta):
        self.manual_gear = True
        new_gear = max(-1, min(6, self.state.getGear() + delta))
        self.control.setGear(new_gear)
            
    def onShutDown(self):
        pass
    
    def onRestart(self):
        pass
