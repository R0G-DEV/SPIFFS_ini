/*
    ----------------------   SPIFFS_ini ver. 1.0  ----------------------
      (c) 2019 SpeedBit, reg. Czestochowa, Poland
    --------------------------------------------------------------------
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


#include <SPIFFS_ini.h>
#include "FS.h"
#include <SPIFFS.h>

File ini;

bool spiffsWriting = false;

bool ini_open(const char * ini_name) {

  //wait for any writes to complete before closing file
  unsigned long writeTimeout = millis();
  if(spiffsWriting) {
    do
    {
      vTaskDelay(10);
    } while (spiffsWriting && millis() - writeTimeout <= 6000);
  }
  
  if (ini) ini_close();

  if (SPIFFS.exists(ini_name)) ini = SPIFFS.open(ini_name, "r");

  return ini;
}

bool ini_close() {
  if (ini) ini.close();
  return true;
}

bool ini_eof() {
  if (!ini) return true;
  return (ini.available() <= 0);
}


String ini_read_line() {
  if (!ini) return "";

  String  s = "";
  char    c = 0;
  bool test = false;

  unsigned long readTimeout = millis();
  do { // read string until CR or LF
    if ( ini_eof() ) break;
    c = ini.read();
    test = (c != 13) && (c != 10); // not CR and not LF
    if (test) s +=  c;
  } while (test && millis() - readTimeout <= 3000);

  // if last char was CR then read LF (if exists)
  if ( (c==13) && !ini_eof() ) {
    c = ini.peek();
    if (c == 10) ini.read();
  }

  s.trim(); // trim string

  return s;
}



String ini_read(String section, String key, String def) {
  if (!ini) return "";

  section.toUpperCase();
  key.toUpperCase();

  String s = "";
  String k = "";
  String v = "";
  bool found = false;
  int  ind   = 0;

  ini.seek(0);
  unsigned long findSectionTimeout = millis();
  while (!ini_eof() && millis() - findSectionTimeout <= 3000) { // find section
    s = ini_read_line();
    if ( (s.charAt(0) == '#') || (s.charAt(0) == ';') ) continue; // comment - get next line
    s.toUpperCase();
    if ( (s.charAt(0) == '[') && (s.charAt(s.length() - 1) == ']') ) { // it's section
      if ( s == "[" + section + "]") { found = true; break; } // got it!
    }
  }

  if (found) { // got section
    found = false;
    unsigned long findKeyTimeout = millis();
    while (!ini_eof() && millis() - findKeyTimeout <= 3000) { // find key
      s = ini_read_line();
      if ( (s.charAt(0) == '#') || (s.charAt(0) == ';') ) continue; // comment - get next line
      s.toUpperCase();
      if ( (s.charAt(0) != '[') && (s.charAt(s.length() - 1) != ']') ) {  // it is not next section...
        ind = s.indexOf('=');
        if (ind>=0) { // found "="
          k = s.substring(0, ind);
          v = s.substring(ind + 1);
          k.trim(); // trim key string
          v.trim(); // trim value string
          k.toUpperCase();
          if (k == key) { // got key
            found = true;
            break;
          }
        }
      }
      else break; // got next section - break
    }
  }

  if (found) return v;
  else return def;
}

bool ini_delete(String section) {
  if (!ini) return false;

  bool res = false;
  File tmpini;
  const String tmpname = "tmpdel.ini";

  String sec_upp = section;
  sec_upp.toUpperCase();

  String s = "";
  bool section_found = false;

  String inifile = String(ini.name());
  int    ind     = inifile.lastIndexOf('/');
  String path    = inifile.substring( 0, ind + 1);
  String tmpfile = path + tmpname;

  if (SPIFFS.exists(tmpfile)) SPIFFS.remove(tmpfile); // if exists tmpfile - remove it

  tmpini = SPIFFS.open(tmpfile, "w"); // open tmpfile
  if (tmpini) {
    ini.seek(0);
    tmpini.seek(0);

    unsigned long findSectionTimeout = millis();
    while (!ini_eof() && millis() - findSectionTimeout <= 3000) { // find section
      s = ini_read_line();
      tmpini.println(s);  // save to new file
      // search for section
      if ( (s.length() != (sec_upp.length() + 2) ) || (s.charAt(0) == '#') || (s.charAt(0) == ';') ) continue; // comment - get next line
      s.toUpperCase();
      if ( (s.charAt(0) == '[') && (s.charAt(s.length() - 1) == ']') ) { // it's section
        if ( s == "[" + sec_upp + "]" ) { section_found = true; break; } // got it!
      }
    }

    // we are inside the section
    if (section_found) {
      res = true;
        unsigned long nextSectionTimeout = millis();
        while (!ini_eof() && millis() - nextSectionTimeout <= 3000) { // find section
        s = ini_read_line();
        s.toUpperCase();
        //found next section break
        if((s.charAt(0) == '[') && (s.charAt(s.length() - 1) == ']')) break;
      }
    }

    // save all to the end of file
    unsigned long saveTimeout = millis();
    while (!ini_eof() && millis() - saveTimeout <= 3000) {
      s = ini_read_line();
      tmpini.println( s );
    }
  }

  // close ini files
  if (tmpini) tmpini.close();
  if (ini   ) ini.close();
  SPIFFS.remove( inifile ); //remove old ini
  SPIFFS.rename( tmpfile, inifile ); // tmpini is now ini
  if (ini) ini_close();
  if (SPIFFS.exists(inifile)) ini = SPIFFS.open(inifile, "r");

  return res;
}


bool ini_write(String section, String key, String value) {
  if (!ini) return false;

  spiffsWriting = true;

  bool res = false;
  File tmpini;
  const String tmpname = "tmpwrini.ini";

  String sec_upp = section;
  String key_upp = key;
  String val_upp = value;
  sec_upp.toUpperCase();
  key_upp.toUpperCase();
  val_upp.toUpperCase();

  String s = "";
  String k = "";
  String v = "";
  bool section_found = false;
  bool key_found = false;
  bool spaceBetweenSection = false;
  bool addedNewKey = false;

  String inifile = String(ini.name());
  int    ind     = inifile.lastIndexOf('/');
  String path    = inifile.substring( 0, ind + 1);
  String tmpfile = path + tmpname;

  if (SPIFFS.exists(tmpfile)) SPIFFS.remove(tmpfile); // if exists tmpfile - remove it

  tmpini = SPIFFS.open(tmpfile, "w"); // open tmpfile
  if (tmpini) {
    ini.seek(0);
    tmpini.seek(0);

    unsigned long findSectionTimeout = millis();
    while (!ini_eof() && millis() - findSectionTimeout <= 3000) { // find section
      s = ini_read_line();
      tmpini.println(s);  // save to new file
      // search for section
      if ( (s.length() != (sec_upp.length() + 2) ) || (s.charAt(0) == '#') || (s.charAt(0) == ';') ) continue; // comment - get next line
      s.toUpperCase();
      if ( (s.charAt(0) == '[') && (s.charAt(s.length() - 1) == ']') ) { // it's section
        if ( s == "[" + sec_upp + "]" ) { section_found = true; break; } // got it!
      }
    }

    // we are inside the section
    if (section_found) {
      key_found = false;
      unsigned long findKeyTimeout = millis();
      while (!ini_eof() && millis() - findKeyTimeout <= 3000) { // find key
        s = ini_read_line();
        if ( (s.charAt(0) == '#') || (s.charAt(0) == ';') ) { // comment - get next line
          tmpini.println(s);  // comment - save to new file
          continue;
        }
        s.toUpperCase();
        if ( (s.charAt(0) != '[') && (s.charAt(s.length() - 1) != ']') ) {  // it is not next section...
          ind = s.indexOf('=');
          if (ind>=0) { // found "="
            k = s.substring(0, ind);
            v = s.substring(ind + 1);
            k.trim(); // trim key string
            v.trim(); // trim value string
            k.toUpperCase();
            if (k == key_upp) { // got key
              key_found = true;
              res = true;
              tmpini.println( key_upp + "=" + val_upp );  // new key - save to new file
              break;
            }
            else
              tmpini.println(s);  // not this key - save to new file
          }
          else {  //if space between last key and next section
            spaceBetweenSection = true;
            if(key_found == false) {
              addedNewKey = true;
              tmpini.println( key_upp + "=" + val_upp );  // new key - save to new file
            }

            tmpini.println(s);  // not a key - save to new file
          }
        }
        else { // got next section - break

          if(spaceBetweenSection != true) {
            // key not found create new key
            if(key_found == false) {
              addedNewKey = true;
              tmpini.println( key_upp + "=" + val_upp );  // new key - save to new file
            }
          }

          tmpini.println(s);  // new section - save to new file
          break;
        }
      }
    }
    else { // there is no such section - save new one
      tmpini.println(); // one new line before section
      tmpini.println("[" + section + "]");  // save new section
      tmpini.println( key_upp + "=" + val_upp );  // save new key
      addedNewKey = true;
      res = true;
    }

    if(addedNewKey == false) {
      // key not found create new key
      if(key_found == false) {
        tmpini.println( key_upp + "=" + val_upp );  // new key - save to new file
      }
    }

    // save all to the end of file
    unsigned long saveTimeout = millis();
    while (!ini_eof() && millis() - saveTimeout <= 3000) {
      s = ini_read_line();
      tmpini.println( s );
    }
  }

  // close ini files
  if (tmpini) tmpini.close();
  if (ini   ) ini.close();

  SPIFFS.remove( inifile ); //remove old ini
  SPIFFS.rename( tmpfile, inifile ); // tmpini is now ini
  if (ini) ini_close();
  if (SPIFFS.exists(inifile)) ini = SPIFFS.open(inifile, "r");

  spiffsWriting = false;

  return res;
}