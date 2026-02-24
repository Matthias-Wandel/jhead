//--------------------------------------------------------------------------
// imgfile.c - Thunking layer for jhead to handle both JPEG and PNG
//--------------------------------------------------------------------------
#include "jhead.h"

// Define image types
#define TYPE_UNKNOWN 0
#define TYPE_JPEG    1
#define TYPE_PNG     2
static int ImgTypeLoaded = TYPE_UNKNOWN;

// Storage for simplified info extracted from file.
ImageInfo_t ImageInfo;

void DiscardImgData(void)
{
    if (ImgTypeLoaded == TYPE_JPEG) DiscardJpegData();
    if (ImgTypeLoaded == TYPE_PNG) DiscardPngData();
}

//--------------------------------------------------------------------------
// Process contents of COM marker or PNG 'tEXt' section
// We want to print out the marker contents as legible text;
// we must guard against random junk and varying newline representations.
// Not taking utf8 into consideration.
//--------------------------------------------------------------------------
void ProcessImgComment (const uchar * Data, int length)
{
    int ch;
    char Comment[MAX_COMMENT_SIZE+1];
    int nch;
    int a;

    nch = 0;

    if (length > MAX_COMMENT_SIZE) length = MAX_COMMENT_SIZE; // Truncate if it won't fit in our structure.

    for (a=0;a<length;a++){
        ch = Data[a];

        if (ch == '\r' && a < length-1 && Data[a+1] == '\n') continue; // Remove cr followed by lf.

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
// Thunked Read Function
//--------------------------------------------------------------------------
int ReadImgFile(const char * FileName, ReadMode_t ReadMode) {
    unsigned char Sig[8];
    FILE * f = fopen(FileName, "rb");

    if (!f) {
        fprintf(stderr, "can't open '%s'\n", FileName);
        return FALSE;
    }

    int read = fread(Sig, 1, 8, f);
    fseek(f,0,SEEK_SET);

    //printf("Sig: %02x %02x\n",Sig[0],Sig[1]);

    if (read >= 2 && Sig[0] == 0xff && Sig[1] == 0xd8){
        ImgTypeLoaded = TYPE_JPEG;
    }

    if (read >= 8 && memcmp(Sig, "\x89PNG\r\n\x1a\n", 8) == 0){
        ImgTypeLoaded = TYPE_PNG;
    }


    if (ImgTypeLoaded == TYPE_JPEG) {
        return ReadJpegFile(f, ReadMode);
    } else if (ImgTypeLoaded == TYPE_PNG) {
        return ReadPngFile(f, ReadMode);
    }

    fclose(f);
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
        ErrFatal("not implemented 1\n");
        return FALSE;
    }
}

int RemoveUnknownImgSections(void)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return RemoveUnknownJpegSections();
    }else{
        ErrFatal("not implemented 2\n");
        return FALSE;
    }
}

Section_t * FindImgExifSection()
{
    if (ImgTypeLoaded == TYPE_JPEG) return FindJpegSection(M_EXIF);
    //if (ImgTypeLoaded == TYPE_PNG) return FindPngExifSection();
    ErrFatal("not implemented 3\n");
    return FALSE;
}

void SetImgCommentTo(char * NewComment)
{
    if (ImgTypeLoaded == TYPE_JPEG) {
        SetJpegCommentTo(NewComment);
    } else if (ImgTypeLoaded == TYPE_PNG) {
        SetPngCommentTo(NewComment);
    } else {
        ErrFatal("No image loaded to set comment");
    }
}

Section_t * FindImgSection(int SectionType)
{
    // Section type values differ between Jpeg and Png, so calls to this
    // function needs to be replaced with calls referring to specific section types.
    if (ImgTypeLoaded == TYPE_JPEG){
        return FindJpegSection(SectionType);
    }else{
        if (SectionType == M_IPTC) return NULL; // Not used in PNG files

        printf("want %d\n",SectionType);
        ErrFatal("not implemented 4\n");
        return NULL;
    }
}

Section_t * CreateImgSection(int SectionType, unsigned char * Data, int Size)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return CreateJpegSection(SectionType, Data, Size);
    }else{
        ErrFatal("not implemented 5\n");
        return NULL;
    }
}

int SaveImgThumbnail(char * ThumbFileName)
{
    if (ImgTypeLoaded == TYPE_JPEG){
        return SaveJpegThumbnail(ThumbFileName);
    }else{
        ErrFatal("not implemented 6\n");
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
