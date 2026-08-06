    cmake_minimum_required(VERSION 3.5)

    project(Smart_Lab_Controller LANGUAGES CXX)

    set(CMAKE_INCLUDE_CURRENT_DIR ON)

    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    find_package(Qt5 COMPONENTS Core Quick Network Sql Charts REQUIRED SerialPort
        SerialBus)

    set(PROJECT_SOURCES
      main.cpp
      qml.qrc
      backend.h
      backend.cpp
      basedevice.h
      basedevice.cpp
      FanDevice.h
      LaserDevice.h
      FileUtil.h
      FileUtil.cpp
      logworker.h
      logworker.cpp
      databaseworker.h
      databaseworker.cpp
      ConfigManager.h
      ConfigManager.cpp
      src/controller/labcontroller.h
      src/controller/labcontroller.cpp
      src/controller/labapplicationservice.h
      src/controller/labapplicationservice.cpp
      src/domain/models/devicesnapshot.h
      src/domain/models/controlcommand.h
      src/domain/models/telemetryframe.h
      src/domain/models/connectionstate.h
      src/domain/devices/idevicestrategy.h
      src/domain/devices/laserstrategy.h
      src/domain/devices/fanstrategy.h
      src/protocol/packetheader.h
      src/protocol/protocolmessage.h
      src/protocol/protocolserializer.h
      src/protocol/protocolserializer.cpp
      src/protocol/protocoldeserializer.h
      src/protocol/protocoldeserializer.cpp
      src/protocol/receivebuffer.h
      src/protocol/receivebuffer.cpp
      src/infrastructure/transport/transporttypes.h
      src/infrastructure/transport/itransportchannel.h
      src/infrastructure/transport/tcpdevicechannel.h
      src/infrastructure/transport/tcpdevicechannel.cpp
      src/Modbus/modbusmanager.h
      src/Modbus/modbusmanager.cpp
    )

    if(ANDROID)
        add_library(Smart_Lab_Controller SHARED ${PROJECT_SOURCES})
    else()
        add_executable(Smart_Lab_Controller ${PROJECT_SOURCES})
    endif()

    target_compile_definitions(Smart_Lab_Controller
      PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
    target_link_libraries(Smart_Lab_Controller
      PRIVATE Qt5::Core Qt5::Quick Qt5::Network Qt5::Sql Qt5::Charts Qt5::SerialPort
      Qt5::SerialBus)
  target_include_directories(Smart_Lab_Controller PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
