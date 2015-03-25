#ifndef __ERROR_H__
#define __ERROR_H__


typedef enum
{
    ERROR_OK = 0,
    ERROR_PAGE_BAD_MAGIC,
    ERROR_PAGE_BAD_CRC32,
    ERROR_PAGE_BAD_SIZE,
    ERROR_PAGE_BAD_KIND
} ErrorCode;


extern ErrorCode error_code;


const char* error_message(ErrorCode);


#endif /* __ERROR_H__ */
