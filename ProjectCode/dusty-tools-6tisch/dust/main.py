#!/usr/bin/python

import subprocess
import signal
import serial
import string 
import time
import datetime as dtm
import random
import sys
import getopt
import re
from functools import reduce

import socket

from random import randint

import binascii
import Crc

# Eventes
hdlc_event_boot = 0x0001 # The mote booted up
hdlc_event_alarmChange =  0x0002  # Alarm(s) were opened or closed
hdlc_event_timeChange = 0x0004 # UTC time-mapping on the mote changed
hdlc_event_joinFail = 0x0008 # Join operation failed
hdlc_event_disconnected = 0x0010 # The mote disconnected from the network
hdlc_event_operational = 0x0020 # Mote has connection to the network and is ready to send data
hdlc_event_svcChange = 0x0080 # Service allocation has changed
hdlc_event_joinStarted = 0x0100 # Mote started joining the network

hdlc_notif_events = 0x0F # Event(s)
hdlc_notifreceive = 0x19 # Packet received
hdlc_notiftxDone = 0x25 # Transmit done

class NBIoTSerial(object):

    def __init__(self,argv):
        
        # Serial params
        self.ser = None
        self.filename = '7-inticonn-socket-sendData.txt'
        self.serialPort = 'COM7'
        #self.serialPort = 'COM17'
        self.baudrate = 115200
        self.iter = '1'
        self.run = '0'
        self.delay = '0'
        self.parseArgs(argv)
        self.initFile = "./dust-tests/init-connection.txt"
        self.timeout=1

        # Dust Protocol Params
        self.crc = Crc.Crc()
        self.packetToSend  = {}
        
        #self.packetToSend['port'] = bytes.fromhex("F0B8")
        self.packetToSend['cmd'] = bytearray.fromhex('18')
        self.packetToSend['len'] = bytearray.fromhex('00')
        self.packetToSend['flags'] = bytearray.fromhex('08')
        
        self.packetToSend['dest']= bytearray.fromhex('ff020000000000000000000000000002')#bytearray.fromhex('ff8000000000000000170d0000594d3e')
        self.packetToSend['port']= bytearray.fromhex('F0B9')
        self.packetToSend['service'] = bytearray.fromhex('00')
        self.packetToSend['prio'] = bytearray.fromhex('00')
        self.packetToSend['socketId'] = bytearray.fromhex('16')

        self.packetToSend['crc'] = bytearray.fromhex('0000')
        self.packetToSend['seqNumBytes'] = bytearray.fromhex('0000')
        self.packetToSend['seqNumRaw'] =  randint(0,65535)
        
        self._HDLC_FLAG     = 0x7E   # the HDLC flag at the beginning/end of a frame
        self._HDLC_ESCAPE   = 0x7D   # escape character when the HDLC flag in payload
        self._HDLC_MASK     = 0x20   # mask used to recover from escape character
        self._FCS_LENGTH    = 2  # number of bytes in the FCS field

        self.dltime = dtm.datetime.utcnow()


    def parseArgs(self,argv):
        opts = None
        args = None
        try:
            opts, args = getopt.getopt(argv, 'h:p:b:f:d:i:r:', ["port=", "baud=", "file=", "delay=", "iter=","run="])
        except getopt.GetoptError:
            print('main.py -p <port> -b <baudrate> -f <filename> -i <0,1>')
            sys.exit(2)
        print(opts)
        for opt, arg in opts:
            if opt == '-h':
                print('main.py -p <port> -b <baudrate> -f <filename> -i <0,1> ')
                sys.exit()
            elif opt in ("-p", "--port"):
                self.serialPort = arg
                #print(self.serialPort)
            elif opt in ("-b", "--baud"):
                self.baudrate = arg
                #print(self.baudrate)
            elif opt in ("-f", "--file"):
                self.filename = arg
                print(self.filename)
            elif opt in ("-d", "--delay"):
                self.delay  = arg
            elif opt in ("-i", "--iter"):
                self.iter  = arg
            elif opt in ("-r", "--run"):
                self.run = arg
                print("RUN Datalogger {}s".format(self.run))

    def openSerial(self,serialport,baud,timeout):
        
        #self.ser = serial.Serial(port=serialport baudrate=baud, timeout=timeout, rtscts=False, dsrdtr=True)
        self.ser = serial.Serial(baudrate=baud, timeout=timeout, rtscts=False, dsrdtr=False)
        self.ser.rts = True
        self.ser.dts = True
        
        self.ser.port = serialport
        self.ser.open()        
 
    #convert string to hex
    def toHex(self,s):
        lst = []
        for ch in s:
            hv = hex(ord(ch)).replace('0x', '')
            if len(hv) == 1:
                hv = '0'+hv
            lst.append(hv)
        return reduce(lambda x,y:x+y, lst)

    #def toRaw(self,s):
    #    lst = []
    #    for ch in s:    
    #    return reduce(lambda x,y:x+y, lst)


    def parseMsg(self,message):
        
        
        self.packetToSend['payload'] = bytearray.fromhex(message)
        self.packetToSend['len'][0] = int(len(message)/2)+23

        #self.packetToSend['flags'][0] = self.packetToSend['flags'][0]^2
        self.packetToSend['seqNumBytes'] = self.packetToSend['seqNumRaw'].to_bytes(2, byteorder='big')
        # Print Sequence Number
        print(binascii.hexlify(self.packetToSend['seqNumBytes']))
        
        msg = self.packetToSend['cmd'] + self.packetToSend['len'] + self.packetToSend['flags'] \
        + self.packetToSend['socketId'] + self.packetToSend['dest'] + self.packetToSend['port'] \
        + self.packetToSend['service']  + self.packetToSend['prio'] + self.packetToSend['seqNumBytes'] \
        + self.packetToSend['payload'] + self.packetToSend['crc']

        # calculate fcs
        crc = self.crc.calculate(msg[0:-2]) # Exclude CRC fields
        msg[-2]=crc[0]
        msg[-1]=crc[1]

        # add HDLC escape characters - create list for 
        lstpacketBytes = list(msg)
        index = 0
        while index<len(lstpacketBytes):
            if lstpacketBytes[index]==self._HDLC_FLAG or lstpacketBytes[index]==self._HDLC_ESCAPE:
                lstpacketBytes.insert(index,self._HDLC_ESCAPE)
                index += 1
                lstpacketBytes[index] = lstpacketBytes[index]^self._HDLC_MASK
            index += 1
        
        # Add HDLC tokens
        lstpacketBytes.insert(0,self._HDLC_FLAG)
        lstpacketBytes.insert(len(lstpacketBytes),self._HDLC_FLAG)

        # Inc SeqNumber (for next transmision)
        self.packetToSend['seqNumRaw'] += 1
        
        return bytearray(lstpacketBytes)
    
    
    def parseCmd(self,message):
        
        # Remove HDLC Tokens (string format, 2 cars per hex byte.)
       	msg = bytearray.fromhex(message[2:-2]) 
       	
        # calculate fcs
        crc = self.crc.calculate(msg[0:-2]) # Exclude CRC fields
        
        msg[-2]=crc[0]
        msg[-1]=crc[1]

        # add HDLC escape characters - create list for 
        lstpacketBytes = list(msg)
        index = 0
        while index<len(lstpacketBytes):
            if lstpacketBytes[index]==self._HDLC_FLAG or lstpacketBytes[index]==self._HDLC_ESCAPE:
                lstpacketBytes.insert(index,self._HDLC_ESCAPE)
                index += 1
                lstpacketBytes[index] = lstpacketBytes[index]^self._HDLC_MASK
            index += 1
        
        # Add HDLC tokens
        lstpacketBytes.insert(0,self._HDLC_FLAG)
        lstpacketBytes.insert(len(lstpacketBytes),self._HDLC_FLAG)

        return bytearray(lstpacketBytes)
    
    def sendCmd(self,cmd):
        self.repeat = False
        if cmd.startswith('#'):
            #cope with code comments
            return
    
        if 'WAIT' in cmd:
            sec2wait = int(cmd.split(" ")[-1]) # get the seconds to wait
            print("waiting ..." + str(sec2wait))
            time.sleep(sec2wait)
            return

        if 'UKN' in cmd:
            sec2wait = int(cmd.split(" ")[-1]) # get the seconds to wait          
            print("Waiting {}".format(sec2wait))
            WT=False
            for i in range(0,sec2wait):
                tmp=str(self.ser.readline())
                print(str(time.strftime("%H:%M:%S", time.gmtime())) + ' - ' + tmp)
            return

        if 'FLUSH' in cmd:
            time.sleep(1)    
            self.ser.reset_input_buffer()


        if 'async' in cmd:
            
            print(">>> " + dtm.datetime.now().strftime("%H:%M:%S.%f")[0:-3])
            print(">>> {}".format(dtm.datetime.utcnow()-self.dltime))
            
            patt = cmd.split(" ")[-1].rstrip() # get the command to wait           
            
            byt = bytes.fromhex(patt)
            print("NoAck Command - Sent:")
            print(binascii.hexlify(byt))

            self.ser.write(byt)
            self.ser.flush()
    

        if 'txcmd' in cmd:
            
            print(">>T " + dtm.datetime.now().strftime("%H:%M:%S.%f")[0:-3])
            print(">>D {}".format(dtm.datetime.utcnow()-self.dltime))
            
            patt = cmd.split(" ")[-1].rstrip() # get the command to wait           
            print("TxCommand - Sending {}".format(patt))

			# BMH: CRC msg
            # byt = bytes.fromhex(patt)
            byt=self.parseCmd(patt)
            
            print("TxCommand - Sent:")
            print(binascii.hexlify(byt))

            
            #self.ser.rts = True      
            self.ser.write(byt)
            self.ser.flush()
            #time.sleep(0.5)
            #self.ser.rts = False
            

            print("TxCommand - Response:")
            #self.ser.rts = True

            read_buffer = b''
            ok = 0
            while ok < 2:
                            
                msg=(self.ser.read(1))
                #time.sleep(1)                                           
                #print(str(msg))

                if(len(msg)>0):
                    #print(str(msg))
                    print("{0:02x}".format(ord(msg)),end='')
                    read_buffer += msg
                else:
                    print(".",end='')
                    sys.stdout.flush()
                    
                if bytes.fromhex("7E") == msg:
                    ok = ok +1

            #print("\n")
            print(">>F {}".format(dtm.datetime.utcnow()-self.dltime))
                
            if((read_buffer[3]&1)==1):
                #print("Received TX-Response [{0:02x}]".format(read_buffer[3]))
                pass
            else:
                print("TX-Response Error [{0:02x}]".format(read_buffer[3]))

            # 
            # time.sleep(0.5)

            return            

        if 'txdata' in cmd:
            
            print(">>T> " + dtm.datetime.now().strftime("%H:%M:%S.%f")[0:-3])
            print(">>D {}".format(dtm.datetime.utcnow()-self.dltime))
            
            patt = cmd.split(" ")[-1].rstrip() # get the command to wait
            #print("TxData - Sending {}".format(patt))

            byt=self.parseMsg(patt)
            print("TxData - Sent:")
            print(binascii.hexlify(byt))

            # Write to Serial
            # self.ser.rts = True
            self.ser.write(byt)
            self.ser.flush()
            #time.sleep(0.5)
            #self.ser.rts = False
           
            print("TxData - Response:")

            # self.ser.rts = True

            read_buffer = b''
            ok = 0
            while ok < 2:
                            
                msg=(self.ser.read(1))
                #time.sleep(1)                                           
                #print(str(msg))

                if(len(msg)>0):
                    #print(str(msg))
                    print("{0:02x}".format(ord(msg)),end='')
                    read_buffer += msg
                else:
                    print(".",end='')
                    sys.stdout.flush()
                    
                if bytes.fromhex("7E") == msg:
                    ok = ok +1

            #self.ser.rts = False
            #print("\n")
            print(">>F {}".format(dtm.datetime.utcnow()-self.dltime))

            if((read_buffer[3]&1)==1):
                print(read_buffer)
                # print("Received TxData Response [{0:02x}] ".format(read_buffer[3]))
                pass
            else:
                print("TxData Response Error [{0:02x}] ".format(read_buffer[3]))
            # time.sleep(0.5)
            return

        if 'txloop' in cmd:
			
            print(">>T> " + dtm.datetime.now().strftime("%H:%M:%S.%f")[0:-3])
            print(">>D {}".format(dtm.datetime.utcnow()-self.dltime))
            count = int(cmd.split(" ")[-1].rstrip()) # get the num packets to send
            rate  = int(cmd.split(" ")[-2].rstrip()) # get the sending rate
            patt  = cmd.split(" ")[-3].rstrip() # get the command to send
            print("count = {}, rate = {}, payload = {}".format(count,rate,patt))
            time.sleep(1)
            #print("TxData - Sending {}".format(patt))
            #print(binascii.hexlify(byt))
            i = 0
            while True:
			
                print("TxData - Sent {}/{}".format(i,count))

                #wait to achieve the desired rate
                time.sleep(rate/1000)
                
                byt=self.parseMsg(patt)
                print("{}".format(binascii.hexlify(byt)))

                # Write to Serial
                self.ser.write(byt)
                self.ser.flush()

                print("TxData - Response:")

                read_buffer = b''
                ok = 0
                while ok < 2:
                    msg=(self.ser.read(1))
                    if(len(msg)>0):
                        #print(str(msg))
                        #print("{0:02x}".format(ord(msg)),end='')
                        read_buffer += msg
                    else:
                        print(".",end='')
                        sys.stdout.flush()

                    if bytes.fromhex("7E") == msg:
                        ok = ok +1

                print(binascii.hexlify(read_buffer))
                print(">>F {}".format(dtm.datetime.utcnow()-self.dltime))
                i += 1
				# Ckeck Parity (API Header)

                #if((read_buffer[3]&1)==2):
                    #print("Received TxData Response [{0:02x}] ".format(read_buffer[3]))
                    #pass
                #else:
                    #print("TxData Response Error [{0:02x}] ".format(read_buffer[3]))
                 
                # Check Command Response (API Data)
                if((read_buffer[4])!=0):
                    print("Reported Tx Error [{0:02x}]".format(read_buffer[4]))
                 
                # ------------------------------------------------------ 
				# Wait Sent Notification	
                read_buffer = b''
                ok = 0
                while ok < 2:		
                    msg=(self.ser.read(1))
                    if(len(msg)>0):
                        print("{0:02x}".format(ord(msg)),end='')
                        read_buffer += msg
                    else:
                        print(".",end='')
                        sys.stdout.flush()

                    if bytes.fromhex("7E") == msg:
                        ok = ok +1

                #print("\n")
                print(">>F {}".format(dtm.datetime.utcnow()-self.dltime))

                if((read_buffer[3]&1)==1):
                    print("Notif Error [{0:02x}]".format(read_buffer[3]))
                else:
                    print("Received Notif [{0:02x}]".format(read_buffer[3]))

                # Print Sequence Number
                print(binascii.hexlify(read_buffer[4:6]))
       
                if((read_buffer[6])!=0):
                    print("Notification Tx Error [{0:02x}]".format(read_buffer[4]))

                # Generate Response
                write_buffer = bytearray.fromhex("7E0F00000000007E")
                byte1 = int('00000000', 2)  

                # Set Flag to Response
                write_buffer[3] |= 1
                
                # Set Packet ID (Copy received flag)
                if((read_buffer[3]&2)==2):
                    write_buffer[3] |= 2            

                # Calculate CRC (only first 4 bytes)
                crc=self.crc.calculate(write_buffer[1:5])

                write_buffer[5]=crc[0]
                write_buffer[6]=crc[1]
                print("Notification Ack Sent:")
                print(binascii.hexlify(write_buffer))

                self.ser.write(write_buffer)
                self.ser.flush()

                time.sleep(0.2)                    

            return

        if 'ack' in cmd:

            print(">>T " + dtm.datetime.now().strftime("%H:%M:%S.%f")[0:-3])
            print(">>D {}".format(dtm.datetime.utcnow()-self.dltime))
            
            hexstr = cmd.split(" ")[-2].rstrip() # get the command to wait           
            print("Waiting for Notif: {}".format(hexstr))

            # Clean Serial
            # self.ser.flush()

            read_buffer = b''

            ok = 0
            while ok < 2:
                            
                #msg=str(self.ser.readline())
                #print(msg)

                msg=(self.ser.read(1))
                if(len(msg)>0):
                    #print(str(msg))
                    print("{0:02x}".format(ord(msg)),end='')
                    read_buffer += msg
                else:
                    print(".",end='')
                    sys.stdout.flush()
                    
                if bytes.fromhex("7E") == msg:
                    ok = ok +1
                else:
                    pass     
                    #if "ERROR" in msg: 
                    #print('An error occurred. Exiting...')
                    #sys.exit()

            #print("\n")
            print(">>F {}".format(dtm.datetime.utcnow()-self.dltime))

            # Generate ack
            write_buffer = bytearray.fromhex("7E0F00000000007E")

            byte1 = int('00000000', 2)  

            if((read_buffer[3]&1)==1):
                print("Notif Error [{0:02x}]".format(read_buffer[3]))
            else:
                write_buffer[3] |= 1
                print("Received Notif [{0:02x}]".format(read_buffer[3]))
                      
            if((read_buffer[3]&2)==2):
                write_buffer[3] |= 2            
            
            # Calculate CRC (exclude last two bytes)
            crc=self.crc.calculate(write_buffer[1:5])
            
            write_buffer[5]=crc[0]
            write_buffer[6]=crc[1]
            print("Notification Ack Sent:")
            print(binascii.hexlify(write_buffer))

            #self.ser.rts = True
            self.ser.write(write_buffer)
            self.ser.flush()
            #self.ser.rts = False

            time.sleep(0.5)

            # Hex String to Bytes
            # print("Emulate {}".format(hexstr))
            # byteblock = bytes.fromhex(hexstr)
            # print(byteblock)
            # print("Flags {}".format(byteblock[3]))           
            # self.ser.write(byteblock)
                        
            return  

        # self.ser.write(cmd.encode())
        # time.sleep(0.5)
        # self.ser.write(cmd.encode())
                

    def executeCmdsFromFile(self,filename):

        with open(filename) as f:
            content = f.readlines()
            for line in content:
                if line.startswith('END'):
                    return
                # Echo Line
                if not line.startswith('#'):
                    print('\n')
                    print(">>> Read Cmd: " + line,end='')
                #time.sleep(1)
                cmd = line.rstrip('\r\n')
                cmd = cmd + "\r\n"
                self.sendCmd(cmd)
            f.close()


    def main(self):
        
        self.openSerial(self.serialPort, self.baudrate, self.timeout)
          
        if not self.delay is '0':
            next_time = time.time() + int(self.delay)

        for i in range(0,int(self.iter)):

            st = dtm.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S.%f')
            print("\n--------------------------------------------------------------------------------".format(i))
            #print("Run: {}".format(i))
            print("Run: {} [{}]".format(i,st))

            
            if not self.run is '0':

                HOST = ''
                PORT = 50007
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.bind((HOST, PORT))
                print("Listen on {}".format(PORT))
                s.listen(1)
                print("Waiting Connection...")
                
                # Call DataLogger 
                print("Initializing the DataLogger for {}s sample, please wait ...".format(self.run))
                #pid = subprocess.Popen(r'"D:\users\borja\dust_local\testing-tools\bin\DataLoggerStd.exe" {}'.format(self.run), shell=False,stdin=None, stdout=None, stderr=None, close_fds=True)

                # Accept blocks until clientr establish connection
                conn, addr = s.accept()

                # time.sleep(3)
                #with open(r'D:\users\borja\Dropbox\tmp\data_record.log') as f:
                #    rdtime = f.readline()

                while True:
                    msg = conn.recv(1024)
                    if(len(msg)>0):
                        break
                s.close()
                rdtime = msg.decode("utf-8") 

                print("\nRead: {}".format(rdtime))
                self.dltime = dtm.datetime.strptime(rdtime,'%Y-%m-%d %H:%M:%S.%f')

            
            # execute the commands from the file.
            self.executeCmdsFromFile(self.filename)
            
            # Wait extra time when in acquisition mode (min. to guarantee nDAQ)
            if not self.run is '0':
               
                print("\nDAQ time {}s".format(self.run))
                delta = dtm.datetime.utcnow()-self.dltime
                
                print("\nElapsed time {}s".format(delta.seconds))
                remtime = int(self.run)-delta.seconds
                
                if( remtime > 0):
                    print("\nWaiting for DAQ {}s".format(remtime))
                    time.sleep(remtime)

                time.sleep(5)

            if not self.delay is '0':
                time.sleep(max(0, next_time - time.time()))
                next_time = next_time + int(self.delay)
                
        # Single Execution
        # self.executeCmdsFromFile(self.filename)
        
        
if __name__=="__main__":
    app = NBIoTSerial(sys.argv[1:])
    app.main()
