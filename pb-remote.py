#!/usr/bin/python

import os
import sys
import socket
import struct
import subprocess

xbox_port = 9269 # "XBOX" on a phone ;)

try:  
   NXDK_DIR = os.environ["NXDK_DIR"]
except KeyError: 
   print("Please set the environment variable NXDK_DIR")
   exit(1)


vp20compiler = NXDK_DIR +"/tools/vp20compiler/vp20compiler"

def _run(command, *args, cwd = None):
    line = [command] + list(args)
    #logger.debug("Running: " + " ".join(line))
    if not cwd:
        raise RunError("No cwd set")
    p = subprocess.Popen(line, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    #if stdout:
    #    logger.debug("  stdout: " + stdout.decode("utf-8"))
    #if stderr:
    #    logger.debug("  stderr: " + stderr.decode("utf-8"))
    return p.returncode, stdout.decode("utf-8"), stderr.decode("utf-8")

def CompileShaderAndConstants(path):
  # Filter lines which describe constants

  program_buffer = bytearray(136 * 4 * 4);
  constant_buffer = bytearray(192 * 4 * 4);

  with open(path) as f: 
    lines = f.readlines()
    for line in lines:
      line = line.strip()
      line = line.replace('\t', ' ')
      if line[0:9] == "#const c[":
        line = line[9:]
        line = line.partition("]")
        index = int(line[0]) # To integer
        print("Index: " + str(index))
        line = line[2].partition("=")
        # assert: line[0] should be all whitespaces
        constant = line[2].split(" ") #FIXME: Split by other whitespaces too?      
        constant = [x for x in constant if x] # Remove empty parts
        assert(len(constant) <= 4)
        print("Values: " + str(constant))
        #FIXME: If value starts with "0x" it should instead be 
        #FIXME: pack_into
        cursor = index * 4 * 4
        for value in constant:
          if value[0:2] == "0x":
            print("Loading '" + value + "' as hexadecimal")
            struct.pack_into("<I", constant_buffer, cursor, int(value, 16))
          else:
            value_f = float(value)
            print("Loading '" + value + "' as float: " + str(value_f))
            struct.pack_into("<f", constant_buffer, cursor, value_f)
          cursor += 4

  # Now compile the shader
  #FIXME: Pipe it into the launched compilation process!?
  out = _run(vp20compiler, path, cwd=os.getcwd())
  if out[0] != 0:
    print(out[2])
    return None

  print('-----')
  print(out[1])
  print('-----')
  words = out[1].split(",")
  while not words[-1].strip():
    words = words[0:-1]
  cursor = 0
  for word in words:
    word = word.strip()
    assert(word[0:2] == "0x")
    struct.pack_into("<I", program_buffer, cursor, int(word.strip(), 16))
    cursor += 4
    

  # Read the resulting file into the program_buffer
  return [program_buffer, constant_buffer]

def RunStateShader():
  pass

xbox_host = sys.argv[1]

path = sys.argv[2]

print("Yo! Compiling and running '" + path + "' on xbox at '" + xbox_host + "'")

program_buffer, constant_buffer = CompileShaderAndConstants(path)

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print("Connecting")
s.connect((xbox_host, xbox_port))
print("Connected")
send_buffer = bytearray(0)
send_buffer.extend(program_buffer)
send_buffer.extend(constant_buffer)
print("Sending " + str(len(send_buffer)) + " bytes")
s.send(send_buffer)
print("Waiting for results")
s.settimeout(5.0) # Wait time in seconds for response

response_size = 192 * 4 * 4
response = bytearray(0)
while(len(response) <= response_size):
  chunk = s.recv(response_size)
  if not chunk: 
    break
  response.extend(chunk)
print("Response was " + str(len(response)) + "/" + str(response_size) + " bytes")
s.close()

#assert(len(response) == len(constant_buffer))

print("Start of differences")

cursor = 0
for i in range(192):
  for j in range(4):
    #FIXME: Create intermediate which is an integer as python handles floats differently.
    #       We only care about the bits for accuracy / precision testing anyway
    value = struct.unpack_from("<I", constant_buffer, cursor)[0]
    value_f = struct.unpack_from("<f", constant_buffer, cursor)[0]
    response_value = struct.unpack_from("<I", response, cursor)[0]
    response_value_f = struct.unpack_from("<f", response, cursor)[0]
    cursor += 4
    if value != response_value:
      print("Difference in c[" + str(i) + "]." + "xyzw"[j] + ": " + str(value_f) + ' (0x' + format(value, '08X') + ') => ' + str(response_value_f) + ' (0x' + format(response_value, '08X') + ')')  

print("End of differences")
