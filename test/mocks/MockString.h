#ifndef MOCK_STRING_H
#define MOCK_STRING_H

#include <cstring>
#include <cstdlib>

// Simplified Arduino String mock for native testing
class String {
public:
    String() : mBuffer(nullptr), mLength(0) {
        mBuffer = (char*)malloc(1);
        mBuffer[0] = '\0';
    }
    
    String(const char *str) : mBuffer(nullptr), mLength(0) {
        if (str) {
            mLength = strlen(str);
            mBuffer = (char*)malloc(mLength + 1);
            strcpy(mBuffer, str);
        } else {
            mBuffer = (char*)malloc(1);
            mBuffer[0] = '\0';
        }
    }
    
    String(const char *str, size_t len) : mBuffer(nullptr), mLength(len) {
        mBuffer = (char*)malloc(len + 1);
        if (str) {
            memcpy(mBuffer, str, len);
        }
        mBuffer[len] = '\0';
    }
    
    String(const String &other) : mBuffer(nullptr), mLength(other.mLength) {
        mBuffer = (char*)malloc(mLength + 1);
        strcpy(mBuffer, other.mBuffer);
    }
    
    ~String() {
        if (mBuffer) free(mBuffer);
    }
    
    String &operator=(const String &other) {
        if (this != &other) {
            free(mBuffer);
            mLength = other.mLength;
            mBuffer = (char*)malloc(mLength + 1);
            strcpy(mBuffer, other.mBuffer);
        }
        return *this;
    }
    
    String &operator=(const char *str) {
        free(mBuffer);
        if (str) {
            mLength = strlen(str);
            mBuffer = (char*)malloc(mLength + 1);
            strcpy(mBuffer, str);
        } else {
            mLength = 0;
            mBuffer = (char*)malloc(1);
            mBuffer[0] = '\0';
        }
        return *this;
    }
    
    const char *c_str() const { return mBuffer; }
    size_t length() const { return mLength; }
    bool isEmpty() const { return mLength == 0; }
    
    bool operator==(const String &other) const {
        return strcmp(mBuffer, other.mBuffer) == 0;
    }
    
    bool operator==(const char *str) const {
        return strcmp(mBuffer, str) == 0;
    }
    
private:
    char *mBuffer;
    size_t mLength;
};

#endif // MOCK_STRING_H
