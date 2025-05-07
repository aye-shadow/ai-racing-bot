#!/usr/bin/env python
'''
Created on Apr 4, 2012

@author: lanquarden
'''
import sys
import argparse
import socket
import driver

if __name__ == '__main__':
    # Configure the argument parser
    parser = argparse.ArgumentParser(description='Python client to connect to the TORCS SCRC server.')
    parser.add_argument('--host', action='store', dest='host_ip', default='localhost',
                        help='Host IP address (default: localhost)')
    parser.add_argument('--port', action='store', type=int, dest='host_port', default=3001,
                        help='Host port number (default: 3001)')
    parser.add_argument('--id', action='store', dest='id', default='SCR',
                        help='Bot ID (default: SCR)')
    parser.add_argument('--maxEpisodes', action='store', dest='max_episodes', type=int, default=1,
                        help='Maximum number of learning episodes (default: 1)')
    parser.add_argument('--maxSteps', action='store', dest='max_steps', type=int, default=0,
                        help='Maximum number of steps (default: 0)')
    parser.add_argument('--track', action='store', dest='track', default=None,
                        help='Name of the track')
    parser.add_argument('--stage', action='store', dest='stage', type=int, default=3,
                        help='Stage (0 - Warm-Up, 1 - Qualifying, 2 - Race, 3 - Unknown)')

    arguments = parser.parse_args()

    # Print summary
    print(f'Connecting to server host ip: {arguments.host_ip} @ port: {arguments.host_port}')
    print(f'Bot ID: {arguments.id}')
    print(f'Maximum episodes: {arguments.max_episodes}')
    print(f'Maximum steps: {arguments.max_steps}')
    print(f'Track: {arguments.track}')
    print(f'Stage: {arguments.stage}')
    print('*********************************************')

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    except socket.error as msg:
        print(f'Could not make a socket: {msg}')
        sys.exit(-1)

    # One second timeout
    sock.settimeout(1.0)

    shutdownClient = False
    curEpisode = 0

    verbose = False

    d = driver.Driver(arguments.stage, "kc-coda")

    while not shutdownClient:
        while True:
            print(f'Sending id to server: {arguments.id}')
            buf = arguments.id + d.init()
            print(f'Sending init string to server: {buf}')
            
            try:
                sock.sendto(buf.encode('utf-8'), (arguments.host_ip, arguments.host_port))
            except socket.error as msg:
                print(f'Failed to send data: {msg}...Exiting...')
                sys.exit(-1)
                
            try:
                buf, addr = sock.recvfrom(1000)
                buf = buf.decode('utf-8')  # Decode received data
            except socket.error as msg:
                print(f"Didn't get response from server: {msg}")
        
            if '***identified***' in buf:
                print(f'Received: {buf}')
                break

        currentStep = 0
        
        while True:
            # Wait for an answer from server
            buf = None
            try:
                buf, addr = sock.recvfrom(1000)
                buf = buf.decode('utf-8')
            except socket.error as msg:
                print(f"Didn't get response from server: {msg}")
            
            if verbose:
                print(f'Received: {buf}')
            
            if buf is not None and '***shutdown***' in buf:
                d.onShutDown()
                shutdownClient = True
                print('Client Shutdown')
                break
            
            if buf is not None and '***restart***' in buf:
                d.onRestart()
                print('Client Restart')
                break
            
            currentStep += 1
            if currentStep != arguments.max_steps:
                if buf is not None:
                    buf = d.drive(buf)
            else:
                buf = '(meta 1)'
            
            if verbose:
                print(f'Sending: {buf}')
            
            if buf is not None:
                try:
                    sock.sendto(buf.encode('utf-8'), (arguments.host_ip, arguments.host_port))
                except socket.error as msg:
                    print(f"Failed to send data: {msg}...Exiting...")
                    sys.exit(-1)
        
        curEpisode += 1
        
        if curEpisode == arguments.max_episodes:
            shutdownClient = True

    sock.close()