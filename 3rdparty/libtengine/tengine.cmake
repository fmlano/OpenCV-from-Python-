SET(TENGINE_VERSION "1.12.0")
SET(OCV_TENGINE_DSTDIRECTORY ${OpenCV_BINARY_DIR}/3rdparty/libtengine)
SET(DEFAULT_OPENCV_TENGINE_SOURCE_PATH ${OCV_TENGINE_DSTDIRECTORY}/Tengine-${TENGINE_VERSION})

IF(EXISTS ${DEFAULT_OPENCV_TENGINE_SOURCE_PATH})
	MESSAGE(STATUS "Tengine is exist already  .")

	SET(Tengine_FOUND ON)
	set(BUILD_TENGINE ON)
ELSE()
	SET(OCV_TENGINE_FILENAME "v${TENGINE_VERSION}.zip")#name2
	SET(OCV_TENGINE_URL "https://github.com/OAID/Tengine/archive/") #url2
	SET(tengine_md5sum d97e5c379281c5aa06e28daf868166a7) #md5sum2

	MESSAGE(STATUS "**** TENGINE DOWNLOAD BEGIN ****")
	ocv_download(FILENAME ${OCV_TENGINE_FILENAME}
						HASH ${tengine_md5sum}
						URL
						"${OPENCV_TENGINE_URL}"
						"$ENV{OPENCV_TENGINE_URL}"
						"${OCV_TENGINE_URL}"
						DESTINATION_DIR ${OCV_TENGINE_DSTDIRECTORY}
						ID TENGINE
						STATUS res
						UNPACK RELATIVE_URL)

	if (NOT res)
		MESSAGE(STATUS "TENGINE DOWNLOAD FAILED .Turning Tengine_FOUND off.")
		SET(Tengine_FOUND OFF)
	else ()
		MESSAGE(STATUS "TENGINE DOWNLOAD success . ")

		SET(Tengine_FOUND ON)
		set(BUILD_TENGINE ON)
	endif()
ENDIF()

if (BUILD_TENGINE)
	set(HAVE_TENGINE 1)

	# android system
	if(ANDROID)
	   if(${ANDROID_ABI} STREQUAL "armeabi-v7a")
			   set(CONFIG_ARCH_ARM32 ON)
	   elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
			   set(CONFIG_ARCH_ARM64 ON)
	   endif()
	endif()

	# linux system
	if(CMAKE_SYSTEM_PROCESSOR STREQUAL arm)
		   set(CONFIG_ARCH_ARM32 ON)
	elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64) ## AARCH64
		   set(CONFIG_ARCH_ARM64 ON)
	endif()

	SET(DEFAULT_OPENCV_TENGINE_SOURCE_PATH ${OCV_TENGINE_DSTDIRECTORY}/Tengine-${TENGINE_VERSION})
	set(BUILT_IN_OPENCV ON) ## set for tengine compile discern .
	set(Tengine_INCLUDE_DIR  ${DEFAULT_OPENCV_TENGINE_SOURCE_PATH}/core/include)
	set(Tengine_LIB   ${CMAKE_BINARY_DIR}/lib/${ANDROID_ABI}/libtengine.a)

	if ( IS_DIRECTORY ${DEFAULT_OPENCV_TENGINE_SOURCE_PATH})
		add_subdirectory("${DEFAULT_OPENCV_TENGINE_SOURCE_PATH}" ${OCV_TENGINE_DSTDIRECTORY}/build)
	endif()
endif()


