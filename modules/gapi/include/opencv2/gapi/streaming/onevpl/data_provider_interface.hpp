// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2021 Intel Corporation

#ifndef GAPI_STREAMING_ONEVPL_ONEVPL_DATA_PROVIDER_INTERFACE_HPP
#define GAPI_STREAMING_ONEVPL_ONEVPL_DATA_PROVIDER_INTERFACE_HPP
#include <exception>
#include <limits>
#include <memory>
#include <string>

#ifdef HAVE_ONEVPL
#include <vpl/mfxvideo.h>
#endif // HAVE_ONEVPL

#include <opencv2/gapi/own/exports.hpp> // GAPI_EXPORTS

namespace cv {
namespace gapi {
namespace wip {
namespace onevpl {

struct GAPI_EXPORTS DataProviderException : public std::exception {
    virtual ~DataProviderException() {}
};

struct GAPI_EXPORTS DataProviderSystemErrorException : public DataProviderException {
    DataProviderSystemErrorException(int error_code, const std::string& desription = std::string());
    virtual ~DataProviderSystemErrorException();
    virtual const char* what() const noexcept override;

private:
    std::string reason;
};

struct GAPI_EXPORTS DataProviderUnsupportedException : public DataProviderException {
    DataProviderUnsupportedException(const std::string& desription);
    virtual ~DataProviderUnsupportedException();
    virtual const char* what() const noexcept override;

private:
    std::string reason;
};

/**
 * @brief Public interface allows to customize extraction of video stream data
 * used by onevpl::GSource instead of reading stream from file (by default).
 *
 * Interface implementation constructor MUST provide consistency and creates fully operable object.
 * If error happened implementation MUST throw `DataProviderException` kind exceptions
 *
 * @note Interface implementation MUST manage stream and other constructed resources by itself to avoid any kind of leak.
 * For simple interface implementation example please see `StreamDataProvider` in `tests/streaming/gapi_streaming_tests.cpp`
 */
struct GAPI_EXPORTS IDataProvider {
    using Ptr = std::shared_ptr<IDataProvider>;
    using mfx_codec_id_type = uint32_t;

    virtual ~IDataProvider() = default;

    /**
     * The function is used by onevpl::GSource to extract codec id from data
     *
     */
    virtual mfx_codec_id_type get_mfx_codec_id() const = 0;

    /**
     * The function is used by onevpl::GSource to extract binary data stream from @ref IDataProvider
     * implementation.
     *
     * It MUST throw `DataProviderException` kind exceptions in fail cases.
     * It MUST return MFX_ERR_MORE_DATA in EOF which considered as not-fail case.
     *
     * @param out_data_bytes_size the available capacity of out_data buffer.
     * @param in_out_bitsream the input-output reference on MFX bitstream buffer which MUST be empty at the first request
     * to allow implementation to allocate it by itself and to return back. Subsequent invocation of `fetch_bitstream_data`
     * MUST use the previously used in_out_bitsream to avoid skipping rest of hasn't been consumed frames
     * @return fetched bytes count.
     */
#ifdef HAVE_ONEVPL
    virtual mfxStatus fetch_bitstream_data(std::shared_ptr<mfxBitstream> &in_out_bitsream) = 0;
#endif // HAVE_ONEVPL
    /**
     * The function is used by onevpl::GSource to check more binary data availability.
     *
     * It MUST return TRUE in case of EOF and NO_THROW exceptions.
     *
     * @return boolean value which detects end of stream
     */
    virtual bool empty() const = 0;
};
} // namespace onevpl
} // namespace wip
} // namespace gapi
} // namespace cv

#endif // GAPI_STREAMING_ONEVPL_ONEVPL_DATA_PROVIDER_INTERFACE_HPP
