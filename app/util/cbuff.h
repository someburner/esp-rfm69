/*******************************************************************************
*
* CIRCULAR BUFFER MODULE
*
*******************************************************************************/
/*******************************************************************************
* Provides a universal 'unsigned char' circular buffer. All contents within this
* file are 'public' and to be used by end user
*******************************************************************************/
#ifndef CBUFF_MODULE_PRESENT__
#define CBUFF_MODULE_PRESENT__

/******************************************************************************/
/*   CBUFF_GET_OK
 *   Signals that cbuffGetByte(), cbuffPeekTail(), and
 *   cbuffPeekHead() functions successfully read a byte
 ******************************************************************************/
#define CBUFF_GET_OK               0x01

/******************************************************************************/
/*   CBUFF_GET_FAIL
 *   Signals that cbuffGetByte(), cbuffPeekTail(), and cbuffPeekHead()
 *   functions failed to read a byte
 ******************************************************************************/
#define CBUFF_GET_FAIL             0x00

/******************************************************************************/
/*   CBUFF_PUT_OK
 *   Signals that cbuffPutByte() function successfully wrote a byte
 ******************************************************************************/
#define CBUFF_PUT_OK               0x01

/******************************************************************************/
/*   CBUFF_PUT_FAIL
 *   Signals that cbuffPutByte() function failed to write a byte - most likely
 *   a sign that the chosen buffer is full
 ******************************************************************************/
#define CBUFF_PUT_FAIL             0x00

/******************************************************************************/
/*   CBUFF_DESTROY_FAIL
 *   Signals that cbuffDestroy() failed to deallocate the requested buffer
 *   object
 ******************************************************************************/
#define CBUFF_DESTROY_FAIL         0x00

/******************************************************************************/
/*  CBUFF_DESTROY_OK
 *  Signals that cbuffDestroy() successfully deallocated the requested object
 ******************************************************************************/
#define CBUFF_DESTROY_OK           0x01

/*******************************************************************************
*                                   DATA TYPES
*******************************************************************************/
/******************************************************************************/
/* CBUFF
 * Data type for use to create arrays for use as circular buffers with the CBUFF
 * module
 ******************************************************************************/
typedef unsigned char CBUFF;

/******************************************************************************/
/* CBUFFNUM
 * Used to hold the unique buffer number that the buffer was assigned at creation
 * with cbuffCreate(), to define which buffer should be used when acquiring a
 * handle with cbuffOpen() and which buffer should be destroyed with
 * cbuffDestroy()
 ******************************************************************************/
typedef unsigned int CBUFFNUM;

/*******************************************************************************
* Structure: CBUFFTYPE
* This structure holds the following information that is needed to understand
* the status of the buffer.
* - startOfBuffer       - Pointer to the start of the memory the user has given
*                         the CBUFF module
* - endOfBuffer         - Pointer to the end of the memory the user has given
*                         the CBUFF module
* - inPointer           - Pointer to the write-in point into the buffer, the
*                         point in the buffer where the next piece of data will
*                         be written
* - outPointer          - Pointer to the read-out point into the buffer, the
*                         point in the buffer where the next piece of data will
*                         be read out of
* - bufferNumer         - The unique reference number assigned to this buffer by
*                         the CBUFF module open its creation
* - localFlag           - Handles status flags for this buffer. The flags are
*                         defined as CBUFF_FULL, CBUFF_EMPTY and CBUFF_OPEN in
*                         cbuff.c
* - nextCircBufferObj   - Pointer to the next CBUFF object in the linked list,
*                         if there is an next object, else NULL.
*******************************************************************************/
struct CBUFFTYPE {
   CBUFF * startOfBuffer;
   CBUFF * endOfBuffer;
   CBUFF * inPointer;
   CBUFF * outPointer;
   CBUFFNUM bufferNumber;
   unsigned char localFlag;
   struct CBUFFTYPE * nextCircBufferObj;
};

/******************************************************************************/
/* CBUFFOBJ
 * The CBUFFOBJ data type is used to create variables that hold all the
 * information needed by the CBUFF module to keep track of the status of the
 * buffer that has been created. One variable is needed per buffer created.
 * Used by cbuffCreate()
 ******************************************************************************/
typedef struct CBUFFTYPE CBUFFOBJ;

/******************************************************************************/
/* HCBUFF
 * The HCBUFF data type is used to create variables to store handles to buffers
 * that have been opened with cbuffOpen(), or to close them with cbuffClose()
 ******************************************************************************/
typedef CBUFFOBJ * HCBUFF;

/*******************************************************************************
*                         FUNCTION PROTOTYPES
*******************************************************************************/
void cbuffInit(void);
void cbuffDeinit(void);

CBUFFNUM cbuffCreate(CBUFF * buffer, unsigned int sizeOfBuffer,CBUFFOBJ * newCircBufferObj);
unsigned char cbuffDestroy(CBUFFNUM bufferNumber);

HCBUFF cbuffOpen(CBUFFNUM bufferNumber);
CBUFFNUM cbuffClose(HCBUFF hCircBuffer);

unsigned char cbuffPutByte(HCBUFF hCircBuffer, CBUFF data);
unsigned char cbuffGetByte(HCBUFF hCircBuffer, CBUFF * data);

unsigned char cbuffUngetByte(HCBUFF hCircBuffer);
unsigned char cbuffUnputByte(HCBUFF hCircBuffer);

unsigned int  cbuffPutArray(HCBUFF hCircBuffer, const CBUFF * data, unsigned int noOfBytes);
unsigned int  cbuffGetArray(HCBUFF hCircBuffer, CBUFF * data, unsigned int noOfBytes);

unsigned char cbuffPeekHead(HCBUFF hCircBuffer, CBUFF * data);
unsigned char cbuffPeekTail(HCBUFF hCircBuffer, CBUFF * data);
unsigned char cbuffPeekTailPtr(HCBUFF hCircBuffer, void * data);

unsigned int cbuffGetSpace(HCBUFF hCircBuffer);
unsigned int cbuffGetFill(HCBUFF hCircBuffer);

void cbuffClearBuffer(HCBUFF hCircBuffer);

/*******************************************************************************
*
*                           CIRCULAR BUFFER MODULE END
*
*******************************************************************************/
#endif
