
set(GTK_MIXER_BIN	gtk-mixer.c
			gtk-mixer-container.c
			gtk-mixer-devs_combo.c
			gtk-mixer-line.c
			gtk-mixer-tray_icon.c
			gtk-mixer-window.c)

set(GTK_MIXER_SHARED	plugin_api.c)


if (ALSA_FOUND)
	list(APPEND GTK_MIXER_PLUGINS	plugin_alsa.c)
endif()
if (OSS_FOUND)
	list(APPEND GTK_MIXER_PLUGINS	plugin_oss3.c)
endif()


add_executable(gtk-mixer ${GTK_MIXER_BIN} ${GTK_MIXER_SHARED} ${GTK_MIXER_PLUGINS})
set_target_properties(gtk-mixer PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(gtk-mixer ${CMAKE_REQUIRED_LIBRARIES} ${GTK3_LIBRARIES} ${CMAKE_EXE_LINKER_FLAGS})

install(TARGETS gtk-mixer RUNTIME DESTINATION bin)
