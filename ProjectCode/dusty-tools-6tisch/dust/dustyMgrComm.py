import serial
import time

ser = serial.Serial('COM7', baudrate=115200)
packet = []
#ser.write(bytearray.fromhex('7E0001000304FF0037317E'))
while True:
    print(ser.read(1).hex())