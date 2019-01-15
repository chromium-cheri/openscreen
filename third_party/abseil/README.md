Note: this third party library is only intended as an interim solution, and
requires manual manipulation of the Abseil source files to work properly,
specifically replacing:

#include "absl

with

#include "third_party/abseil/src/absl

which is less than ideal.

TODO: figure out how to avoid modifying includes