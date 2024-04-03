set(testName UnitTests)

add_executable(${testName}
    main.cpp
    TestActivityClient.cpp
    TestActivityClient.h
)


target_include_directories(${testName} PUBLIC ${CMAKE_SOURCE_DIR})

target_include_directories(${testName} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/headers ${EXTRACHAIN_CORE_INCLUDES})


target_link_libraries(${testName}
    gtest_main
    Qt6::Core
    Qt6::Network
    extrachain
)

add_test(NAME ExampleTest COMMAND ExampleTest)
