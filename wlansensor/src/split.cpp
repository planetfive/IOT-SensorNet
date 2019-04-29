#include <Arduino.h>
#include "split.h"



Split::Split(String s,char delimiter){
  _delimiter = delimiter;
  init(s);
}

Split::Split(String s) {
  init(s);
}

void Split::init(String s) {
  if(s.length() == 0) {
  _next = false;
  _count = 0;
  return;
  }
  _s = s;
  _start = 0;
  _next = true;
  setCount();
}

void Split::setCount() {
  _count = 0;
  int len = _s.length();
  for(int i = 0; i < len; i++) {
    if(_s.charAt(i) == _delimiter)
       _count++;
  }
  _count++;
}

int Split::getCount() {
  return _count;
}

boolean Split::next() {
  return _next;
}

String Split::getNext() {
  setPart();
  return _part;
}

String Split::getIndex(int idx) {
  if(idx >= _count || idx < 0 || _count == 0)
     return "";
  _start = 0;
  _next = true;
  int pos = 0;
  while(next()) {
    setPart();
    if(pos++ == idx)
       return _part;
  }
  return "";
}


void Split::setPart() {
  _end = _s.indexOf(_delimiter,_start);

  if(_end == -1) {  // kein delimiter, letzter Teilstring
    _next = false;
    _part = _s.substring(_start);
    return;
  }
  if(_start == _end) { // leerer String
    _start = _end + 1;
    if(_start > _s.length()) {
      _next = false;
    }
    else
      _next = true;
    _part = "";
    return;
  }
  // teilstring
  _part = _s.substring(_start,_end);
  _start = _end + 1;
  if(_start > _s.length()) {
      _next = false;
  }
  else
      _next = true;
}
