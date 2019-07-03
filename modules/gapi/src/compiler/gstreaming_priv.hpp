// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2019 Intel Corporation


#ifndef OPENCV_GAPI_GSTREAMING_COMPILED_PRIV_HPP
#define OPENCV_GAPI_GSTREAMING_COMPILED_PRIV_HPP

#include <memory> // unique_ptr
#include "executor/gstreamingexecutor.hpp"

namespace cv {

namespace gimpl
{
    struct GRuntimeArgs;
};

// FIXME: GAPI_EXPORTS is here only due to tests and Windows linker issues
class GAPI_EXPORTS GStreamingCompiled::Priv
{
    GMetaArgs  m_metas;    // passed by user
    GMetaArgs  m_outMetas; // inferred by compiler
    std::unique_ptr<cv::gimpl::GStreamingExecutor> m_exec;

    void checkArgs(const cv::gimpl::GRuntimeArgs &args) const;

public:
    void setup(const GMetaArgs &metaArgs,
               const GMetaArgs &outMetas,
               std::unique_ptr<cv::gimpl::GStreamingExecutor> &&pE);
    bool isEmpty() const;

    const GMetaArgs& metas() const;
    const GMetaArgs& outMetas() const;

    void setSource(GRunArgs &&args);
    void start();
    bool pull(cv::GRunArgsP &&outs);
    void stop();
};

} // namespace cv

#endif // OPENCV_GAPI_GSTREAMING_COMPILED_PRIV_HPP
