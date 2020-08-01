#ifndef __PROTOBUF_ENCODER_H__
#define __PROTOBUF_ENCODER_H__
#include <string>
#include <vector>
#include "serialize.h"


namespace serialization {

    class EXPORTAPI PBEncoder {
        typedef void(PBEncoder::*writeValue)(uint64_t);
        static writeValue const functionArray[4];
        template <int isStruct>
        struct valueEncoder {
            template <typename IN>
            static void encode(const IN& in, int32_t type, PBEncoder& encoder) {
                encoder.encodeValue(in, type);
            }
        };
        template <>
        struct valueEncoder<1> {
            template <typename IN>
            static void encode(const IN& in, int32_t type, PBEncoder& encoder) {
                encoder.operator<<(in);
            }
        };
        friend struct valueEncoder<1>;
        BufferWrapper& _buffer;
    public:
        PBEncoder(BufferWrapper& _buffer);
        ~PBEncoder();

        const char* data()const;
        uint32_t size()const;

        template<typename T>
        bool operator<<(const T& value) {
            T* pValue = const_cast<T*>(&value);
            serializeWrapper(*this, *pValue);
            return true;
        }

        template<typename T>
        PBEncoder& operator&(const serializePair<T>& pair) {
            if (!isMessage<T>::YES) {
                uint64_t tag = ((uint64_t)pair.num() << 3) | isMessage<T>::WRITE_TYPE;
                varInt(tag);
                valueEncoder<isMessage<T>::YES>::encode(pair.value(), pair.type(), *this);
                return *this;
            }
            uint64_t tag = ((uint64_t)pair.num() << 3) | WT_LENGTH_DELIMITED;
            varInt(tag);
            BufferWrapper bfTemp;
            bfTemp.swap(_buffer);
            valueEncoder<isMessage<T>::YES>::encode(pair.value(), pair.type(), *this);
            varInt(_buffer.size());
            bfTemp.append(_buffer.data(), _buffer.size());
            _buffer.swap(bfTemp);
            return *this;
        }

        template<typename T>
        PBEncoder& operator&(const serializePair<std::vector<T> >& pair) {
            if (!isMessage<T>::YES && pair.type() == TYPE_PACK) {
                return encodeRepaetedPack(pair);
            }

            uint64_t tag = ((uint64_t)pair.num() << 3) | WT_LENGTH_DELIMITED;
            uint32_t size = (uint32_t)pair.value().size();
            for (uint32_t i = 0; i < size; ++i) {
                varInt(tag);
                if (isMessage<T>::YES) {
                    BufferWrapper bfTemp;
                    bfTemp.swap(_buffer);
                    valueEncoder<isMessage<T>::YES>::encode(pair.value().at(i), pair.type(), *this);
                    _buffer.swap(bfTemp);
                    varInt(bfTemp.size());
                    _buffer.append(bfTemp.data(), bfTemp.size());
                } else {
                    valueEncoder<isMessage<T>::YES>::encode(pair.value().at(i), pair.type(), *this);
                }
            }
            return *this;
        }
    private:
        template<typename T>
        PBEncoder& encodeRepaetedPack(const serializePair<std::vector<T> >& pair) {
            uint64_t tag = ((uint64_t)pair.num() << 3) | WT_LENGTH_DELIMITED;
            varInt(tag);
            BufferWrapper bfTemp;
            bfTemp.swap(_buffer);
            uint32_t size = (uint32_t)pair.value().size();
            for (uint32_t i = 0; i < size; ++i) {
                valueEncoder<isMessage<T>::YES>::encode(pair.value().at(i), pair.type(), *this);
            }
            _buffer.swap(bfTemp);
            varInt(bfTemp.size());
            _buffer.append(bfTemp.data(), bfTemp.size());
            return *this;
        }
        PBEncoder& encodeValue(const std::string& v, int32_t type);
        PBEncoder& encodeValue(const float& v, int32_t type);
        PBEncoder& encodeValue(const double& v, int32_t type);
        template<typename T>
        PBEncoder& encodeValue(const T& v, int32_t type) {
            value(v, type);
            return *this;
        }
        void value(uint64_t value, int32_t type);
        void varInt(uint64_t value);
        void svarInt(uint64_t value);
        void fixed32(uint64_t value);
        void fixed64(uint64_t value);
        void encodeVarint32(uint32_t low, uint32_t high);
    };
}


#endif