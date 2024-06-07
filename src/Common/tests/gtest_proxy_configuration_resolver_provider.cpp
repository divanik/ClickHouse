#include <gtest/gtest.h>

#include <Common/ProxyConfigurationResolverProvider.h>
#include <Common/RemoteProxyConfigurationResolver.h>
#include <Common/ProxyListConfigurationResolver.h>
#include <Common/EnvironmentProxyConfigurationResolver.h>
#include <Common/tests/gtest_global_context.h>
#include <Common/tests/gtest_helper_functions.h>

#include <Poco/Util/MapConfiguration.h>

using ConfigurationPtr = Poco::AutoPtr<Poco::Util::AbstractConfiguration>;

class ProxyConfigurationResolverProviderTests : public ::testing::Test
{
protected:

    static void SetUpTestSuite() {
        context = getContext().context;
    }

    static void TearDownTestSuite() {
        context->setConfig(Poco::AutoPtr(new Poco::Util::MapConfiguration()));
    }

    static DB::ContextMutablePtr context;
};

DB::ContextMutablePtr ProxyConfigurationResolverProviderTests::context;

Poco::URI http_env_proxy_server = Poco::URI("http://http_environment_proxy:3128");
Poco::URI https_env_proxy_server = Poco::URI("http://https_environment_proxy:3128");

Poco::URI http_list_proxy_server = Poco::URI("http://http_list_proxy:3128");
Poco::URI https_list_proxy_server = Poco::URI("http://https_list_proxy:3128");

// Some other tests rely on HTTP clients (e.g, gtest_aws_s3_client), which depend on proxy configuration
// since in https://github.com/ClickHouse/ClickHouse/pull/63314 the environment proxy resolver reads only once
// from the environment, the proxy configuration will always be there.
// The problem is that the proxy server does not exist, causing the test to fail.
// To work around this issue, `no_proxy` is set to bypass all domains.
static std::string no_proxy_hosts = "*";

TEST_F(ProxyConfigurationResolverProviderTests, EnvironmentResolverShouldBeUsedIfNoSettings)
{
    EnvironmentProxySetter setter(http_env_proxy_server, https_env_proxy_server, no_proxy_hosts);
    const auto & config = getContext().context->getConfigRef();

    auto http_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTP, config);
    auto https_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTPS, config);

    ASSERT_TRUE(std::dynamic_pointer_cast<DB::EnvironmentProxyConfigurationResolver>(http_resolver));
    ASSERT_TRUE(std::dynamic_pointer_cast<DB::EnvironmentProxyConfigurationResolver>(https_resolver));
}

TEST_F(ProxyConfigurationResolverProviderTests, ListHTTPOnly)
{
    ConfigurationPtr config = Poco::AutoPtr(new Poco::Util::MapConfiguration());

    config->setString("proxy", "");
    config->setString("proxy.no_proxy", no_proxy_hosts);
    config->setString("proxy.http", "");
    config->setString("proxy.http.uri", http_list_proxy_server.toString());
    context->setConfig(config);

    auto http_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTP, *config);
    auto https_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTPS, *config);

    ASSERT_TRUE(std::dynamic_pointer_cast<DB::ProxyListConfigurationResolver>(http_resolver));
    ASSERT_TRUE(std::dynamic_pointer_cast<DB::EnvironmentProxyConfigurationResolver>(https_resolver));
}

TEST_F(ProxyConfigurationResolverProviderTests, ListHTTPSOnly)
{
    ConfigurationPtr config = Poco::AutoPtr(new Poco::Util::MapConfiguration());

    config->setString("proxy.no_proxy", no_proxy_hosts);
    config->setString("proxy", "");
    config->setString("proxy.https", "");
    config->setString("proxy.https.uri", https_list_proxy_server.toString());
    context->setConfig(config);

    auto http_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTP, *config);
    auto https_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTPS, *config);

    ASSERT_TRUE(std::dynamic_pointer_cast<DB::EnvironmentProxyConfigurationResolver>(http_resolver));
    ASSERT_TRUE(std::dynamic_pointer_cast<DB::ProxyListConfigurationResolver>(https_resolver));
}

TEST_F(ProxyConfigurationResolverProviderTests, ListBoth)
{
    ConfigurationPtr config = Poco::AutoPtr(new Poco::Util::MapConfiguration());

    config->setString("proxy.no_proxy", no_proxy_hosts);
    config->setString("proxy", "");
    config->setString("proxy.http", "");
    config->setString("proxy.http.uri", http_list_proxy_server.toString());

    config->setString("proxy", "");
    config->setString("proxy.https", "");
    config->setString("proxy.https.uri", https_list_proxy_server.toString());

    context->setConfig(config);

    auto http_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTP, *config);
    auto https_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTPS, *config);

    ASSERT_TRUE(std::dynamic_pointer_cast<DB::ProxyListConfigurationResolver>(http_resolver));
    ASSERT_TRUE(std::dynamic_pointer_cast<DB::ProxyListConfigurationResolver>(https_resolver));
}

TEST_F(ProxyConfigurationResolverProviderTests, RemoteResolverIsBasedOnProtocolConfigurationHTTPS)
{
    ConfigurationPtr config = Poco::AutoPtr(new Poco::Util::MapConfiguration());

    config->setString("proxy", "");
    config->setString("proxy.http", "");
    config->setString("proxy.http.resolver", "");
    config->setString("proxy.http.resolver.endpoint", "http://resolver:8080/hostname");

    // even tho proxy protocol / scheme is https, it should not be picked (prior to this PR, it would be picked)
    config->setString("proxy.http.resolver.proxy_scheme", "https");
    config->setString("proxy.http.resolver.proxy_port", "80");
    config->setString("proxy.http.resolver.proxy_cache_time", "10");

    context->setConfig(config);

    auto http_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTP, *config);
    auto https_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTPS, *config);

    ASSERT_TRUE(std::dynamic_pointer_cast<DB::RemoteProxyConfigurationResolver>(http_resolver));
    ASSERT_TRUE(std::dynamic_pointer_cast<DB::EnvironmentProxyConfigurationResolver>(https_resolver));
}

TEST_F(ProxyConfigurationResolverProviderTests, RemoteResolverHTTPSOnly)
{
    ConfigurationPtr config = Poco::AutoPtr(new Poco::Util::MapConfiguration());

    config->setString("proxy", "");
    config->setString("proxy.no_proxy", no_proxy_hosts);
    config->setString("proxy.https", "");
    config->setString("proxy.https.resolver", "");
    config->setString("proxy.https.resolver.endpoint", "http://resolver:8080/hostname");

    // even tho proxy protocol / scheme is http, it should not be picked (prior to this PR, it would be picked)
    config->setString("proxy.https.resolver.proxy_scheme", "http");
    config->setString("proxy.https.resolver.proxy_port", "80");
    config->setString("proxy.https.resolver.proxy_cache_time", "10");

    context->setConfig(config);

    auto http_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTP, *config);
    auto https_resolver = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTPS, *config);

    ASSERT_TRUE(std::dynamic_pointer_cast<DB::EnvironmentProxyConfigurationResolver>(http_resolver));
    ASSERT_TRUE(std::dynamic_pointer_cast<DB::RemoteProxyConfigurationResolver>(https_resolver));
}

template <bool DISABLE_TUNNELING_FOR_HTTPS_REQUESTS_OVER_HTTP_PROXY, bool STRING>
void test_tunneling(DB::ContextMutablePtr context)
{
    ConfigurationPtr config = Poco::AutoPtr(new Poco::Util::MapConfiguration());

    config->setString("proxy", "");
    config->setString("proxy.https", "");
    config->setString("proxy.https.uri", http_list_proxy_server.toString());

    if constexpr (STRING)
    {
        config->setString("proxy.disable_tunneling_for_https_requests_over_http_proxy", DISABLE_TUNNELING_FOR_HTTPS_REQUESTS_OVER_HTTP_PROXY ? "true" : "false");
    }
    else
    {
        config->setBool("proxy.disable_tunneling_for_https_requests_over_http_proxy", DISABLE_TUNNELING_FOR_HTTPS_REQUESTS_OVER_HTTP_PROXY);
    }

    context->setConfig(config);

    auto https_configuration = DB::ProxyConfigurationResolverProvider::get(DB::ProxyConfiguration::Protocol::HTTPS, *config)->resolve();

    ASSERT_EQ(https_configuration.tunneling, !DISABLE_TUNNELING_FOR_HTTPS_REQUESTS_OVER_HTTP_PROXY);
}

TEST_F(ProxyConfigurationResolverProviderTests, TunnelingForHTTPSRequestsOverHTTPProxySetting)
{
    test_tunneling<false, false>(context);
    test_tunneling<false, true>(context);
    test_tunneling<true, false>(context);
    test_tunneling<true, true>(context);
}
