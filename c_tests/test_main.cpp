#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <Python.h>

struct PythonInterpreter
{
  PythonInterpreter() { Py_Initialize(); }
  ~PythonInterpreter() { Py_Finalize(); }
};

PythonInterpreter guard;

TEST_CASE("cast_test", "Is FILE * default constructible")
{

  REQUIRE(std::is_default_constructible<FILE*>());
  // REQUIRE(std::is_default_constructible<std::istream>()); // istream is NOT
  // default constructible/**/!
}
