# --- obsensor ---
if(NOT HAVE_OBSENSOR)
  if(APPLE)
    # force to use orbbec sdk on mac
    set(OBSENSOR_USE_ORBBEC_SDK ON)
  endif()

  if(OBSENSOR_USE_ORBBEC_SDK)
    include(${CMAKE_SOURCE_DIR}/3rdparty/orbbecsdk/orbbecsdk.cmake)
    download_orbbec_sdk(ORBBEC_SDK_ROOT_DIR)
    message(STATUS "ORBBEC_SDK_ROOT_DIR: ${ORBBEC_SDK_ROOT_DIR}")
    if(ORBBEC_SDK_ROOT_DIR)
      set(OrbbecSDK_DIR "${ORBBEC_SDK_ROOT_DIR}")
      find_package(OrbbecSDK REQUIRED)
      message(STATUS "OrbbecSDK_FOUND: ${OrbbecSDK_FOUND}")
      message(STATUS "OrbbecSDK_INCLUDE_DIRS: ${OrbbecSDK_INCLUDE_DIRS}")
      if(OrbbecSDK_FOUND)
        set(HAVE_OBSENSOR TRUE)
        set(HAVE_OBSENSOR_ORBBEC_SDK TRUE)
        ocv_add_external_target(obsensor "${OrbbecSDK_INCLUDE_DIRS}" "${OrbbecSDK_LIBRARY}" "HAVE_OBSENSOR;HAVE_OBSENSOR_ORBBEC_SDK")
        file(COPY ${OrbbecSDK_DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/bin)
        file(COPY ${OrbbecSDK_DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/lib)
      endif()
    endif()
  else()
    if(WIN32)
      check_include_file(mfapi.h HAVE_MFAPI)
      check_include_file(vidcap.h HAVE_VIDCAP)
      if(HAVE_MFAPI AND HAVE_VIDCAP)
        set(HAVE_OBSENSOR TRUE)
        set(HAVE_OBSENSOR_MSMF TRUE)
        ocv_add_external_target(obsensor "" "" "HAVE_OBSENSOR;HAVE_OBSENSOR_MSMF")
      else()
        set(HAVE_OBSENSOR OFF)
        set(HAVE_OBSENSOR_MSMF OFF)
        if(NOT HAVE_MFAPI)
          MESSAGE(STATUS "Could not find mfapi.h. Turning HAVE_OBSENSOR OFF")
        endif()
        if(NOT HAVE_VIDCAP)
          MESSAGE(STATUS "Could not find vidcap.h. Turning HAVE_OBSENSOR OFF")
        endif()
      endif()
    elseif(UNIX)
      check_include_file(linux/videodev2.h HAVE_CAMV4L2_OBSENSOR)
      if(HAVE_CAMV4L2_OBSENSOR)
        set(HAVE_OBSENSOR TRUE)
        set(HAVE_OBSENSOR_V4L2 TRUE)
        ocv_add_external_target(obsensor "" "" "HAVE_OBSENSOR;HAVE_OBSENSOR_V4L2")
      endif()
    endif()
  endif()
endif()
