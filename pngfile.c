//--------------------------------------------------------------------------
// pngfile.c - Drop-in replacement module for testing PNG support in jhead.
// Mirrored from the structure of jpgfile.c
//--------------------------------------------------------------------------
#include "jhead.h"

static Section_t * Sections = NULL;
static int SectionsAllocated;
static int SectionsRead;
static int HaveAll;

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

static unsigned int CalculateCrc(unsigned char *buf, int len) {
    unsigned int c = 0xffffffffL;
    if (!CrcTableGenerated) GenerateCrcTable();
    for (int n = 0; n < len; n++) {
        c = CrcTable[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c ^ 0xffffffffL;
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
void CheckSectionsAllocated(void)
{
    if (SectionsRead >= SectionsAllocated){
        SectionsAllocated += SectionsAllocated/2 + 5;
        Sections = (Section_t *)realloc(Sections, sizeof(Section_t)*SectionsAllocated);
        if (Sections == NULL) ErrFatal("could not allocate data");
    }
}

void ResetPngFile(void)
{
    if (Sections == NULL){
        Sections = (Section_t *)malloc(sizeof(Section_t)*5);
        SectionsAllocated = 5;
    }
    SectionsRead = 0;
    HaveAll = 0;
}

void DiscardPngData(void)
{
    int a;
    for (a=0;a<SectionsRead;a++) free(Sections[a].Data);
    memset(&ImageInfo, 0, sizeof(ImageInfo));
    SectionsRead = 0;
    HaveAll = 0;
}

Section_t * FindSection(int SectionType)
{
    for (int a=0; a<SectionsRead; a++) {
        if (Sections[a].Type == SectionType) return &Sections[a];
    }
    return NULL;
}

int RemovePngSectionType(int SectionType)
{
    printf("Remove section type %d\n",SectionType);
    
    int a, retval = FALSE;
    for (a=0; a<SectionsRead; a++){
        printf("Secition %d\n",Sections[a].Type);
        if (Sections[a].Type == SectionType){
            printf("found to remove");
            free(Sections[a].Data);
            memmove(Sections+a, Sections+a+1, sizeof(Section_t) * (SectionsRead-a-1));
            SectionsRead -= 1;
            a -= 1;
            retval = TRUE;
        }
    }
    return retval;
}

//--------------------------------------------------------------------------
// PNG Parser Core
//--------------------------------------------------------------------------
int ReadPngSections(FILE * infile, ReadMode_t ReadMode)
{
    uchar Sig[8];
    if (fread(Sig, 1, 8, infile) != 8 || memcmp(Sig, "\x89PNG\r\n\x1a\n", 8) != 0) return FALSE;

    ResetPngFile();

    for (;;) {
        uchar LenRaw[4], TypeRaw[4];
        if (fread(LenRaw, 1, 4, infile) != 4 || fread(TypeRaw, 1, 4, infile) != 4) break;

        unsigned int ChunkLen = Get32png(LenRaw);
        int ChunkTypeInt = (TypeRaw[0] << 24) | (TypeRaw[1] << 16) | (TypeRaw[2] << 8) | TypeRaw[3];

        CheckSectionsAllocated();
        uchar * Data = (uchar *)malloc(ChunkLen + 20);
        fread(Data, 1, ChunkLen, infile);
        fseek(infile, 4, SEEK_CUR); // Skip CRC

        Sections[SectionsRead].Type = ChunkTypeInt;
        Sections[SectionsRead].Size = ChunkLen;
        Sections[SectionsRead].Data = Data;
        SectionsRead++;

        if (memcmp(TypeRaw, "IHDR", 4) == 0) {
            ImageInfo.Width = Get32png(Data);
            ImageInfo.Height = Get32png(Data + 4);
        } else if (memcmp(TypeRaw, "eXIf", 4) == 0 && (ReadMode & READ_METADATA)) {
            // PNG eXIf chunk is raw TIFF. Prepend "Exif\0\0" to reuse process_EXIF
            uchar * FakeExif = (uchar *)malloc(ChunkLen + 6);
            memcpy(FakeExif, "Exif\0\0", 6);
            memcpy(FakeExif + 6, Data, ChunkLen);
            process_EXIF(FakeExif - 2, ChunkLen + 8); 
            free(FakeExif);
        } else if (memcmp(TypeRaw, "IEND", 4) == 0) {
            HaveAll = 1;
            break;
        }
    }
    return TRUE;
}

// --- Thunking function names to satisfy linker for testing ---

int ReadPngFile(const char * FileName, ReadMode_t ReadMode)
{
    FILE * infile = fopen(FileName, "rb");
    if (infile == NULL) return FALSE;
    int ret = ReadPngSections(infile, ReadMode);
    fclose(infile);
    return ret;
}

void WritePngFile(const char * FileName)
{
    FILE * outfile = fopen(FileName, "wb");
    if (!outfile) ErrFatal("Could not open for write");

    fwrite("\x89PNG\r\n\x1a\n", 1, 8, outfile);
    for (int a = 0; a < SectionsRead; a++) {
        uchar Header[8], CrcIn[4], CrcRaw[4];
        Put32png(Header, Sections[a].Size);
        Put32png(Header + 4, Sections[a].Type);
        
        fwrite(Header, 1, 8, outfile);
        fwrite(Sections[a].Data, 1, Sections[a].Size, outfile);

        // CRC over Type + Data
        Put32png(CrcIn, Sections[a].Type);
        unsigned int c = 0xffffffffL;
        if (!CrcTableGenerated) GenerateCrcTable();
        for(int i=0; i<4; i++){
            c = CrcTable[(c ^ CrcIn[i]) & 0xff] ^ (c >> 8);
        }
        for(unsigned i=0; i<Sections[a].Size; i++){
            c = CrcTable[(c ^ Sections[a].Data[i]) & 0xff] ^ (c >> 8);
        }
        
        Put32png(CrcRaw, c ^ 0xffffffffL);
        fwrite(CrcRaw, 1, 4, outfile);
    }
    fclose(outfile);
}


void DiscardAllPngButExif(void)
{ 
    // Simplified version: keep only IHDR, eXIf, and IEND
    for (int a=0; a<SectionsRead; a++) {
        if (Sections[a].Type != 0x49484452 && Sections[a].Type != 0x65584966 && Sections[a].Type != 0x49454E44) {
             // Logic to remove... for testing just skip
        }
    }
}

Section_t * CreatePngSection(int SectionType, unsigned char * Data, int Size) {
    CheckSectionsAllocated();
    Sections[SectionsRead].Type = SectionType;
    Sections[SectionsRead].Size = Size;
    Sections[SectionsRead].Data = Data;
    return &Sections[SectionsRead++];
}