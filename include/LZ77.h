#pragma once
#include <stdint.h>
#include <string>

class LZ77
{
public:

  void Compress(std::string input, 
                std::string& output,
                float& compressionRatio);
  void Decompress(std::string input, std::string& output);
  LZ77(size_t minRun = 12, size_t maxCnt = 1024);
  ~LZ77();

private:

  enum EncodeType
  {
    END_OF_BUFFER = 1,
    SEQ_LOOKUP    = 2,
    SEQ_LEN       = 3
  };

  void ProcessHash(const char*& itr, size_t& runLen, size_t& offset);
  void Encode(size_t value, std::string& output);
  void Decode(char*& dataPtr, size_t& value);
  uint32_t __inline Hash32(const char* ptr, size_t len);

  struct SequenceHdr
  {
    static const uint32_t LSB7_MASK = 0x7F;
    static const uint32_t SEQ_LIMIT = 0x80;
    static const uint32_t LEN_SHIFT = 7;
    char To8() { return *reinterpret_cast<char*>(this); }

    size_t data         : 7;
    size_t continueFlag : 1;
  };

  enum CompressionFlagsEnum
  {
    FAST_COMPRESSION = 1
  };

  CompressionFlagsEnum Flags;
  char** LookupBuffer;
  size_t MinSearchLen;
  size_t SearchTblCnt;
};