#ifndef __BIN_FILE_H
#define __BIN_FILE_H

#include <stdint.h>

#define MAX_SREC_SIZE       (256 * 1024)
#define MAX_SIZE_TYPE       8
#define MAX_SIZE_SIG        32
#define MAX_SIZE_SVN_REV    20
#define MAX_SIZE_DATE       30

typedef struct {
    uint32_t    hdr_len;                    
    uint8_t     type[MAX_SIZE_TYPE];        
    uint32_t    start_address;              
    uint32_t    version;                    
    uint32_t    crc32;                      
    uint8_t     signature[MAX_SIZE_SIG];    
    uint8_t     svn_rev[MAX_SIZE_SVN_REV];  
    uint8_t     date[MAX_SIZE_DATE];        
    uint8_t     isp_cfg;                    
} bin_file_hdr;

#endif
