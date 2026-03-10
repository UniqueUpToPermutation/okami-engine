
add_engine_sample(04_sponza main.cpp)

# tinygltf headers are PRIVATE on EngineLib; expose them to this target too.
target_include_directories(04_sponza PRIVATE ${TINYGLTF_INCLUDE_DIRS})