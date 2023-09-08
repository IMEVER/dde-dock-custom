cmake_minimum_required(VERSION 3.7)

set(BIN_NAME dde-dock)
configure_file(environments.h.in environments.h @ONLY)

# Sources files
file(GLOB_RECURSE SRCS "*.h" "*.cpp")

# Find the library
find_package(PkgConfig REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5X11Extras REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(DtkWidget REQUIRED)
# find_package(DtkGui REQUIRED)
find_package(DtkCMake REQUIRED)
find_package(KF5WindowSystem REQUIRED)

pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh xcb-icccm xres x11)
# pkg_check_modules(DFrameworkDBus REQUIRED dframeworkdbus)
pkg_check_modules(DtkGUI REQUIRED dtkgui)
pkg_check_modules(QGSettings REQUIRED gsettings-qt)

# qt5_add_dbus_adaptor(SRCS dbus/com.deepin.dde.Dock.xml window/mainwindow.h MainWindow dbusdockadaptors DBusDockAdaptors)

set_source_files_properties(dbus/me.imever.dde.toppanel.xml PROPERTIES CLASSNAME TopPanelInterface NO_NAMESPACE true INCLUDE dbus/dbusdockadaptors.h)
qt5_add_dbus_interface(SRCS dbus/me.imever.dde.toppanel.xml TopPanelInterface )

set_source_files_properties(dbus/org.deepin.dde.Launcher1.xml PROPERTIES CLASSNAME LauncherInter)
qt5_add_dbus_interface(SRCS dbus/org.deepin.dde.Launcher1.xml LauncherInter)

set_source_files_properties(dbus/org.deepin.dde.kwayland.PlasmaWindow.xml PROPERTIES INCLUDE dbus/dockrect.h)
qt5_add_dbus_interface(SRCS dbus/org.deepin.dde.kwayland.PlasmaWindow.xml org_deepin_dde_kwayland_plasmawindow)
qt5_add_dbus_interface(SRCS dbus/org.deepin.dde.kwayland.WindowManager.xml org_deepin_dde_kwayland_windowmanager)
qt5_add_dbus_interface(SRCS dbus/org.deepin.dde.WMSwitcher1.xml org_deepin_dde_wmswitcher1)

set_source_files_properties(dbus/org.deepin.dde.XEventMonitor1.xml PROPERTIES CLASSNAME XEventMonitorInter INCLUDE dbus/arealist.h)
qt5_add_dbus_interface(SRCS dbus/org.deepin.dde.XEventMonitor1.xml org_deepin_dde_xeventmonitor1)

qt5_add_dbus_interface(SRCS dbus/com.deepin.wm.xml com_deepin_wm)
qt5_add_dbus_interface(SRCS dbus/org.deepin.dde.Appearance1.xml org_deepin_dde_appearance1)


# set_source_files_properties(dbus/org.deepin.dde.Display1.xml PROPERTIES CLASSNAME DisplayInter INCLUDE dbus/screenrect.h)
# qt5_add_dbus_interface(SRCS dbus/org.deepin.dde.Display1.xml DisplayInter)

# driver-manager
add_executable(${BIN_NAME} ${SRCS} item/item.qrc)
target_include_directories(${BIN_NAME} PUBLIC ${DtkWidget_INCLUDE_DIRS}
                                            ${XCB_EWMH_INCLUDE_DIRS}
                                            # ${DFrameworkDBus_INCLUDE_DIRS}
                                            ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
                                            ${PROJECT_BINARY_DIR}
                                            ${DtkGUI_INCLUDE_DIRS}
                                            ${QGSettings_INCLUDE_DIRS}
                                            ${KF5WindowSystem_INCLUDE_DIRS}
                                            ${Qt5Svg_INCLUDE_DIRS}
                                            )

target_link_libraries(${BIN_NAME} PRIVATE
    ${XCB_EWMH_LIBRARIES}
    # ${DFrameworkDBus_LIBRARIES}
    ${DtkWidget_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Concurrent_LIBRARIES}
    ${Qt5X11Extras_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${DtkGUI_LIBRARIES}
    ${QGSettings_LIBRARIES}
    ${Qt5Svg_LIBRARIES}
    KF5::WindowSystem
    xdo
    X11
)

# bin
# install(TARGETS ${BIN_NAME} DESTINATION bin)
