#!/bin/python

import random
import struct

def Constant(fixed, random_mask = 0):
  if random_mask == 0:
    #constants.append(fixed)
    
    print("R: 0x" + str(format(fixed, "08X")) + ' ' + str(struct.unpack("<f", struct.pack("<I", fixed))[0]))
    return
  for i in range(10):
    Constant(fixed | (random.getrandbits(32) & random_mask), 0)
  return
  

def Header():
  Constant(0x00000000) # 0.0
  Constant(0x3F800000) # 1.0
  Constant(0x7F800000) # Inf
  Constant(0x7FFFFFFF) # NaN
  Constant(0x00000001) # Smallest denormal
  Constant(0x00800001) # Smallest number
  Constant(0x7F7FFFFF) # Largest number
  Constant(0x7F800000, 0x003FFFFF) # Random NaN or rarely Inf
  Constant(0x00000000, 0x007FFFFF) # Random denormal
  Constant(0x40000000, 0x03FFFFFF) # Random numbers in range [2; 511[
  Constant(0x00000000, 0x7FFFFFFF) # Random numbers or rarely specials

  for i in range(20):
    pass
  for i in range(20):
    pass
  for i in range(20):
    pass
  return

def Footer():
  print("END")
  return

def Instruction():
  return

def Instruction_s(operation):
  print(operation)
  return

def Instruction_v(operation):
  print(operation)
  return

def Instruction_vv(operation):
  print(operation)
  return

def Instruction_vvv(operation):
  print(operation)
  return

#FIXME: Add output type so we can get more instructions in shorter code

# Instruction_s('ARL') # address register load
Instruction_v('MOV') # move
Instruction_vv('MUL') # multiply
Instruction_vv('ADD') # add
Instruction_vvv('MAD') # multiply and add
Instruction_s('RCP') # reciprocal
Instruction_s('RSQ') # reciprocal square root
Instruction_vv('DP3') # 3-component dot product
Instruction_vv('DP4') # 4-component dot product
Instruction_vv('DST') # distance vector
Instruction_vv('MIN') # minimum
Instruction_vv('MAX') # maximum
Instruction_vv('SLT') # set on less than
Instruction_vv('SGE') # set on greater equal than
Instruction_s('EXP') # exponential base 2
Instruction_s('LOG') # logarithm base 2
Instruction_v('LIT') # light coefficients
Instruction_vv('DPH') # homogeneous dot product
Instruction_s('RCC') # reciprocal clamped
Instruction_vv('SUB') # subtract
Instruction_v('ABS') # absolute value

while instructions:
  Header()
  # We can do a maximum of 136 instructions..
  while i < 136:
    
  Footer()
