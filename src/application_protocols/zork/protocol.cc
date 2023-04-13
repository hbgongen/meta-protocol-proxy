#include "src/application_protocols/zork/protocol.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MetaProtocolProxy {
namespace Zork {

uint64_t ZorkHeader::doReadCombineHeader(Buffer::Instance& buffer, uint64_t pos_) {
  // req_type_ = buffer.peekLEInt<uint16_t>(pos);
  // pos += sizeof(uint16_t);
  // pack_attrs_ = buffer.peekLEInt<uint16_t>(pos);
  // skip type and attrs field
  uint64_t pos = pos_;
  pos += 2 * sizeof(uint16_t);
  // get packet len
  if (pos > buffer.length()) {
    ENVOY_LOG(debug, "Zork [doReadCombine]In buffer length not enough.buffer len-{} pos-{}",
              buffer.length(), pos);
    return 99999999;
  }
  uint16_t len = buffer.peekLEInt<uint16_t>(pos);
  // skip len
  pos += sizeof(uint16_t);
  // begin to read body num
  pos += (len) * sizeof(int8_t);
  ENVOY_LOG(debug, "Zork Request Header [doReadCombineHeader] len-{} pos-{}", len, pos);
  return pos;
}

uint64_t ZorkHeader::CheckIOSRspBag(Buffer::Instance& buffer, uint64_t pos_) {
  uint64_t pos = pos_;

  // tag
  pos += sizeof(int8_t);
  // req_type
  pos += sizeof(uint16_t);
  // pack_attrs_
  pos += sizeof(uint16_t);

  if (pos > buffer.length()) {
    ENVOY_LOG(debug, "[IOS]Zork Response Bag[CheckIOSRspBag] not completed, len-{} pos-{}",
              buffer.length(), pos);
    return 99999999;
  }
  // pack_len_
  uint16_t bag_len = buffer.peekLEInt<uint16_t>(pos);
  pos += sizeof(uint16_t);
  // body
  pos += bag_len * sizeof(int8_t);
  if (pos > buffer.length()) {
    ENVOY_LOG(debug, "[IOS]Zork Response Bag[CheckIOSRspBag] not completed, len-{} pos-{}",
              buffer.length(), pos);
    return 99999999;
  }
  return pos;
}

uint64_t ZorkHeader::IOSReadCombineHeader(Buffer::Instance& buffer, uint64_t pos_) {

  uint64_t pos = pos_;

  // tag
  pos += sizeof(int8_t);
  // req_type
  pos += sizeof(uint16_t);

  // pack_attrs_
  if (pos > buffer.length()) {
    ENVOY_LOG(debug,
              "[IOS]Zork Request Header[IOSReadCombineHeader] Begin parse combine buffer "
              "pack_attrs_ not enough.buffer len-{} pos-{}",
              buffer.length(), pos);
    return 99999999;
  }
  uint16_t attrs_ = buffer.peekLEInt<uint16_t>(pos);
  // 取attrs低字节的高四位中的高两位,ios 组包逻辑 flag is 01,end flag is 11
  // uint8_t low_byte_pack_attrs = attrs_ >> 6 & 0x3;
  // ios_combine_flag = attrs_ >> 6 & 0x3;
  set_ios_combine_flag(attrs_ >> 6 & 0x3);
  pos += sizeof(uint16_t);

  // len
  if (pos > buffer.length()) {
    ENVOY_LOG(debug,
              "[IOS]Zork Request Header[IOSReadCombineHeader] Begin parse combine buffer len not "
              "enough.buffer len-{} pos-{}",
              buffer.length(), pos);
    return 99999999;
  }
  uint16_t len = buffer.peekLEInt<uint16_t>(pos);
  pos += sizeof(uint16_t);

  // begin to read body num
  pos += (len) * sizeof(int8_t);
  ENVOY_LOG(debug, "Zork Request Header [doReadCombineHeader] len-{} pos-{}", len, pos);
  return pos;
}

const uint16_t ZorkHeader::HEADER_SIZE = 7;
bool ZorkHeader::decodeRequest(Buffer::Instance& buffer) {
  if (buffer.length() < HEADER_SIZE) {
    ENVOY_LOG(error, "Zork Header Request decode buffer.length:{} < {}.", buffer.length(),
              HEADER_SIZE);
    return false;
  }

  uint64_t pos = 0;
  tag_ = buffer.peekLEInt<int8_t>(pos);
  pos += sizeof(int8_t);
  req_type_ = buffer.peekLEInt<uint16_t>(pos);
  pos += sizeof(uint16_t);
  pack_attrs_ = buffer.peekLEInt<uint16_t>(pos);
  pos += sizeof(uint16_t);
  pack_length_ = buffer.peekLEInt<uint16_t>(pos);
  pos += sizeof(uint16_t);

  // 取attrs低字节的高四位中的高两位,ios 组包逻辑 flag is 01,end flag is 11
  // uint8_t low_byte_pack_attrs = pack_attrs_ >> 6 & 0x3;
  // ios_combine_flag = attrs_ >> 6 & 0x3;
  set_ios_combine_flag(pack_attrs_ >> 6 & 0x3);
  pack_attrs_ = pack_attrs_ & 0xF;
  ENVOY_LOG(debug, "Zork Request Header ios_combine_flag-{}", get_ios_combine_flag());
  // ios begin flag is 01,end flag is 11

  // if (buffer.length() < HEADER_SIZE + pack_length_){
  //   ENVOY_LOG(error, "zork Header Request decode,pack_len buffer.length:{} < {}.",
  //   buffer.length(), HEADER_SIZE + pack_length_); return false;
  // }

  // seq_id_ = buffer.peekLEInt<int32_t>(pos);

  // 此处逻辑为兜底逻辑
  // 两种处理逻辑，唯一默认pack_attrs_为0时是打到默认集群
  // 配置文件中配置market_list.此处做遍历匹配
  //  switch (pack_attrs_)
  //  {
  //  case MARKET_VS_FLAG:
  //    pack_attrs_ = MARKET_VS_FLAG;
  //    break;
  //  case MARKET_VS_FLAG_COMPRESS:
  //    pack_attrs_ = MARKET_VS_FLAG_COMPRESS;
  //    break;
  //  case MARKET_VZ_FLAG:
  //    pack_attrs_ = MARKET_VZ_FLAG;
  //    break;
  //  case MARKET_VZ_FLAG_COMPRESS:
  //    pack_attrs_ = MARKET_VZ_FLAG_COMPRESS;
  //    break;
  //  case MARKET_US_FLAG:
  //    pack_attrs_ = MARKET_US_FLAG;
  //    break;
  //  case MARKET_US_FLAG_COMPRESS:
  //    pack_attrs_ = MARKET_US_FLAG_COMPRESS;
  //    break;
  //  case MARKET_UK_FLAG:
  //    pack_attrs_ = MARKET_UK_FLAG;
  //    break;
  //  case MARKET_UK_FLAG_COMPRESS:
  //    pack_attrs_ = MARKET_UK_FLAG_COMPRESS;
  //    break;
  //  case MARKET_VK_FLAG:
  //    pack_attrs_ = MARKET_VK_FLAG;
  //    break;
  //  case MARKET_VK_FLAG_COMPRESS:
  //    pack_attrs_ = MARKET_VK_FLAG_COMPRESS;
  //    break;
  //  default:
  //    pack_attrs_ = MARKET_DEFAULT_FLAG;
  //    break;
  //  }
  // for debug little Endian
  ENVOY_LOG(debug, "Zork Request Header tag_-{} req_type_-{} pack_attrs_-{} pack_length-{} pos-{}",
            tag_, req_type_, pack_attrs_, pack_length_, pos);
  ASSERT(pos == HEADER_SIZE);

  // if(1 == ios_combine_flag){
  //   if (buffer.length() >=  (HEADER_SIZE + pack_length_) ){
  //       pos += (pack_length_) * sizeof(int8_t);
  //       if (pos > buffer.length()){
  //         ENVOY_LOG(debug, "[IOS]Zork Request Header[decodeRequest] Begin parse combine buffer
  //         length not enough.buffer len-{} pos-{}",buffer.length(),pos); return false;
  //       }
  //       //this is next bag first pos.
  //       while (pos <= buffer.length()){
  //           pos = IOSReadCombineHeader(buffer,pos);
  //           if (pos > buffer.length() || pos == 99999999){
  //               ENVOY_LOG(debug, "Zork Request Header[decodeRequest] After doReadCombine buffer
  //               length not enough.buffer len-{} pos-{}",buffer.length(),pos); return false;
  //             }
  //           if ( 3 == ios_combine_flag){
  //             return true;
  //           }
  //       }
  //       if (ios_combine_flag != 3){
  //           return false;
  //       }
  //   }
  // }

  if (tag_ == '{') {
    // for combine bag.
    if (buffer.length() >= (HEADER_SIZE + pack_length_ + 1)) {
      pos += (pack_length_) * sizeof(int8_t);
      if (pos > buffer.length()) {
        ENVOY_LOG(debug,
                  "Zork Request Header[decodeRequest] Begin parse combine buffer length not "
                  "enough.buffer len-{} pos-{}",
                  buffer.length(), pos);
        return false;
      }
      ctag_ = buffer.peekLEInt<int8_t>(pos);
      pos += sizeof(int8_t);
      ENVOY_LOG(debug, "Zork Request Header[decodeRequest] ctag_-{} pos-{}", ctag_, pos);
      // if ctag_ is } means that is not a combine bag,to deal it in body.
      //  if (ctag_ == '}'){
      //    return true;
      //  }
      // if ctag_ is : means that is a combine bag.header not contain tag field.
      if (ctag_ == ':') {
        // this pos is at the beginning of combination single bag.
        // ENVOY_LOG(debug, "Zork Request Header[decodeRequest] ctag_ innnnnnnnnnnnnnnnnnnnn!");
        int8_t tmp_ctag = 0;
        while (pos <= buffer.length()) {
          pos = doReadCombineHeader(buffer, pos);
          if (pos > buffer.length() || pos == 99999999) {
            ENVOY_LOG(debug,
                      "Zork Request Header[decodeRequest] After doReadCombine buffer length not "
                      "enough.buffer len-{} pos-{}",
                      buffer.length(), pos);
            return false;
          }
          tmp_ctag = buffer.peekLEInt<int8_t>(pos);
          // ENVOY_LOG(debug, "Zork Request Header[decodeRequest-doReadCombineHeader] out pos-{}
          // tmp_ctag-{}",pos,tmp_ctag);
          if (tmp_ctag == '}') {
            // ENVOY_LOG(debug, "Zork Request Header[decodeRequest] ctag_
            // completeddddddddddddddddd!");
            pos += sizeof(int8_t);
            set_combine_pack_len(pos);
            break;
          }
          pos += sizeof(int8_t);
        }
        // if (pos >= buffer.length()){
        //   return false;
        // }
        if (tmp_ctag != '}') {
          return false;
        }
      }
    } else {
      return false;
    }
  } else {
    if (buffer.length() < (HEADER_SIZE + pack_length_)) {
      return false;
    }
    if (1 == get_ios_combine_flag()) {
      ENVOY_LOG(debug, "Enter IOS COMBINE BAG LOGIC!");
      pos += (pack_length_) * sizeof(int8_t);
      if (pos > buffer.length()) {
        ENVOY_LOG(debug,
                  "[IOS]Zork Request Header[decodeRequest] Begin parse combine buffer length not "
                  "enough.buffer len-{} pos-{}",
                  buffer.length(), pos);
        return false;
      }
      // this is next bag first pos.
      while (pos <= buffer.length()) {
        pos = IOSReadCombineHeader(buffer, pos);
        if (pos > buffer.length() || pos == 99999999) {
          ENVOY_LOG(debug,
                    "Zork Request Header[decodeRequest] After doReadCombine buffer length not "
                    "enough.buffer len-{} pos-{}",
                    buffer.length(), pos);
          return false;
        }
        if (3 == get_ios_combine_flag()) {
          ENVOY_LOG(debug, "Zork Request Header[decodeRequest] IOS COMPLETED!.buffer len-{} pos-{}",
                    buffer.length(), pos);
          set_combine_pack_len(pos);
          return true;
        }
      }
      if (get_ios_combine_flag() != 3) {
        return false;
      }
    }
  }

  return true;
}

bool ZorkHeader::decodeResponse(Buffer::Instance& buffer) {
  if (buffer.length() < HEADER_SIZE) {
    ENVOY_LOG(error, "Zork Header Response decode buffer.length:{} < {}.", buffer.length(),
              HEADER_SIZE);
    return false;
  }

  uint64_t pos = 0;
  tag_ = buffer.peekLEInt<int8_t>(pos);
  pos += sizeof(int8_t);
  req_type_ = buffer.peekLEInt<uint16_t>(pos);
  pos += sizeof(uint16_t);
  pack_attrs_ = buffer.peekLEInt<uint16_t>(pos);
  pos += sizeof(uint16_t);
  pack_length_ = buffer.peekLEInt<uint16_t>(pos);
  pos += sizeof(uint16_t);

  set_ios_combine_flag(pack_attrs_ >> 6 & 0x3);
  pack_attrs_ = pack_attrs_ & 0xF;
  // seq_id_ = buffer.peekLEInt<int32_t>(pos);

  // switch (pack_attrs_)
  // {
  // case MARKET_VS_FLAG:
  //   pack_attrs_ = MARKET_VS_FLAG;
  //   break;
  // case MARKET_VS_FLAG_COMPRESS:
  //   pack_attrs_ = MARKET_VS_FLAG_COMPRESS;
  //   break;
  // case MARKET_VZ_FLAG:
  //   pack_attrs_ = MARKET_VZ_FLAG;
  //   break;
  // case MARKET_VZ_FLAG_COMPRESS:
  //   pack_attrs_ = MARKET_VZ_FLAG_COMPRESS;
  //   break;
  // case MARKET_US_FLAG:
  //   pack_attrs_ = MARKET_US_FLAG;
  //   break;
  // case MARKET_US_FLAG_COMPRESS:
  //   pack_attrs_ = MARKET_US_FLAG_COMPRESS;
  //   break;
  // case MARKET_UK_FLAG:
  //   pack_attrs_ = MARKET_UK_FLAG;
  //   break;
  // case MARKET_UK_FLAG_COMPRESS:
  //   pack_attrs_ = MARKET_UK_FLAG_COMPRESS;
  //   break;
  // case MARKET_VK_FLAG:
  //   pack_attrs_ = MARKET_VK_FLAG;
  //   break;
  // case MARKET_VK_FLAG_COMPRESS:
  //   pack_attrs_ = MARKET_VK_FLAG_COMPRESS;
  //   break;
  // default:
  //   pack_attrs_ = MARKET_DEFAULT_FLAG;
  //   break;
  // }
  // for debug little Endian
  ENVOY_LOG(debug, "Zork Response Header tag_-{} req_type_-{} pack_attrs_-{} pack_length-{} pos-{}",
            tag_, req_type_, pack_attrs_, pack_length_, pos);

  // if (buffer.length() < HEADER_SIZE + pack_length_){
  //	  ENVOY_LOG(error, "zork Header Request decode,pack_len buffer.length:{} < {}.",
  //buffer.length(), HEADER_SIZE + pack_length_); 	  return false;
  // }

  ASSERT(pos == HEADER_SIZE);

  if (tag_ == '{') {
    // for combine bag.
    if (buffer.length() >= (HEADER_SIZE + pack_length_ + 1)) {
      pos += (pack_length_) * sizeof(int8_t);
      if (pos > buffer.length()) {
        ENVOY_LOG(debug,
                  "Zork Response Header[decodeResponse] Begin parse combine buffer length not "
                  "enough.buffer len-{} pos-{}",
                  buffer.length(), pos);
        return false;
      }
      ctag_ = buffer.peekLEInt<int8_t>(pos);
      pos += sizeof(int8_t);
      ENVOY_LOG(debug, "Zork Response Header[decodeResponse] ctag_-{} pos-{}", ctag_, pos);
      // if ctag_ is } means that is not a combine bag,to deal it in body.
      //  if (ctag_ == '}'){
      //    return true;
      //  }
      // if ctag_ is : means that is a combine bag.header not contain tag field.
      if (ctag_ == ':') {
        // this pos is at the beginning of combination single bag.
        // ENVOY_LOG(debug, "Zork Response Header[decodeResponse] ctag_ innnnnnnnnnnnnnnnnnnnn!");
        int8_t tmp_ctag = 0;
        while (pos <= buffer.length()) {
          pos = doReadCombineHeader(buffer, pos);
          if (pos > buffer.length() || pos == 99999999) {
            ENVOY_LOG(debug,
                      "Zork Response Header[decodeResponse] After doReadCombine buffer length not "
                      "enough.buffer len-{} pos-{}",
                      buffer.length(), pos);
            return false;
          }
          tmp_ctag = buffer.peekLEInt<int8_t>(pos);
          ENVOY_LOG(
              debug,
              "Zork Response Header[decodeResponse-doReadCombineHeader] out pos-{} tmp_ctag-{}",
              pos, tmp_ctag);
          if (tmp_ctag == '}') {
            // ENVOY_LOG(debug, "Zork Response Header[decodeResponse] ctag_
            // completeddddddddddddddddd!");
            pos += sizeof(int8_t);
            set_combine_pack_len(pos);
            break;
          }
          pos += sizeof(int8_t);
        }
        // if (pos >= buffer.length()){
        //   return false;
        // }
        if (tmp_ctag != '}') {
          return false;
        }
      }
    } else {
      return false;
    }
  } else {
    if (buffer.length() < (HEADER_SIZE + pack_length_)) {
      return false;
    }
    // skip first bag len
    pos += pack_length_ * sizeof(int8_t);
    while (pos < buffer.length()) {
      pos = CheckIOSRspBag(buffer, pos);
      if (pos == buffer.length()) {
        return true;
      }
    }
    if (pos != buffer.length()) {
      return false;
    }
  }

  return true;
}

bool ZorkHeader::encodeRequest(Buffer::Instance& buffer) {
  // buffer.writeLEInt<int8_t>(tag_);
  // buffer.writeLEInt<uint16_t>(req_type_);
  // buffer.writeLEInt<uint16_t>(pack_attrs_);
  // buffer.writeLEInt<uint16_t>(sizeof(uint16_t));
  // buffer.writeLEInt<uint16_t>(rsp_code_);
  (void)buffer;
  return true;
}

bool ZorkHeader::encodeResponse(Buffer::Instance& buffer) {
  // buffer.writeLEInt<int8_t>(tag_);
  // buffer.writeLEInt<uint16_t>(req_type_);
  // buffer.writeLEInt<uint16_t>(pack_attrs_);
  // buffer.writeLEInt<uint16_t>(sizeof(uint16_t));
  // buffer.writeLEInt<uint16_t>(rsp_code_);
  (void)buffer;
  return true;
}

} // namespace Zork
} // namespace MetaProtocolProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
