juce_add_console_app(FoundationTest PRODUCT_NAME "FoundationTest")
target_sources(FoundationTest PRIVATE FoundationTest.cpp)
target_include_directories(FoundationTest PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/RaveStation/Source
    ${CMAKE_SOURCE_DIR}/plugins/MelodyStudio/Source
    ${CMAKE_SOURCE_DIR}/plugins/SpaceFX/Source)
target_link_libraries(FoundationTest PRIVATE nwa_core PUBLIC juce::juce_recommended_config_flags)

add_test(NAME foundation COMMAND FoundationTest)
