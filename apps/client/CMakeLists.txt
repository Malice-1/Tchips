add_library(tchips_client_core STATIC
  src/camera2d.cpp
  src/movement_system.cpp
  src/asset_manager.cpp
  src/prediction.cpp
)
target_include_directories(tchips_client_core PUBLIC include)
target_link_libraries(tchips_client_core PUBLIC tchips_geometry EnTT::EnTT PRIVATE tchips_options)
if(TCHIPS_ENABLE_NETWORK)
  add_library(tchips_client_network STATIC src/network_client.cpp)
  target_include_directories(tchips_client_network PUBLIC include)
  target_link_libraries(tchips_client_network PUBLIC tchips_client_core tchips_network_dependencies tchips_protocol PRIVATE tchips_options)
endif()
if(NOT TCHIPS_BUILD_CLIENT)
  return() # Core tests remain available in the Linux headless preset.
endif()
add_library(tchips_client_platform STATIC src/input_manager.cpp src/renderer.cpp src/application.cpp)
target_include_directories(tchips_client_platform PRIVATE shaders/sdl-3.4.20)
target_link_libraries(tchips_client_platform PUBLIC tchips_client_core SDL3::SDL3 PRIVATE tchips_options)
add_executable(tchips_client src/main.cpp)
target_link_libraries(tchips_client PRIVATE tchips_client_platform tchips_options)
add_custom_target(tchips_client_assets ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/sprites"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/content/sprites/temporary.sprite" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/sprites/temporary.sprite"
  VERBATIM
)
add_dependencies(tchips_client tchips_client_assets)
add_dependencies(tchips_client_assets tchips_map_assets)
if(TCHIPS_ENABLE_NETWORK)
  target_link_libraries(tchips_client_platform PUBLIC tchips_client_network)
  target_compile_definitions(tchips_client_platform PRIVATE TCHIPS_NETWORK_ENABLED=1)
endif()
if(TCHIPS_ENABLE_UI)
  target_link_libraries(tchips_client PRIVATE RmlUi::RmlUi)
endif()
if(TCHIPS_ENABLE_AUDIO)
  target_link_libraries(tchips_client PRIVATE tchips_miniaudio)
endif()
if(TCHIPS_ENABLE_PROFILING)
  target_link_libraries(tchips_client_platform PRIVATE Tracy::TracyClient)
  target_compile_definitions(tchips_client_platform PRIVATE TCHIPS_TRACY_ENABLED=1)
endif()
