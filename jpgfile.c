//--------------------------------------------------------------------------
// Program to pull the information out of various types of EXIF digital 
// camera files and show it in a reasonably consistent way
//
// This module handles basic Jpeg file handling
//
// Matthias Wandel,  Dec 1999 - Dec 2002 
//--------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#ifdef _WIN32
    #include <process.h>
    #include <io.h>
    #include <sys/utime.h>
#else
    #include <utime.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <errno.h>
    #include <limits.h>
#endif

#include "jhead.h"

// Storage for simplified info extracted from file.
ImageInfo_t ImageInfo;


#define MAX_SECTIONS 20
static Section_t Sections[MAX_SECTIONS];
static int SectionsRead;
static int HaveAll;



#define PSEUDO_IMAGE_MARKER 0x123; // Extra value.
//--------------------------------------------------------------------------
// Get 16 bits motorola order (always) for jpeg header stuff.
//--------------------------------------------------------------------------
static int Get16m(const void * Short)
{
    return (((uchar *)Short)[0] << 8) | ((uchar *)Short)[1];
}


//--------------------------------------------------------------------------
// Process a COM marker.
// We want to print out the marker contents as legible text;
// we must guard against random junk and varying newline representations.
//--------------------------------------------------------------------------
static void process_COM (const uchar * Data, int length)
{
    int ch;
    char Comment[MAX_COMMENT+1];
    int nch;
    int a;

    nch = 0;

    if (length > MAX_COMMENT) length = MAX_COMMENT; // Truncate if it won't fit in our structure.

    for (a=2;a<length;a++){
        ch = Data[a];

        if (ch == '\r' && Data[a+1] == '\n') continue; // Remove cr followed by lf.

        if (ch >= 32 || ch == '\n' || ch == '\t'){
            Comment[nch++] = (char)ch;
        }else{
            Comment[nch++] = '?';
        }
    }

    Comment[nch] = '\0'; // Null terminate

    if (ShowTags){
        printf("COM marker comment: %s\n",Comment);
    }

    strcpy(ImageInfo.Comments,Comment);
}

 
//--------------------------------------------------------------------------
// Process a SOFn marker.  This is useful for the image dimensions
//--------------------------------------------------------------------------
static void process_SOFn (const uchar * Data, int marker)
{
    int data_precision, num_components;

    data_precision = Data[2];
    ImageInfo.Height = Get16m(Data+3);
    ImageInfo.Width = Get16m(Data+5);
    num_components = Data[7];

    if (num_components == 3){
        ImageInfo.IsColor = 1;
    }else{
        ImageInfo.IsColor = 0;
    }

    ImageInfo.Process = marker;

    if (ShowTags){
        printf("JPEG image is %uw * %uh, %d color components, %d bits per sample\n",
                   ImageInfo.Width, ImageInfo.Height, num_components, data_precision);
    }
}


 

//--------------------------------------------------------------------------
// Parse the marker stream until SOS or EOI is seen;
//--------------------------------------------------------------------------
int ReadJpegSections (FILE * infile, ReadMode_t ReadMode)
{
    int a;
    int HaveCom = FALSE;

    a = fgetc(infile);


    if (a != 0xff || fgetc(infile) != M_SOI){
        return FALSE;
    }
    for(;;){
        int itemlen;
        int marker = 0;
        int ll,lh, got;
        uchar * Data;

        if (SectionsRead >= MAX_SECTIONS){
            ErrFatal("Too many sections in jpg file");
        }

        for (a=0;a<7;a++){
            marker = fgetc(infile);
            if (marker != 0xff) break;

            if (a >= 6){
                printf("too many padding bytes\n");
                return FALSE;
            }
        }

        if (marker == 0xff){
            // 0xff is legal padding, but if we get that many, something's wrong.
            ErrFatal("too many padding bytes!");
        }

        Sections[SectionsRead].Type = marker;
  
        // Read the length of the section.
        lh = fgetc(infile);
        ll = fgetc(infile);

        itemlen = (lh << 8) | ll;

        if (itemlen < 2){
            ErrFatal("invalid marker");
        }

        Sections[SectionsRead].Size = itemlen;

        Data = (uchar *)malloc(itemlen);
        if (Data == NULL){
            ErrFatal("Could not allocate memory");
        }
        Sections[SectionsRead].Data = Data;

        // Store first two pre-read bytes.
        Data[0] = (uchar)lh;
        Data[1] = (uchar)ll;

        got = fread(Data+2, 1, itemlen-2, infile); // Read the whole section.
        if (got != itemlen-2){
            ErrFatal("Premature end of file?");
        }
        SectionsRead += 1;

        switch(marker){

            case M_SOS:   // stop before hitting compressed data 
                // If reading entire image is requested, read the rest of the data.
                if (ReadMode & READ_IMAGE){
                    int cp, ep, size;
                    // Determine how much file is left.
                    cp = ftell(infile);
                    fseek(infile, 0, SEEK_END);
                    ep = ftell(infile);
                    fseek(infile, cp, SEEK_SET);

                    size = ep-cp;
                    Data = (uchar *)malloc(size);
                    if (Data == NULL){
                        ErrFatal("could not allocate data for entire image");
                    }

                    got = fread(Data, 1, size, infile);
                    if (got != size){
                        ErrFatal("could not read the rest of the image");
                    }

                    Sections[SectionsRead].Data = Data;
                    Sections[SectionsRead].Size = size;
                    Sections[SectionsRead].Type = PSEUDO_IMAGE_MARKER;
                    SectionsRead ++;
                    HaveAll = 1;
                }
                return TRUE;

            case M_EOI:   // in case it's a tables-only JPEG stream
                printf("No image in jpeg!\n");
                return FALSE;

            case M_COM: // Comment section
                if (HaveCom || ((ReadMode & READ_EXIF) == 0)){
                    // Discard this section.
                    free(Sections[--SectionsRead].Data);
                }else{
                    process_COM(Data, itemlen);
                    HaveCom = TRUE;
                }
                break;

            case M_JFIF:
                // Regular jpegs always have this tag, exif images have the exif
                // marker instead, althogh ACDsee will write images with both markers.
                // this program will re-create this marker on absence of exif marker.
                // hence no need to keep the copy from the file.
                free(Sections[--SectionsRead].Data);
                break;

            case M_EXIF:
                // Seen files from some 'U-lead' software with Vivitar scanner
                // that uses marker 31 for non exif stuff.  Thus make sure 
                // it says 'Exif' in the section before treating it as exif.
                if ((ReadMode & READ_EXIF) && memcmp(Data+2, "Exif", 4) == 0){
                    process_EXIF((char *)Data, itemlen);
                }else{
                    // Discard this section.
                    free(Sections[--SectionsRead].Data);
                }
                break;

            case M_SOF0: 
            case M_SOF1: 
            case M_SOF2: 
            case M_SOF3: 
            case M_SOF5: 
            case M_SOF6: 
            case M_SOF7: 
            case M_SOF9: 
            case M_SOF10:
            case M_SOF11:
            case M_SOF13:
            case M_SOF14:
            case M_SOF15:
                process_SOFn(Data, marker);
                break;
            default:
                // Skip any other sections.
                if (ShowTags){
                    printf("Jpeg section marker 0x%02x size %d\n",marker, itemlen);
                }
                break;
        }
    }
    return TRUE;
}

//--------------------------------------------------------------------------
// Discard read data.
//--------------------------------------------------------------------------
void DiscardData(void)
{
    int a;
    for (a=0;a<SectionsRead;a++){
        free(Sections[a].Data);
    }
    memset(&ImageInfo, 0, sizeof(ImageInfo));
    SectionsRead = 0;
    HaveAll = 0;
}

//--------------------------------------------------------------------------
// Read image data.
//--------------------------------------------------------------------------
int ReadJpegFile(const char * FileName, ReadMode_t ReadMode)
{
    FILE * infile;
    int ret;

    infile = fopen(FileName, "rb"); // Unix ignores 'b', windows needs it.

    if (infile == NULL) {
        fprintf(stderr, "can't open '%s'\n", FileName);
        return FALSE;
    }

    // Scan the JPEG headers.
    ret = ReadJpegSections(infile, ReadMode);
    if (!ret){
        printf("Not JPEG: %s\n",FileName);
    }

    fclose(infile);

    if (ret == FALSE){
        DiscardData();
    }
    return ret;
}

//--------------------------------------------------------------------------
// Remove exif thumbnail
//--------------------------------------------------------------------------
int TrimExifFunc(void)
{
    int a;
    for (a=0;a<SectionsRead-1;a++){
        if (Sections[a].Type == M_EXIF && memcmp(Sections[a].Data+2, "Exif",4)==0){
            unsigned int NewSize;
            NewSize = RemoveThumbnail(Sections[a].Data, Sections[a].Size);
            // Truncate the thumbnail section of the exif.
            printf("%d bytes removed\n",Sections[a].Size-NewSize);
            if (Sections[a].Size == NewSize) return FALSE; // Nothing removed.
            Sections[a].Size = NewSize;
            Sections[a].Data[0] = (uchar)(NewSize >> 8);
            Sections[a].Data[1] = (uchar)NewSize;
            return TRUE;
        }
    }
    // Not an exif image.  Can't remove exif thumbnail.
    return FALSE;
}

//--------------------------------------------------------------------------
// Discard everything but the exif and comment sections.
//--------------------------------------------------------------------------
void DiscardAllButExif(void)
{
    Section_t ExifKeeper;
    Section_t CommentKeeper;
    int a;

    memset(&ExifKeeper, 0, sizeof(ExifKeeper));
    memset(&CommentKeeper, 0, sizeof(ExifKeeper));

    for (a=0;a<SectionsRead;a++){
        if (Sections[a].Type == M_EXIF && ExifKeeper.Type == 0){
            ExifKeeper = Sections[a];
        }else if (Sections[a].Type == M_COM && CommentKeeper.Type == 0){
            CommentKeeper = Sections[a];
        }else{
            free(Sections[a].Data);
        }
    }
    SectionsRead = 0;
    if (ExifKeeper.Type){
        Sections[SectionsRead++] = ExifKeeper;
    }
    if (CommentKeeper.Type){
        Sections[SectionsRead++] = CommentKeeper;
    }
}    

//--------------------------------------------------------------------------
// Write image data back to disk.
//--------------------------------------------------------------------------
void WriteJpegFile(const char * FileName)
{
    FILE * outfile;
    int a;

    if (!HaveAll){
        ErrFatal("Can't write back - didn't read all");
    }

    outfile = fopen(FileName,"wb");
    if (outfile == NULL){
        ErrFatal("Could not open file for write");
    }

    // Initial static jpeg marker.
    fputc(0xff,outfile);
    fputc(0xd8,outfile);
    
    if (Sections[0].Type != M_EXIF && Sections[0].Type != M_JFIF){
        // The image must start with an exif or jfif marker.  If we threw those away, create one.
        static uchar JfifHead[18] = {
            0xff, M_JFIF,
            0x00, 0x10, 'J' , 'F' , 'I' , 'F' , 0x00, 0x01, 
            0x01, 0x01, 0x01, 0x2C, 0x01, 0x2C, 0x00, 0x00 
        };
        fwrite(JfifHead, 18, 1, outfile);
    }

    // Write all the misc sections
    for (a=0;a<SectionsRead-1;a++){
        fputc(0xff,outfile);
        fputc(Sections[a].Type, outfile);
        fwrite(Sections[a].Data, Sections[a].Size, 1, outfile);
    }

    // Write the remaining image data.
    fwrite(Sections[a].Data, Sections[a].Size, 1, outfile);
       
    fclose(outfile);
}


//--------------------------------------------------------------------------
// Check if image has exif header.
//--------------------------------------------------------------------------
Section_t * FindSection(int SectionType)
{
    int a;
    for (a=0;a<SectionsRead-1;a++){
        if (Sections[a].Type == SectionType){
            return &Sections[a];
        }
    }
    // Could not be found.
    return NULL;
}

//--------------------------------------------------------------------------
// Remove a certain type of section.
//--------------------------------------------------------------------------
int RemoveSectionType(int SectionType)
{
    int a;
    for (a=0;a<SectionsRead-1;a++){
        if (Sections[a].Type == SectionType){
            // Free up this section
            free (Sections[a].Data);
            // Move succeding sections back by one to close space in array.
            memmove(Sections+a, Sections+a+1, sizeof(Section_t) * (SectionsRead-a));
            SectionsRead -= 1;
            return TRUE;
        }
    }
    return FALSE;
}

//--------------------------------------------------------------------------
// Add a section (assume it doesn't already exist) - used for 
// adding comment sections.
//--------------------------------------------------------------------------
Section_t * CreateSection(int SectionType, unsigned char * Data, int Size)
{
    Section_t * NewSection;
    int a;

    // Insert it in third position - seems like a safe place to put 
    // things like comments.

    if (SectionsRead < 2){
        ErrFatal("Too few sections!");
    }
    if (SectionsRead >= MAX_SECTIONS){
        ErrFatal("Too many sections!");
    }

    for (a=SectionsRead;a>2;a--){
        Sections[a] = Sections[a-1];          
    }
    SectionsRead += 1;

    NewSection = Sections+2;

    NewSection->Type = SectionType;
    NewSection->Size = Size;
    NewSection->Data = Data;

    return NewSection;
}


//--------------------------------------------------------------------------
// Initialisation.
//--------------------------------------------------------------------------
void ResetJpgfile(void)
{
    memset(&Sections, 0, sizeof(Sections));
    SectionsRead = 0;
    HaveAll = 0;
}
