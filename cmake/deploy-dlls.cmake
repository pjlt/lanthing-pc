function(deploy_thirdparty_dlls)
	set(THIRDPARTY_DLLS
		${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/avcodec-59.dll
		${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/avutil-57.dll
		${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib/swresample-4.dll
		${CMAKE_SOURCE_DIR}/third_party/g3log/build/install/${THIRD_PARTY_BUILD_TYPE}/bin/g3log.dll
		${CMAKE_SOURCE_DIR}/third_party/libuv/build/install/${THIRD_PARTY_BUILD_TYPE}/bin/uv.dll
		${CMAKE_SOURCE_DIR}/third_party/protobuf/build2/install/${THIRD_PARTY_BUILD_TYPE}/bin/libprotobuf-lite.dll
		${CMAKE_SOURCE_DIR}/third_party/protobuf/build2/install/${THIRD_PARTY_BUILD_TYPE}/bin/libprotobuf.dll
		${CMAKE_SOURCE_DIR}/third_party/SDL/build/install/${THIRD_PARTY_BUILD_TYPE}/bin/SDL2.dll
	)
	install(
		FILES ${THIRDPARTY_DLLS}
		DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
endfunction()

function(deploy_qt6)
	install(CODE "execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} ${CMAKE_INSTALL_BINDIR}/app.exe)")
endfunction()

deploy_thirdparty_dlls()
deploy_qt6()