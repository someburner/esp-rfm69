// Headers to the various functions in the rom (as we discover them)

#ifndef _ROM_H_
#define _ROM_H_

#include "c_types.h"
#include "ets_sys.h"
#include <stdarg.h>

// SHA1 is assumed to match the netbsd sha1.h headers
#define SHA1_DIGEST_LENGTH		20
#define SHA1_DIGEST_STRING_LENGTH	41

typedef struct {
	uint32_t state[5];
	uint32_t count[2];
	uint8_t buffer[64];
} SHA1_CTX;

extern void SHA1Transform(uint32_t[5], const uint8_t[64]);
extern void SHA1Init(SHA1_CTX *);
extern void SHA1Final(uint8_t[SHA1_DIGEST_LENGTH], SHA1_CTX *);
extern void SHA1Update(SHA1_CTX *, const uint8_t *, unsigned int);


// MD5 is assumed to match the NetBSD md5.h header
#define MD5_DIGEST_LENGTH		16
typedef struct
{
  uint32_t state[5];
  uint32_t count[2];
  uint8_t buffer[64];
} MD5_CTX;

extern void MD5Init(MD5_CTX *);
extern void MD5Update(MD5_CTX *, const unsigned char *, unsigned int);
extern void MD5Final(unsigned char[MD5_DIGEST_LENGTH], MD5_CTX *);

// base64_encode/decode derived by Cal
// Appears to match base64.h from netbsd wpa utils.
extern unsigned char * base64_encode(const unsigned char *src, size_t len, size_t *out_len);
extern unsigned char * base64_decode(const unsigned char *src, size_t len, size_t *out_len);
// Unfortunately it that seems to require the ROM memory management to be
// initialized because it uses mem_malloc

extern void mem_init(void * start_addr);

// Interrupt Service Routine functions
//typedef void (*ets_isr_fn) (void *arg, uint32_t sp);
//extern int ets_isr_attach(unsigned int interrupt, ets_isr_fn, void *arg);
extern void ets_isr_mask(unsigned intr);
extern void ets_isr_unmask(unsigned intr);

// Cycle-counter
extern unsigned int xthal_get_ccount(void);
extern int xthal_set_ccompare(unsigned int timer_number, unsigned int compare_value);

// 2, 3 = reset (module dependent?), 4 = wdt
int rtc_get_reset_reason(void);

// Hardware exception handling
struct exception_frame
{
  uint32_t epc;
  uint32_t ps;
  uint32_t sar;
  uint32_t unused;
  union {
    struct {
      uint32_t a0;
      // note: no a1 here!
      uint32_t a2;
      uint32_t a3;
      uint32_t a4;
      uint32_t a5;
      uint32_t a6;
      uint32_t a7;
      uint32_t a8;
      uint32_t a9;
      uint32_t a10;
      uint32_t a11;
      uint32_t a12;
      uint32_t a13;
      uint32_t a14;
      uint32_t a15;
    };
    uint32_t a_reg[15];
  };
  uint32_t cause;
};

/**
 * C-level exception handler callback prototype.
 *
 * Does not need an RFE instruction - it is called through a wrapper which
 * performs state capture & restore, as well as the actual RFE.
 *
 * @param ef An exception frame containing the relevant state from the
 *           exception triggering. This state may be manipulated and will
 *           be applied on return.
 * @param cause The exception cause number.
 */
typedef void (*exception_handler_fn) (struct exception_frame *ef, uint32_t cause);

/**
 * Sets the exception handler function for a particular exception cause.
 * @param handler The exception handler to install.
 *                If NULL, reverts to the XTOS default handler.
 * @returns The previous exception handler, or NULL if none existed prior.
 */
exception_handler_fn _xtos_set_exception_handler (uint32_t cause, exception_handler_fn handler);

void ets_update_cpu_frequency (uint32_t mhz);
uint32_t ets_get_cpu_frequency (void);

void ets_delay_us(uint32_t us);

void ets_timer_disarm(ETSTimer *a);
void ets_timer_setfn(ETSTimer *t, ETSTimerFunc *fn, void *parg);

void Cache_Read_Enable(uint32_t b0, uint32_t b1, uint32_t use_40108000);
void Cache_Read_Disable(void);

void ets_intr_lock(void);
void ets_intr_unlock(void);

void ets_install_putc1(void *routine);
void uart_div_modify(int no, unsigned int freq);

void ets_str2macaddr(uint8_t *dst, const char *str);

/* ets libc declarations & references */

/* memmcpy ref:
 * function: copies n bytes from src to dst.
 * Assumes src and dst regions don't overlap. If so, use memmove.
 */
void *ets_memcpy(void *dst, const void *src, size_t n);

/* memmove ref:
 * function: copies n bytes from src to dst.
 * The copy is done such that if the two regions overlap, src is always read
 * before that byte is changed by writing to the destination.
 */
void *ets_memmove(void *dst, const void *src, size_t n);

/* memset ref:
 * function: stores n copies of c starting at dst
 */
void *ets_memset(void *dst, int c, size_t n);

/* memcmp ref:
 * Compares two memory regions for n bytes
 * Returns:
 * zero: s1 == s2
 * positive: s1 > s2
 * negative: s1 < s2
 */
int ets_memcmp (const void *s1, const void *s2, size_t n);

/* bzero ref:
 * function: data at s is filled with n zeros.
 */
void ets_bzero(void *s, size_t n);

/* strcpy ref:
 * function: copy string. dst should be large enough to contain string and null char
 * returns: destination ptr
 */
char *ets_strcpy(char *dst, const char *src);

/* strlen ref:
 * Returns: the number chars in string, null-char not included
 */
size_t ets_strlen(const char *s);

/* strcmp ref:
 * Returns:
 * <0: the 1st char that does not match has a lower value in s1 than in s2
 * 0: strings are equal
 * >0: the 1st char that does not match has a greater value in s1 than in s2
 */
int ets_strcmp(const char *s1, const char *s2);

/* strncmp ref:
 * Same as strcmp, but only compares first n characters
 */
int ets_strncmp(const char *s1, const char *s2, size_t n);

/* printf ref:
 * Prints formatted string to uart.
 */
int ets_printf(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));

/* sprintf ref:
 * Sends formatted output from the arguments (...) to str.
 * Use snprintf to avoid buffer overruns.
 * Returns: number of chars written
 */
int ets_sprintf(char *str, const char *format, ...)  __attribute__ ((format (printf, 2, 3)));

int ets_vsprintf(char *str, const char *format, va_list argptr);
// see http://embeddedgurus.com/stack-overflow/2009/02/effective-c-tips-1-using-vsprintf/

/* snprintf ref:
 * Function: Similar to sprintf, however, the size n of the buffer is also taken into account.
 * This function will write n - 1 characters. If n is 0, str is untouched.
 */
int ets_snprintf(char *str, size_t n, const char *format, ...)  __attribute__ ((format (printf, 3, 4)));

int ets_vsnprintf(char *buffer, size_t sizeOfBuffer,  const char *format, char * argptr);
// see http://embeddedgurus.com/stack-overflow/2009/02/effective-c-tips-1-using-vsprintf/

/* atoi ref:
 * Function: Convert as much of the string as possible to an equivalent integer value.
 * Returns: value in integer form. 0 if it does not represent a number
 */
int atoi(const char *nptr);

/* strtoul ref:
 * function: convert as much of nptr as looks like an appropriate number
 * into the value of that number. If endp is not a null pointer, *endp is set
 * to point to the first unused character. The base argument indicates what base
 * the digits (or letters) should be treated as. If base is zero, the base is
 * determined by looking for 0x, 0X, or 0 as the first part of the string,
 * and sets the base used to 16, 16, or 8 if it finds one.
 * The default base is 10 if none of those prefixes are found.
 */
unsigned long int strtoul(const char *nptr, char **endptr, int base);



#endif
