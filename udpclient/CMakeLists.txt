add_library(client_lib udpclient.cpp udpclient.h)
add_executable(udpclient main.cpp)
target_link_libraries(udpclient client_lib Boost::system)