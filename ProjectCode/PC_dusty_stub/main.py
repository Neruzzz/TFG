# This is a sample Python script.

# Press Mayús+F10 to execute it or replace it with your code.
# Press Double Shift to search everywhere for classes, files, tool windows, actions, and settings.

import serial
import time

# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    ser = serial.Serial('COM7', 115200)
    #i = 0
    #while (1):
    ser.write(b'el de la mesa 10: callate!\n')
    ser.close()
   # time.sleep(1)
   # a = ser.read()
   #  print(a)
   # while a is not None:
   #     print(a)
   #     a = ser.read()

     #time.sleep(1000/1000) #sleep for 50 ms



# See PyCharm help at https://www.jetbrains.com/help/pycharm/
