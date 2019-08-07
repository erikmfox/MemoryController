
import os

tRCD	= 24*2	#Time before read issued after activate
tRAS	= 52*2	#time before Precharge issued after activate
tRP	= 24*2	#Duration of precharge
tRC	= 76*2	#(tRAS + tRP)	//Duration of row read/write cycle

tRTP	= 12*2	#Time between a read and a precharge command

tCCD_L	= 8*2	#Successive read/write commands within a bank group
tCCD_S	= 4*2	#Successive read/write commands between bank groups

tWR     = 20*2	#interval between write data and precharge command

tBURST	= 4	#Burst length
tWTR_L	= 12	#end of write data burst to column read command delay within a bank group
tWTR_S	= 4	#end of write data burst to column read command delay between bank groups

waitBurstLong = (tWTR_L + tBURST)
waitBurstShort = (tWTR_S + tBURST)

recentRow = -1
array = []

bankGroup = -1
bankGroupLast = -1

for fileName in os.listdir('.'):
    counter_tRCD = 0	#Time before read issued after activate
    counter_tRAS = 0	#time before Precharge issued after activate
    counter_tRP	= 0	#Duration of precharge

    counter_tRTP = 0	#Time between a read and a precharge command

    counter_tCCD_L = 0	#Successive read/write commands within a bank group
    counter_tCCD_S = 0	#Successive read/write commands between bank groups

    counter_waitBurstLong = 0
    counter_waitBurstShort = 0

    counter_tWR = 0	#interval between write data and precharge command
    
    if "DRAMoutput-" in fileName:
        print("checking file: " + fileName)
        fileHandle = open(fileName, 'r')
        lineNum = 0
        timeLast = -1
        time = 0
        
        counter_tWR = 0	#interval between write data and precharge command
        for line in fileHandle:
            array = line.split()
            time = int(array[0])
            timeElapsed = time - timeLast
            
            counter_tRCD -= timeElapsed	#Time before read issued after activate
            counter_tRAS -= timeElapsed	#time before Precharge issued after activate
            counter_tRP -= timeElapsed	#Duration of precharge
            counter_tRTP -= timeElapsed	#Time between a read and a precharge command
            counter_tCCD_L -= timeElapsed	#Successive read/write commands within a bank group
            counter_tCCD_S -= timeElapsed	#Successive read/write commands between bank groups
            counter_tWR -= timeElapsed	#interval between write data and precharge command
            counter_waitBurstLong -= timeElapsed
            counter_waitBurstShort -= timeElapsed
            
            lineNum += 1
            
            if "PRE" in line:
                if counter_tRAS > 0:
                    print("tRAS violation in " + fileName + " line: " + str(lineNum) )
                if counter_tWR > 0:
                    print("tWR violation in " + fileName + " line: " + str(lineNum) )
                if counter_tRTP > 0:
                    print("tRTP violation in " + fileName + " line: " + str(lineNum) )
                counter_tRP = tRP
            elif "ACT" in line:
                if counter_tRP > 0:
                    print("tRP violation in " + fileName + " line: " + str(lineNum) )
                counter_tRCD = tRCD
                counter_tRAS = tRAS
            elif "RD" in line:
                bankGroup = array[2]
                if bankGroup == bankGroupLast:
                    if counter_tCCD_L > 0:
                        print("tCCD_L violation in " + fileName + " line: " + str(lineNum) )
                    if counter_waitBurstLong > 0:
                        print("tWTR_L violation in " + fileName + " line: " + str(lineNum) )
                else:
                    if counter_tCCD_S > 0:
                        print("tCCD_S violation in " + fileName + " line: " + str(lineNum) )
                    if counter_waitBurstShort > 0:
                        print("tWTR_S violation in " + fileName + " line: " + str(lineNum) )

                if counter_tRCD > 0:
                    print("tRCD violation in " + fileName + " line: " + str(lineNum) )
                    
                bankGroupLast = bankGroup
                
                counter_tCCD_L = tCCD_L
                counter_tCCD_S = tCCD_S
                counter_tRTP = tRTP
            elif "WR" in line:

                bankGroup = array[2]
                if bankGroup == bankGroupLast:
                    if counter_tCCD_L > 0:
                        print("tCCD_L violation in " + fileName + " line: " + str(lineNum) )
                else:
                    if counter_tCCD_S > 0:
                        print("tCCD_S violation in " + fileName + " line: " + str(lineNum) )

                if counter_tRCD > 0:
                    print("tRCD violation in " + fileName + " line: " + str(lineNum) )

                bankGroupLast = bankGroup
                
                counter_tCCD_L = tCCD_L
                counter_tCCD_S = tCCD_S
                counter_tWR = tWR
                counter_waitBurstLong = waitBurstLong
                counter_waitBurstShort = waitBurstShort
            else:
                print("Invalid command in " + fileName + " line: " + str(lineNum))

            timeLast = time

    
        fileHandle.close()
