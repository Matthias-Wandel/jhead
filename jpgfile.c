//--------------------------------------------------------------------------
// Program to pull the information out of various types of EXIF digital
// camera files and show it in a reasonably consistent way
//
// This module handles basic Jpeg file handling
//
// Matthias Wandel
//--------------------------------------------------------------------------
#include "jhead.h"

static ImgSect_t * JpgSections = NULL;
static int JpgSectionsAllocated;
static int JpgSectionsRead;
static int HaveAllOfJpeg;

#define PSEUDO_IMAGE_MARKER 0x123; // Extra value.
//--------------------------------------------------------------------------
// Get 16 bits motorola order (always) for jpeg header stuff.
//--------------------------------------------------------------------------
static int Get16m(const void * Short)
{
    return (((uchar *)Short)[0] << 8) | ((uchar *)Short)[1];
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
// Check sections array to see if it needs to be increased in size.
//--------------------------------------------------------------------------
static void CheckJpgSectionsAllocated(void)
{
    if (JpgSectionsRead > JpgSectionsAllocated){
        ErrFatal("allocation screwup");
    }
    if (JpgSectionsRead >= JpgSectionsAllocated){
        JpgSectionsAllocated += JpgSectionsAllocated/2;
        JpgSections = (ImgSect_t *)realloc(JpgSections, sizeof(ImgSect_t)*JpgSectionsAllocated);
        if (JpgSections == NULL){
            ErrFatal("could not allocate data for entire image");
        }
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

    ImageInfo.JfifHeader.XDensity = ImageInfo.JfifHeader.YDensity = 300;
    ImageInfo.JfifHeader.ResolutionUnits = 1;

    for(;;){
        int itemlen;
        int prev;
        int marker = 0;
        int ll,lh, got;
        uchar * Data;

        CheckJpgSectionsAllocated();

        prev = 0;
        for (a=0;;a++){
            marker = fgetc(infile);
            if (marker != 0xff && prev == 0xff) break;
            if (marker == EOF){
                ErrFatal("Unexpected end of file");
            }
            prev = marker;
        }

        if (a > 10){
            ErrNonfatal("Extraneous %d padding bytes before section %02X",a-1,marker);
        }

        JpgSections[JpgSectionsRead].Type = marker;

        // Read the length of the section.
        lh = fgetc(infile);
        ll = fgetc(infile);
        if (lh == EOF || ll == EOF){
            ErrFatal("Unexpected end of file");
        }

        itemlen = (lh << 8) | ll;

        if (itemlen < 2){
            ErrFatal("invalid marker");
        }

        JpgSections[JpgSectionsRead].Size = itemlen;

        // Allocate an extra 20 bytes more than needed, because sometimes when reading structures,
        // if the section erroneously ends before short structures that should be there, that can trip
        // memory checkers in combination with fuzzers.
        Data = (uchar *)malloc(itemlen+20);
        if (Data == NULL){
            ErrFatal("Could not allocate memory");
        }
        memset(Data, 0, 20);
        JpgSections[JpgSectionsRead].Data = Data;

        // Store first two pre-read bytes.
        Data[0] = (uchar)lh;
        Data[1] = (uchar)ll;

        got = fread(Data+2, 1, itemlen-2, infile); // Read the whole section.
        if (got != itemlen-2){
            ErrFatal("Premature end of file?");
        }
        JpgSectionsRead += 1;

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

                    CheckJpgSectionsAllocated();
                    JpgSections[JpgSectionsRead].Data = Data;
                    JpgSections[JpgSectionsRead].Size = size;
                    JpgSections[JpgSectionsRead].Type = PSEUDO_IMAGE_MARKER;
                    JpgSectionsRead ++;
                    HaveAllOfJpeg = 1;
                }
                return TRUE;

            case M_DQT:
                // Use for jpeg quality guessing
                process_DQT(Data, itemlen);
                break;

            case M_DHT:
                // Use for jpeg quality guessing
                process_DHT(Data, itemlen);
                break;

            case M_EOI:   // in case it's a tables-only JPEG stream
                fprintf(stderr,"No image in jpeg!\n");
                return FALSE;

            case M_COM: // Comment section
                if (HaveCom || ((ReadMode & READ_METADATA) == 0)){
                    // Discard this section.
                    free(JpgSections[--JpgSectionsRead].Data);
                }else{
                    ProcessImgComment(Data+2, itemlen-2);
                    HaveCom = TRUE;
                }
                break;

            case M_JFIF:
                // Regular jpegs always have this tag, exif images have the exif
                // marker instead, although ACDsee will write images with both markers.
                // this program will re-create this marker on absence of exif marker.
                // hence no need to keep the copy from the file.
                if (itemlen < 16){
                    fprintf(stderr,"Jfif header too short\n");
                    goto ignore;
                }
                if (memcmp(Data+2, "JFIF\0",5)){
                    fprintf(stderr,"Header missing JFIF marker\n");
                }

                ImageInfo.JfifHeader.Present = TRUE;
                ImageInfo.JfifHeader.ResolutionUnits = Data[9];
                ImageInfo.JfifHeader.XDensity = (Data[10]<<8) | Data[11];
                ImageInfo.JfifHeader.YDensity = (Data[12]<<8) | Data[13];
                if (ShowTags){
                    printf("JFIF SOI marker: Units: %d ",ImageInfo.JfifHeader.ResolutionUnits);
                    switch(ImageInfo.JfifHeader.ResolutionUnits){
                        case 0: printf("(aspect ratio)"); break;
                        case 1: printf("(dots per inch)"); break;
                        case 2: printf("(dots per cm)"); break;
                        default: printf("(unknown)"); break;
                    }
                    printf("  X-density=%d Y-density=%d\n",ImageInfo.JfifHeader.XDensity, ImageInfo.JfifHeader.YDensity);

                    if (Data[14] || Data[15]){
                        fprintf(stderr,"Ignoring jfif header thumbnail\n");
                    }
                }

                ignore:

                free(JpgSections[--JpgSectionsRead].Data);
                break;

            case M_EXIF:
                // There can be different section using the same marker.
                if (ReadMode & READ_METADATA){
                    if (memcmp(Data+2, "Exif\0\0", 6) == 0){
                        if (!process_EXIF(Data+8, itemlen-8)){
                            // malformatted exif sections, discard.
                            free(JpgSections[--JpgSectionsRead].Data);
						}
                        break;
                    }else if (memcmp(Data+2, "http:", 5) == 0){
                        JpgSections[JpgSectionsRead-1].Type = M_XMP; // Change tag for internal purposes.
                        if (ShowTags){
                            printf("Image contains XMP section, %d bytes long\n", itemlen);
                            if (ShowTags){
                                ShowXmp(JpgSections[JpgSectionsRead-1]);
                            }
                        }
                        break;
                    }
                }
                // Otherwise, discard this section.
                free(JpgSections[--JpgSectionsRead].Data);
                break;

            case M_IPTC:
                if (ReadMode & READ_METADATA){
                    if (ShowTags){
                        printf("Image contains IPTC section, %d bytes long\n", itemlen);
                    }
                    // Note: We just store the IPTC section.  Its relatively straightforward
                    // and we don't act on any part of it, so just display it at parse time.
                }else{
                    free(JpgSections[--JpgSectionsRead].Data);
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
                if (itemlen < 8){
                    fprintf(stderr,"Section too short\n");
                    break;
                }
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
void DiscardJpegData(void)
{
    int a;

    for (a=0;a<JpgSectionsRead;a++){
        free(JpgSections[a].Data);
    }
    
    JpgSectionsRead = 0;
    HaveAllOfJpeg = 0;
}

//--------------------------------------------------------------------------
// Read image data.
//--------------------------------------------------------------------------
int ReadJpegFile(FILE * infile, ReadMode_t ReadMode)
{
    int ret;

    ret = ReadJpegSections(infile, ReadMode);
    if (!ret){
        if (ReadMode == READ_ANY){
            // Process any files mode.  Ignore the fact that it's not
            // a jpeg file.
            ret = TRUE;
        }else{
            fprintf(stderr,"Not JPEG\n");
        }
    }

    fclose(infile);

    if (ret == FALSE){
        DiscardJpegData();
    }
    return ret;
}


//--------------------------------------------------------------------------
// Replace or remove exif thumbnail
//--------------------------------------------------------------------------
int SaveJpegThumbnail(char * ThumbFileName)
{
    FILE * ThumbnailFile;

    if (ImageInfo.ThumbnailOffset == 0 || ImageInfo.ThumbnailSize == 0){
        fprintf(stderr,"Image contains no thumbnail\n");
        return FALSE;
    }

    if (strcmp(ThumbFileName, "-") == 0){
        // A filename of '-' indicates thumbnail goes to stdout.
        // This doesn't make much sense under Windows, so this feature is unix only.
        ThumbnailFile = stdout;
    }else{
        ThumbnailFile = fopen(ThumbFileName,"wb");
    }

    if (ThumbnailFile){
        uchar * ThumbnailPointer;
        ImgSect_t * ExifSection;
        ExifSection = FindJpegSection(M_EXIF);
        ThumbnailPointer = ExifSection->Data+ImageInfo.ThumbnailOffset+8;

        fwrite(ThumbnailPointer, ImageInfo.ThumbnailSize ,1, ThumbnailFile);
        fclose(ThumbnailFile);
        return TRUE;
    }else{
        ErrFatal("Could not write thumbnail file");
        return FALSE;
    }
}

//--------------------------------------------------------------------------
// Replace or remove exif thumbnail
//--------------------------------------------------------------------------
int ReplaceJpegThumbnail(const char * ThumbFileName)
{
    FILE * ThumbnailFile;
    int ThumbLen, NewExifSize;
    ImgSect_t * ExifSection;
    uchar * ThumbnailPointer;

    if (ImageInfo.ThumbnailOffset == 0 || ImageInfo.ThumbnailAtEnd == FALSE){
        if (ThumbFileName == NULL){
            // Delete of nonexistent thumbnail (not even pointers present)
            // No action, no error.
            return FALSE;
        }

        // Adding or removing of thumbnail is not possible - that would require rearranging
        // of the exif header, which is risky, and jhead doesn't know how to do.
        fprintf(stderr,"Image contains no thumbnail to replace - add is not possible\n");
        return FALSE;
    }

    if (ThumbFileName){
        ThumbnailFile = fopen(ThumbFileName,"rb");

        if (ThumbnailFile == NULL){
            noread:
            ErrFatal("Could not read thumbnail file");
            return FALSE;
        }

        // get length
        fseek(ThumbnailFile, 0, SEEK_END);

        ThumbLen = ftell(ThumbnailFile);
        fseek(ThumbnailFile, 0, SEEK_SET);

        if (ThumbLen + ImageInfo.ThumbnailOffset > 0x10000-20){
            ErrFatal("Thumbnail is too large to insert into exif header");
        }
    }else{
        if (ImageInfo.ThumbnailSize == 0){
             return FALSE;
        }

        ThumbLen = 0;
        ThumbnailFile = NULL;
    }

    ExifSection = FindJpegSection(M_EXIF);

    NewExifSize = ImageInfo.ThumbnailOffset+8+ThumbLen;
    ExifSection->Data = (uchar *)realloc(ExifSection->Data, NewExifSize);

    ThumbnailPointer = ExifSection->Data+ImageInfo.ThumbnailOffset+8;

    if (ThumbnailFile){
        if (fread(ThumbnailPointer, 1, ThumbLen, ThumbnailFile) != ThumbLen){
            goto noread;
        }
        fclose(ThumbnailFile);
    }

    ImageInfo.ThumbnailSize = ThumbLen;

    Put32u(ExifSection->Data+ImageInfo.ThumbnailSizeOffset+8, ThumbLen);

    ExifSection->Data[0] = (uchar)(NewExifSize >> 8);
    ExifSection->Data[1] = (uchar)NewExifSize;
    ExifSection->Size = NewExifSize;

    return TRUE;
}


//--------------------------------------------------------------------------
// Discard everything but the exif and comment sections.
//--------------------------------------------------------------------------
void DiscardAllJpegButExif(void)
{
    ImgSect_t ExifKeeper;
    ImgSect_t CommentKeeper;
    ImgSect_t IptcKeeper;
    ImgSect_t XmpKeeper;
    int a;

    memset(&ExifKeeper, 0, sizeof(ExifKeeper));
    memset(&CommentKeeper, 0, sizeof(CommentKeeper));
    memset(&IptcKeeper, 0, sizeof(IptcKeeper));
    memset(&XmpKeeper, 0, sizeof(IptcKeeper));

    for (a=0;a<JpgSectionsRead;a++){
        if (JpgSections[a].Type == M_EXIF && ExifKeeper.Type == 0){
           ExifKeeper = JpgSections[a];
        }else if (JpgSections[a].Type == M_XMP && XmpKeeper.Type == 0){
           XmpKeeper = JpgSections[a];
        }else if (JpgSections[a].Type == M_COM && CommentKeeper.Type == 0){
            CommentKeeper = JpgSections[a];
        }else if (JpgSections[a].Type == M_IPTC && IptcKeeper.Type == 0){
            IptcKeeper = JpgSections[a];
        }else{
            free(JpgSections[a].Data);
        }
    }
    JpgSectionsRead = 0;
    if (ExifKeeper.Type){
        CheckJpgSectionsAllocated();
        JpgSections[JpgSectionsRead++] = ExifKeeper;
    }
    if (CommentKeeper.Type){
        CheckJpgSectionsAllocated();
        JpgSections[JpgSectionsRead++] = CommentKeeper;
    }
    if (IptcKeeper.Type){
        CheckJpgSectionsAllocated();
        JpgSections[JpgSectionsRead++] = IptcKeeper;
    }

    if (XmpKeeper.Type){
        CheckJpgSectionsAllocated();
        JpgSections[JpgSectionsRead++] = XmpKeeper;
    }
}

//--------------------------------------------------------------------------
// Write image data back to disk.
//--------------------------------------------------------------------------
void WriteJpegFile(const char * FileName)
{
    FILE * outfile;
    int a;

    if (!HaveAllOfJpeg){
        ErrFatal("Can't write back - didn't read all");
    }

    outfile = fopen(FileName,"wb");
    if (outfile == NULL){
        ErrFatal("Could not open file for write");
    }

    // Initial static jpeg marker.
    fputc(0xff,outfile);
    fputc(0xd8,outfile);

    if (JpgSections[0].Type != M_EXIF && JpgSections[0].Type != M_JFIF){
        // The image must start with an exif or jfif marker.  If we threw those away, create one.
        static uchar JfifHead[18] = {
            0xff, M_JFIF,
            0x00, 0x10, 'J' , 'F' , 'I' , 'F' , 0x00, 0x01,
            0x01, 0x01, 0x01, 0x2C, 0x01, 0x2C, 0x00, 0x00
        };

        if (ImageInfo.ResolutionUnit == 2 || ImageInfo.ResolutionUnit == 3){
            // Use the exif resolution info to fill out the jfif header.
            // Usually, for exif images, there's no jfif header, so if we discard
            // the exif header, use info from the exif header for the jfif header.

            ImageInfo.JfifHeader.ResolutionUnits = (char)(ImageInfo.ResolutionUnit-1);
            // Jfif is 1 and 2, Exif is 2 and 3 for In and cm respectively
            ImageInfo.JfifHeader.XDensity = (int)ImageInfo.xResolution;
            ImageInfo.JfifHeader.YDensity = (int)ImageInfo.yResolution;
        }

        JfifHead[11] = ImageInfo.JfifHeader.ResolutionUnits;
        JfifHead[12] = (uchar)(ImageInfo.JfifHeader.XDensity >> 8);
        JfifHead[13] = (uchar)ImageInfo.JfifHeader.XDensity;
        JfifHead[14] = (uchar)(ImageInfo.JfifHeader.YDensity >> 8);
        JfifHead[15] = (uchar)ImageInfo.JfifHeader.YDensity;


        fwrite(JfifHead, 18, 1, outfile);

        // use the values from the exif data for the jfif header, if we have found values
        if (ImageInfo.ResolutionUnit != 0) {
            // JFIF.ResolutionUnit is {1,2}, EXIF.ResolutionUnit is {2,3}
            JfifHead[11] = (uchar)ImageInfo.ResolutionUnit - 1;
        }
        if (ImageInfo.xResolution > 0.0 && ImageInfo.yResolution > 0.0) {
            JfifHead[12] = (uchar)((int)ImageInfo.xResolution>>8);
            JfifHead[13] = (uchar)((int)ImageInfo.xResolution);

            JfifHead[14] = (uchar)((int)ImageInfo.yResolution>>8);
            JfifHead[15] = (uchar)((int)ImageInfo.yResolution);
        }
    }


    // Write all the misc sections
    for (a=0;a<JpgSectionsRead-1;a++){
        fputc(0xff,outfile);
        fputc((unsigned char)JpgSections[a].Type, outfile);
        fwrite(JpgSections[a].Data, JpgSections[a].Size, 1, outfile);
    }

    // Write the remaining image data.
    fwrite(JpgSections[a].Data, JpgSections[a].Size, 1, outfile);

    fclose(outfile);
}


//--------------------------------------------------------------------------
// Check if image has exif header.
//--------------------------------------------------------------------------
ImgSect_t * FindJpegSection(int SectionType)
{
    int a;

    for (a=0;a<JpgSectionsRead;a++){
        if (JpgSections[a].Type == SectionType){
            return &JpgSections[a];
        }
    }
    // Could not be found.
    return NULL;
}

//--------------------------------------------------------------------------
// Remove a certain type of section.
//--------------------------------------------------------------------------
int RemoveJpegSectionByType(int SectionType)
{
    int a;
    int retval = FALSE;

    for (a=0;a<JpgSectionsRead-1;a++){
        if (JpgSections[a].Type == SectionType){
            // Free up this section
            free (JpgSections[a].Data);
            // Move succeeding sections back by one to close space in array.
            memmove(JpgSections+a, JpgSections+a+1, sizeof(ImgSect_t) * (JpgSectionsRead-a-1));
            JpgSectionsRead -= 1;
            a -= 1;
            retval = TRUE;
        }
    }
    return retval;
}

//--------------------------------------------------------------------------
// Remove sections not part of image and not exif or comment sections.
//--------------------------------------------------------------------------
int RemoveUnknownJpegSections(void)
{
    int a;
    int Modified = FALSE;
    for (a=0;a<JpgSectionsRead-1;){
        switch(JpgSections[a].Type){
            case  M_SOF0:
            case  M_SOF1:
            case  M_SOF2:
            case  M_SOF3:
            case  M_SOF5:
            case  M_SOF6:
            case  M_SOF7:
            case  M_SOF9:
            case  M_SOF10:
            case  M_SOF11:
            case  M_SOF13:
            case  M_SOF14:
            case  M_SOF15:
            case  M_SOI:
            case  M_EOI:
            case  M_SOS:
            case  M_JFIF:
            case  M_EXIF:
            case  M_XMP:
            case  M_COM:
            case  M_DQT:
            case  M_DHT:
            case  M_DRI:
            case  M_IPTC:
                // keep.
                a++;
                break;
            default:
                // Unknown.  Delete.
                free (JpgSections[a].Data);
                // Move succeeding sections back by one to close space in array.
                memmove(JpgSections+a, JpgSections+a+1, sizeof(ImgSect_t) * (JpgSectionsRead-a-1));
                JpgSectionsRead -= 1;
                Modified = TRUE;
        }
    }
    return Modified;
}

//--------------------------------------------------------------------------
// Add a section (assume it doesn't already exist) - used for
// adding comment sections and exif sections
//--------------------------------------------------------------------------
ImgSect_t * CreateJpegSection(int SectionType, unsigned char * Data, int Size)
{
    ImgSect_t * NewSection;
    int a;
    int NewIndex;

    NewIndex = 0; // Figure out where to put the comment section.
    if (SectionType == M_EXIF){
        // Exif always goes first!
    }else{
        for (;NewIndex < 3;NewIndex++){ // Maximum fourth position (just for the heck of it)
            if (JpgSections[NewIndex].Type == M_JFIF) continue; // Put it after Jfif
            if (JpgSections[NewIndex].Type == M_EXIF) continue; // Put it after Exif
            break;
        }
    }

    if (JpgSectionsRead < NewIndex){
        ErrFatal("Too few sections!");
    }

    CheckJpgSectionsAllocated();
    for (a=JpgSectionsRead;a>NewIndex;a--){
        JpgSections[a] = JpgSections[a-1];
    }
    JpgSectionsRead += 1;

    NewSection = JpgSections+NewIndex;

    NewSection->Type = SectionType;
    NewSection->Size = Size;
    NewSection->Data = Data;

    return NewSection;
}


//--------------------------------------------------------------------------
// Make a new minmial exif header (replacing existing one if there is one)
//--------------------------------------------------------------------------
void CreateMinimalJpegExif(void)
{
    char Buffer[256];
    memcpy(Buffer+2, "Exif\0\0",6);
    
    int len = CreateMinimalExif(Buffer+8); // create the actual Exif structure
    len += 8; // For the length bytes and 'Exif\0\0'

    Buffer[0] = (unsigned char)(len >> 8);
    Buffer[1] = (unsigned char)len;

    // Remove old exif section, if there was one.
    RemoveJpegSectionByType(M_EXIF);
    
    // Sections need malloced buffers, so do that now, especially because
    // we now know how big it needs to be allocated.
    unsigned char * NewBuf = malloc(len);
    if (NewBuf == NULL){
        ErrFatal("Could not allocate memory");
    }
    memcpy(NewBuf, Buffer, len);

    CreateImgSection(M_EXIF, NewBuf, len);

    // Re-parse new exif section, now that its in place
    // otherwise, we risk touching data that has already been freed.
    process_EXIF(NewBuf+8, len-8);
}


//--------------------------------------------------------------------------
// Set or replace the comment section (M_COM)
//--------------------------------------------------------------------------
void SetJpegCommentTo(char * NewCommentStr)
{
    if (NewCommentStr == NULL){
        // Actually want to remove comment section.
        RemoveJpegSectionByType(M_COM);
        return;
    }

    ImgSect_t * CommentSec;
    int CommentSize = strlen(NewCommentStr);
    CommentSec = FindImgSection(M_COM);

    if (CommentSec){
        // Discard the old data section, as length will likey be different.
        free(CommentSec->Data);
    }else{
        CommentSec = CreateImgSection(M_COM, NULL,2);
    }
    // Discard old comment section and put a new one in.
    int size;
    size = CommentSize+2;
    CommentSec->Size = size;
    CommentSec->Data = malloc(size);
    CommentSec->Data[0] = (uchar)(size >> 8);
    CommentSec->Data[1] = (uchar)(size);
    memcpy((CommentSec->Data)+2, NewCommentStr, CommentSize);
}


//--------------------------------------------------------------------------
// Initialisation.
//--------------------------------------------------------------------------
void ResetJpegFile(void)
{
    if (JpgSections == NULL){
        JpgSections = (ImgSect_t *)malloc(sizeof(ImgSect_t)*5);
        JpgSectionsAllocated = 5;
    }

    JpgSectionsRead = 0;
    HaveAllOfJpeg = 0;
}
