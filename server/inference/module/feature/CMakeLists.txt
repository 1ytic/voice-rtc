add_library(streaming_inference_modules_feature
  ${CMAKE_CURRENT_LIST_DIR}/LogMelFeature.cpp
)

add_dependencies(streaming_inference_modules_feature cereal)

target_link_libraries(
  streaming_inference_modules_feature
  PUBLIC
    ${cereal_LIBRARIES}
    streaming_inference_common
)

target_include_directories(
  streaming_inference_modules_feature
  PUBLIC
    ${cereal_INCLUDE_DIRS}
)
