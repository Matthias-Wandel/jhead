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

        if (ChunkType == 0x45584946) { // "EXIF"
            process_EXIF(Data, ChunkLen);
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
// Write modified file back out (after exif changes)
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