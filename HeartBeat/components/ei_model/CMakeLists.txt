# Builds the Edge Impulse anomaly model for ESP-IDF (replaces the Zephyr CMakeLists from the export).
# Only the parts of the SDK this model needs are compiled.

set(EI_SDK_DIR ${CMAKE_CURRENT_LIST_DIR}/edge-impulse-sdk)
file(GLOB EI_KISSFFT_SRCS ${EI_SDK_DIR}/dsp/kissfft/*.cpp)

idf_component_register(
    SRCS
        "ei_anomaly.cpp"
        "tflite-model/model.cpp"
        "edge-impulse-sdk/porting/espressif/ei_classifier_porting.cpp"
        "edge-impulse-sdk/dsp/memory.cpp"
        ${EI_KISSFFT_SRCS}
    INCLUDE_DIRS
        "."
        "edge-impulse-sdk"
        "edge-impulse-sdk/third_party"
        "model-parameters"
        "tflite-model"
    REQUIRES esp_timer
)

# The ESP-NN / ESP-DSP speed-ups are for neural networks, not needed for K-means
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=0
    EIDSP_USE_ESP_DSP=0
    EIDSP_USE_CMSIS_DSP=0
)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=gnu++17
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -Wno-error=format=
    -Wno-format-security
    -Wno-deprecated-declarations
)
