#ifndef _AICSPYLIBCZI_URLSTREAM_H
#define _AICSPYLIBCZI_URLSTREAM_H

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "inc_libCZI.h"

namespace pylibczi {

/*!
 * @brief The libCZI stream class that reads over http/https, backed by libcurl.
 */
extern const char* kCurlHttpStreamClass;

/*!
 * @brief Check whether this build can read CZI files over http/https.
 * @return true if libCZI was compiled with its curl-based stream class.
 */
bool curlStreamAvailable();

/*!
 * @brief Throw a descriptive error if this build cannot read over http/https.
 */
void throwIfCurlStreamUnavailable();

/*!
 * @brief Get the names of the options accepted by createStreamFromUrl.
 * @return the libCZI property-bag names, ie "CurlHttp_Timeout".
 */
std::vector<std::string> streamOptionNames();

/*!
 * @brief Resolve an option name to its libCZI property-bag key and value type.
 *
 * Both the full libCZI name ("CurlHttp_Timeout") and its suffix ("timeout") are accepted,
 * case-insensitively.
 *
 * @param name_ the option name to resolve
 * @return the property id and type, or {-1, Invalid} if the name is not recognized
 */
std::pair<int, libCZI::StreamsFactory::Property::Type> resolveStreamOption(const std::string& name_);

/*!
 * @brief Create a stream that reads a CZI file from an http/https URL.
 * @param url_ the URL of the CZI file, in UTF-8
 * @param property_bag_ libCZI stream properties, ie timeouts and credentials
 * @return a stream that can be handed to Reader
 */
std::shared_ptr<libCZI::IStream> createStreamFromUrl(
  const std::string& url_,
  const std::map<int, libCZI::StreamsFactory::Property>& property_bag_);

}

#endif //_AICSPYLIBCZI_URLSTREAM_H
