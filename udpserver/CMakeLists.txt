add_library(server_lib 
    datastorage.h 
    generator.h 
    generator.cpp 
    submitinfo.h 
    submitinfo.cpp 
    udpserver.h
    udpserver.cpp 
)

add_executable(udpserver main.cpp)
target_link_libraries(udpserver server_lib Boost::system)
