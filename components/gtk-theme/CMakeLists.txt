cmake_minimum_required(VERSION 3.16)
project(bayu-gtk-theme NONE)
include(GNUInstallDirs)

# bayu-services selects Bayu or Bayu-dark through GSettings (gtk-theme).
install(DIRECTORY Bayu Bayu-dark DESTINATION ${CMAKE_INSTALL_DATADIR}/themes)
