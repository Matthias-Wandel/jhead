# A python script for checking for allocation gaps in the exif header.
# Any data unaccounted for could potentially be referenced by the maker notes
# This to help investigate the feasibility of rewriting the whole exif header.
#
# Dec 27 2005  Matthias Wandel
import sys, os, string

# Run jhead.  Must be compiled with EXIF_MAP option turned on in jhead.h
os.system("jhead -exifmap "+sys.argv[1]+" > foo.txt")

ExifData = [];
ExifMap = [];

# read in the output from jhead
for line in open("foo.txt"):
    if line[0:4] != "Map:": continue
    ExifData.append(line)
    
    if line[10] == '-':
        if line.find("makernote") >= 0: continue
        ExifMap.append(line);
        if line.find("End of exif") >= 0: continue
        if line[5:9] != line[11:15]:
            ExifData.append("Map: "+line[11:16]+"*End   "+line[17:])

ExifMap.sort();

# Find unallocated pieces of the exif header
usedto = "00008"
for line in ExifMap:
    segs = line[5:10]
    sege = line[11:16]
    if usedto < segs:
        print ("Map: "+ usedto+"-"+segs+ " Gap!!!\n")
        ExifData.append("Map: "+ usedto+"-"+segs+ " Gap!!!\n")
        ExifData.append("Map: "+segs+" End of gap\n");

    if usedto > segs:
        print ("Map: "+ usedto+"-"+segs+ " Negative Gap!!!\n");
        ExifData.append("Map: "+ usedto+"-"+segs+ " Negative Gap!!!\n");
        
    usedto = sege

print

ExifData.sort()

# Print the exif data, as well as any gaps found in it.
for line in ExifData:
    print (line.rstrip("\n"))



