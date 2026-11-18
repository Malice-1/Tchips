add_library(tchips_protocol STATIC session.proto src/codec.cpp)
target_link_libraries(tchips_protocol PUBLIC protobuf::libprotobuf tchips_contracts PRIVATE tchips_options)
target_include_directories(tchips_protocol PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" include)
protobuf_generate(TARGET tchips_protocol LANGUAGE cpp)
