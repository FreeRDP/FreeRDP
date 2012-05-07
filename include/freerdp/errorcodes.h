/* 
 * File:   errorcodes.h
 * Author: Arvid
 *
 * Created on April 13, 2012, 9:09 AM
 */

#ifndef ERRORCODES_H
#define	ERRORCODES_H

#ifdef	__cplusplus
extern "C" {
#endif
    
/**
* This static variable holds an error code if the return value from connect is false.
* This variable is always set to 0 in the beginning of the connect sequence.
* The returned code can be used to inform the user of the detailed connect error.
* The value can hold one of the defined error codes below OR an error according to errno
*/
extern int connectErrorCode ;   
#define ERRORSTART 10000
#define PREECONNECTERROR ERRORSTART + 1
#define UNDEFINEDCONNECTERROR ERRORSTART + 2
#define POSTCONNECTERROR ERRORSTART + 3
#define DNSERROR ERRORSTART + 4      /* general DNS ERROR */
#define DNSNAMENOTFOUND ERRORSTART + 5 /* EAI_NONAME */
#define CONNECTERROR ERRORSTART + 6  /* a connect error if errno is not define during tcp connect */
#define MCSCONNECTINITIALERROR ERRORSTART + 7
#define TLSCONNECTERROR ERRORSTART + 8
#define AUTHENTICATIONERROR ERRORSTART + 9


#ifdef	__cplusplus
}
#endif

#endif	/* ERRORCODES_H */

