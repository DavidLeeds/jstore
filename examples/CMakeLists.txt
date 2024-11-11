if(JSTORE_ENABLE_DBUS)
    # Wi-fi manager example
    add_executable(wifi_manager wifi_manager.cpp)
    target_link_libraries(wifi_manager jstore)
endif()
