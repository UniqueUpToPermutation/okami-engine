
add_engine_sample(05_animation main.cpp)

# tinygltf headers are PRIVATE on EngineLib; expose them to this target too.
target_include_directories(05_animation PRIVATE ${TINYGLTF_INCLUDE_DIRS})