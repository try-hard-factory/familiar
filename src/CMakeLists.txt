
set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        canvasview.cpp
        canvasview.h
        moveitem.cpp
        moveitem.h
        movepixmapitem.cpp
        movepixmapitem.h
        canvasscene.cpp
        canvasscene.h
        itemgroup.cpp
        itemgroup.h
        mainselectedarea.cpp
        mainselectedarea.h
        debug_macros.h
        Logger.h
        MessageBuilder.h
        borderdot.cpp
        borderdot.h
        Vec2d.h
        image_downloader.cpp
        image_downloader.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(familiar
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(familiar SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(familiar
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(familiar PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)
