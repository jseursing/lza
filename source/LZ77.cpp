#include "LZ77.h"
#include <cassert>
#include <cstring>
#include <iostream>

// --------------------------------------------------------------------------------------
// Function: Compress
// Notes: None
// --------------------------------------------------------------------------------------
void LZ77::Compress(std::string input, std::string& output, float& compressionRatio)
{
  std::string pendingData; // Temporary data that is not compressed

  // Initialize pointers to the input buffer
  const char* begPtr = &input[0];
  const char* endPtr = begPtr + input.size();
  const char* itr    = begPtr;

  // Encode the length of uncompressed data.
  Encode(endPtr - begPtr, output);

  // Begin compressing data
  while (itr <= endPtr)
  {
    // If we do not have enough remaining data to compress, encode the length
    // of remaining data and append the string. Decompress should look out 
    // for this special case where the length reaches the end of buffer (0).
    if (itr > (endPtr - MinSearchLen))
    {
      Encode(END_OF_BUFFER, output);

      // If there is pending data, add it before the rest of the data.
      if (0 != pendingData.size())
      {
        output += pendingData;
      }

      output += input.substr(itr - begPtr);
      break;
    }
    
    // Search for an existing hash and update the lookup table.
    size_t runLen = 0;
    size_t offset = 0;
    ProcessHash(itr, runLen, offset);

    // If we did not find a match or meet/exceed the minimum specified
    // search length, update pending data.
    if (runLen < MinSearchLen)
    {
      pendingData += *itr++;

      // If we've exceeded the input buffer, encode the pending data and exit.
      // End of buffer denotes (0) as the encode type.
      if (itr > endPtr)
      {
        Encode(END_OF_BUFFER, output);
        output += pendingData;
        break;
      }

      continue;
    }

    // We reached this point because a match was found. First append
    // any pending data. Because this wasn't compressed, encode the
    // appropriate type and then the length.
    if (0 != pendingData.size())
    {
      Encode(SEQ_LEN, output);
      Encode(pendingData.size(), output);

      output += pendingData;
      pendingData.clear();
    }

    // Shift iterator to the next position
    itr += runLen;

    // Encode the type followed by the offset and run length. 
    // The offset and run length are 16-bits each in length.
    Encode(SEQ_LOOKUP, output);
    Encode(offset, output);
    Encode(runLen, output);
  }

  // Set compression ratio statistic
  compressionRatio = static_cast<float>(input.size() - output.size()) /
                     static_cast<float>(input.size());
}

// --------------------------------------------------------------------------------------
// Function: Decompress
// Notes: None
// --------------------------------------------------------------------------------------
void LZ77::Decompress(std::string input, std::string& output)
{
  // Do not process an empty input
  if (0 == input.length())
  {
    return;
  }

  // Assign iterator 
  char* begPtr = &input[0];
  char* endPtr = &input[input.size()];
  char* itr = begPtr;

  // Retrieve uncompressed length and resize output
  size_t uncompressedLen = 0;
  Decode(itr, uncompressedLen);
  output.resize(uncompressedLen, ' ');

  // Begin decompression
  size_t decompPos = 0;
  while (itr < endPtr)
  {
    // Decode the type
    size_t type = 0;
    Decode(itr, type);

    switch (type)
    {
      // Simply retrieve the rest of the data.
      case END_OF_BUFFER:
      {
        memcpy(&output[decompPos], &input[itr - begPtr], endPtr - itr);
        itr = endPtr;
      }
      break;
    
      // Retrieve the length of the data and retrieve.
      case SEQ_LEN:
      {
        size_t length = 0;
        Decode(itr, length);

        memcpy(&output[decompPos], &input[itr - begPtr], length);
        decompPos += length;
        itr += length;
      }
      break;
      
      // Retrieve the offset and run, then retrieve.
      case SEQ_LOOKUP:
      {
        size_t offset = 0;
        Decode(itr, offset);

        size_t runLen = 0;
        Decode(itr, runLen);

        memcpy(&output[decompPos], &output[decompPos - offset], runLen);
        decompPos += runLen;
      }
      break;
    }
  }
}

// --------------------------------------------------------------------------------------
// Function: LZ77
// Notes: 
// * MinSearchLen may improve compression quality at the cost of compression size.
// * SearchTblCnt may improve compression quality at the cost of compression time.
// --------------------------------------------------------------------------------------
LZ77::LZ77(size_t minRun, size_t maxCnt) :
  Flags(FAST_COMPRESSION),
  LookupBuffer(nullptr),
  MinSearchLen(minRun),
  SearchTblCnt(maxCnt)
{
  // Allocate our LookupBuffer which should acommodate:
  // sizeof(char*) * SearchTblCnt
  // The index is represented as the hash of the string while the value
  // is a pointer to the string.
  LookupBuffer = reinterpret_cast<char**>(new char[sizeof(char*) * SearchTblCnt]);
  memset(LookupBuffer, 0, sizeof(char*) * SearchTblCnt);
}

// --------------------------------------------------------------------------------------
// Function: ~LZ77
// Notes: None
// --------------------------------------------------------------------------------------
LZ77::~LZ77()
{
  if (nullptr != LookupBuffer)
  {
    delete[] LookupBuffer;
  }
}

// --------------------------------------------------------------------------------------
// Function: ProcessHash
// Notes: Offset specifies the start position of the matching string sequence 
//        going backwards while runLen specifies the length of the matching
//        string sequence. 
//        The LSB 4-bytes identifies the sequence hash while the remaining
//        data specifies the pointer to the start of the string sequence.
// --------------------------------------------------------------------------------------
void LZ77::ProcessHash(const char*& itr, size_t& runLen, size_t& offset)
{
  // Establish boundaries
  char** begPtr = LookupBuffer;
  char** endPtr = &LookupBuffer[SearchTblCnt];

  // Calculate string hash and establish base/current iterator
  uint32_t sequenceHash = Hash32(itr, MinSearchLen);
  char** baseItr = &LookupBuffer[sequenceHash];
  char** currItr = baseItr;

  // Track compression quality
  float compressionQuality = 0.0f;

  do
  {
    // If the current entry is not initialized, exit the loop.
    if (nullptr == *currItr)
    {
      break;
    }

    // Retrieve the sequence length (matching)
    size_t sequenceLen = 0;

    while (itr[sequenceLen] == (*currItr)[sequenceLen])
    {
      ++sequenceLen;

      // Exit the loop if runLength exceeds the end of buffer or if 
      // the currItr + sequenceLen reaches itr.
      if ((sequenceLen >= static_cast<size_t>(*endPtr - itr)) ||
          ((*currItr + sequenceLen) >= itr))
      {
        break;
      }
    }

    // If the sequenceLen exceeds the minimum search length, calculate
    // the quality using offset and sequence length. Update runLen
    // and offset everytime quality improves.
    if (sequenceLen >= MinSearchLen)
    {
      size_t delta = itr - *currItr;
      float quality = static_cast<float>((sequenceLen * SearchTblCnt) / delta);
      if (quality > compressionQuality)
      {
        compressionQuality = quality;
        runLen = sequenceLen;
        offset = delta;
      }

      if (FAST_COMPRESSION & Flags)
      {
        break;
      }
    }

    // Get the previous entry while handling buffer wrap around.
    currItr = currItr - 1;
    if (currItr < begPtr)
    {
      currItr = endPtr - 1;
    }
  }
  while (*currItr != *baseItr);

  // Update the current table entry so that we can continue to compress
  // large amounts of data by keeping pointers up to date.
  *currItr = const_cast<char*>(itr);
}

// --------------------------------------------------------------------------------------
// Function: EncodeLength
// Notes: 
// * The first encode should be the length of the uncompressed data
// * The LSB is set to indicate there is more data to be retrieve
// --------------------------------------------------------------------------------------
void LZ77::Encode(size_t value, std::string& output)
{
  // Encode 7-bits at a time, the 8th bit is set to identify continuation.
  uint32_t remaining = value;
  while (remaining >= SequenceHdr::SEQ_LIMIT)
  {
    // Encode sequence header
    SequenceHdr seq;
    seq.continueFlag = 1;
    seq.data = remaining & SequenceHdr::LSB7_MASK;
    output += seq.To8();

    // shr remaining by 7
    remaining >>= SequenceHdr::LEN_SHIFT;
  }

  // Encode the remaining length
  output += static_cast<char>(remaining);
}

// --------------------------------------------------------------------------------------
// Function: Decode
// Notes: None
// --------------------------------------------------------------------------------------
void LZ77::Decode(char*& dataPtr, size_t& value)
{
  size_t shift = 0;
  size_t bitShift = SequenceHdr::LEN_SHIFT;

  // Construct our value 7-bits at a time
  for (; reinterpret_cast<SequenceHdr*>(dataPtr)->continueFlag;)
  {
    value |= reinterpret_cast<SequenceHdr*>(dataPtr++)->data << (bitShift * shift);
    ++shift;
  }

  // Retrieve final portion of value
  value |= static_cast<size_t>(*dataPtr++) << (bitShift * shift);
}

// --------------------------------------------------------------------------------------
// Function: Hash32
// Notes: None.
// --------------------------------------------------------------------------------------
uint32_t __inline LZ77::Hash32(const char* ptr, size_t len)
{
  uint32_t hash = 0x13371337;

  if (Flags & FAST_COMPRESSION)
  {
    uint32_t maxChars = SearchTblCnt / 0xFF;
    for (size_t i = 0; i < maxChars + 1; ++i)
    {
      hash ^= (ptr[i] * 0xDEADBEEF);
    }
  }
  else
  {
    std::string sequence(len, ' ');
    memcpy(&sequence[0], ptr, len);
    hash = std::hash<std::string>{}(sequence) % SearchTblCnt;
  }

  return hash % SearchTblCnt;
}