include(FetchContent)

# GIT_TAGs used were latest as of 2020/11/01

FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/spurious/SDL-mirror
  GIT_TAG        f606bee
)
FetchContent_MakeAvailable(SDL2)
