cmake_minimum_required(VERSION 3.19)
project(WsprSked VERSION 0.2.3 LANGUAGES CXX)   # version is sent to wsprnet.org as "WS-<version>"

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Multimedia Network SerialPort)

# Non-UI core (database, repeats, WSPR encoder/messages/modulator, credentials, log) - shared by the app and the tests
add_library(slotcore STATIC
    slotrecord.h
    slotstore.h slotstore.cpp
    repeatplanner.h repeatplanner.cpp
    wsprencode.h wsprencode.cpp
    credentials.h credentials.cpp
    wsprmodulator.h wsprmodulator.cpp
    txlog.h txlog.cpp
    wsprmessage.h wsprmessage.cpp
    maidenhead.h maidenhead.cpp
    rigctl.h rigctl.cpp
    rigctld.h rigctld.cpp
    arduinoswitch.h arduinoswitch.cpp
    w410a.h w410a.cpp
    othertx.h othertx.cpp
    rxspot.h rxspot.cpp
    rxstore.h rxstore.cpp
    wsprnetupload.h wsprnetupload.cpp
    wsprdrunner.h wsprdrunner.cpp
    wavrecorder.h wavrecorder.cpp
    wsprspectrum.h wsprspectrum.cpp
    txplan.h txplan.cpp
    suntimes.h suntimes.cpp
    ntpcheck.h ntpcheck.cpp
    remoteserver.h remoteserver.cpp
    mysqlstarter.h mysqlstarter.cpp
    dbbackoff.h
)
target_link_libraries(slotcore PUBLIC Qt6::Core Qt6::Sql Qt6::Network Qt6::SerialPort)
target_include_directories(slotcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Everything in the app except main.cpp, shared with the window test
set(WSPRSKED_APP_SOURCES
    mainwindow.cpp
    mainwindow_slots.cpp
    mainwindow_rx.cpp
    mainwindow_rig.cpp
    mainwindow_freqant.cpp
    mainwindow_hw.cpp
    mainwindow_tx.cpp
    mainwindow_sun.cpp
    mainwindow_config.cpp
    mainwindow_tests.cpp
    mainwindow_otherrig.cpp
    mainwindow_remote.cpp
    mainwindow_help.cpp
    rtfhtml.h rtfhtml.cpp
    mainwindow.h
    version.h
    mainwindow.ui
    slottablemodel.h slottablemodel.cpp
    msg.h msg.cpp
    loaderror.h loaderror.cpp
    wspraudioout.h wspraudioout.cpp
    rxform.h rxform.cpp rxform.ui
    spectrumform.h spectrumform.cpp
    rxtablemodel.h rxtablemodel.cpp
    rxcycle.h rxcycle.cpp
    wspraudioin.h wspraudioin.cpp
)
if(WIN32)   # icon and version details for the programs (Explorer, taskbar, installer)
    set(WSPRSKED_ICON "${CMAKE_CURRENT_SOURCE_DIR}/wsprsked.ico")
    macro(wspr_rc out name exe ver)
        string(REPLACE "." ";" _v "${ver}")
        list(GET _v 0 RC_MAJOR)
        list(GET _v 1 RC_MINOR)
        list(GET _v 2 RC_PATCH)
        set(RC_VERSION "${ver}")
        set(RC_NAME "${name}")
        set(RC_EXE "${exe}")
        configure_file(wsprsked.rc.in "${CMAKE_CURRENT_BINARY_DIR}/${exe}.rc" @ONLY)
        set(${out} "${CMAKE_CURRENT_BINARY_DIR}/${exe}.rc")
    endmacro()
    wspr_rc(WSPRSKED_RC "WSPR Scheduler 2" WsprSked ${PROJECT_VERSION})
endif()
add_executable(WsprSked WIN32 main.cpp appfont.h ${WSPRSKED_APP_SOURCES} ${WSPRSKED_RC})
target_link_libraries(WsprSked PRIVATE slotcore Qt6::Widgets Qt6::Multimedia)
target_compile_definitions(WsprSked PRIVATE WSPRSKED_VERSION="${PROJECT_VERSION}")

# Encoder test on a real sound card (console program): wsprplay "G4GCI IO90 23" --device ...
add_executable(wsprplay tools/wsprplay.cpp wspraudioout.cpp)
target_link_libraries(wsprplay PRIVATE slotcore Qt6::Multimedia)

# Receiver test on a real sound card (console program): wsprrx --device ... --wsprd C:/WSJT/wsjtx/bin
add_executable(wsprrx tools/wsprrx.cpp rxcycle.cpp wspraudioin.cpp)
target_link_libraries(wsprrx PRIVATE slotcore Qt6::Multimedia)

# Windows: copy the Qt DLLs and plugins next to the programs after each build (windeployqt), so they
# also run from a plain command prompt or Explorer, not only from Qt Creator.
# The MySQL driver (sqldrivers/qsqlmysql.dll) and libmysql.dll are not part of Qt - copy those yourself.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${QT6_INSTALL_PREFIX}/bin" "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        foreach(_t WsprSked wsprplay wsprrx)
            add_custom_command(TARGET ${_t} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --verbose 0 "$<TARGET_FILE:${_t}>"
                COMMENT "windeployqt ${_t}")
        endforeach()
    else()
        message(WARNING "windeployqt not found - run the programs from Qt Creator or add the Qt bin folder to PATH")
    endif()
endif()

option(WSPRSKED_TESTS "Build the slot store / repeat planner tests (needs a scratch MySQL)" OFF)
if(WSPRSKED_TESTS)
    add_executable(slots_test tests/slots_test.cpp)
    target_link_libraries(slots_test PRIVATE slotcore)
    add_executable(encode_test tests/encode_test.cpp)   # no database needed
    target_link_libraries(encode_test PRIVATE slotcore)
    add_executable(modulator_test tests/modulator_test.cpp wspraudioout.cpp)   # no database or sound card needed
    target_link_libraries(modulator_test PRIVATE slotcore Qt6::Multimedia)
    add_executable(message_test tests/message_test.cpp)   # type 2/3 vs WSJT-X wsprcode, slot rules
    target_link_libraries(message_test PRIVATE slotcore)
    add_executable(maidenhead_test tests/maidenhead_test.cpp)
    target_link_libraries(maidenhead_test PRIVATE slotcore)
    add_executable(rigctl_test tests/rigctl_test.cpp)     # needs Hamlib rigctld installed (uses the dummy rig)
    target_link_libraries(rigctl_test PRIVATE slotcore)
    add_executable(rigctld_test tests/rigctld_test.cpp)   # needs Hamlib; dummy rig, no radio
    target_link_libraries(rigctld_test PRIVATE slotcore)
    add_executable(arduino_test tests/arduino_test.cpp)   # fake board, no hardware needed
    target_link_libraries(arduino_test PRIVATE slotcore)
    add_executable(w410a_test tests/w410a_test.cpp)       # virtual serial pair (socat), Linux only
    target_link_libraries(w410a_test PRIVATE slotcore)
    add_executable(othertx_test tests/othertx_test.cpp)   # fake HTTP + serial rig, Linux only
    target_link_libraries(othertx_test PRIVATE slotcore)
    add_executable(txlog_test tests/txlog_test.cpp)
    target_link_libraries(txlog_test PRIVATE slotcore)
    add_executable(sun_ntp_test tests/sun_ntp_test.cpp)   # fake web and time servers; scratch MySQL optional
    target_link_libraries(sun_ntp_test PRIVATE slotcore)
    add_executable(rtf_test tests/rtf_test.cpp rtfhtml.cpp)   # help file RTF -> HTML
    target_link_libraries(rtf_test PRIVATE Qt6::Widgets)
    target_include_directories(rtf_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(mysqlstarter_test tests/mysqlstarter_test.cpp)   # start-up MySQL check, with a fake server
    target_link_libraries(mysqlstarter_test PRIVATE slotcore)
    add_executable(remote_test tests/remote_test.cpp)   # HTTPS remote page; needs openssl to make the certificate
    target_link_libraries(remote_test PRIVATE slotcore)
    add_executable(txplan_test tests/txplan_test.cpp)   # fake Arduino board, no hardware
    target_link_libraries(txplan_test PRIVATE slotcore)
    add_executable(wsprsyms tools/wsprsyms.cpp)          # message -> 162 symbols (compare with WSJT-X wsprcode)
    target_link_libraries(wsprsyms PRIVATE slotcore)
    add_executable(rx_test tests/rx_test.cpp)            # wsprd output, bands, timing, resampler, upload form
    target_link_libraries(rx_test PRIVATE slotcore)
    add_executable(rxdecode_test tests/rxdecode_test.cpp rxcycle.cpp wspraudioin.cpp)   # needs wsprd; fake wsprnet
    target_link_libraries(rxdecode_test PRIVATE slotcore Qt6::Multimedia)
    add_executable(rxform_uitest tests/rxform_uitest.cpp rxform.cpp rxform.ui spectrumform.cpp rxtablemodel.cpp rxcycle.cpp
                   wspraudioin.cpp msg.cpp)          # the window itself, scratch MySQL; QT_QPA_PLATFORM=offscreen
    target_link_libraries(rxform_uitest PRIVATE slotcore Qt6::Widgets Qt6::Multimedia)
    add_executable(mainwindow_uitest tests/mainwindow_uitest.cpp ${WSPRSKED_APP_SOURCES})   # scratch MySQL; offscreen
    target_link_libraries(mainwindow_uitest PRIVATE slotcore Qt6::Widgets Qt6::Multimedia)
    target_compile_definitions(mainwindow_uitest PRIVATE WSPRSKED_VERSION="${PROJECT_VERSION}")
    add_executable(rxstore_test tests/rxstore_test.cpp)  # scratch MySQL, see tests/rx_fixture.sql
    target_link_libraries(rxstore_test PRIVATE slotcore)
    add_executable(wsprwav tools/wsprwav.cpp)            # WSPR signal to WAV, for checking with wsprd
    target_link_libraries(wsprwav PRIVATE slotcore)
endif()
