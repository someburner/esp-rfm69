/*******************************************************************************
*
* CIRCULAR BUFFER MODULE
*
*******************************************************************************/
/* def CBUFF_MODULE__
* Defines that the CBUFF module is present. To be used by other software modules
* to check for the presence of the CBUFF module.
*******************************************************************************/
#define CBUFF_MODULE__

#include "cbuff.h"

/*******************************************************************************
*                                 LOCAL DEFINES
*******************************************************************************/

/*******************************************************************************
* The CBUFF_FULL bit flag is used to indicate that the associated buffer is full
* in 'hCircBuffer->localFlag'. The following functions use this definition:
* See Also: cbuffPutByte(), cbuffGetSpace(), cbuffGetByte(), cbuffUnputByte(),
* cbuffPutArray(), CBUFFOBJ
*******************************************************************************/
#define CBUFF_FULL         (0x01 << 7)

/*******************************************************************************
* Bit flag used to signal buffer is empty. The following functions use this
* definition:
* See Also: cbuffCreate(), cbuffPutByte(), cbuffClearBuffer(), cbuffGetByte(),
* cbuffPeekHead(), cbuffUnputByte(), cbuffPutArray(), cbuffGetArray(), CBUFFOBJ
*******************************************************************************/
#define CBUFF_EMPTY        (0x01 << 6)

/*******************************************************************************
* Bit flag used to signal that the buffer is open and in use. The following
* functions use this definition:
* See Also: cbuffOpen(), cbuffClose(), , CBUFFOBJ
*******************************************************************************/
#define CBUFF_OPEN         (0x01 << 5)

/*******************************************************************************
* static CBUFFOBJ * startOfCbuffObjs
* Pointer to a linked list of the created circular buffer objects.
* Note: This has only local scope only within this module
*******************************************************************************/
static CBUFFOBJ * startOfCbuffObjs;

/*******************************************************************************
* static CBUFFNUM activeCbuffObjs
* Used to note how many circular buffer objects exist. Each bit in this variable
* relates to one active buffer object.
* Note: This has only local scope only within this module
*******************************************************************************/
static CBUFFNUM activeCbuffObjs;

/******************************************************************************/
/*
* Function - cbuffInit()
* Initialises the circular buffer module.
*
* Initialises all global variables needed by the CBUFF module. Must be called
* before using any of the other functions in this module. No check will be made
* to ensure that you did actually initialise the module before using it, so it
* is up to you!
*
* note
* -# Must be called before using any functions in the CBUFF module
*
*******************************************************************************/
void cbuffInit(void)
{
   /* Initialise the linked list pointer */
   startOfCbuffObjs = (CBUFFOBJ *) 0;
   /* Clear active buffers allocated     */
   activeCbuffObjs = 0;
}

/******************************************************************************/
/*
* Function - cbuffDeinit()
* Deinitialises the circular buffer module.
*
* Initialises all global variables needed by the CBUFF module. Must be called
* before using any of the other functions in this module. No check will be made
* to ensure that you did actually initialise the module before using it, so it
* is up to you!
*
* note
* -# Caller is reposible for returning all current handles and deallocating
*   all buffers before calling cbuffDeinit()
* warning
* - The content of any buffers will remain in memory after this function is
*   called. If you have any data there that you don't want other to see, ensure
*   that you clear the buffer's contents before deinitialising this module.
*******************************************************************************/
void cbuffDeinit(void)
{
   /* Clear the linked list pointer      */
   startOfCbuffObjs = (CBUFFOBJ *) 0;
   /* Clear active buffers allocated     */
   activeCbuffObjs = 0;
}

/******************************************************************************/
/*
* Function - cbuffCreate()
* Creates a new buffer object and adds it to the global linked list of
* buffers.
*
* New buffer object will be added to the global linked list of buffers providing
* that there is space for another buffer. In total 16 buffers can be supported
* simultaneously.
*
* param buffer             - a buffer defined by the caller
* param sizeOfBuffer       - size of aformentioned buffer in bytes
* param newCircBufferObj   - buffer object to insert into linked list of buffer
*						      objects
*
* retval     0 - failed to create the buffer
* retval   >=1 - the number asigned to the buffer created
*
* note
* -# The CBUFF module can only handle up to a maximum of 16 buffers
* -# It is recommended that the 'sizeOfBuffer' should always be at least 3 or
*    greater to be useful. This will, however, not be checked by this function.
*******************************************************************************/
CBUFFNUM cbuffCreate(CBUFF * buffer, unsigned int sizeOfBuffer,CBUFFOBJ * newCircBufferObj)
{
   CBUFFOBJ * localCircBufferObj;
   CBUFFNUM circBuffNum = 0x0001;      /* Assign starting buffer number, 1,  */
   /* for use when searching for the     */
   /* next free buffer number            */
   /* 0 reserved for alloc failed!       */

   /* Ensure there is a buffer and it    */
   /* has size                           */
   if ((buffer != (unsigned char *) 0) && (sizeOfBuffer != 0))
   {
      /* Ensure we got somewhere to create  */
      /* our buffer object                  */
      if (newCircBufferObj != (CBUFFOBJ *) 0)
      {
         /* Find place for buffer object in    */
         /* the linked list and assign number  */
         /* If there is nothing in the list,   */
         /* just put this object in the first  */
         /* position                           */
         if (activeCbuffObjs == 0 && startOfCbuffObjs == (CBUFFOBJ *) 0)
         {
            /* Insert buffer object at start of   */
            /* list                               */
            startOfCbuffObjs = newCircBufferObj;
            /* Setup buffer object                */
            startOfCbuffObjs->startOfBuffer = buffer;
            startOfCbuffObjs->endOfBuffer   = buffer + (sizeof(CBUFF) *
            (sizeOfBuffer - 1));
            startOfCbuffObjs->inPointer  = buffer;
            startOfCbuffObjs->outPointer = buffer;
            /* Clear all flags                    */
            startOfCbuffObjs->localFlag = 0x00;
            /* Set buffer empty flag              */
            startOfCbuffObjs->localFlag |= CBUFF_EMPTY;
            /* Ensure we point to NULL            */
            startOfCbuffObjs->nextCircBufferObj = (CBUFFOBJ *) 0;
            /* Assign it a number                 */
            activeCbuffObjs |= circBuffNum;
            startOfCbuffObjs->bufferNumber = circBuffNum;
            /* Return buffer number               */
            return startOfCbuffObjs->bufferNumber;
         }
         /* Otherwise, if we haven't allocated */
         /* all the cbuff objects we can       */
         /* support, insert another            */
         else if (activeCbuffObjs != 0xFFFF)
         {
            /* Insert this object at the start of */
            /* the list                           */
            localCircBufferObj = startOfCbuffObjs;
            startOfCbuffObjs = newCircBufferObj;
            /* Setup buffer object                */
            startOfCbuffObjs->startOfBuffer = buffer;
            startOfCbuffObjs->endOfBuffer   = buffer + (sizeof(CBUFF) *
            (sizeOfBuffer - 1));
            startOfCbuffObjs->inPointer  = buffer;
            startOfCbuffObjs->outPointer = buffer;
            /* Clear all flags                    */
            startOfCbuffObjs->localFlag = 0x00;
            /* Set buffer empty flag              */
            startOfCbuffObjs->localFlag |= CBUFF_EMPTY;
            /* Ensure we point to next buffer     */
            /* object                             */
            startOfCbuffObjs->nextCircBufferObj = localCircBufferObj;
            /* Find a free cbuff number for this  */
            /* buffer                             */
            /* Cbuff Number '1' is assigned, so   */
            /* start looking from '2' for a free  */
            /* number                             */
            circBuffNum <<= 1;

            do
            {
               if (!(activeCbuffObjs & circBuffNum))
               {
                  /* Assign the buffer number           */
                  activeCbuffObjs |= circBuffNum;
                  startOfCbuffObjs->bufferNumber = circBuffNum;
                  return startOfCbuffObjs->bufferNumber;
               }
               /* That wasn't free; try next bit     */
               circBuffNum <<= 1;
               /* Mask just in case unsigned int is  */
               /* bigger than 16-bits                */
               circBuffNum &= 0xFFFF;
            } while(circBuffNum != 0x0000);
         }
      }
   }
   /* If buffer allocation failed,       */
   /* return 0 to callee                 */
   return 0;
}

/******************************************************************************/
/*
* Function - cbuffDestroy()
* Destroys an exisiting buffer object and removes it to the global linked
* list of buffers.
*
* The buffer object will be removed from the global linked list of buffers,
* providing that the buffer exists and that the buffer has been closed.
*
* param bufferNumber  - number of buffer to destroy as returned by cbuffClose()
*
* retval   CBUFF_DESTROY_FAIL  - failed to destroy requested object
* retval   CBUFF_DESTROY_OK    - destroyed requested object successfully
*
* note
* -# Caller must have 'created' (cbuffCreate()) at least one circular buffer
*   object before calling this function
* -# Destroying a buffer object does not delete the buffer's content, nor free
*   the associated CBUFFOBJ memory used
* -# An open buffer cannot be destroyed. Such a case returns a FAIL
*******************************************************************************/
unsigned char cbuffDestroy(CBUFFNUM bufferNumber)
{
   CBUFFOBJ * localCircBufferObj;
   CBUFFOBJ * previousCircBufferObj;
   /* Make sure there is something to    */
   /* remove                             */
   if (startOfCbuffObjs != (CBUFFOBJ *) 0)
   {
      /* Check if first object is the one   */
      /* we are looking for                 */
      if (startOfCbuffObjs->bufferNumber == bufferNumber)
      {
         /* Copy where this object is pointing */
         /* to into startOfCbuffObjs           */
         startOfCbuffObjs = startOfCbuffObjs->nextCircBufferObj;
         /* Clear this buffers bit in the      */
         /* active buffers variable            */
         activeCbuffObjs &= ~bufferNumber;
         /* Destroyed the desired object       */
         return CBUFF_DESTROY_OK;
      }
      /* Otherwise loop through all objects */
      /* if there are some                  */
      else if (startOfCbuffObjs->nextCircBufferObj != (CBUFFOBJ *) 0)
      {
         /* Keep a copy of previous object     */
         /* pointer                            */
         previousCircBufferObj = startOfCbuffObjs;
         /* Get next object in the list        */
         localCircBufferObj = startOfCbuffObjs->nextCircBufferObj;
         /* Loop through each item and check   */
         do
         {
            if (localCircBufferObj->bufferNumber == bufferNumber)
            {
               /* Remove this buffer from list       */
               previousCircBufferObj->nextCircBufferObj =
               localCircBufferObj->nextCircBufferObj;
               /* Clear this buffers bit in the      */
               /* active buffers variable            */
               activeCbuffObjs &= ~bufferNumber;
               /* Destroyed the desired object       */
               return CBUFF_DESTROY_OK;
            }
            else
            {
               /* That wasn't it                     */
               /* Move to next object in the list    */
               previousCircBufferObj = localCircBufferObj;
               localCircBufferObj = localCircBufferObj->nextCircBufferObj;
            }
         } while (localCircBufferObj != (CBUFFOBJ *) 0);
      }
   }
   /* Couldn't find the desired object   */
   return CBUFF_DESTROY_FAIL;
}

/******************************************************************************/
/*
* Function - cbuffOpen()
* Opens a circular buffer for use by caller and initialises an HCBUFFOBJ
* handle to it
*
* The buffer requested will be initialised for first use. The buffer requested
* by bufferNumber must exist, otherwise this function will fail.
*
* param bufferNumber       - number of an existing buffer to use
*
* retval   NULL	- if buffer couldn't be created
* retval   handle  - if buffer was created properly
*
* note
* -# Caller must have created (cbuffCreate()) at least one circular buffer
*    object before calling this function
*******************************************************************************/
HCBUFF cbuffOpen(CBUFFNUM bufferNumber)
{
   CBUFFOBJ * localCircBufferObj;

   /* Check that a CBUFFNUM object    */
   /* exists with bufferNumber           */
   localCircBufferObj = startOfCbuffObjs;

   do
   {
      if (localCircBufferObj->bufferNumber == bufferNumber)
      {
         /* Check buffer is not already open   */
         if (!(localCircBufferObj->localFlag & CBUFF_OPEN))
         {
            /* Note that it is now in use         */
            localCircBufferObj->localFlag |= CBUFF_OPEN;
            /* Return handle to it                */
            return localCircBufferObj;
         }
         /* If buffer is already open, return  */
         /* NULL handle pointer                */
         else
         {
            return (CBUFFOBJ *) 0;
         }
      }
      else
      {
         localCircBufferObj = localCircBufferObj->nextCircBufferObj;
      }
   } while (localCircBufferObj != (CBUFFOBJ *) 0);

   /* Return handle to NULL otherwise    */
   return (CBUFFOBJ *) 0;
}

/******************************************************************************/
/*
* Function - cbuffClose()
* Closes a circular buffer and releases the handle to it
*
* The buffer requested will be closed and will no longer be available for use.
* Attempting to put or get data will fail until the buffer is reopened. Any data
* that is in the memory where the buffer exists will be retained.
*
* param hCircBuffer        - handle of the open buffer to be closed
*
* retval   >0  - number of buffer object closed if buffer was open
* retval   0   - if the buffer object was not open
*
* note
* -# Caller must have 'allocated' and opened at least one circular buffer object
*    before calling this function
*******************************************************************************/
CBUFFNUM cbuffClose(HCBUFF hCircBuffer)
{
   /* Check buffer is actually open      */
   if (hCircBuffer->localFlag & CBUFF_OPEN)
   {
      /* Note that this buffer object is    */
      /* closed                             */
      hCircBuffer->localFlag &= ~CBUFF_OPEN;
      /* Return buffer object's buffer      */
      /* number                             */
      return hCircBuffer->bufferNumber;
   }
   /* Otherwise return 0 to say that     */
   /* buffer object wasn't open          */
   else
   {
      return (CBUFFNUM) 0;
   }
}

/******************************************************************************/
/*
* Function - cbuffPutByte()
* Puts a single CBUFF into the buffer
*
* The CBUFF value provided will be added to the buffer requested. This function
* will fail only if the buffer is full.
*
* param hCircBuffer        - handle of the open buffer to be used
* param data               - the CBUFF value to be added to the buffer
*
* retval   #CBUFF_PUT_OK   - operation was successful
* retval   #CBUFF_PUT_FAIL - operation failed due to buffer being full
*
* note
* -# cbuffOpen() must have been successfully called before using this
*    function
*******************************************************************************/
unsigned char cbuffPutByte(HCBUFF hCircBuffer, CBUFF data)
{
   /* Check that there is somewhere to   */
   /* put data. If the CBUFF_FULL   */
   /* flag is set, there is no space     */
   if (hCircBuffer->localFlag & CBUFF_FULL)
   {
      return CBUFF_PUT_FAIL;
   }

   *hCircBuffer->inPointer = data;     /* Put data in buffer                 */

   hCircBuffer->inPointer++;           /* Increment pointer                  */

   /* Implement circular buffer          */
   /* wrap-around                        */
   if (hCircBuffer->inPointer > hCircBuffer->endOfBuffer)
   {
      hCircBuffer->inPointer = hCircBuffer->startOfBuffer;
   }
   /* Check that the buffer is not now   */
   /* full                               */

   if (hCircBuffer->inPointer == hCircBuffer->outPointer)
   {
      /* Set CBUFF_FULL flag so that        */
      /* we don't attempt to write more     */
      /* data into the buffer until some-   */
      /* thing is read out                  */
      hCircBuffer->localFlag |= CBUFF_FULL;
   }

   /* Note that the buffer is no longer  */
   /* empty                              */
   hCircBuffer->localFlag &= ~CBUFF_EMPTY;

   return CBUFF_PUT_OK;
}

/******************************************************************************/
/*
* Function - cbuffGetSpace()
* Returns how much more data room the buffer can accept
*
* Use this function to find out how much space remains in the chosen buffer.
*
* param hCircBuffer        - handle to HCBUFF variable for this buffer instance
*
* returns  amount of space remaining in the buffer
*
* note
* -# openCircBuffer() must have been successfully called before using this
*    function
*******************************************************************************/
unsigned int cbuffGetSpace(HCBUFF hCircBuffer)
{
   #if 0
   unsigned int localEndOfBuffer;
   unsigned int localStartOfBuffer;
   unsigned int localOutPointer;
   unsigned int localInPointer;

   localEndOfBuffer =   (unsigned int) &hCircBuffer->endOfBuffer
   + sizeof(CBUFF);
   localStartOfBuffer = (unsigned int) &hCircBuffer->startOfBuffer;

   localOutPointer = (unsigned int) &hCircBuffer->outPointer;
   localInPointer  = (unsigned int) &hCircBuffer->inPointer;
   #endif

   /* If CBUFF_FULL is              */
   /* set, then there is no space in     */
   /* buffer; simply return 0            */
   if (hCircBuffer->localFlag & CBUFF_FULL)
   {
      return 0;
   }
   /* If CBUFF_EMPTY is set, then   */
   /* simply return size of buffer       */
   if (hCircBuffer->localFlag & CBUFF_EMPTY)
   {
      #if 1
      return (unsigned int) ((hCircBuffer->endOfBuffer + sizeof(CBUFF)) -
      hCircBuffer->startOfBuffer);
      #endif
      #if 0
      return (localEndOfBuffer - localStartOfBuffer);
      #endif
   }
   /* If inPointer is larger than the    */
   /* outPointer in physical value, the  */
   /* remaining buffer space is the      */
   /* (endOfBuffer - inPointer + 1) +    */
   /* (outPointer - startOfBuffer + 1)   */
   if (hCircBuffer->inPointer > hCircBuffer->outPointer)
   {
      return (unsigned int) ( (hCircBuffer->endOfBuffer -
      hCircBuffer->startOfBuffer + sizeof(CBUFF))
      - (hCircBuffer->inPointer -
      hCircBuffer->outPointer));
   }
   /* If outPointer is larger than the   */
   /* inPointer in physical value, the   */
   /* remaining buffer space is the      */
   /* outPointer - inPointer        */
   else /*if (hCircBuffer->inPointer < hCircBuffer->outPointer)*/
   {
      #if 1
      return (unsigned int) (hCircBuffer->outPointer -
      hCircBuffer->inPointer);
      #endif
      #if 0
      return (localOutPointer - localInPointer);
      #endif
   }
}

/******************************************************************************/
/*
* Function - cbuffGetFill()
* Returns how much data is in the buffer
*
* Use this function to find out how much space has been used in the buffer or,
* alternatively, how much data the buffer contains.
*
* param hCircBuffer        - handle to HCBUFF variable for this buffer instance
*
* returns  number of CBUFF bytes of data in the buffer
*
* note
* -# openCircBuffer() must have been successfully called before using this
*    function
*******************************************************************************/
unsigned int cbuffGetFill(HCBUFF hCircBuffer)
{
   /* Simply calculate buffer size and subtract value returned by cbuffGetSpace */
   return ( (hCircBuffer->endOfBuffer - hCircBuffer->startOfBuffer +
            sizeof(CBUFF)) - cbuffGetSpace(hCircBuffer) );
}

/******************************************************************************/
/*
* Function - cbuffClearBuffer()
* Resets the buffers head and tail pointers and marks buffer as empty
*
* The head and tail pointers in the requested buffer will be reset to point at
* the beginning of the buffer and the buffer will be marked as empty. The data
* currently in the buffer is, however, *not* deleted.
*
* param hCircBuffer        - handle to HCBUFF variable for this buffer instance
*
* note
* -# openCircBuffer() must have been successfully called before using this
*    function.
* -# The associated buffer itself is not emptied. Any data in the buffer will
*    still be in memory after this function is called, although it won't be
*    accessible by this module anymore because the module thinks the buffer
*    is empty.
*
* TODO: Consider renaming this function to 'cbuffResetBuffer()' and using this
*       function name for the function to acutally clear all of the data in the
*       buffer.
*******************************************************************************/
void cbuffClearBuffer(HCBUFF hCircBuffer)
{
   /* Reset buffer pointers              */
   hCircBuffer->inPointer  = hCircBuffer->startOfBuffer;
   hCircBuffer->outPointer = hCircBuffer->startOfBuffer;
   /* Clear all flags                    */
   hCircBuffer->localFlag  = 0x00;
   /* Set buffer empty flag              */
   hCircBuffer->localFlag |= CBUFF_EMPTY;
}

/******************************************************************************/
/*
* Function - cbuffGetByte()
* Removes and returns one CBUFF from chosen buffer
*
* A single byte is removed from the buffer indicated by hCircBuffer and written
* into * data. This function will not allow the buffer to underflow. The buffer
* must exist and be open before calling this function.
*
* param hCircBuffer        - handle to HCBUFF variable for this buffer instance
* param data               - pointer to variable to store read byte
*
* retval   #CBUFF_GET_OK   - operation completed successfully
* retval   #CBUFF_GET_FAIL - operation failed due to buffer being empty
*
* note
* - openCircBuffer() must have been successfully called before using this
*   function
* - This function does not allow a buffer underflow
*
* TODO Check if *data doesn't need a const to prevent the function modifying
* the pointer.
*******************************************************************************/
unsigned char cbuffGetByte(HCBUFF hCircBuffer, CBUFF * data)
{
   /* Check we're don't already have an  */
   /* empty buffer.                      */
   if (hCircBuffer->localFlag & CBUFF_EMPTY)
   {
      return CBUFF_GET_FAIL;
   }
   /* Read requested byte                */
   *data = *hCircBuffer->outPointer;

   /* Increment outPointer first         */
   hCircBuffer->outPointer++;

   /* Implement wrap around              */
   if (hCircBuffer->outPointer > hCircBuffer->endOfBuffer)
   {
      hCircBuffer->outPointer = hCircBuffer->startOfBuffer;
   }

   /* Check we're not now pointing to    */
   /* the inPointer (buffer empty)       */
   if (hCircBuffer->outPointer == hCircBuffer->inPointer)
   {
      /* Note that the buffer is empty      */
      hCircBuffer->localFlag |= CBUFF_EMPTY;
   }
   /* Clear CBUFF_FULL flag as      */
   /* there is now at least one byte     */
   /* free                               */
   hCircBuffer->localFlag &= ~CBUFF_FULL;

   return CBUFF_GET_OK;
}

/******************************************************************************/
/*
* Function - cbuffPeekTail()
* Returns value of the next CBUFF that would be read from buffer without
* actually removing it
*
* This function allows the caller to see what piece if data would be returned if
* it were to be removed from the buffer (using cbuffGetByte() for example). This
* is particularly useful when evaluating data received that uses some sort of
* 'stop' or 'start' byte as part of the protocol e.g. new NMEA GPS Data always
* starts with a ';'
*
* param hCircBuffer        - handle to HCBUFF variable for this buffer instance
* param data               - pointer to variable to store read byte
*
* retval   #CBUFF_GET_OK   - operation completed successfully
* retval   #CBUFF_GET_FAIL - operation failed due to buffer being empty
*
* note
* -# openCircBuffer() must have been successfully called before using this
*    function
* -# This function does not allow a buffer underflow
*
* TODO Check if *data doesn't need a const to prevent the function modifying
* the pointer.
*******************************************************************************/
unsigned char cbuffPeekTail(HCBUFF hCircBuffer,CBUFF * data)
{
   /* Check buffer isn't empty           */
   if (!(hCircBuffer->localFlag & CBUFF_EMPTY))
   {
      /* Return head value in data          */
      *data = *hCircBuffer->outPointer;
      return CBUFF_GET_OK;
   }
   else
   {
      return CBUFF_GET_FAIL;
   }
}

unsigned char cbuffPeekTailPtr(HCBUFF   hCircBuffer, void         * data)
{
   /* Check buffer isn't empty           */
   if (!(hCircBuffer->localFlag & CBUFF_EMPTY))
   {
      // CBUFF  * temp = (CBUFF *) data;
      CBUFF  * temp = (CBUFF  *)data;
      // CBUFF t1 = &data;
      // temp = (CBUFF *) t1;
      temp =hCircBuffer->outPointer;

      /* Return head value in data          */
      //   *data = *hCircBuffer->outPointer;
      return CBUFF_GET_OK;
   }
   else
   {
      return CBUFF_GET_FAIL;
   }
}


/******************************************************************************/
/*
* Function - cbuffPeekHead()
* Returns value of the last data item that was stored in the buffer.
*
* This function allows the caller to see the most recent piece of data that was
* written into the buffer (using cbuffPutByte() for example). This could be
* useful when evaluating data received that uses some sort of 'stop' or 'start'
* byte as part of the protocol and you want advance warning of this as the data
* is put into the buffer e.g. new NMEA GPS Data always starts with a ';'
*
* param hCircBuffer        - handle to HCBUFF variable for this buffer instance
* param data               - pointer to variable to store read byte
*
* retval   #CBUFF_GET_OK   - operation completed successfully
* retval   #CBUFF_GET_FAIL - operation failed due to buffer being empty
*
* note
* -# openCircBuffer() must have been successfully called before using this
*    function
*
* TODO Check if *data doesn't need a const to prevent the function modifying
* the pointer.
*******************************************************************************/
unsigned char cbuffPeekHead(HCBUFF hCircBuffer, CBUFF * data)
{
   /* Check buffer isn't empty           */
   if (!(hCircBuffer->localFlag & CBUFF_EMPTY))
   {
      /* If inPointer doesn't point to      */
      /* startOfBuffer, return value at     */
      /* (inPointer - 1)                    */
      if (hCircBuffer->inPointer != hCircBuffer->startOfBuffer)
      {
         *data = *(hCircBuffer->inPointer - sizeof(CBUFF));
      }
      /* Otherwise handle buffer wrap       */
      /* situation                          */
      else
      {
         *data = *hCircBuffer->endOfBuffer;
      }
      return CBUFF_GET_OK;
   }
   else
   {
      return CBUFF_GET_FAIL;
   }
}

/********************************************************************************/
/*
* CBUFF Module's functions used to return the last data value read back into the
* buffer, or to remove the last data value written into the buffer from the
* buffer.
* @{
*******************************************************************************/
/******************************************************************************/
/** \cond
* Function - cbuffUnputByte()
* \endcond
*
* \brief Removes the last data item that was stored in the buffer.
*
* This function allows the caller to remove the most recent piece of data that
* was written into the buffer (using cbuffPutByte() for example).
*
* \sa cbuffUngetByte()
*
* \param hCircBuffer        - handle to HCBUFF variable for this buffer instance
*
* \retval   0 (zero)    - if successful
* \retval   non-zero    - if there was no data to remove
*
* \note
* -# openCircBuffer() must have been successfully called before using this
*    function
* -# This function does not allow underflow of the buffer
* -# If the last data byte has since been removed from the buffer, i.e. the
*    buffer is now empty, this function will fail in its attempt
* -# The data itself is not removed; only the head pointer to the buffer is
*    moved back one position
*******************************************************************************/
unsigned char cbuffUnputByte(HCBUFF  hCircBuffer)
{
   /* Check to see if we can unput at    */
   /* all. If buffer is empty, do        */
   /* nothing                            */
   if (hCircBuffer->localFlag & CBUFF_EMPTY)
   {
      return 1;
   }
   /* unput the last byte - check first  */
   /* that pointer in not on start       */
   /* boundary                           */
   if (hCircBuffer->inPointer != hCircBuffer->startOfBuffer)
   {
      hCircBuffer->inPointer--;
   }
   /* For boundary case implent wrap-    */
   /* around                             */
   else
   {
      hCircBuffer->inPointer = hCircBuffer->endOfBuffer;
   }
   /* Check if buffer is now empty       */
   if (hCircBuffer->inPointer == hCircBuffer->outPointer)
   {
      hCircBuffer->localFlag |= CBUFF_EMPTY;
   }
   /* Buffer is now officially not full  */
   hCircBuffer->localFlag &= ~CBUFF_FULL;

   return 0;
}

/******************************************************************************/
/*
* Function - cbuffUngetByte()
* Returns the last data item that was removed from the buffer.
*
* This function allows the caller to return the most recent piece of data that
* was written into the buffer (using cbuffGetByte() for example).
*
* param hCircBuffer        - handle to HCBUFF variable for this buffer instance
*
* retval   0 (zero)    - if successful
* retval   non-zero    - if there was no data to return
*
* \note
* -# openCircBuffer() must have been successfully called before using this
*    function
* -# this function only shifts the outPointer back; it doesn't write the data
*    that was read back into the circular buffer. It assumes the data that was
*    read out is still in the buffer. If the data has since been overwritten,
*    i.e. buffer is now full, the function will fail in its attempt
* -# This function does not allow underflow of the buffer
* -# If the buffer was not filled with data, then either random values or the
*    values left over after a 'cbuffClearBuffer' will be 'ungot'. The buffer
*    can still be 'ungot' until the tail pointer gets back to the point
*    where it reaches the head pointer.
*******************************************************************************/
unsigned char  cbuffUngetByte(HCBUFF  hCircBuffer)
{
   /* Check to see if we can unget at    */
   /* all. If buffer is full, do         */
   /* nothing                            */
   if (hCircBuffer->localFlag & CBUFF_FULL)
   {
      return 1;
   }
   /* unget the last byte - check first  */
   /* that pointer in not on start       */
   /* boundary                           */
   if (hCircBuffer->outPointer != hCircBuffer->startOfBuffer)
   {
      hCircBuffer->outPointer--;
   }
   /* For boundary case implementnt      */
   /* wraparound                         */
   else
   {
      hCircBuffer->outPointer = hCircBuffer->endOfBuffer;
   }
   /* Check if buffer is now full        */
   if (hCircBuffer->outPointer == hCircBuffer->inPointer)
   {
      hCircBuffer->localFlag |= CBUFF_FULL;
   }
   /* Buffer is now officially not empty */
   hCircBuffer->localFlag &= ~CBUFF_EMPTY;
   return 0;
}

/******************************************************************************/
/*
* Function - cbuffPutArray()
*
* Allows more than one piece of data to be written into the buffer.
*
* This function allows the caller to write as much data as possible into the
* chosen buffer object until all the data is consumed, or the buffer becomes
* full.
*
* param hCircBuffer    - handle to HCBUFF variable for this buffer instance
* param data           - pointer to data to be written to buffer
* param noOfBytes      - number of bytes being requested to write into buffer
*
* returns  number of bytes actually written in the buffer (may be 0 if buffer
* is full)
*
* note
* -# openCircBuffer() must have been successfully called before using this
*    function
* -# This function will not allow the buffer to overflow
*******************************************************************************/
unsigned int  cbuffPutArray(HCBUFF hCircBuffer, const CBUFF * data,unsigned int noOfBytes)
{
   unsigned int bytesWritten = 0;
   /* Check that there is somewhere to   */
   /* put data. If the CBUFF_FULL   */
   /* flag is set, there is no space     */
   if (hCircBuffer->localFlag & CBUFF_FULL)
   {
      return bytesWritten;
   }

   while (!(hCircBuffer->localFlag & CBUFF_FULL))
   {
      /* Put data in buffer                 */
      *hCircBuffer->inPointer = *data;

      hCircBuffer->inPointer++; /* Increment pointer to circular      */
      /* buffer */

      data++; /* Increment pointer to array data    */

      /* Implement circular buffer          */
      /* wrap-around                        */
      if (hCircBuffer->inPointer > hCircBuffer->endOfBuffer)
      {
         hCircBuffer->inPointer = hCircBuffer->startOfBuffer;
      }

      /* Check that the buffer is not now   */
      /* full                               */
      if (hCircBuffer->inPointer == hCircBuffer->outPointer)
      {
         /* Set CBUFF_FULL flag so that   */
         /* we don't attempt to write more     */
         /* data into the buffer until some-   */
         /* thing is read out                  */
         hCircBuffer->localFlag |= CBUFF_FULL;
      }
      /* Note how much data we've written   */
      bytesWritten++;
      /* Check to see if we've written all  */
      /* requested data                     */
      if (bytesWritten == noOfBytes)
      {
         break;
      }
   }
   /* Note that the buffer is no longer  */
   /* empty                              */
   hCircBuffer->localFlag &= ~CBUFF_EMPTY;

   return bytesWritten;
}

/******************************************************************************/
/** \cond
* Function - cbuffGetArray()
* \endcond
*
* \brief Allows more than one piece of data to be read from the buffer.
*
* This function allows the caller to read as much data as possible from the
* chosen buffer object until either all the data has been read, or the
* destination array is full.
*
* \sa cbuffPutArray(), cbuffPutByte(), cbuffGetByte()
*
* \param hCircBuffer    - handle to HCBUFF variable for this buffer instance
* \param data           - pointer to location to store data read from buffer
* \param noOfBytes      - number of bytes being requested to be read from buffer
*
* \returns  number of bytes actually read from the buffer (may be 0 if buffer
            is empty)
*
* \note
* -# openCircBuffer() must have been successfully called before using this
*    function
* -# This function doesn't allow the buffer to underflow
* -# User must ensure that CBUFF * data points to a space of free memory large
*    enough to accomodate noOfBytes of data
*******************************************************************************/
unsigned int  cbuffGetArray(HCBUFF hCircBuffer, CBUFF * data, unsigned int noOfBytes)
{
   unsigned int bytesRead = 0;
   /* Check we're don't already have an  */
   /* empty buffer.                      */
   if (hCircBuffer->localFlag & CBUFF_EMPTY)
   {
      return bytesRead;
   }

   while(!(hCircBuffer->localFlag & CBUFF_EMPTY))
   {
      /* Read requested byte          */
      *data = *hCircBuffer->outPointer;

      /* Increment outPointer first   */
      hCircBuffer->outPointer++;
      /* Increment array data pointer */
      data++;

      /* Implement wrap around        */
      if (hCircBuffer->outPointer > hCircBuffer->endOfBuffer)
      {
         hCircBuffer->outPointer = hCircBuffer->startOfBuffer;
      }

      /* Check we're not now pointing to  */
      /* the inPointer (buffer empty)     */
      if (hCircBuffer->outPointer == hCircBuffer->inPointer)
      {
         /* Note that the buffer is empty   */
         hCircBuffer->localFlag |= CBUFF_EMPTY;
      }
      /* Note how many bytes we've read     */
      bytesRead++;

      /* Stop function from reading too     */
      /* many bytes                         */
      if (bytesRead == noOfBytes)
      {
         break;
      }
   }
   /* Clear CBUFF_FULL flag as       */
   /* there is now at least one byte */
   /* free                           */
   hCircBuffer->localFlag &= ~CBUFF_FULL;
   return bytesRead;
}

/*******************************************************************************
*
*                           CIRCULAR BUFFER MODULE END
*
*******************************************************************************/
