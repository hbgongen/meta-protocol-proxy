#include <any>

#include "envoy/buffer/buffer.h"

#include "source/common/common/logger.h"

#include "src/meta_protocol_proxy/codec/codec.h"
#include "src/application_protocols/zork/zork_codec.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MetaProtocolProxy {
namespace Zork {

MetaProtocolProxy::DecodeStatus ZorkCodec::decode(Buffer::Instance& buffer,
                                                  MetaProtocolProxy::Metadata& metadata) {
  //ENVOY_LOG(debug, "Zork decode: {} bytes available, msg type: {}", buffer.length(), metadata.getMessageType());
  messageType_ = metadata.getMessageType();
  ASSERT(messageType_ == MetaProtocolProxy::MessageType::Request ||
         messageType_ == MetaProtocolProxy::MessageType::Response);

    

      if (messageType_ == MetaProtocolProxy::MessageType::Request){
         while (decode_status_request != ZorkDecodeStatus::DecodeDone) {
            ZorkDecodeStatus nextState_request = handleRequestState(buffer);
        if (nextState_request == ZorkDecodeStatus::WaitForData) {
          ENVOY_LOG(debug, "ZorkCodec handleState is wait for data.");
          metadata.putString("decodestatus","Request WaitForData");
          return DecodeStatus::WaitForData;
        }
        decode_status_request = nextState_request;
      }
        metadata.putString("decodestatus","Request Done");
        toMetadata(metadata);
        decode_status_request = ZorkDecodeStatus::DecodeHeader;
      }else if(messageType_ == MetaProtocolProxy::MessageType::Response){
         while (decode_status_response != ZorkDecodeStatus::DecodeResponseDone) {
            ZorkDecodeStatus nextState_response = handleResponseState(buffer);
        if (nextState_response == ZorkDecodeStatus::ResponseWaitForData) {
          ENVOY_LOG(debug, "ZorkCodec handleState is wait for data.");
          metadata.putString("decodestatus","Response WaitForData");
          return DecodeStatus::WaitForData;
        }
        decode_status_response = nextState_response;
      }
        metadata.putString("decodestatus","Response Done");
        toRespMetadata(metadata);
        decode_status_response = ZorkDecodeStatus::DecodeResponseHeader;
      }else{
            //todo.
            //ENVOY_LOG(debug, "It's a {} type.",messageType_);
      }

      
      return DecodeStatus::Done;
}

void ZorkCodec::encode(const MetaProtocolProxy::Metadata& ,
                      const MetaProtocolProxy::Mutation& , Buffer::Instance& ) {
  // TODO we don't need to implement encode for now.
  // This method only need to be implemented if we want to modify the respose message
  // (void)metadata;
  // (void)mutation;
  // (void)buffer;
}

void ZorkCodec::onError(const MetaProtocolProxy::Metadata& metadata,
                        const MetaProtocolProxy::Error& error, Buffer::Instance& buffer) {
   (void)metadata;
   (void)error;
   (void)buffer;                   
  // ZorkHeader response;
  // // Make sure to set the request id if the application protocol has one, otherwise MetaProtocol framework will 
  // // complaint that the id in the error response is not equal to the one in the request
  // //response.set_seq_id(metadata.getRequestId());
  // response.set_pack_len(ZorkHeader::HEADER_SIZE);
  // switch (error.type) {
  // case MetaProtocolProxy::ErrorType::RouteNotFound:
  //   response.set_pack_attrs(static_cast<int16_t>(ZorkCode::NoRoute));
  //   break;
  // default:
  //   response.set_pack_attrs(static_cast<int16_t>(ZorkCode::Error));
  //   break;
  // }
  // response.encode(buffer);
}

ZorkDecodeStatus ZorkCodec::handleRequestState(Buffer::Instance& buffer) {
  switch (decode_status_request)
  {
  case ZorkDecodeStatus::DecodeHeader:
    return decodeRequestHeader(buffer);
  case ZorkDecodeStatus::DecodePayload:
    return decodeRequestBody(buffer);
  default:
    PANIC("not reached");
  }
  return ZorkDecodeStatus::DecodeDone;
}

ZorkDecodeStatus ZorkCodec::handleResponseState(Buffer::Instance& buffer) {
  switch (decode_status_response)
  {
  case ZorkDecodeStatus::DecodeResponseHeader:
    return decodeResponseHeader(buffer);
  case ZorkDecodeStatus::DecodeResponsePayload:
    return decodeResponseBody(buffer);
  default:
    PANIC("not reached");
  }
  return ZorkDecodeStatus::DecodeResponseDone;
}

ZorkDecodeStatus ZorkCodec::decodeRequestHeader(Buffer::Instance& buffer) {
  //ENVOY_LOG(debug, "decode zork request header: {}", buffer.length());
  // Wait for more data if the header is not complete
  if (!zork_header_request_.decodeRequest(buffer)) {
      ENVOY_LOG(debug, "request header continue {}", buffer.length());
      //buffer.drain(buffer.length());
    return ZorkDecodeStatus::WaitForData;
  }

  // if(!zork_header_request_.decodeRequest(buffer)) {
  //   buffer.drain(buffer.length());
  //   //throw EnvoyException(fmt::format("zork request header invalid"));
  //   return ZorkDecodeStatus::WaitForData;
  // }

  

  //ENVOY_LOG(debug, "decode request header over,buffer length is {}", buffer.length());
  return ZorkDecodeStatus::DecodePayload;

}

ZorkDecodeStatus ZorkCodec::decodeResponseHeader(Buffer::Instance& buffer) {
  //ENVOY_LOG(debug, "decode zork response header: {}", buffer.length());
  // Wait for more data if the header is not complete
  // if (buffer.length() < ZorkHeader::HEADER_SIZE) {
  //     ENVOY_LOG(debug, "response header continue {}", buffer.length());
  //   return ZorkDecodeStatus::ResponseWaitForData;
  // }

  if(!zork_header_response_.decodeResponse(buffer)) {
    ENVOY_LOG(debug, "response header continue {}", buffer.length());
    return ZorkDecodeStatus::ResponseWaitForData;
  }

  

  //ENVOY_LOG(debug, "decode response header over,buffer length is {}", buffer.length());
  return ZorkDecodeStatus::DecodeResponsePayload;

}

ZorkDecodeStatus ZorkCodec::decodeRequestBody(Buffer::Instance& buffer) {
  // Wait for more data if the buffer is not a complete message	
  
  // if (zork_header_request_.get_tag() == '{'){  //123-{
	//       if (buffer.length() < ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len()+1) {
	// 		  ENVOY_LOG(debug, "decodeRequestBody-{}< header_size-{} packet_len-{}+1", buffer.length(),ZorkHeader::HEADER_SIZE,zork_header_request_.get_pack_len()+1);
	// 	          return ZorkDecodeStatus::WaitForData;
	//       }
  //       // if (zork_header_request_.get_ctag() != '}'){
  //       //   ENVOY_LOG(debug,"decodeRequestBody first is { but last is not },data is not comleted!");
  //       //   return ZorkDecodeStatus::WaitForData;
  //       // }
	//      origin_msg_request_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len()+1);
  // }else{
  //      if (buffer.length() < ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len()) {
	// 		  ENVOY_LOG(debug, "decodeRequestBody-{}< header_size-{} packet_len-{}+1", buffer.length(),ZorkHeader::HEADER_SIZE,zork_header_request_.get_pack_len()+1);
	// 	          return ZorkDecodeStatus::WaitForData;
	//       }
  //       // if (zork_header_request_.get_ctag() != '}'){
  //       //   ENVOY_LOG(debug,"decodeRequestBody first is { but last is not },data is not comleted!");
  //       //   return ZorkDecodeStatus::WaitForData;
  //       // }
	//      origin_msg_request_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len()+1);
  // }

  
  

  if (zork_header_request_.get_ctag() == ':' || zork_header_request_.get_ios_combine_flag() == 3){
    //coming_buffer_len = buffer.length();
    coming_buffer_len = zork_header_request_.get_combine_pack_len();
    //origin_msg_request_.move(buffer);
    origin_msg_request_.move(buffer,coming_buffer_len);
  }else if(zork_header_request_.get_tag() == '{'){
    coming_buffer_len = ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len()+1;
    origin_msg_request_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len()+1);
  }else{
    coming_buffer_len = ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len();
    origin_msg_request_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len());
  }
  
  //origin_msg_request_ = std::make_unique<Buffer::OwnedImpl>();
  
  // move the decoded message out of the buffer
  //origin_msg_request_->move(buffer);
  //origin_msg_request_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len());
  return ZorkDecodeStatus::DecodeDone;
}

ZorkDecodeStatus ZorkCodec::decodeResponseBody(Buffer::Instance& buffer) {
  // Wait for more data if the buffer is not a complete message
  // if (zork_header_response_.get_tag() == 123){  //123-{
  // 	if (buffer.length() < ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len()+1) {
	//   ENVOY_LOG(debug, "decodeResponseBody-{}< header_size-{} packet_len-{}+1", buffer.length(),ZorkHeader::HEADER_SIZE,zork_header_response_.get_pack_len()+1);
  //         return ZorkDecodeStatus::ResponseWaitForData;
	// }
	// origin_msg_response_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len()+1);
  // }else{ 
  // 	if (buffer.length() < ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len()) {
  //   		ENVOY_LOG(debug, "decodeResponseBody-{}< header_size-{} packet_len-{}", buffer.length(),ZorkHeader::HEADER_SIZE,zork_header_response_.get_pack_len());
  //  		 return ZorkDecodeStatus::ResponseWaitForData;
  // 	}
	// origin_msg_response_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len());
  // }
  // if (zork_header_response_.get_tag() == '{'){  //123-{
	//       if (buffer.length() < ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len()+1) {
	// 		  ENVOY_LOG(debug, "decodeResponseBody-{}< header_size-{} packet_len-{}+1", buffer.length(),ZorkHeader::HEADER_SIZE,zork_header_response_.get_pack_len()+1);
	// 	          return ZorkDecodeStatus::ResponseWaitForData;
	//       }
  //       if (zork_header_response_.get_ctag() != '}'){
  //         ENVOY_LOG(debug,"decodeResponseBody first is { but last is not },data is not comleted!");
  //         return ZorkDecodeStatus::ResponseWaitForData;
  //       }
	//      //origin_msg_request_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len()+1);
  // }else{
 	//  if (buffer.length() < ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len()) {
  //  	 ENVOY_LOG(debug, "decodeResponseBody-{}< header_size-{} packet_len-{}", buffer.length(),ZorkHeader::HEADER_SIZE,zork_header_response_.get_pack_len());
  //  	 return ZorkDecodeStatus::ResponseWaitForData;
  // 	}
	//  //origin_msg_request_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_request_.get_pack_len());
  // }
  //冒泡排序
  
  //origin_msg_response_ = std::make_unique<Buffer::OwnedImpl>();
  if (zork_header_response_.get_ctag() == ':'){
    //coming_buffer_len = buffer.length();
    coming_buffer_len = zork_header_response_.get_combine_pack_len();
    //origin_msg_response_.move(buffer);
    origin_msg_response_.move(buffer,coming_buffer_len);
  }else if(zork_header_response_.get_tag() == '{'){
    coming_buffer_len = ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len()+1;
    origin_msg_response_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len()+1);
  }else{
    // coming_buffer_len = ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len();
    // origin_msg_response_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len());

    coming_buffer_len = buffer.length();
    origin_msg_response_.move(buffer);
  }

  // move the decoded message out of the buffer
  //origin_msg_response_->move(buffer);
  //origin_msg_response_.move(buffer,ZorkHeader::HEADER_SIZE + zork_header_response_.get_pack_len());
  return ZorkDecodeStatus::DecodeResponseDone;
}

void ZorkCodec::toMetadata(MetaProtocolProxy::Metadata& metadata) {
    metadata.putString("market", std::to_string(zork_header_request_.get_pack_attrs()));
    metadata.putString("type", std::to_string(zork_header_request_.get_req_type()));
  //   metadata.setHeaderSize(ZorkHeader::HEADER_SIZE);
  // if (zork_header_request_.get_tag() == '{'){
	// 	metadata.setBodySize(zork_header_request_.get_pack_len()+1);
	// }else{
	// 	metadata.setBodySize(zork_header_request_.get_pack_len());
	// }
  //   metadata.originMessage().move(origin_msg_request_);
  metadata.setBodySize(coming_buffer_len);
  metadata.originMessage().move(origin_msg_request_);
}

void ZorkCodec::toRespMetadata(MetaProtocolProxy::Metadata& metadata) {
  //   metadata.setHeaderSize(ZorkHeader::HEADER_SIZE);
  //       if (zork_header_response_.get_tag() == '{'){
  // 		metadata.setBodySize(zork_header_response_.get_pack_len()+1);
	// }else{
	// 	metadata.setBodySize(zork_header_response_.get_pack_len());
	// }
  //   metadata.originMessage().move(origin_msg_response_);
  metadata.setBodySize(coming_buffer_len);
  metadata.originMessage().move(origin_msg_response_);
}



} // namespace Zork
} // namespace MetaProtocolProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
