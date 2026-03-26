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
        if (ShowTags){
            printf("Chunk type %x '",ChunkType);
            for (int a=0;a<4;a++) putchar((char)(ChunkType>>((3-a)*8)));
            printf("' length %d\n", ChunkLen);
        }

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


        if (ChunkType == 0x56503858) { // "VP8X
            if (ShowTags) printf("VP8X Metadata present flags = 0x%X\n",Data[0]);
        }

        if (ChunkType == 0x584d5020) { // "XMP "
            if (ShowTags){
                ShowXmp(Data, ChunkLen);
            }
        }

        if (ChunkType == 0x45584946) { // "EXIF"
            process_EXIF(Data, ChunkLen);
        }

        if (ChunkType == 0x434f4d4d) { // "COMM"
            // Pass the raw data and size to the thunked comment processor
            ProcessImgComment(Data, ChunkLen);
        }

        // Process geometry if we haven't found it yet
        if (ImageInfo.Width == 0) {
            ProcessWebpGeometry(ChunkType, Data, ChunkLen);
        }

    }
    return TRUE;
}
//--------------------------------------------------------------------------
// Find the Exif section in the WebP sections list.
// WebP uses chunk type "EXIF" (0x45584946)
//--------------------------------------------------------------------------
ImgSect_t * GetWebpSection(int Type)
{
    int i;
    for (i = 0; i < WebpSectionsRead; i++) {
        if (WebpSections[i].Type == Type){// 0x45584946) { // "EXIF"
            return &WebpSections[i];
        }
    }
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
// Ensure VP8X header exists if needed and is synchronized with chunks.
//--------------------------------------------------------------------------
static void EnsureVP8XHeaderExists(void)
{
    // Make a VP8X section at the start if if we don't have one, as one of
    // these is needed if the webp file has more than just one junk.
    if (WebpSections[0].Type != 0x56503858) {
        uchar * Data = (uchar *)calloc(1, 10);

        // Shift array to make room at index 0
        if (WebpSectionsRead >= WebpSectionsAllocated) {
            WebpSectionsAllocated += 10;
            WebpSections = (ImgSect_t *)realloc(WebpSections, sizeof(ImgSect_t) * WebpSectionsAllocated);
        }
        memmove(WebpSections + 1, WebpSections, sizeof(ImgSect_t) * WebpSectionsRead);
        WebpSectionsRead++;

        WebpSections[0].Type = 0x56503858; // 'VP8X'
        WebpSections[0].Size = 10;
        WebpSections[0].Data = Data;

        // Set dimensions (Width-1 and Height-1)
        unsigned int w = ImageInfo.Width - 1;
        unsigned int h = ImageInfo.Height - 1;
        Data[4] = (uchar)(w & 0xFF);
        Data[5] = (uchar)((w >> 8) & 0xFF);
        Data[6] = (uchar)((w >> 16) & 0xFF);
        Data[7] = (uchar)(h & 0xFF);
        Data[8] = (uchar)((h >> 8) & 0xFF);
        Data[9] = (uchar)((h >> 16) & 0xFF);

    }
}

//--------------------------------------------------------------------------
// Create a new WebP section, near the beginning, so if we only read
// part of the file, we get the exif header
//--------------------------------------------------------------------------
ImgSect_t * CreateWebpSection(int SectionType, unsigned char * Data, int Size)
{
    int a;
    EnsureVP8XHeaderExists();

    if (WebpSectionsRead >= WebpSectionsAllocated){
        WebpSectionsAllocated += 10;
        WebpSections = (ImgSect_t *)realloc(WebpSections, sizeof(ImgSect_t)*WebpSectionsAllocated);
        if (WebpSections == NULL) ErrFatal("could not allocate data");
    }

    // Shift to make room
    for (a=WebpSectionsRead; a>1; a--){
        WebpSections[a] = WebpSections[a-1];
    }
    WebpSectionsRead += 1;

    WebpSections[1].Type = SectionType;
    WebpSections[1].Size = Size;
    WebpSections[1].Data = Data;

    return &WebpSections[1];
}

//--------------------------------------------------------------------------
// Adjust exif section length up or down
//--------------------------------------------------------------------------
uchar * ChangeWebpExifSectionLength(int NewLength)
{
    ImgSect_t * ExifSection;
printf("Ajust length to %d\n",NewLength);
    if (NewLength == 0){
        RemoveWebpSectionByType(0x45584946); // EXIF
        return NULL;
    }

    ExifSection = GetWebpSection(0x45584946); // EXIF
    if (!ExifSection) ErrFatal("no exif section");

    NewLength = (NewLength+1) & 0xfffe; // Round up to even length.

    if (NewLength > ExifSection->Size){
        // If bigger than before, re-allocate it.  Otherwise, just set length shorter.
printf("realloc\n");
        ExifSection->Data = realloc(ExifSection->Data, NewLength);
        ExifSection->Size = NewLength;
    }
    ExifSection->Size = NewLength;
    return ExifSection->Data;
}

//--------------------------------------------------------------------------
// Create a minimal Exif header for WebP
//--------------------------------------------------------------------------
void CreateMinimalWebpExif(void)
{
    unsigned char ExifData[256];
    memset(ExifData, 0, sizeof(ExifData));
    unsigned int ExifLen;

    // Create the minimal Exif header
    ExifLen = CreateMinimalExif(ExifData);
    ExifLen = (ExifLen+1) & 0xfffe; // Round to even length

    // reprocess the new minimal exif header to make sure data is up to date.
    process_EXIF(ExifData, ExifLen);

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
    CommentLen = (CommentLen+1) & 0xfffe; // Round up to even length

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

    // If we only have VP8X and ONE image chunk and no flags are set,
    // delete VP8X to make it a "Simple" WebP.
    if (WebpSectionsRead == 2 && WebpSections[0].Type == 0x56503858) {
        // Delete VP8X.
        free(WebpSections[0].Data);
        memmove(WebpSections, WebpSections + 1, sizeof(ImgSect_t) * (WebpSectionsRead - 1));
        WebpSectionsRead--;
    }else if (WebpSections[0].Type == 0x56503858) {
        uchar Flags = 0; // Clear and rebuild

        // Update flags in VP8 header for which metadata is present.
        if (GetWebpSection(0x414e494d)) Flags |= 0x02; // Animation bit
        if (GetWebpSection(0x584d5020)) Flags |= 0x04; // XMP bit
        if (GetWebpSection(0x45584946)) Flags |= 0x08; // Exif bit
        if (GetWebpSection(0x414c5048)) Flags |= 0x10; // Alpha bit
        if (GetWebpSection(0x49434350)) Flags |= 0x20; // ICC Profile bit

        WebpSections[0].Data[0] = Flags;
    }

    FILE * outfile = fopen(FileName, "wb");
    if (!outfile) ErrFatal("Could not open WebP for write");

    // Calculate total RIFF size
    unsigned int TotalSize = 4; // "WEBP"
    for (int i = 0; i < WebpSectionsRead; i++) {
        TotalSize += 8 + ((WebpSections[i].Size + 1) & ~1);
    }

    // Write header that goes before any chunks inside the webp structure
    fwrite("RIFF", 1, 4, outfile);
    uchar SizeBuf[4];
    Put32webp(SizeBuf, TotalSize);
    fwrite(SizeBuf, 1, 4, outfile);
    fwrite("WEBP", 1, 4, outfile);

    // Write the webp chunks
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