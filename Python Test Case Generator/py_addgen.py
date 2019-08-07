# Python COde
#Author: TB
#--------------------------------------
# TO run | python py_addgen.py > <filename>

from random import *
import time
#Row=0xF0F #0-65535
#BG=0x2 #0-3
#B=0x2 #0-3
#LC=0x7 #0-7
#UC=0x12 #0-127
#BS=0x5 #0-7
cycles=10
INS=['FETCH','READ','WRITE']
commandNum = int(input("Enter the number of commands you want to generate: "))
#Address=(Row << 17) | (BS) | (BG <<6) | (LC << 3) | (BG << 15) | (UC << 8)
#print (Address)
#print(hex(Address))

fileHandle = open("Random-" + str(commandNum)  + ".txt", 'w')

for counter in range(0, commandNum - 1):
	BG=randint(0,3)
	B=randint(0,3)
	C=randint(0,1023)
	R=randint(0,65535)
	BS=randint(0,7)
	Address=(R << 17) | (BS) | (B <<6) | (BG << 15) | (C << 3)      
	rn=randint(0,2) #Random number to select instructions
	ti=randint(3,40) #for cycles between individual instructions
	cycles=cycles+ti
	#print (str(cycles)+ " "+ INS[rn]+" " + str(hex(Address)))
	fileHandle.write(str(cycles)+ " " + INS[rn]+" " + str(hex(Address)) + "\n")

fileHandle.close()
