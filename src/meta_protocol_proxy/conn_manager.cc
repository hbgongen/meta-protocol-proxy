#include "src/meta_protocol_proxy/conn_manager.h"

#include <cstdint>

#include "envoy/common/exception.h"

#include "source/common/common/fmt.h"
#include "src/meta_protocol_proxy/app_exception.h"
#include "src/meta_protocol_proxy/heartbeat_response.h"
#include "src/meta_protocol_proxy/codec_impl.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MetaProtocolProxy {

constexpr uint32_t BufferLimit = UINT32_MAX;

ConnectionManager::ConnectionManager(Config& config, Random::RandomGenerator& random_generator,
                                     TimeSource& time_system)
    : config_(config), time_system_(time_system), stats_(config_.stats()),
      random_generator_(random_generator), codec_(config.createCodec()),
      decoder_(std::make_unique<RequestDecoder>(codec_, *this)) {}

Network::FilterStatus ConnectionManager::onData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(debug, "meta protocol: read {} bytes", data.length());
  request_buffer_.move(data);
  dispatch();

  if (end_stream) {
    ENVOY_CONN_LOG(debug, "meta protocol: downstream connection has been closed",
                   read_callbacks_->connection());

    resetAllMessages(false);
    clearStream();
    read_callbacks_->connection().close(Network::ConnectionCloseType::FlushWrite);
  }

  return Network::FilterStatus::StopIteration;
}

Network::FilterStatus ConnectionManager::onNewConnection() {
  //init idle timer.
  if(config_.idleTimeout()){
    idle_timer_ = read_callbacks_->connection().dispatcher().createTimer(
        [this]() { this->onIdleTimeout(); });  
    resetIdleTimer();           
  }
  return Network::FilterStatus::Continue;
}

void ConnectionManager::initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) {
  read_callbacks_ = &callbacks;
  read_callbacks_->connection().addConnectionCallbacks(*this);
  read_callbacks_->connection().enableHalfClose(true);
  read_callbacks_->connection().setBufferLimits(BufferLimit);
}

void ConnectionManager::onEvent(Network::ConnectionEvent event) {
  if (event == Network::ConnectionEvent::LocalClose) {
    disableIdleTimer();
    resetAllMessages(true);
  } else if (event == Network::ConnectionEvent::RemoteClose) {
    disableIdleTimer();
    resetAllMessages(false);
  }
}

void ConnectionManager::onAboveWriteBufferHighWatermark() {
  ENVOY_CONN_LOG(debug, "onAboveWriteBufferHighWatermark", read_callbacks_->connection());
  read_callbacks_->connection().readDisable(true);
}

void ConnectionManager::onBelowWriteBufferLowWatermark() {
  ENVOY_CONN_LOG(debug, "onBelowWriteBufferLowWatermark", read_callbacks_->connection());
  read_callbacks_->connection().readDisable(false);
}

MessageHandler& ConnectionManager::newMessageHandler() {
  ENVOY_LOG(debug, "meta protocol: create the new decoder event handler");

  ActiveMessagePtr new_message(std::make_unique<ActiveMessage>(*this));
  new_message->createFilterChain();
  LinkedList::moveIntoList(std::move(new_message), active_message_list_);
  return **active_message_list_.begin();
}

void ConnectionManager::onHeartbeat(MetadataSharedPtr metadata) {
  stats_.request_event_.inc();

  if (read_callbacks_->connection().state() != Network::Connection::State::Open) {
    ENVOY_LOG(warn, "meta protocol: downstream connection is closed or closing");
    return;
  }

  HeartbeatResponse heartbeat;
  Buffer::OwnedImpl response_buffer;

  heartbeat.encode(*metadata, codec_, response_buffer);
  read_callbacks_->connection().write(response_buffer, false);
}

void ConnectionManager::dispatch() {
  if (0 == request_buffer_.length()) {
    ENVOY_LOG(debug, "meta protocol: it's empty data");
    return;
  }
  //when data is not empty,it will enable timer again.
  resetIdleTimer();
  try {
    bool underflow = false;
    // decoder return underflow in th following two cases:
    // 1. decoder needs more data to complete the decoding of the current message, in this case, the buffer contains
    // part of the incomplete message.
    // 2. all the messages in the buffer have been processed, in this case, the buffer is already empty.
    while (!underflow) {
      decoder_->onData(request_buffer_, underflow);
    }
    return;
  } catch (const EnvoyException& ex) {
    ENVOY_CONN_LOG(error, "meta protocol error: {}", read_callbacks_->connection(), ex.what());
    read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
    stats_.request_decoding_error_.inc();
  }
  resetAllMessages(true);
}

void ConnectionManager::sendLocalReply(Metadata& metadata, const DirectResponse& response,
                                       bool end_stream) {
  if (read_callbacks_->connection().state() != Network::Connection::State::Open) {
    return;
  }

  DirectResponse::ResponseType result = DirectResponse::ResponseType::ErrorReply;

  try {
    Buffer::OwnedImpl buffer;
    result = response.encode(metadata, codec_, buffer);

    read_callbacks_->connection().write(buffer, end_stream);
  } catch (const EnvoyException& ex) {
    ENVOY_CONN_LOG(error, "meta protocol error: {}", read_callbacks_->connection(), ex.what());
  }

  if (end_stream) {
    read_callbacks_->connection().close(Network::ConnectionCloseType::FlushWrite);
  }

  switch (result) {
  case DirectResponse::ResponseType::SuccessReply:
    stats_.local_response_success_.inc();
    break;
  case DirectResponse::ResponseType::ErrorReply:
    stats_.local_response_error_.inc();
    break;
  case DirectResponse::ResponseType::Exception:
    stats_.local_response_business_exception_.inc();
    break;
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

Stream& ConnectionManager::newActiveStream(uint64_t stream_id) {
  ENVOY_CONN_LOG(debug, "meta protocol: create an active stream: {}", connection(), stream_id);
  StreamPtr new_stream(std::make_unique<Stream>(stream_id, connection(), *this, codec_));
  active_stream_map_[stream_id] = std::move(new_stream);
  return *active_stream_map_.find(stream_id)->second;
}

Stream& ConnectionManager::getActiveStream(uint64_t stream_id) {
  auto iter = active_stream_map_.find(stream_id);
  ASSERT(iter != active_stream_map_.end());
  return *iter->second;
}

bool ConnectionManager::streamExisted(uint64_t stream_id) {
  auto iter = active_stream_map_.find(stream_id);
  return (iter != active_stream_map_.end());
}

void ConnectionManager::closeStream(uint64_t stream_id) {
  ENVOY_LOG(debug, "meta protocol: close stream {} ", stream_id);
  active_stream_map_.erase(stream_id);
}

void ConnectionManager::deferredDeleteMessage(ActiveMessage& message) {
  if (!message.inserted()) {
    return;
  }
  ENVOY_LOG(debug, "meta protocol: deferred delete message, id is {}",
            message.metadata()->getRequestId());
  read_callbacks_->connection().dispatcher().deferredDelete(
      message.removeFromList(active_message_list_));
}

void ConnectionManager::resetAllMessages(bool local_reset) {
  while (!active_message_list_.empty()) {
    if (local_reset) {
      ENVOY_CONN_LOG(debug, "local close with active request", read_callbacks_->connection());
      stats_.cx_destroy_local_with_active_rq_.inc();
    } else {
      ENVOY_CONN_LOG(debug, "remote close with active request", read_callbacks_->connection());
      stats_.cx_destroy_remote_with_active_rq_.inc();
    }

    active_message_list_.front()->onReset();
  }
}

void ConnectionManager::onIdleTimeout() {
  ENVOY_CONN_LOG(debug, "meta protocol:Session timed out", read_callbacks_->connection());
  stats_.idle_timeout_.inc();
  resetAllMessages(true);
  clearStream();
  read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
}

void ConnectionManager::resetIdleTimer() {
  if (idle_timer_ != nullptr) {
    ASSERT(config_.idleTimeout());
    idle_timer_->enableTimer(config_.idleTimeout().value());
  }
}

void ConnectionManager::disableIdleTimer() {
  if (idle_timer_ != nullptr) {
    idle_timer_->disableTimer();
    idle_timer_.reset();
  }
}

} // namespace  MetaProtocolProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
