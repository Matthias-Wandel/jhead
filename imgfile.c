//--------------------------------------------------------------------------
// imgfile.c - Thunking layer for jhead to handle both JPEG and PNG
//--------------------------------------------------------------------------
#include "jhead.h"

// Define image types
#define TYPE_UNKNOWN 0
#define TYPE_JPEG    1
#define TYPE_PNG     2


static int ImgTypeLoaded = TYPE_UNKNOWN;

// Helper to detect file type without advancing file pointer significantly
void DetectFileType(const char * FileName) {
    unsigned char Sig[8];
    FILE * f = fopen(FileName, "rb");
    if (!f) return TYPE_UNKNOWN;

    int read = fread(Sig, 1, 8, f);
    fclose(f);
    
    //printf("Sig: %02x %02x\n",Sig[0],Sig[1]);

    if (read >= 2 && Sig[0] == 0xff && Sig[1] == 0xd8){
        ImgTypeLoaded = TYPE_JPEG;
    }
    
    if (read >= 8 && memcmp(Sig, "\x89PNG\r\n\x1a\n", 8) == 0){
        ImgTypeLoaded = TYPE_PNG;
    }
}

void DiscardImgData(void)
{
    if (ImgTypeLoaded == TYPE_JPEG) DiscardJpegData();
    if (ImgTypeLoaded == TYPE_PNG) DiscardPngData();
}

//--------------------------------------------------------------------------
// Thunked Read Function
//--------------------------------------------------------------------------
int ReadImgFile(const char * FileName, ReadMode_t ReadMode) {
    DetectFileType(FileName);
    
    if (ImgTypeLoaded == TYPE_JPEG) {
        return ReadJpegFile(FileName, ReadMode);
    } else if (ImgTypeLoaded == TYPE_PNG) {
        return ReadPngFile(FileName, ReadMode);
    }
    
    fprintf(stderr, "Not JPEG or PNG: %s\n", FileName);
    return FALSE;
}

//--------------------------------------------------------------------------
// Thunked Write Function
//--------------------------------------------------------------------------
void WriteImgFile(const char * FileName)
{
    if (ImgTypeLoaded == TYPE_JPEG) WriteJpegFile(FileName);
    if (ImgTypeLoaded == TYPE_PNG) WritePngFile(FileName);
}

void DiscardAllButExif(){
    if (ImgTypeLoaded == TYPE_JPEG) DiscardAllJpegButExif();
    if (ImgTypeLoaded == TYPE_PNG) DiscardAllPngButExif();
}

int ReplaceImgThumbnail(const char * ThumbFileName)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return ReplaceJpegThumbnail(ThumbFileName);
    }else{
        ErrFatal("Error, not implemented 1\n");
        return FALSE;
    }
}

int RemoveUnknownImgSections(void)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return RemoveUnknownJpegSections();
    }else{
        ErrFatal("Error, not implemented 2\n");
        return FALSE;
    }
}

Section_t * FindImgExifSection()
{
    if (ImgTypeLoaded == TYPE_JPEG) return FindJpegSection(M_EXIF);
    //if (ImgTypeLoaded == TYPE_PNG) return FindPngExifSection();
    ErrFatal("Error, not implemented 3\n");
    return FALSE;
}

Section_t * FindImgSection(int SectionType)
{
    // Section type values differ between Jpeg and Png, so calls to this
    // function needs to be replaced with calls referring to specific section types.
    if (ImgTypeLoaded == TYPE_JPEG){
        return FindJpegSection(SectionType);
    }else{
        ErrFatal("Error, not implemented 4\n");
        return NULL;
    }
}
Section_t * CreateImgSection(int SectionType, unsigned char * Data, int Size)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return CreateImgSection(SectionType, Data, Size);
    }else{
        ErrFatal("Error, not implemented 5\n");
        return NULL;
    }
}

int SaveImgThumbnail(char * ThumbFileName)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return SaveJpegThumbnail(ThumbFileName);
    }else{
        ErrFatal("Error, not implemented 6\n");
        return FALSE;
    }
}

//--------------------------------------------------------------------------
// Initialisation.
//--------------------------------------------------------------------------
void ResetImgfile(void)
{
    ResetJpegFile();
    ResetPngFile();
}
