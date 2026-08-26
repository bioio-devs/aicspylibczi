#ifndef _AICSPYLIBCZI_PB_STREAM_OPTIONS_H
#define _AICSPYLIBCZI_PB_STREAM_OPTIONS_H

#include <map>
#include <sstream>
#include <string>

#include <pybind11/pybind11.h>

#include "UrlStream.h"
#include "inc_libCZI.h"

namespace pb_helpers {

/// @brief Convert a python value to a libCZI stream property of the requested type.
inline libCZI::StreamsFactory::Property
streamPropertyFromPyObject(const pybind11::handle& value_,
                           libCZI::StreamsFactory::Property::Type type_,
                           const std::string& name_)
{
  using Type = libCZI::StreamsFactory::Property::Type;
  try {
    switch (type_) {
      case Type::Int32:
        return libCZI::StreamsFactory::Property(value_.cast<std::int32_t>());
      case Type::Float:
        return libCZI::StreamsFactory::Property(value_.cast<float>());
      case Type::Double:
        return libCZI::StreamsFactory::Property(value_.cast<double>());
      case Type::Boolean:
        return libCZI::StreamsFactory::Property(value_.cast<bool>());
      case Type::String:
        return libCZI::StreamsFactory::Property(value_.cast<std::string>());
      default:
        break;
    }
  } catch (const pybind11::cast_error&) {
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
streamPropertyBagFromDict(const pybind11::dict& options_)
{
  std::map<int, libCZI::StreamsFactory::Property> propertyBag;
  for (const auto& item : options_) {
    const auto name = item.first.cast<std::string>();
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
