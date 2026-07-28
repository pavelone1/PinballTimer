#include "ui/TextEntry.h"

#include <cstdio>
#include <cstring>

namespace {

// Rotation cycles through these characters, then DONE, then DEL, then
// wraps back to the first character.
constexpr char kCharset[] =
    "abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.!@#$%&*";
constexpr uint8_t kCharsetLen = sizeof(kCharset) - 1; // exclude the trailing '\0'
constexpr uint8_t kDoneIndex = kCharsetLen;
constexpr uint8_t kDeleteIndex = kCharsetLen + 1;
constexpr uint8_t kTotalPickerPositions = kCharsetLen + 2;

} // namespace

void TextEntry::reset(const char* initialText)
{
    strncpy(buffer_, initialText, MAX_LENGTH);
    buffer_[MAX_LENGTH] = '\0';
    length_ = static_cast<uint8_t>(strlen(buffer_));
    pickerIndex_ = 0;
}

TextEntry::Result TextEntry::handleEncoderEvent(const EncoderEvent& event)
{
    switch (event.type) {
        case EncoderEventType::RotatedClockwise:
            pickerIndex_ = (pickerIndex_ + 1) % kTotalPickerPositions;
            return Result::None;

        case EncoderEventType::RotatedCounterClockwise:
            pickerIndex_ = (pickerIndex_ + kTotalPickerPositions - 1) % kTotalPickerPositions;
            return Result::None;

        case EncoderEventType::SwShortPress:
            if (pickerIndex_ < kCharsetLen) {
                if (length_ < MAX_LENGTH) {
                    buffer_[length_++] = kCharset[pickerIndex_];
                    buffer_[length_] = '\0';
                }
                return Result::None;
            }

            if (pickerIndex_ == kDoneIndex) {
                return Result::Done;
            }

            // kDeleteIndex
            if (length_ > 0) {
                length_--;
                buffer_[length_] = '\0';
            }
            return Result::None;

        case EncoderEventType::SwLongPress:
            return Result::Cancel;

        default:
            return Result::None;
    }
}

const char* TextEntry::text() const
{
    return buffer_;
}

uint8_t TextEntry::length() const
{
    return length_;
}

void TextEntry::currentPickerLabel(char* outBuf, uint8_t bufSize) const
{
    if (pickerIndex_ < kCharsetLen) {
        snprintf(outBuf, bufSize, "%c", kCharset[pickerIndex_]);
    } else if (pickerIndex_ == kDoneIndex) {
        snprintf(outBuf, bufSize, "DONE");
    } else {
        snprintf(outBuf, bufSize, "DEL");
    }
}
