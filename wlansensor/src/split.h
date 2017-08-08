#ifndef SPLIT_H
#define SPLIT_H




class Split
{

public:

Split(String s);
Split(String s,char delimiter);

boolean next();
String getNext();
int getCount();
String getIndex(int idx);

private:

String _s;
String _part;
int _start,_end,_count;
boolean _next;
char _delimiter = ',';

void setPart();
void init(String s);
void setCount();
  
};

#endif


