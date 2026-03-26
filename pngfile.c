//--------------------------------------------------------------------------
// pngfile.c - Drop-in replacement module for testing PNG support in jhead.
// Mirrored from the structure of jpgfile.c
//--------------------------------------------------------------------------
#include "jhead.h"

static ImgSect_t * PngSections = NULL;
static int PngSectionsAllocated;
static int PngSectionsRead;
static int HaveAllOfPng;

// CRC-32 for PNG writing
static unsigned int CrcTable[256];
static int CrcTableGenerated = 0;

static void GenerateCrcTable(void) {
    unsigned int c;
    for (int n = 0; n < 256; n++) {
        c = (unsigned int)n;
        for (int k = 0; k < 8; k++) {
            if (c & 1) c = 0xedb88320L ^ (c >> 1);
            else c = c >> 1;
        }
        CrcTable[n] = c;
    }
    CrcTableGenerated = 1;
}

//--------------------------------------------------------------------------
// Update a running CRC calculation with more data.
// Note: This does NOT perform the final XOR; it allows for chaining.
//--------------------------------------------------------------------------
static unsigned int UpdateCrc(unsigned int c, unsigned char *buf, int len)
{
    if (!CrcTableGenerated) GenerateCrcTable();
    for (int n = 0; n < len; n++) {
        c = CrcTable[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

static unsigned int Get32png(const uchar * Data) {
    return (Data[0] << 24) | (Data[1] << 16) | (Data[2] << 8) | Data[3];
}

static void Put32png(uchar * Data, unsigned int Value) {
    Data[0] = (uchar)(Value >> 24);
    Data[1] = (uchar)(Value >> 16);
    Data[2] = (uchar)(Value >> 8);
    Data[3] = (uchar)Value;
}

//--------------------------------------------------------------------------
// Section Management (Identical to jpgfile.c logic)
//--------------------------------------------------------------------------
void CheckPngSectionsAllocated(void)
{
    if (PngSectionsRead >= PngSectionsAllocated){
        PngSectionsAllocated += PngSectionsAllocated/2 + 5;
        PngSections = (ImgSect_t *)realloc(PngSections, sizeof(ImgSect_t)*PngSectionsAllocated);
        if (PngSections == NULL) ErrFatal("could not allocate data");
    }
}

void ResetPngFile(void)
{
    if (PngSections == NULL){
        PngSections = (ImgSect_t *)malloc(sizeof(ImgSect_t)*5);
        PngSectionsAllocated = 5;
    }
    PngSectionsRead = 0;
    HaveAllOfPng = 0;
}

void DiscardPngData(void)
{
    int a;
    for (a=0;a<PngSectionsRead;a++) free(PngSections[a].Data);
    PngSectionsRead = 0;
    HaveAllOfPng = 0;
}

ImgSect_t * FindPngSection(int SectionType)
{
    for (int a=0; a<PngSectionsRead; a++) {
        if (PngSections[a].Type == SectionType) return &PngSections[a];
    }
    return NULL;
}

int RemovePngSectionByType(int SectionType)
{
    int a;
    int modified = FALSE;
    for (a=0; a<PngSectionsRead; a++){
        if (PngSections[a].Type == SectionType){
            free(PngSections[a].Data);
            memmove(PngSections+a, PngSections+a+1, sizeof(ImgSect_t) * (PngSectionsRead-a-1));
            PngSectionsRead -= 1;
            a -= 1;
            modified = TRUE;
        }
    }
    return modified;
}

//--------------------------------------------------------------------------
// Create a minimal Exif header for PNG
//--------------------------------------------------------------------------
void CreateMinimalPngExif(void)
{
    unsigned char ExifData[256];
    unsigned int ExifLen;

    // Create the minimal Exif header using existing exif.c logic.
    ExifLen = CreateMinimalExif(ExifData);

    // reprocess the new minimal exif header to make sure data is up to date.
    process_EXIF(ExifData, ExifLen);

    // PNG eXIf chunks start directly at the TIFF header (II* or MM*).
    // So we skip the first 6 bytes ("Exif\0\0").
    unsigned char * PngExifData = malloc(ExifLen);
    memcpy(PngExifData, ExifData, ExifLen);

    RemovePngSectionByType(0x65584966); // 'eXIf' section

    // Insert into sections array.
    CreatePngSection(0x65584966, PngExifData, ExifLen);
}

//--------------------------------------------------------------------------
// PNG Parser Core
//--------------------------------------------------------------------------
int ReadPngSections(FILE * infile, ReadMode_t ReadMode)
{
    uchar Sig[8];
    if (fread(Sig, 1, 8, infile) != 8 || memcmp(Sig, "\x89PNG\r\n\x1a\n", 8) != 0) return FALSE;

    ResetPngFile();
    int HaveCom = FALSE;

    for (;;) {
        uchar LenRaw[4], TypeRaw[4], CrcRaw[4];
        if (fread(LenRaw, 1, 4, infile) != 4 || fread(TypeRaw, 1, 4, infile) != 4) break;

        unsigned int ChunkLen = Get32png(LenRaw);
        if (ChunkLen > 1<<31){
            ErrFatal("bad PNG chunk length");
            return FALSE;
        }
        int ChunkTypeInt = (TypeRaw[0] << 24) | (TypeRaw[1] << 16) | (TypeRaw[2] << 8) | TypeRaw[3];

        if (ShowTags){
            printf("PNG Chunk type '%.4s' 0x%08x length %d\n", TypeRaw, ChunkTypeInt, ChunkLen);
        }

        CheckPngSectionsAllocated();
        uchar * Data = (uchar *)malloc(ChunkLen + 20);
        if (Data == NULL) ErrFatal("Out of memory");

        if (fread(Data, 1, ChunkLen, infile) != ChunkLen) {
            ErrFatal("Premature end of file");
            free(Data);
            break;
        }

        // Read the 4-byte CRC from the file
        if (fread(CrcRaw, 1, 4, infile) != 4) {
            free(Data);
            break;
        }
        unsigned int StoredCrc = Get32png(CrcRaw); // PNG integers are Big-Endian

        // Calculate the CRC over Type + Data
        unsigned int c = 0xffffffffL; // Start value for CRC-32
        c = UpdateCrc(c, TypeRaw, 4); // Include the 4-byte Type
        c = UpdateCrc(c, Data, ChunkLen); // Include the Data payload
        unsigned int ComputedCrc = c ^ 0xffffffffL; // Final XOR

        if (StoredCrc != ComputedCrc){
            ErrFatal("PNG CRC corrupt");
        }
        // ------------------------------

        PngSections[PngSectionsRead].Type = ChunkTypeInt;
        PngSections[PngSectionsRead].Size = ChunkLen;
        PngSections[PngSectionsRead].Data = Data;
        PngSectionsRead++;

        if (TypeRaw[3] == 'X'){
            // X means unsafe to copy, so discard it now.
            PngSectionsRead--;
            free(PngSections[PngSectionsRead].Data);
        }

        if (memcmp(TypeRaw, "IHDR", 4) == 0) {
            ImageInfo.Width = Get32png(Data);
            ImageInfo.Height = Get32png(Data + 4);
            ImageInfo.IsColor = TRUE;
            int color_type = Data[9];
            switch (color_type) {
                case 0: // Grayscale
                case 4:
                    ImageInfo.PngNumColors = 1 << Data[8];
                    ImageInfo.IsColor = FALSE;
                    break;
                case 2: // Truecolor (RGB)
                case 6: // Truecolor + Alpha
                    // For truecolor, "NumColors" is technically 16 million+,
                    ImageInfo.PngNumColors = 1<<24; // 24 bit color
                    break;
                case 3: // Indexed
                    // We get the actual count from the PLTE chunk size
                    break;
            }
        } else if (memcmp(TypeRaw, "PLTE", 4) == 0){
            ImageInfo.PngNumColors = ChunkLen / 3;
        } else if (memcmp(TypeRaw, "eXIf", 4) == 0 && (ReadMode & READ_METADATA)) {
            process_EXIF(Data, ChunkLen);
        } else if (memcmp(TypeRaw, "tIME", 4) == 0){
            if (ShowTags){
                printf("    Last modification Time:  %04d:%02d:%02d  %02d:%02d:%02d\n",
                     (Data[0]<<8)+Data[1],Data[2],Data[3],Data[4],Data[5],Data[6]);
            }
        } else if (memcmp(TypeRaw, "tEXt", 4) == 0){
            if (ShowTags){
                printf("    Text:");
                for (unsigned a=0;a<ChunkLen;a++){
                    if (Data[a] == '\0'){
                        putchar(' ');
                    }else{
                        putchar(Data[a]);
                    }
                }
                printf("\n");
            }
            if (ChunkLen > 8 && memcmp("Comment",Data,8) ==0 && HaveCom == FALSE){
                ProcessImgComment(Data+8, ChunkLen-8);
                HaveCom = TRUE;
            }
        } else if (memcmp(TypeRaw, "IDAT", 4) == 0){
            if (!(ReadMode & READ_IMAGE)){
                // Only want the metadata (not going to write the image)
                // Assume all metadata preceeds the actual data sections.
                if (ShowTags) printf("Skip rest of file (want metadata only)\n");
                return TRUE;
            }
        } else if (memcmp(TypeRaw, "IEND", 4) == 0) {
            HaveAllOfPng = 1;
            break;
        }
    }
    return TRUE;
}

int ReadPngFile(FILE * infile, ReadMode_t ReadMode)
{
    int ret = ReadPngSections(infile, ReadMode);
    return ret;
}

//--------------------------------------------------------------------------
// Adds a PNG section, after initial seciton and before the image data.
//--------------------------------------------------------------------------
ImgSect_t * CreatePngSection(int SectionType, unsigned char * Data, int Size)
{
    CheckPngSectionsAllocated();

    int NewIndex = 1; // First (0'th) section needs to stay at 0'th position.

    // Make room for the new section
    for (int a = PngSectionsRead; a > NewIndex; a--) {
        PngSections[a] = PngSections[a-1];
    }
    PngSectionsRead++;

    PngSections[NewIndex].Type = SectionType;
    PngSections[NewIndex].Size = Size;
    PngSections[NewIndex].Data = Data;

    return &PngSections[NewIndex];

}

//--------------------------------------------------------------------------
// Remove a PNG section by its pointer.
//--------------------------------------------------------------------------
static void RemovePngSection(ImgSect_t * RemoveSection)
{
    int a;
    int Index = (int)(RemoveSection - PngSections);

    if (Index < 0 || Index >= PngSectionsRead) {
        ErrFatal("Invalid section pointer for removal");
    }

    free(PngSections[Index].Data);

    // Shift the remaining sections down to fill the gap
    for (a = Index; a < PngSectionsRead-1; a++) {
        PngSections[a] = PngSections[a+1];
    }
    PngSectionsRead -= 1;
}

//--------------------------------------------------------------------------
// Set PNG comment (tEXt chunk with "Description" keyword)
//--------------------------------------------------------------------------
void SetPngCommentTo(char * NewCommentStr)
{
    ImgSect_t * CommentSec = NULL;
    int a;

    // Look for an existing tEXt chunk that starts with "Description"
    for (a=0; a<PngSectionsRead; a++) {
        if (PngSections[a].Type == 0x74455874) { // 'tEXt'
            if (strncmp((char*)PngSections[a].Data, "Comment", 8) == 0) {
                CommentSec = &PngSections[a];
                break;
            }
        }
    }

    if (NewCommentStr == NULL) {
        // Remove the section if it exists
        if (CommentSec) {
            RemovePngSection(CommentSec); // Use existing removal logic
        }
        return;
    }

    // Prepare the data: "Comment\0CommentText"
    int CommentLen = strlen(NewCommentStr);
    int KeyLen = 8; // "Comment" + null terminator
    int TotalSize = KeyLen + CommentLen;

    if (CommentSec) {
        free(CommentSec->Data);
    } else {
        // Create a new 'tEXt' section.
        // In PNG, metadata chunks should come after IHDR.
        CommentSec = CreatePngSection(0x74455874, NULL, TotalSize);
    }

    CommentSec->Size = TotalSize;
    CommentSec->Data = malloc(TotalSize);
    memcpy(CommentSec->Data, "Comment", 8);
    memcpy(CommentSec->Data + 8, NewCommentStr, CommentLen);
}

//--------------------------------------------------------------------------
// Write PNG File, re-generating the CRCs
//--------------------------------------------------------------------------
void WritePngFile(const char * FileName)
{
    if (!HaveAllOfPng) ErrFatal("Can't write back - didn't read all PNG");
    FILE * outfile = fopen(FileName, "wb");
    if (!outfile) ErrFatal("Could not open for write");

    fwrite("\x89PNG\r\n\x1a\n", 1, 8, outfile);

    for (int a = 0; a < PngSectionsRead; a++) {
        uchar Header[8], CrcRaw[4];

        unsigned int c = 0xffffffffL; // Start CRC value
        
        // Write Length and Type
        Put32png(Header, PngSections[a].Size);
        Put32png(Header + 4, PngSections[a].Type);
        fwrite(Header, 1, 8, outfile);

        c = UpdateCrc(c, Header+4, 4);// Feed Type into CRC

        // Write Data
        fwrite(PngSections[a].Data, 1, PngSections[a].Size, outfile);

        // Feed data into CRC.
        c = UpdateCrc(c, PngSections[a].Data, PngSections[a].Size); // Feed Data

        // Finalize and write CRC
        Put32png(CrcRaw, c ^ 0xffffffffL);
        fwrite(CrcRaw, 1, 4, outfile);
    }
    fclose(outfile);
}


//--------------------------------------------------------------------------
//
//--------------------------------------------------------------------------
void DiscardAllPngButExif(void)
{
    // Simplified version: keep only IHDR, eXIf, and IEND
    for (int a=0; a<PngSectionsRead; a++) {
        if (PngSections[a].Type != 0x49484452 && PngSections[a].Type != 0x65584966 && PngSections[a].Type != 0x49454E44) {
             // Logic to remove... for testing just skip
        }
    }
}
