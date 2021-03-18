link_directories(
        ${GTKMM_LIBRARY_DIRS}  )

include_directories(
        ${GTKMM_INCLUDE_DIRS}  )

add_executable(familiar # name of the executable on Windows will be example.exe
        main.cpp
        SysLogger.cpp
        radiobuttons.cpp
        radiobuttons.h
        main_window.cpp
        main_window.h
        ImagesArea.cpp
        ImagesArea.h CanvasArea.cpp CanvasArea.h Point.h Fleck.h ContentController.h Rectangle.h ContentController.cpp FocusPoints.cpp FocusPoints.h utils.h ImageWidget.cpp ImageWidget.h Rectangle.cpp)

target_link_libraries(familiar
        ${GTKMM_LIBRARIES} pthread )