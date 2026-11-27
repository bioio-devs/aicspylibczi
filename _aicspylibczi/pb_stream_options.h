#ifndef _AICSPYLIBCZI_PB_STREAM_OPTIONS_H
#define _AICSPYLIBCZI_PB_STREAM_OPTIONS_H

#include <map>
#include <sstream>
#include <string>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include "UrlStream.h"
#include "inc_libCZI.h"

namespace pb_helpers {

/// @brief Convert a python value to a libCZI stream property of the requested type.
inline libCZI::StreamsFactory::Property
streamPropertyFromPyObject(const nanobind::handle& value_,
                           libCZI::StreamsFactory::Property::Type type_,
                           const std::string& name_)
{
  using Type = libCZI::StreamsFactory::Property::Type;
  try {
    switch (type_) {
      case Type::Int32:
        return libCZI::StreamsFactory::Property(nanobind::cast<std::int32_t>(value_));
      case Type::Float:
        return libCZI::StreamsFactory::Property(nanobind::cast<float>(value_));
      case Type::Double:
        return libCZI::StreamsFactory::Property(nanobind::cast<double>(value_));
      case Type::Boolean:
        return libCZI::StreamsFactory::Property(nanobind::cast<bool>(value_));
      case Type::String:
        return libCZI::StreamsFactory::Property(nanobind::cast<std::string>(value_));
      default:
        break;
    }
  } catch (const nanobind::cast_error&) {
    std::ostringstream msg;
    msg << "Stream option '" << name_ << "' could not be converted to the type libCZI expects for it.";
    throw std::invalid_argument(msg.str());
  }
  std::ostringstream msg;
  msg << "Stream option '" << name_ << "' has a type that is not supported.";
  throw std::invalid_argument(msg.str());
}

/// @brief Convert a python dict of stream options into a libCZI property bag.
inline std::map<int, libCZI::StreamsFactory::Property>
streamPropertyBagFromDict(const nanobind::dict& options_)
{
  std::map<int, libCZI::StreamsFactory::Property> propertyBag;
  for (const auto& item : options_) {
    const auto name = nanobind::cast<std::string>(item.first);
    const auto resolved = pylibczi::resolveStreamOption(name);
    if (resolved.first < 0) {
      std::ostringstream msg;
      msg << "Unknown stream option '" << name << "'. Known options are: ";
      const auto known = pylibczi::streamOptionNames();
      for (auto it = known.begin(); it != known.end(); ++it) {
        msg << (it == known.begin() ? "" : ", ") << *it;
      }
      msg << ".";
      throw std::invalid_argument(msg.str());
    }
    propertyBag.emplace(resolved.first, streamPropertyFromPyObject(item.second, resolved.second, name));
  }
  return propertyBag;
}

}

#endif //_AICSPYLIBCZI_PB_STREAM_OPTIONS_H
