#pragma once

#include <string>

#include "api/meta_protocol_proxy/v1alpha/meta_protocol_proxy.pb.h"
#include "api/meta_protocol_proxy/v1alpha/meta_protocol_proxy.pb.validate.h"

#include "source/extensions/filters/network/common/factory_base.h"
#include "source/extensions/filters/network/well_known_names.h"
#include "src/meta_protocol_proxy/conn_manager.h"
#include "src/meta_protocol_proxy/filters/filter.h"
#include "src/meta_protocol_proxy/route/route_config_provider_manager.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MetaProtocolProxy {

constexpr char CanonicalName[] = "aeraki.meta_protocol_proxy";

/**
 * Config registration for the meta protocol proxy filter. @see NamedNetworkFilterConfigFactory.
 */
class MetaProtocolProxyFilterConfigFactory
    : public Common::FactoryBase<aeraki::meta_protocol_proxy::v1alpha::MetaProtocolProxy> {
public:
  MetaProtocolProxyFilterConfigFactory() : FactoryBase(CanonicalName, true) {}

private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const aeraki::meta_protocol_proxy::v1alpha::MetaProtocolProxy& proto_config,
      Server::Configuration::FactoryContext& context) override;
};

/**
 * Utility class for shared logic between meta protocol connection manager factories.
 */
class Utility {
public:
  struct Singletons {
    Route::RouteConfigProviderManagerSharedPtr route_config_provider_manager_;
  };

  /**
   * Create/get singletons needed for config creation.
   *
   * @param context supplies the context used to create the singletons.
   * @return Singletons struct containing all the singletons.
   */
  static Singletons createSingletons(Server::Configuration::FactoryContext& context);
};

class ConfigImpl : public Config,
                   public Route::Config,
                   public FilterChainFactory,
                   Logger::Loggable<Logger::Id::config> {
public:
  using MetaProtocolProxyConfig = aeraki::meta_protocol_proxy::v1alpha::MetaProtocolProxy;
  using MetaProtocolFilterConfig = aeraki::meta_protocol_proxy::v1alpha::MetaProtocolFilter;
  using CodecConfig = aeraki::meta_protocol_proxy::v1alpha::Codec;

  ConfigImpl(const MetaProtocolProxyConfig& config, Server::Configuration::FactoryContext& context,
             Route::RouteConfigProviderManager& route_config_provider_manager);
  ~ConfigImpl() override {
    ENVOY_LOG(trace, "********** MetaProtocolProxy ConfigImpl destructed ***********");
    codec_map_.clear();
  }

  // FilterChainFactory
  void createFilterChain(FilterChainFactoryCallbacks& callbacks) override;

  Route::RouteConfigProvider* routeConfigProvider() override {
    return route_config_provider_.get();
  }

  // Route::Config
  Route::RouteConstSharedPtr route(const Metadata& metadata, uint64_t random_value) const override;

  // Config
  MetaProtocolProxyStats& stats() override { return stats_; }
  FilterChainFactory& filterFactory() override { return *this; }
  Route::Config& routerConfig() override { return *this; }
  Codec& createCodec() override;
  std::string applicationProtocol() override { return application_protocol_; };
  absl::optional<std::chrono::milliseconds> idleTimeout() override { return idle_timeout_;};
private:
  void registerFilter(const MetaProtocolFilterConfig& proto_config);

  Server::Configuration::FactoryContext& context_;
  const std::string stats_prefix_;
  MetaProtocolProxyStats stats_;
  // Router::RouteMatcherPtr route_matcher_;
  std::string application_protocol_;
  CodecConfig codecConfig_;
  std::list<FilterFactoryCb> filter_factories_;
  Route::RouteConfigProviderSharedPtr route_config_provider_;
  Route::RouteConfigProviderManager& route_config_provider_manager_;
  std::map<std::string, CodecPtr> codec_map_;
  absl::optional<std::chrono::milliseconds> idle_timeout_;
};

} // namespace MetaProtocolProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
