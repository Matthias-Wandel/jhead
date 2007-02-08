//--------------------------------------------------------------------------
//  Process IPTC data.
//--------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE 1
#include <stdio.h>
#include <stdlib.h>

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
#include "iptc.h"

// Supported IPTC entry types
#define IPTC_SUPLEMENTAL_CATEGORIES 0x14
#define IPTC_KEYWORDS               0x19
#define IPTC_CAPTION                0x78
#define IPTC_AUTHOR                 0x7A
#define IPTC_HEADLINE               0x69
#define IPTC_SPECIAL_INSTRUCTIONS   0x28
#define IPTC_CATEGORY               0x0F
#define IPTC_BYLINE                 0x50
#define IPTC_BYLINE_TITLE           0x55
#define IPTC_CREDIT                 0x6E
#define IPTC_SOURCE                 0x73
#define IPTC_COPYRIGHT_NOTICE       0x74
#define IPTC_OBJECT_NAME            0x05
#define IPTC_CITY                   0x5A
#define IPTC_STATE                  0x5F
#define IPTC_COUNTRY                0x65
#define IPTC_TRANSMISSION_REFERENCE 0x67
#define IPTC_DATE                   0x37
#define IPTC_COPYRIGHT              0x0A
#define IPTC_COUNTRY_CODE           0x64
#define IPTC_REFERENCE_SERVICE      0x2D

static unsigned char numIptcEntries = 0;

//--------------------------------------------------------------------------
//  Process IPTC marker. Return FALSE if unable to process marker.
//
//  IPTC block consists of:
//      - Marker:               1 byte      (0xED)
//      - Block length:         2 bytes
//      - IPTC Signature:       14 bytes    ("Photoshop 3.0\0")
//      - 8BIM Signature        4 bytes     ("8BIM")
//      - IPTC Block start      2 bytes     (0x04, 0x04)
//      - IPTC Header length    1 byte
//      - IPTC header           Header is padded to even length, counting the length byte
//      - Length                4 bytes
//      - IPTC Data which consists of a number of entries, each of which has the following format:
//              - Signature     2 bytes     (0x1C02)
//              - Entry type    1 byte      (for defined entry types, see #defines above)
//              - entry length  2 bytes
//              - entry data    'entry length' bytes
//
//--------------------------------------------------------------------------
int process_IPTC (unsigned char* Data, unsigned int itemlen)
{
    const char IptcSignature1[] = "Photoshop 3.0";
    const char IptcSignature2[] = "8BIM";
    const char IptcSignature3[] = {0x04, 0x04};

    char* pos       = Data + sizeof(short);   // position data pointer after length field
    char  headerLen = 0;
    long  length;

    // Check IPTC signatures
    if (memcmp(pos, IptcSignature1, strlen(IptcSignature1)) != 0) return FALSE;
    pos += strlen(IptcSignature1) + 1;      // move data pointer to the next field

    if (memcmp(pos, IptcSignature2, strlen(IptcSignature2)) != 0) return FALSE;
    pos += strlen(IptcSignature2);          // move data pointer to the next field

    if (memcmp(pos, IptcSignature3, sizeof(IptcSignature3)) != 0) return FALSE;
    pos += sizeof(IptcSignature3);          // move data pointer to the next field

    // IPTC section found

    // Skip header
    headerLen = *pos++;                     // get header length and move data pointer to the next field
    pos += headerLen + 1 - (headerLen % 2); // move data pointer to the next field (Header is padded to even length, counting the length byte)

    // Get length (from motorola format)
    length = Get32u(pos);
    pos += sizeof(long);                    // move data pointer to the next field

    // Now read IPTC data
    numIptcEntries = 0;
    while (pos < (Data + itemlen-5)) {
        short signature = (*pos << 8) + (*(pos+1));
        char  type = 0;
        short length = 0;
        char* description = NULL;

        pos += sizeof(short);
        if (signature != 0x1C02)
            break;

        type    = *pos++;
        length  = (*pos << 8) + (*(pos+1));
        pos    += sizeof(short);                // Skip tag length
        // Process tag here
        switch (type) {
            case IPTC_SUPLEMENTAL_CATEGORIES:  description = "SuplementalCategories"; break;
            case IPTC_KEYWORDS:                description = "Keywords"; break;
            case IPTC_CAPTION:                 description = "Caption"; break;
            case IPTC_AUTHOR:                  description = "Author"; break;
            case IPTC_HEADLINE:                description = "Headline"; break;
            case IPTC_SPECIAL_INSTRUCTIONS:    description = "Spec.Instr."; break;
            case IPTC_CATEGORY:                description = "Category"; break;
            case IPTC_BYLINE:                  description = "Byline"; break;
            case IPTC_BYLINE_TITLE:            description = "BylineTitle"; break;
            case IPTC_CREDIT:                  description = "Credit"; break;
            case IPTC_SOURCE:                  description = "Source"; break;
            case IPTC_COPYRIGHT_NOTICE:        description = "(C)Notice"; break;
            case IPTC_OBJECT_NAME:             description = "ObjectName"; break;
            case IPTC_CITY:                    description = "City"; break;
            case IPTC_STATE:                   description = "State"; break;
            case IPTC_COUNTRY:                 description = "Country"; break;
            case IPTC_TRANSMISSION_REFERENCE:  description = "OriginalTransmissionReference"; break;
            case IPTC_DATE:                    description = "DateCreated"; break;
            case IPTC_COPYRIGHT:               description = "(C)Flag"; break;
            case IPTC_REFERENCE_SERVICE:       description = "CountryCode"; break;
            case IPTC_COUNTRY_CODE:            description = "Ref.Service"; break;
            default:
//                printf("Unrecognised IPTC tag: 0x%02x \n", type);
            break;
        }
        if (description != NULL) {
            if (numIptcEntries < MAX_NUM_IPTC_ENTRIES) {
                char* value = (char *)malloc(length+1);
                sprintf(value, "%*.*s", length, length, pos);

                IptcInfo[numIptcEntries].tag = description;
                IptcInfo[numIptcEntries].val = value;
                numIptcEntries++;
            }
            else {
                printf("Too many IPTC entries!!!!\n\n");
            }
        }
        pos += length;
    }
    return TRUE;
}


//--------------------------------------------------------------------------
// Display IPTC info
//--------------------------------------------------------------------------
void ShowIptcInfo (void)
{
    unsigned i;
    if (numIptcEntries == 0) return;

    for (i=0; i<numIptcEntries; i++) {
        if (IptcInfo[i].tag == NULL) {
            break;
        }
        printf("%-13s: %s\n", IptcInfo[i].tag, IptcInfo[i].val);
    }
    printf("\n");
}

/*
To do:
Just store iptc, and parese on display of it
Make sure iptc gets cleared
Include file reorg, and _CRT_SECURE_NO_DEPRECATE
Option to delete iptc only

Much later:
  iptc transplant
  iptc modify


*/