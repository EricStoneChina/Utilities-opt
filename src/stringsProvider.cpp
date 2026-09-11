#include <fxcg/display.h>
#include <fxcg/file.h>
#include <fxcg/keyboard.h>
#include <fxcg/system.h>
#include <fxcg/misc.h>
#include <fxcg/app.h>
#include <fxcg/serial.h>
#include <fxcg/rtc.h>
#include <fxcg/heap.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "stringsProvider.hpp"
#include "graphicsProvider.hpp"
#include "settingsProvider.hpp"

int isMBfirst(char s) { // there is probably a syscall for this, but it's not documented
  return (s == '\xE5' || s == '\xE6' || s == '\xE7' || s == '\x7F' || s == '\xF7' || s == '\xF9');
}

int isChineseLanguage() {
  return getSetting(SETTING_LANGUAGE) == LANGUAGE_CHINESE;
}

/*
 * The Prizm Chinese font is selected by the two-byte 0x03 0xA8 prefix passed
 * to PrintXY. Keep that prefix out of this table so translated strings can
 * still be measured and padded by the menu renderer.
 */
static const char* translateText(const char* text) {
  if (!text) return text;
  if (!strcmp(text, "Power options")) return "\xb5\xe7\xd4\xb4\xd1\xa1\xcf\xee";
  if (!strcmp(text, "Auto power off")) return "\xd7\xd4\xb6\xaf\xb9\xd8\xbb\xfa";
  if (!strcmp(text, "Backlight duration")) return "\xb1\xb3\xb9\xe2\xca\xb1\xb3\xa4";
  if (!strcmp(text, "Backlight level")) return "\xb1\xb3\xb9\xe2\xc1\xc1\xb6\xc8";
  if (!strcmp(text, "Power information")) return "\xb5\xe7\xd4\xb4\xd0\xc5\xcf\xa2";
  if (!strcmp(text, "Light tools")) return "\xb5\xc6\xb9\xe2\xb9\xa4\xbe\xdf";
  if (!strcmp(text, "Lantern")) return "\xd5\xd5\xc3\xf7\xb5\xc6";
  if (!strcmp(text, "Flashlight")) return "\xca\xd6\xb5\xe7\xcd\xb2";
  if (!strcmp(text, "Morse light")) return "\xc4\xa6\xb6\xfb\xcb\xb9\xb5\xc6";
  if (!strcmp(text, "Color light")) return "\xb2\xca\xc9\xab\xb5\xc6";
  if (!strcmp(text, "Time tools")) return "\xca\xb1\xbc\xe4\xb9\xa4\xbe\xdf";
  if (!strcmp(text, "Calendar")) return "\xc8\xd5\xc0\xfa";
  if (!strcmp(text, "Tasks")) return "\xc8\xce\xce\xf1";
  if (!strcmp(text, "Chronometer")) return "\xbc\xc6\xca\xb1\xc6\xf7";
  if (!strcmp(text, "Tools")) return "\xb9\xa4\xbe\xdf";
  if (!strcmp(text, "Balance manager")) return "\xd3\xe0\xb6\xee\xb9\xdc\xc0\xed";
  if (!strcmp(text, "Password generator")) return "\xc3\xdc\xc2\xeb\xc9\xfa\xb3\xc9\xc6\xf7";
  if (!strcmp(text, "TOTP authenticator")) return "\xd2\xbb\xb4\xce\xd0\xd4\xc3\xdc\xc2\xeb";
  if (!strcmp(text, "Memory & System")) return "\xc4\xda\xb4\xe6\xd3\xeb\xcf\xb5\xcd\xb3";
  if (!strcmp(text, "Device dashboard")) return "\xc9\xe8\xb1\xb8\xb8\xc5\xc0\xc0";
  if (!strcmp(text, "File manager")) return "\xce\xc4\xbc\xfe\xb9\xdc\xc0\xed\xc6\xf7";
  if (!strcmp(text, "Memory usage")) return "\xc4\xda\xb4\xe6\xca\xb9\xd3\xc3";
  if (!strcmp(text, "Function key color")) return "\xb9\xa6\xc4\xdc\xbc\xfc\xd1\xd5\xc9\xab";
  if (!strcmp(text, "System information")) return "\xcf\xb5\xcd\xb3\xd0\xc5\xcf\xa2";
  if (!strcmp(text, "Add-In manager")) return "\xbc\xd3\xd4\xd8\xcf\xee\xb9\xdc\xc0\xed\xc6\xf7";
  if (!strcmp(text, "Settings")) return "\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Set time")) return "\xc9\xe8\xd6\xc3\xca\xb1\xbc\xe4";
  if (!strcmp(text, "Set date")) return "\xc9\xe8\xd6\xc3\xc8\xd5\xc6\xda";
  if (!strcmp(text, "Time format")) return "\xca\xb1\xbc\xe4\xb8\xf1\xca\xbd";
  if (!strcmp(text, "Date format")) return "\xc8\xd5\xc6\xda\xb8\xf1\xca\xbd";
  if (!strcmp(text, "Home settings")) return "\xd6\xf7\xd2\xb3\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Calendar settings")) return "\xc8\xd5\xc0\xfa\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Calc. lock settings")) return "\xcb\xf8\xb6\xa8\xc9\xe8\xd6\xc3";
  if (!strcmp(text, "Chrono. notification")) return "\xbc\xc6\xca\xb1\xcd\xa8\xd6\xaa";
  if (!strcmp(text, "Show advanced tools")) return "\xb8\xdf\xbc\xb6\xb9\xa6\xc4\xdc";
  if (!strcmp(text, "Startup brightness")) return "\xc6\xf4\xb6\xaf\xc1\xc1\xb6\xc8";
  if (!strcmp(text, "Language")) return "\xd3\xef\xd1\xd4";
  if (!strcmp(text, "About this add-in")) return "\xb9\xd8\xd3\xda\xb1\xbe\xb3\xcc\xd0\xf2";
  if (!strcmp(text, "English")) return "\xd3\xa2\xce\xc4";
  if (!strcmp(text, "Chinese")) return "\xd6\xd0\xce\xc4";
  if (!strcmp(text, "Set time format")) return "\xc9\xe8\xd6\xc3\xca\xb1\xbc\xe4\xb8\xf1\xca\xbd";
  if (!strcmp(text, "Set date format")) return "\xc9\xe8\xd6\xc3\xc8\xd5\xc6\xda\xb8\xf1\xca\xbd";
  if (!strcmp(text, "No notification")) return "\xb2\xbb\xcd\xa8\xd6\xaa";
  if (!strcmp(text, "Flashing pop-up")) return "\xc9\xc1\xcb\xb8\xb5\xaf\xb4\xb0";
  if (!strcmp(text, "Simple pop-up")) return "\xc6\xd5\xcd\xa8\xb5\xaf\xb4\xb0";
  if (!strcmp(text, "Note on home")) return "\xd6\xf7\xd2\xb3\xcc\xe1\xca\xbe";
  if (!strcmp(text, "Chrono. notif.")) return "\xbc\xc6\xca\xb1\xcd\xa8\xd6\xaa";
  if (!strcmp(text, "Set start brightness")) return "\xc9\xe8\xd6\xc3\xc6\xf4\xb6\xaf\xc1\xc1\xb6\xc8";
  if (!strcmp(text, "Set password")) return "\xc9\xe8\xd6\xc3\xc3\xdc\xc2\xeb";
  if (!strcmp(text, "Show last character")) return "\xcf\xd4\xca\xbe\xc4\xa9\xce\xbb\xd7\xd6\xb7\xfb";
  if (!strcmp(text, "Off after locking")) return "\xcb\xf8\xb6\xa8\xba\xf3\xb9\xd8\xbb\xfa";
  if (!strcmp(text, "Lock on [EXE]")) return "\xb0\xb4 EXE \xcb\xf8\xb6\xa8";
  if (!strcmp(text, "Show owner info")) return "\xcf\xd4\xca\xbe\xcb\xf9\xd3\xd0\xd5\xdf";
  if (!strcmp(text, "Run-Mat on unlock")) return "\xbd\xe2\xcb\xf8\xba\xf3\xd4\xcb\xd0\xd0\xd6\xf7\xb3\xcc\xd0\xf2";
  if (!strcmp(text, "Select Panel Type")) return "\xd1\xa1\xd4\xf1\xc3\xe6\xb0\xe5\xc0\xe0\xd0\xcd";
  if (!strcmp(text, "Disabled")) return "\xbd\xfb\xd3\xc3";
  if (!strcmp(text, "Today events")) return "\xbd\xf1\xc8\xd5\xca\xc2\xbc\xfe";
  if (!strcmp(text, "Run-Mat shortcut")) return "\xd6\xf7\xb3\xcc\xd0\xf2\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "File mgr.shortcut")) return "\xce\xc4\xbc\xfe\xb9\xdc\xc0\xed\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Calendar shortcut")) return "\xc8\xd5\xc0\xfa\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Tasks shortcut")) return "\xc8\xce\xce\xf1\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Chrono. shortcut")) return "\xbc\xc6\xca\xb1\xc6\xf7\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Bal. mgr.shortcut")) return "\xd3\xe0\xb6\xee\xb9\xdc\xc0\xed\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Pw. gen. shortcut")) return "\xc3\xdc\xc2\xeb\xc9\xfa\xb3\xc9\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "TOTP shortcut")) return "\xd2\xbb\xb4\xce\xd0\xd4\xc3\xdc\xc2\xeb\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Add-in mgr. s.cut")) return "\xbc\xd3\xd4\xd8\xcf\xee\xb9\xdc\xc0\xed\xbf\xec\xbd\xdd\xb7\xbd\xca\xbd";
  if (!strcmp(text, "Clock type")) return "\xca\xb1\xd6\xd3\xc0\xe0\xd0\xcd";
  if (!strcmp(text, "Date appearance")) return "\xc8\xd5\xc6\xda\xcf\xd4\xca\xbe";
  if (!strcmp(text, "Show seconds")) return "\xcf\xd4\xca\xbe\xc3\xeb\xca\xfd";
  if (!strcmp(text, "Show F. keys labels")) return "\xcf\xd4\xca\xbe\xb9\xa6\xc4\xdc\xbc\xfc\xb1\xea\xc7\xa9";
  if (!strcmp(text, "Display statusbar")) return "\xcf\xd4\xca\xbe\xd7\xb4\xcc\xac\xc0\xb8";
  if (!strcmp(text, "Dark theme")) return "\xc9\xee\xc9\xab\xd6\xf7\xcc\xe2";
  if (!strcmp(text, "Top home panel")) return "\xd6\xf7\xd2\xb3\xc9\xcf\xb7\xbd\xc3\xe6\xb0\xe5";
  if (!strcmp(text, "Right home panel")) return "\xd6\xf7\xd2\xb3\xd3\xd2\xb2\xe0\xc3\xe6\xb0\xe5";
  if (!strcmp(text, "Bottom home panel")) return "\xd6\xf7\xd2\xb3\xcf\xc2\xb7\xbd\xc3\xe6\xb0\xe5";
  if (!strcmp(text, "Left home panel")) return "\xd6\xf7\xd2\xb3\xd7\xf3\xb2\xe0\xc3\xe6\xb0\xe5";
  if (!strcmp(text, "Set long date format")) return "\xc9\xe8\xd6\xc3\xb3\xa4\xc8\xd5\xc6\xda\xb8\xf1\xca\xbd";
  if (!strcmp(text, "Show events count")) return "\xcf\xd4\xca\xbe\xca\xc2\xbc\xfe\xca\xfd\xc1\xbf";
  if (!strcmp(text, "Default calendar view")) return "\xc4\xac\xc8\xcf\xc8\xd5\xc0\xfa\xca\xd3\xcd\xbc";
  if (!strcmp(text, "First day of week")) return "\xc3\xbf\xd6\xdc\xc6\xf0\xca\xbc\xc8\xd5";
  if (!strcmp(text, "Show busy timetables")) return "\xcf\xd4\xca\xbe\xc3\xa6\xcf\xd0\xca\xb1\xbc\xe4\xb1\xed";
  if (!strcmp(text, "Default Calendar")) return "\xc4\xac\xc8\xcf\xc8\xd5\xc0\xfa";
  if (!strcmp(text, "Week")) return "\xd6\xdc";
  if (!strcmp(text, "Month")) return "\xd4\xc2";
  if (!strcmp(text, "Week starts on")) return "\xc3\xbf\xd6\xdc\xbf\xaa\xca\xbc\xd3\xda";
  if (!strcmp(text, "Calculator model")) return "\xbc\xc6\xcb\xe3\xc6\xf7\xd0\xcd\xba\xc5";
  if (!strcmp(text, "Storage free")) return "\xb4\xe6\xb4\xa2\xca\xa3\xd3\xe0";
  if (!strcmp(text, "Main memory free")) return "\xd6\xf7\xc4\xda\xb4\xe6\xca\xa3\xd3\xe0";
  if (!strcmp(text, "Battery voltage")) return "\xb5\xe7\xb3\xd8\xb5\xe7\xd1\xb9";
  if (!strcmp(text, "Real hardware")) return "\xd5\xe6\xca\xb5\xd3\xb2\xbc\xfe";
  if (!strcmp(text, "Yes")) return "\xca\xc7";
  if (!strcmp(text, "No")) return "\xb7\xf1";
  if (!strcmp(text, "Press EXIT to close")) return "\xb0\xb4 EXIT \xb9\xd8\xb1\xd5";
  if (!strcmp(text, "Unknown")) return "\xce\xb4\xd6\xaa";
  if (!strcmp(text, "Delete the")) return "\xc9\xbe\xb3\xfd";
  if (!strcmp(text, "Selected Add-In?")) return "\xd1\xa1\xd6\xd0\xb5\xc4\xbc\xd3\xd4\xd8\xcf\xee\xa3\xbf";
  if (!strcmp(text, "Events starting today")) return "\xbd\xf1\xc8\xd5\xbf\xaa\xca\xbc\xb5\xc4\xca\xc2\xbc\xfe";
  return text;
}

const char* localizeText(const char* text) {
  if (!isChineseLanguage()) return text;
  return translateText(text);
}

int isChineseText(const char* text) {
  if (!text) return 0;
  while (*text == ' ') text++;
  unsigned char first = (unsigned char)*text;
  // E5/E6/E7/F7/F9 are the add-in's legacy glyph encodings, not GB18030
  // text. Excluding them avoids changing arrows, checkboxes and icons.
  if (first == 0xE5 || first == 0xE6 || first == 0xE7 ||
      first == 0xF7 || first == 0xF9) return 0;
  return first >= 0xB0 && first <= 0xF7;
}
/* copy over the next token from an input string, WITHOUT
skipping leading blanks. The token is terminated by the
first appearance of tokchar, or by the end of the source
string.

The caller must supply sufficient space in token to
receive any token, Otherwise tokens will be truncated.

Returns: a pointer past the terminating tokchar.

This will happily return an infinity of empty tokens if
called with src pointing to the end of a string. Tokens
will never include a copy of tokchar.

A better name would be "strtkn", except that is reserved
for the system namespace. Change to that at your risk.

released to Public Domain, by C.B. Falconer.
Published 2006-02-20. Attribution appreciated.
Modified by gbl08ma not to skip blanks at the beginning,
and to not break on a token if it is the body of a multi-byte char.
*/

const char *toksplit(const char *src, /* Source of tokens */
const char tokchar, /* token delimiting char */
char *token, /* receiver of parsed token */
int lgh) /* length token can receive */
/* not including final '\0' */
{
  int prevWasMBLead = 0;
  if (src) {
    while (*src && (tokchar != *src || prevWasMBLead)) {
      if (lgh) {
        *token++ = *src;
        --lgh;
      }
      if(isMBfirst(*src)) prevWasMBLead = 1;
      else prevWasMBLead = 0;
      src++;
    }
    if (*src && (tokchar == *src)) src++;
  }
  *token = '\0';
  return src;
} /* toksplit */


int strEndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    //return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
    return strncasecmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

// not really for strings, but anyway:
// based on http://dsss.be/w/c:memmem
// added case-insensitive functionality
void* memmem(char* haystack, int hlen, const char* needle, int nlen, int matchCase) {
  if (nlen > hlen) return 0;
  int i,j=0;
  switch(nlen) { // we have a few specialized compares for certain needle sizes
  case 0: // no needle? just give the haystack
    return haystack;
  case 1: // just use memchr for 1-byte needle
    if(matchCase) return memchr(haystack, needle[0], hlen);
    else {
      void* lc = memchr(haystack, tolower(needle[0]), hlen);
      if(lc!=NULL) return lc;
      else return memchr(haystack, toupper(needle[0]), hlen);
    }
  default: // generic compare for any other needle size
    // walk i through the haystack, matching j as long as needle[j] matches haystack[i]
    for (i=0; i<hlen-nlen+1; i++) {
      if (matchCase ? haystack[i]==needle[j] : tolower(haystack[i])==tolower(needle[j])) {
        if (j==nlen-1) { // end of needle and it all matched?  win.
          return haystack+i-j;
        } else { // keep advancing j (and i, implicitly)
          j++;
        }
      } else { // no match, rewind i the length of the failed match (j), and reset j
        i-=j;
        j=0;
      }
    }
  }
  return NULL;
}

// strncpy_retlen works like strncpy, but returns the length copied to the string.
int strncpy_retlen(char* dest, const char* src, int n) {
  int i;
  for (i = 0; i < n; i++) {
    if(src[i]) dest[i] = src[i];
    else break;
  }
  int l = i;
  for (; i < n; i++) dest[i] = 0;
  return (l > n ? n : l);
}

// strncpy_replace works like strncpy, but replaces all occurences of char a for char b, unless a
// is the second half of a MB char.
char* strncpy_replace(char* dest, const char* src, int n, char a, char b) {
  int i;
  for (i = 0; i < n; i++) {
    if(src[i]) {
      if(src[i] != a || (i > 0 && isMBfirst(src[i-1])))
        dest[i] = src[i];
      else dest[i] = b;
    }
    else break;
  }
  for (; i < n; i++) dest[i] = 0;
  return dest;
}


// convert a normal text string into a multibyte one where letters become their mini variants
// (F5 screen of the OS's character select dialog)
// dest must be at least double the size of orig.
void stringToMini(char* dest, const char* orig) {
  int len = strlen(orig);
  int dlen = 0;
  for (int i = 0; i < len; i++) {
    if((orig[i] >= 65 && orig[i] <= 90) || (orig[i] >= 97 && orig[i] <= 122)) { // A-Z a-z
      dest[dlen++] = '\xe7';
      dest[dlen++] = orig[i];
    } else if((orig[i] >= 48 && orig[i] <= 57)) { // 0-9
      dest[dlen++] = '\xe5';
      dest[dlen++] = orig[i]-48+208;
    } else if(orig[i] == '+') {
      dest[dlen++] = '\xe5';
      dest[dlen++] = '\xdb';
    } else dest[dlen++] = orig[i];
  }
  dest[dlen] = '\0';
}

int base32_decode(const unsigned char *encoded, unsigned char *result, int size) {
  // assumes encoded value is perfect (no whitespace, invalid chars, etc.)
  // further assumes all letters are upper-case
  int bytes_buffer = 0, bits_count = 0, i = 0;
  for(; i < size && *encoded; encoded++) {
    unsigned char cur_bits;
    if(isupper(*encoded)) {
      cur_bits = *encoded - 'A';
    } else if(*encoded > '1' && *encoded < '8') {
      cur_bits = *encoded - '2' + 26;
    } else return -1;
    // add new bits to end of queue:
    bytes_buffer <<= 5;
    bytes_buffer |= cur_bits;
    bits_count += 5;
    if(bits_count >= 8) {
      // we have enough to fill another byte of result
      bits_count -= 8;
      result[i++] = bytes_buffer >> bits_count;
    }
  }
  if (i < size) {
    result[i] = 0;
  }
  return i;
}

int base32_validate(const char* string) {
  // returns 1 if the provided string is valid base32
  // returns 0 if not
  for(; *string; string++) {
    if(!(isupper(*string) || (*string > '1' && *string < '8'))) return 0;
  }
  return 1;
}

void itoa_zeropad(int n, char* buffer, int digits) {
  // pad with zeros:
  char* ptr = buffer;
  int tens = ipow(10,--digits);
  for(; digits; digits--) {
    if(n < tens) {
      *ptr = '0';
      ptr++;
    }
    tens /= 10;
  }
  itoa(n, (unsigned char*)ptr);
}
