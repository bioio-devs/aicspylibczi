#include "UrlStream.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace pylibczi {

const char* kCurlHttpStreamClass = "curl_http_inputstream";

namespace {

std::string
squash(const std::string& str_)
{
  std::string out;
  out.reserve(str_.size());
  for (const unsigned char ch : str_) {
    if (ch != '_') {
      out.push_back(static_cast<char>(std::tolower(ch)));
    }
  }
  return out;
}

#if defined(__linux__)
/// Where distributions keep the system CA bundle. The libcurl bundled into the Linux wheel
/// defaults to the path from its build container, which need not exist on the running machine.
const char* const kCaBundlePaths[] = {
  "/etc/ssl/certs/ca-certificates.crt", // Debian, Ubuntu, Alpine
  "/etc/pki/tls/certs/ca-bundle.crt",   // RHEL, Fedora
  "/etc/ssl/ca-bundle.pem",             // SUSE
  "/etc/ssl/cert.pem",                  // Alpine, BSD
};
#endif

/// The CA bundle to verify https servers with when the caller did not name one, or "" to leave
/// libcurl's default in place. macOS and Windows verify through the OS certificate store.
std::string
defaultCaBundle()
{
#if defined(__linux__)
  for (const char* var : { "SSL_CERT_FILE", "CURL_CA_BUNDLE" }) {
    const char* value = std::getenv(var);
    if (value != nullptr && *value != '\0') {
      return value;
    }
  }
  for (const char* path : kCaBundlePaths) {
    if (std::ifstream(path).good()) {
      return path;
    }
  }
#endif
  return "";
}

}

bool
curlStreamAvailable()
{
  const int count = libCZI::StreamsFactory::GetStreamClassesCount();
  for (int i = 0; i < count; ++i) {
    libCZI::StreamsFactory::StreamClassInfo info;
    if (libCZI::StreamsFactory::GetStreamInfoForClass(i, info) && info.class_name == kCurlHttpStreamClass) {
      return true;
    }
  }
  return false;
}

void
throwIfCurlStreamUnavailable()
{
  libCZI::StreamsFactory::Initialize();
  if (!curlStreamAvailable()) {
    throw std::runtime_error("This build of aicspylibczi cannot read from http/https: "
                             "libCZI was compiled without its curl-based stream class.");
  }
}

std::vector<std::string>
streamOptionNames()
{
  std::vector<std::string> names;
  int count = 0;
  const auto* info = libCZI::StreamsFactory::GetStreamPropertyBagPropertyInfo(&count);
  for (int i = 0; i < count; ++i) {
    names.emplace_back(info[i].property_name);
  }
  return names;
}

std::pair<int, libCZI::StreamsFactory::Property::Type>
resolveStreamOption(const std::string& name_)
{
  const std::string wanted = squash(name_);
  int count = 0;
  const auto* info = libCZI::StreamsFactory::GetStreamPropertyBagPropertyInfo(&count);
  for (int i = 0; i < count; ++i) {
    const std::string candidate(info[i].property_name);
    const auto underscore = candidate.find('_');
    const bool matches = squash(candidate) == wanted ||
                         (underscore != std::string::npos && squash(candidate.substr(underscore + 1)) == wanted);
    if (matches) {
      return { info[i].property_id, info[i].property_type };
    }
  }
  return { -1, libCZI::StreamsFactory::Property::Type::Invalid };
}

std::shared_ptr<libCZI::IStream>
createStreamFromUrl(const std::string& url_, const std::map<int, libCZI::StreamsFactory::Property>& property_bag_)
{
  throwIfCurlStreamUnavailable();

  libCZI::StreamsFactory::CreateStreamInfo streamInfo;
  streamInfo.class_name = kCurlHttpStreamClass;
  streamInfo.property_bag = property_bag_;

  using Props = libCZI::StreamsFactory::StreamProperties;
  if (streamInfo.property_bag.count(Props::kCurlHttp_CaInfo) == 0 &&
      streamInfo.property_bag.count(Props::kCurlHttp_CaInfoBlob) == 0) {
    const std::string bundle = defaultCaBundle();
    if (!bundle.empty()) {
      streamInfo.property_bag.emplace(Props::kCurlHttp_CaInfo, libCZI::StreamsFactory::Property(bundle));
    }
  }

  auto stream = libCZI::StreamsFactory::CreateStream(streamInfo, url_);
  if (!stream) {
    std::ostringstream msg;
    msg << "libCZI could not create an http/https stream for " << url_ << ".";
    throw std::runtime_error(msg.str());
  }
  return stream;
}

}
