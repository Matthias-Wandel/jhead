//--------------------------------------------------------------------------
// webpfile.c - WebP support for jhead
//--------------------------------------------------------------------------
#include "jhead.h"

static ImgSect_t * WebpSections = NULL;
static int WebpSectionsAllocated;
static int WebpSectionsRead;

// WebP (RIFF) uses little-endian for lengths
static unsigned int Get32webp(const uchar * Data) {
    return (Data[3] << 24) | (Data[2] << 16) | (Data[1] << 8) | Data[0];
}

static void Put32webp(uchar * Data, unsigned int Value) {
    Data[3] = (uchar)(Value >> 24);
    Data[2] = (uchar)(Value >> 16);
    Data[1] = (uchar)(Value >> 8);
    Data[0] = (uchar)Value;
}

void ResetWebpFile(void) {
    if (WebpSections == NULL) {
        WebpSections = (ImgSect_t *)malloc(sizeof(ImgSect_t) * 10);
        WebpSectionsAllocated = 10;
    }
    WebpSectionsRead = 0;
}


//--------------------------------------------------------------------------
// Process WebP geometry chunks (VP8, VP8L, or VP8X)
//--------------------------------------------------------------------------
static void ProcessWebpGeometry(int ChunkType, const uchar * Data, int ChunkLen)
{
    if (ChunkType == 0x56503858) { // "VP8X" (Extended)
        if (ChunkLen >= 10) {
            // Width is 24 bits at offset 4, Height is 24 bits at offset 7
            // Note: WebP stores these as (dimension - 1)
            ImageInfo.Width = 1 + (Data[4] | (Data[5] << 8) | (Data[6] << 16));
            ImageInfo.Height = 1 + (Data[7] | (Data[8] << 8) | (Data[9] << 16));
        }
    }
    else if (ChunkType == 0x56503820) { // "VP8 " (Lossy)
        if (ChunkLen >= 10) {
            // Verify sync code 0x9d 0x01 0x2a
            if (Data[3] == 0x9d && Data[4] == 0x01 && Data[5] == 0x2a) {
                ImageInfo.Width = (Data[6] | (Data[7] << 8)) & 0x3fff;
                ImageInfo.Height = (Data[8] | (Data[9] << 8)) & 0x3fff;
            }
        }
    }
    else if (ChunkType == 0x5650384c) { // "VP8L" (Lossless)
        if (ChunkLen >= 5 && Data[0] == 0x2f) {
            // Width and Height are 14 bits each, starting at bit 8
            // Width: bits 8-21, Height: bits 22-35
            unsigned int bits = Data[1] | (Data[2] << 8) | (Data[3] << 16) | (Data[4] << 24);
            ImageInfo.Width = 1 + (bits & 0x3fff);
            ImageInfo.Height = 1 + ((bits >> 14) & 0x3fff);
        }
    }

    if (ShowTags && ImageInfo.Width) {
        printf("WebP Image: %dx%d\n", ImageInfo.Width, ImageInfo.Height);
    }
}

//--------------------------------------------------------------------------
// Read Webp file -- either entierly or just as far as the exif header.
//--------------------------------------------------------------------------
int ReadWebpSections(FILE * infile, ReadMode_t ReadMode) {
    uchar Sig[12];

    if (fread(Sig, 1, 12, infile) != 12) return FALSE;
    // Check for "RIFF" .... "WEBP"
    if (memcmp(Sig, "RIFF", 4) != 0 || memcmp(Sig + 8, "WEBP", 4) != 0){
        printf("no sig");
        return FALSE;
    }

    ResetWebpFile();

    while (!feof(infile)) {
        uchar RawHeader[8];
        if (fread(RawHeader, 1, 8, infile) != 8) break;

        unsigned int ChunkLen = Get32webp(RawHeader + 4);
        int ChunkType = (RawHeader[0] << 24) | (RawHeader[1] << 16) | (RawHeader[2] << 8) | RawHeader[3];

        // WebP chunks are padded to even bytes
        unsigned int ReadLen = (ChunkLen + 1) & ~1;

        uchar * Data = (uchar *)malloc(ReadLen);
        if (fread(Data, 1, ReadLen, infile) != ReadLen) {
            free(Data);
            break;
        }

        if (WebpSectionsRead >= WebpSectionsAllocated) {
            WebpSectionsAllocated += 10;
            WebpSections = (ImgSect_t *)realloc(WebpSections, sizeof(ImgSect_t) * WebpSectionsAllocated);
        }

        WebpSections[WebpSectionsRead].Data = Data;
        WebpSections[WebpSectionsRead].Type = ChunkType;
        WebpSections[WebpSectionsRead].Size = ChunkLen;
        WebpSectionsRead++;

        // Process geometry if we haven't found it yet
        if (ImageInfo.Width == 0) {
            ProcessWebpGeometry(ChunkType, Data, ChunkLen);
        }

        if (ChunkType == 0x45584946) { // "EXIF"
            process_EXIF(Data, ChunkLen);
        }

        if (ChunkType == 0x434f4d4d) { // "COMM"
            // Pass the raw data and size to the thunked comment processor
            ProcessImgComment(Data, ChunkLen);
        }
    }
    return TRUE;
}
//--------------------------------------------------------------------------
// Find the Exif section in the WebP sections list.
// WebP uses chunk type "EXIF" (0x45584946)
//--------------------------------------------------------------------------
ImgSect_t * GetWebpExifSection(void)
{
    int i;
    for (i = 0; i < WebpSectionsRead; i++) {
        if (WebpSections[i].Type == 0x45584946) { // "EXIF"
            printf("found exif\n");
            return &WebpSections[i];
        }
    }
    printf("exif not found\n");
    return NULL;
}

//--------------------------------------------------------------------------
// Remove a WebP section by its type (FourCC)
//--------------------------------------------------------------------------
int RemoveWebpSectionByType(int SectionType)
{
    int i, j;
    int modified = FALSE;
    for (i = 0; i < WebpSectionsRead; i++) {
        if (WebpSections[i].Type == SectionType) {
            free(WebpSections[i].Data);
            // Shift remaining sections down to fill the gap
            for (j = i; j < WebpSectionsRead - 1; j++) {
                WebpSections[j] = WebpSections[j + 1];
            }
            WebpSectionsRead--;
            i--; // Check the new item shifted into this index
            modified = TRUE;
        }
    }
    return modified;
}

//--------------------------------------------------------------------------
// Create a new WebP section, near the beginning, so if we only read
// part of the file, we get the exif header
//--------------------------------------------------------------------------
ImgSect_t * CreateWebpSection(int SectionType, unsigned char * Data, int Size)
{
    int a;
    int NewIndex = 0;

    // WebP ordering: Metadata (EXIF, XMP) should appear after the header (VP8X)
    // but MUST appear before the actual image data (VP8 or VP8L).
    for (NewIndex=0; NewIndex < WebpSectionsRead; NewIndex++){
        if (WebpSections[NewIndex].Type == 0x56503820) break; // 'VP8 '
        if (WebpSections[NewIndex].Type == 0x5650384c) break; // 'VP8L'
    }

    if (WebpSectionsRead >= WebpSectionsAllocated){
        WebpSectionsAllocated += 10;
        WebpSections = (ImgSect_t *)realloc(WebpSections, sizeof(ImgSect_t)*WebpSectionsAllocated);
        if (WebpSections == NULL) ErrFatal("could not allocate data");
    }

    // Shift to make room
    for (a=WebpSectionsRead; a>NewIndex; a--){
        WebpSections[a] = WebpSections[a-1];
    }
    WebpSectionsRead += 1;

    WebpSections[NewIndex].Type = SectionType;
    WebpSections[NewIndex].Size = Size;
    WebpSections[NewIndex].Data = Data;

    return &WebpSections[NewIndex];
}

//--------------------------------------------------------------------------
// Create a minimal Exif header for WebP
//--------------------------------------------------------------------------
void CreateMinimalWebpExif(void)
{
    unsigned char ExifData[256];
    unsigned int ExifLen;

    // Create the minimal Exif header using existing exif.c logic.
    ExifLen = CreateMinimalExif(ExifData);

    // WebP EXIF chunks contain the raw TIFF data. 
    unsigned char * WebpExifData = malloc(ExifLen);
    if (WebpExifData == NULL) ErrFatal("Out of memory");
    memcpy(WebpExifData, ExifData, ExifLen);

    RemoveWebpSectionByType(0x45584946); // "EXIF" section

    // Insert into sections array using our ordering logic.
    CreateWebpSection(0x45584946, WebpExifData, ExifLen);
}


//--------------------------------------------------------------------------
// Set or replace the comment section (COMM) for WebP
//--------------------------------------------------------------------------
void SetWebpCommentTo(char * NewCommentStr)
{
    // Remove any existing 'COMM' chunk to avoid duplicates.
    RemoveWebpSectionByType(0x434f4d4d); // "COMM"

    if (NewCommentStr == NULL) {
        ImageInfo.Comments[0] = '\0';
        return;
    }

    int CommentLen = (int)strlen(NewCommentStr);
    unsigned char * Data = (unsigned char *)malloc(CommentLen);
    if (Data == NULL) ErrFatal("Out of memory");
    memcpy(Data, NewCommentStr, CommentLen);

    // Create section near the beginning for the comment
    CreateWebpSection(0x434f4d4d, Data, CommentLen);

    // Update copy im imginfo struct.
    strncpy(ImageInfo.Comments, NewCommentStr, MAX_COMMENT_SIZE);
}


//--------------------------------------------------------------------------
// Write modified file back out (after exif or comment changes)
//--------------------------------------------------------------------------
void WriteWebpFile(const char * FileName) {
    FILE * outfile = fopen(FileName, "wb");
    if (!outfile) ErrFatal("Could not open WebP for write");

    // Calculate total RIFF size
    unsigned int TotalSize = 4; // "WEBP"
    for (int i = 0; i < WebpSectionsRead; i++) {
        TotalSize += 8 + ((WebpSections[i].Size + 1) & ~1);
    }

    fwrite("RIFF", 1, 4, outfile);
    uchar SizeBuf[4];
    Put32webp(SizeBuf, TotalSize);
    fwrite(SizeBuf, 1, 4, outfile);
    fwrite("WEBP", 1, 4, outfile);

    for (int i = 0; i < WebpSectionsRead; i++) {
        uchar Header[8];
        Header[0] = (uchar)(WebpSections[i].Type >> 24);
        Header[1] = (uchar)(WebpSections[i].Type >> 16);
        Header[2] = (uchar)(WebpSections[i].Type >> 8);
        Header[3] = (uchar)(WebpSections[i].Type);
        Put32webp(Header + 4, WebpSections[i].Size);

        fwrite(Header, 1, 8, outfile);
        fwrite(WebpSections[i].Data, 1, WebpSections[i].Size, outfile);
        if (WebpSections[i].Size & 1) fwrite("\0", 1, 1, outfile); // Padding
    }
    fclose(outfile);
}

void DiscardWebpData(void) {
    for (int i = 0; i < WebpSectionsRead; i++) free(WebpSections[i].Data);
    WebpSectionsRead = 0;
}