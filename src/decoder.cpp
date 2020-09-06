#include "decoder.h"


namespace proto {


    template <class Dest, class Source>
    inline Dest bit_cast(const Source& source) {
        static_assert(sizeof(Dest) == sizeof(Source), "Sizes do not match");
        Dest dest = Dest();
        memcpy(&dest, &source, sizeof(dest));
        return dest;
    }

    inline bool ConsumeBytes(const uint8_t*& current, size_t how_many, size_t& remaining) {
        if (how_many > remaining) {
            return false;
        }
        current += how_many;
        remaining -= how_many;
        return true;
    }

    template <class T>
    inline bool ReadFromBytes(const uint8_t*& current, size_t& remaining, T& value) {
        bool bConsume = ConsumeBytes(current, sizeof(T), remaining);
        if (bConsume) {
            value = *(bit_cast<T*>(current - sizeof(T)));
        }
        return bConsume;
    }

    bool ReadWireTypeAndFieldNumber(const uint8_t*& current, size_t& remaining, uint8_t& wire_type, uint32_t& field_number) {
        uint8_t wire_type_and_field_number = 0;
        if (!ReadFromBytes(current, remaining, wire_type_and_field_number))
            return false;
        wire_type = wire_type_and_field_number & 0x07;
        field_number = wire_type_and_field_number >> 3;
        if (field_number >= 16) {
            field_number = field_number & 0xf;
            bool keep_going = false;
            do {
                uint8_t next_number = 0;
                if (!ReadFromBytes(current, remaining, next_number))
                    return false;
                keep_going = (next_number >= 128);
                field_number = (field_number << 7) | (next_number & 0x7f);
            } while (keep_going);
        }
        return true;
    }

    /*=====================================message====================================*/
    bool Message::ReadVarInt(const uint8_t*& current, size_t& remaining, uint64_t& result) {
        bool keep_going = false;
        int shift = 0;
        do {
            uint8_t next_number = 0;
            if (!ReadFromBytes(current, remaining, next_number))
                return false;
            keep_going = (next_number >= 128);
            result += (uint64_t)(next_number & 0x7f) << shift;
            shift += 7;
        } while (keep_going);
        return true;
    }

    Message::Message(const uint8_t* sz, uint32_t size) :_sz(sz), _size(size) {
    }

    bool Message::ParseFromBytes() {
        const uint8_t* current = _sz;
        size_t remaining = _size;
        while (remaining > 0) {
            uint8_t wire_type = 0;
            uint32_t field_number = 0;
            if (!ReadWireTypeAndFieldNumber(current, remaining, wire_type, field_number))
                return false;
            switch (wire_type) {
                case serialization::internal::WIRETYPE_VARINT: {
                    std::map<uint32_t, converter>::const_iterator it = _functionSet.find(field_number);
                    if (it != _functionSet.end()) {
                        uint64_t value = 0;
                        if (!ReadVarInt(current, remaining, value) || !it->second(&value))
                            return false;
                    } // else 新协议数据兼容老协议
                } break;
                case serialization::internal::WIRETYPE_64BIT: {
                    std::map<uint32_t, converter>::const_iterator it = _functionSet.find(field_number);
                    if (it != _functionSet.end()) {
                        uint64_t value = 0;
                        if (!ReadFromBytes(current, remaining, value) || !it->second(&value))
                            return false;
                    } // else 新协议数据兼容老协议
                } break;
                case serialization::internal::WIRETYPE_LENGTH_DELIMITED: {
                    std::map<uint32_t, converter>::const_iterator it = _functionSet.find(field_number);
                    if (it != _functionSet.end()) {
                        uint64_t size = 0;
                        if (!ReadVarInt(current, remaining, size))
                            return false;
                        const uint8_t* data = current;
                        current += size;
                        remaining -= size;
                        bin_type bin(data, size);
                        if (!it->second(&bin))
                            return false;
                    } // else 新协议数据兼容老协议
                } break;
                case serialization::internal::WIRETYPE_GROUP_START:
                case serialization::internal::WIRETYPE_GROUP_END:
                    return false;
                case serialization::internal::WIRETYPE_32BIT: {
                    std::map<uint32_t, converter>::const_iterator it = _functionSet.find(field_number);
                    if (it != _functionSet.end()) {
                        uint32_t value = 0;
                        if (!ReadFromBytes(current, remaining, value) || !it->second(&value))
                            return false;
                    } // else 新协议数据兼容老协议
                } break;
                default: {
                    return false;
                } break;
            }
        }
        return true;
    }

}  // namespace proto

namespace serialization {

    PBDecoder::PBDecoder(const uint8_t* sz, unsigned int size)
        : _msg(sz, size), _bParseResult(true) {
    }

    bool PBDecoder::decodeValue(serializeItem<bool>& v) {
        return _msg.bind<uint64_t, bool>(&proto::Message::convertValue, v);
    }

    bool PBDecoder::decodeValue(serializeItem<int32_t>& v) {
        return _msg.bind<uint64_t, int32_t>(&proto::Message::convertValue, v);
    }

    bool PBDecoder::decodeValue(serializeItem<uint32_t>& v) {
        return _msg.bind<uint64_t, uint32_t>(&proto::Message::convertValue, v);
    }

    bool PBDecoder::decodeValue(serializeItem<int64_t>& v) {
        return _msg.bind<uint64_t, int64_t>(&proto::Message::convertValue, v);
    }

    bool PBDecoder::decodeValue(serializeItem<uint64_t>& v) {
        return _msg.bind<uint64_t, uint64_t>(&proto::Message::convertValue, v);
    }

    bool PBDecoder::decodeValue(serializeItem<float>& v) {
        return _msg.bind<uint32_t, float>(&proto::Message::convertValue, v);
    }

    bool PBDecoder::decodeValue(serializeItem<double>& v) {
        return _msg.bind<uint64_t, double>(&proto::Message::convertValue, v);
    }

    bool PBDecoder::decodeValue(serializeItem<std::string>& v) {
        return _msg.bind<proto::bin_type, std::string>(&proto::Message::convertValue, v);
    }

    bool PBDecoder::decodeRepaeted(serializeItem<std::vector<bool> >& v) {
        if (v.type == serialization::TYPE_PACK)
            return _msg.bind<proto::bin_type, std::vector<bool> >(&proto::Message::convertArray, v);
        else
            return _msg.bind<uint64_t, std::vector<bool> >(&proto::Message::convertArray, v);
    }

    bool PBDecoder::decodeRepaeted(serializeItem<std::vector<int32_t> >& v) {
        if (v.type == serialization::TYPE_PACK)
            return _msg.bind<proto::bin_type, std::vector<int32_t> >(&proto::Message::convertArray, v);
        else
            return _msg.bind<uint64_t, std::vector<int32_t> >(&proto::Message::convertArray, v);
    }

    bool PBDecoder::decodeRepaeted(serializeItem<std::vector<uint32_t> >& v) {
        if (v.type == serialization::TYPE_PACK)
            return _msg.bind<proto::bin_type, std::vector<uint32_t> >(&proto::Message::convertArray, v);
        else
            return _msg.bind<uint64_t, std::vector<uint32_t> >(&proto::Message::convertArray, v);
    }

    bool PBDecoder::decodeRepaeted(serializeItem<std::vector<int64_t> >& v) {
        if (v.type == serialization::TYPE_PACK)
            return _msg.bind<proto::bin_type, std::vector<int64_t> >(&proto::Message::convertArray, v);
        else
            return _msg.bind<uint64_t, std::vector<int64_t> >(&proto::Message::convertArray, v);
    }

    bool PBDecoder::decodeRepaeted(serializeItem<std::vector<uint64_t> >& v) {
        if (v.type == serialization::TYPE_PACK)
            return _msg.bind<proto::bin_type, std::vector<uint64_t> >(&proto::Message::convertArray, v);
        else
            return _msg.bind<uint64_t, std::vector<uint64_t> >(&proto::Message::convertArray, v);
    }

    bool PBDecoder::decodeRepaeted(serializeItem<std::vector<float> >& v) {
        return _msg.bind<uint64_t, std::vector<float> >(&proto::Message::convertArray, v);
    }

    bool PBDecoder::decodeRepaeted(serializeItem<std::vector<double> >& v) {
        return _msg.bind<uint64_t, std::vector<double> >(&proto::Message::convertArray, v);
    }

    bool PBDecoder::decodeRepaeted(serializeItem<std::vector<std::string> >& v) {
        return _msg.bind<proto::bin_type, std::vector<std::string> >(&proto::Message::convertArray, v);
    }

    bool PBDecoder::ParseFromBytes() {
        return _msg.ParseFromBytes();
    }

}
