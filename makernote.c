//--------------------------------------------------------------------------
// Parse some maker specific onformation.
// (Very limited right now - add maker specific stuff to this module)
//--------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#ifndef _WIN32
    #include <limits.h>
#endif

#include "jhead.h"
//--------------------------------------------------------------------------
// Process exif format directory, as used by Cannon maker note
//--------------------------------------------------------------------------
void ProcessCannonMakerNoteDir(unsigned char * DirStart, unsigned char * OffsetBase, 
        unsigned ExifLength)
{
    int de;
    int a;
    int NumDirEntries;

    NumDirEntries = Get16u(DirStart);
    #define DIR_ENTRY_ADDR(Start, Entry) (Start+2+12*(Entry))

    {
        unsigned char * DirEnd;
        DirEnd = DIR_ENTRY_ADDR(DirStart, NumDirEntries);
        if (DirEnd > (OffsetBase+ExifLength)){
            // Note: Files that had thumbnails trimmed with jhead 1.3 or earlier
            // might trigger this.
            ErrNonfatal("Illegally sized directory",0,0);
            return;
        }
    }

    if (ShowTags){
        printf("(dir has %d entries)\n",NumDirEntries);
    }

    for (de=0;de<NumDirEntries;de++){
        int Tag, Format, Components;
        unsigned char * ValuePtr;
        int ByteCount;
        char * DirEntry;
        DirEntry = DIR_ENTRY_ADDR(DirStart, de);

        Tag = Get16u(DirEntry);
        Format = Get16u(DirEntry+2);
        Components = Get32u(DirEntry+4);

        if ((Format-1) >= NUM_FORMATS) {
            // (-1) catches illegal zero case as unsigned underflows to positive large.
            ErrNonfatal("Illegal number format %d for tag %04x", Format, Tag);
            continue;
        }

        ByteCount = Components * BytesPerFormat[Format];

        if (ByteCount > 4){
            unsigned OffsetVal;
            OffsetVal = Get32u(DirEntry+8);
            // If its bigger than 4 bytes, the dir entry contains an offset.
            if (OffsetVal+ByteCount > ExifLength){
                // Bogus pointer offset and / or bytecount value
                ErrNonfatal("Illegal value pointer for tag %04x", Tag,0);
                continue;
            }
            ValuePtr = OffsetBase+OffsetVal;
        }else{
            // 4 bytes or less and value is in the dir entry itself
            ValuePtr = DirEntry+8;
        }

        if (ShowTags){
            // Show tag name
            printf("            Canon maker tag %04x Value = ", Tag);
        }

        // Show tag value.
        switch(Format){

            case FMT_UNDEFINED:
                // Undefined is typically an ascii string.

            case FMT_STRING:
                // String arrays printed without function call (different from int arrays)
                {
                    printf("\"");
                    for (a=0;a<ByteCount;a++){
                        int ZeroSkipped = 0;
                        if (ValuePtr[a] >= 32){
                            if (ZeroSkipped){
                                printf("?");
                                ZeroSkipped = 0;
                            }
                            putchar(ValuePtr[a]);
                        }else{
                            if (ValuePtr[a] == 0){
                                ZeroSkipped = 1;
                            }
                        }
                    }
                    printf("\"\n");
                }
                break;

            default:
                // Handle arrays of numbers later (will there ever be?)
                PrintFormatNumber(ValuePtr, Format, ByteCount);
                printf("\n");
        }
        
    }
}

//--------------------------------------------------------------------------
// Show generic maker note - just hex bytes.
//--------------------------------------------------------------------------
void ShowMakerNoteGeneric(unsigned char * ValuePtr, int ByteCount)
{
    int a;
    for (a=0;a<ByteCount;a++){
        if (a > 10){
            printf("...");
            break;
        }
        printf(" %02x",ValuePtr[a]);
    }
    printf(" (%d bytes)", ByteCount);
    printf("\n");

}

//--------------------------------------------------------------------------
// Process maker note - to the limited extent that its supported.
//--------------------------------------------------------------------------
void ProcessMakerNote(unsigned char * ValuePtr, int ByteCount, 
        unsigned char * OffsetBase, unsigned ExifLength)
{

    if (ShowTags){
        if (strstr(ImageInfo.CameraMake, "Canon")){
            ProcessCannonMakerNoteDir(ValuePtr, OffsetBase, ExifLength);
        }else{
            ShowMakerNoteGeneric(ValuePtr, ByteCount);
        }
    }
}

