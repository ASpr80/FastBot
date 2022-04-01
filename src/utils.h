#pragma once
#include <Arduino.h>
void FB_unicode(String &uStr);

struct FB_Parser {
    FB_Parser() {
      str.reserve(20);
    }
    
    bool parseNT(const String& s) {
      while (!end) {
        char c1 = s[++i];
        if (c1 == '\t' || c1 == '\n' || c1 == '\0') {
          int to = i;
          if (s[to - 1] == ' ') to--;
          if (s[from] == ' ') from++;
          str = s.substring(from, to);
          from = i + 1;
          end = (c1 == '\0');
          div = c1;
          return 1;
        }
      }
      return 0;
    }

    bool parse(const String& s) {
        if (i == (int)s.length()) return 0;
        i = s.indexOf(',', from);
        if (i < 0) i = s.length();
        int to = i;
        if (s[to - 1] == ' ') to--;
        if (s[from] == ' ') from++;
        str = s.substring(from, to);
        from = i + 1;
        return 1;    
    }
    
    int i = 0, from = 0;
    char div;
    bool end = false;
    String str;
};