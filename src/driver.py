'''
Created on Apr 4, 2012

@author: lanquarden
'''

import msgParser
import carState
import csv
import os
import keyboard
import carControl

class Driver(object):
    '''
    A driver object for the SCRC
    '''

    def __init__(self, stage,carmodel):
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

        with open(self.log_file, "ab") as file:
            writer = csv.writer(file)
            if not file_exists:  # Write headers if file doesn't exist
                writer.writerow(["Car model","curLapTime", "speedX", "speedY", "speedZ", 
                                 "trackPos", "steer", "gear", "rpm", "damage",
                                 "track", "opponents", "racePos", "acceleration", "brake"])
        self.manual_steer = True
        self.manual_throttle = True
        self.manual_gear = True
        self.setup_keyboard_listeners()
                
    
    def init(self):
        '''Return init string with rangefinder angles'''
        self.angles = [0 for x in range(19)]
        
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
        carmodel= self.car_model
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



        with open(self.log_file, "ab") as file:
            writer = csv.writer(file)
            writer.writerow([
                carmodel,curLapTime, speedX, speedY, speedZ,
                trackPos, steer, gear, rpm,
                damage, track, opponents, racePos, accel, brake,
                
            ])
        
        
        self.manual_steer = True
        self.manual_throttle = True
        self.manual_gear = True

        return self.control.toMsg()
    
    def setup_keyboard_listeners(self):
        keyboard.on_press_key('left', lambda _: self.set_manual_steer(0.5))
        keyboard.on_press_key('right', lambda _: self.set_manual_steer(-0.5))
        keyboard.on_release_key('left', lambda _: self.set_manual_steer(0))
        keyboard.on_release_key('right', lambda _: self.set_manual_steer(0))

        keyboard.on_press_key('up', lambda _: self.set_manual_throttle(0.7))
        keyboard.on_press_key('down', lambda _: self.set_manual_throttle(-0.7))
        keyboard.on_release_key('up', lambda _: self.set_manual_throttle(0))
        keyboard.on_release_key('down', lambda _: self.set_manual_throttle(0))

        
        keyboard.on_press_key('v', lambda _: self.set_manual_gear(1))  # Upshift
        keyboard.on_press_key('b', lambda _: self.set_manual_gear(-1)) # Downshift

    def set_manual_steer(self, value):
        self.manual_steer = True
        self.control.setSteer(value)

    def set_manual_throttle(self, value):
        self.manual_throttle = True
        if value > 0:
            self.control.setAccel(value)  # Throttle
            self.control.setBrake(0)
        else:
            self.control.setBrake(-value) # Brake
            self.control.setAccel(0)

    def set_manual_gear(self, delta):
        self.manual_gear = True
        new_gear = max(-1, min(6, self.state.getGear() + delta))
        self.control.setGear(new_gear)
    
    
            
        
    def onShutDown(self):
        pass
    
    def onRestart(self):
        pass
        