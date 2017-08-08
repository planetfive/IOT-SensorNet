
#ifndef SPIFFS_H
#define SPIFFS_H

extern String dirFiles();
extern boolean writeFile(const char* fName, const char* txt);
extern boolean deleteFile(const char* fName);
extern String getFileContents(const char* filename);
extern String getFileContents(const __FlashStringHelper *filename);
extern boolean uploadFile(char* url,char* file);
extern void fileAnzeigen(char* filename);
extern void fileLoeschen(char* filename);
extern void showFiles();

#endif
