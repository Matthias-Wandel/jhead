#ifndef __IPTC_H
#define __IPTC_H


typedef unsigned char BOOLEAN;
#ifndef TRUE
    #define TRUE 1
    #define FALSE 0
#endif

// IPTC Info - an array of tag/description string pairs
typedef struct {
    char* tag;
    char* val;
}   T_IPTC_INFO;
#define MAX_NUM_IPTC_ENTRIES    100

T_IPTC_INFO IptcInfo [MAX_NUM_IPTC_ENTRIES];

// IPTC functions
int     process_IPTC (unsigned char * CharBuf, unsigned int length);
void    ShowIptcInfo (void);

#endif      // __IPTC_H