#ifndef XOP_JPEG_PARSER_H
#define XOP_JPEG_PARSER_H

#include <cstdint> 
#include <utility> 
namespace xop
{

class JPEGFrameParser
{
public:
    JPEGFrameParser();
    ~JPEGFrameParser();
    
    unsigned char width(void)     { return _width; }
    unsigned char height(void)    { return _height; }
    unsigned char type(void)      { return _type; }
    unsigned char precision(void) { return _precision; }
    unsigned char qFactor(void)   { return _qFactor; }
    unsigned char driFound(void) { return _driFound; }
    unsigned int jpegheaderLength(void)   { return _jpegheaderLength; }

    unsigned short restartInterval(void) { return _restartInterval; }
 
    unsigned char const* quantizationTables(unsigned short& length)
    {
        length = _qTablesLength;
        return _qTables;
    }
 
    int parse(unsigned char* data, unsigned int size);
 
    unsigned char const* scandata(unsigned int& length)
    {
        length = _scandataLength;
 
        return _scandata;
    }
 
private:
    unsigned int scanJpegMarker(const unsigned char* data,
                                unsigned int size,
                                unsigned int* offset);
    int readSOF(const unsigned char* data,
                unsigned int size, unsigned int* offset);
    unsigned int readDQT(const unsigned char* data,
                         unsigned int size, unsigned int offset);
    int readDRI(const unsigned char* data,
                unsigned int size, unsigned int* offset);
 
private:
    unsigned char _width;
    unsigned char _height;
    unsigned char _type;
    unsigned char _precision;
    unsigned char _qFactor;
    unsigned char _driFound;
    unsigned int  _jpegheaderLength;
    unsigned char* _qTables;
    unsigned short _qTablesLength;
 
    unsigned short _restartInterval;
 
    unsigned char* _scandata;
    unsigned int   _scandataLength;
};
    
}

#endif 

