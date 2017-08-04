/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2017, Intel Corporation, all rights reserved.
// Copyright (c) 2016-2017 Fabian David Tschopp, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#ifndef _OPENCV_LIBDNN_HPP_
#define _OPENCV_LIBDNN_HPP_
#include "../../precomp.hpp"
#include <iomanip>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "common.hpp"

using namespace cv;

#ifdef HAVE_OPENCL
struct LibDNNConvConfig
{
    LibDNNConvConfig() : in_shape(3, 1),
                         out_shape(3, 1),
                         kernel(1, 1),
                         pad(0, 0),
                         stride(1, 1),
                         dilation(1, 1)
    {}
    std::vector<int_tp> in_shape;
    std::vector<int_tp> out_shape;
    std::vector<int_tp> kernel;
    std::vector<int_tp> pad;
    std::vector<int_tp> stride;
    std::vector<int_tp> dilation;
    int_tp group = 1;
    bool bias_term = false;
    bool weights_backward = true;
    bool bias_backward = true;
    bool phase_test = true;
};

template<typename Dtype>
class LibDNNConvSpatial
{
    public:
        explicit LibDNNConvSpatial(LibDNNConvConfig config);
        ~LibDNNConvSpatial();
        bool Forward(const Dtype* bottom_data, const Dtype* weight,
                     const Dtype* bias,
                     Dtype* top_data, int_tp batch_size);

    private:
        struct kernelConfig
        {
            std::string kernelName;
            float executionTime;
            size_t local_work_size[3];
            size_t global_work_size[3];
            int_tp workItem_output[3];
            bool verified;
            bool autoTune;
            bool tested;
            bool swizzle_weights;
            bool use_null_local;
            int_tp kernelType;

            kernelConfig()
            {}

            kernelConfig(std::string name, size_t* global_size, size_t* local_size,
                         int_tp* workItem,
                         bool tune, bool swizzle, bool null_local,
                         int_tp type = 0)
            {
                kernelName = name;
                for (int_tp x = 0; x < 3; x++)
                {
                    local_work_size[x] = local_size[x];
                    global_work_size[x] = global_size[x];
                    workItem_output[x] = workItem[x];
                }
                autoTune = tune;
                swizzle_weights = swizzle;
                use_null_local = null_local;
                verified = false;
                tested = false;
                kernelType = type;
            }
        };

        template<class T>
        inline void addDef(std::stringstream& ss,  // NOLINT
                           const char* name, T value)
        {
            ss << "#ifdef " << name << std::endl;
            ss << "#undef " << name << std::endl;
            ss << "#endif" << std::endl;
            if (std::is_same<T, float>::value)
            {
                ss << "#define " << name << " (float) " << std::setprecision(32) << value
                   << std::endl;
            }
            else if (std::is_same<T, double>::value)
            {
                ss << "#define " << name << " (double) " << std::setprecision(32) << value
                   << std::endl;
            }
            else
            {
                ss << "#define " << name << " " << value << std::endl;
            }
        }

        template<class T>
        inline void addDef(std::stringstream& ss,  // NOLINT
                            const std::string name, T value)
        {
            addDef(ss, name.c_str(), value);
        }

        void tune(Dtype* top_data,
                  const Dtype* weight,
                  const Dtype* bias,
                  const Dtype* bottom_data,
                  int_tp batch_size);
        void generateKernelSrc();
        uint64 crc64(const uchar* data, size_t size, uint64 crc0 = 0);
        std::string generateHeader();
        std::string generateDefs();
        std::string generateKernels(int_tp kernelType,
                                    int_tp blockM,
                                    int_tp blockK,
                                    int_tp blockN);

        ocl::Program compileKernel();
        typedef std::map<std::string, ocl::Program> phash_t;
        phash_t phash;
        void calculateBenchmark(const Dtype* bottom,
                                const Dtype* w,
                                const Dtype* bias,
                                Dtype* verify_data);

        void setupConvolution(const Dtype *bottom,
                              Dtype *top,
                              const Dtype *verify_blob);
        void createConvolutionKernel(int_tp kernelType,
                                     int_tp blockWidth,
                                     int_tp blockHeight,
                                     int_tp blockDepth);
        bool setupIDLF(int_tp blockWidth,
                       int_tp blockHeight,
                       int_tp blockDepth);
        bool createBasicKernel(int_tp blockWidth,
                               int_tp blockHeight,
                               int_tp blockDepth);
        bool createGEMMLikeConvKernel(int_tp blockWidth,
                                      int_tp blockHeight,
                                      int_tp blockDepth);
        cl_int convolve(const Dtype *bottom,
                        const Dtype *top, int_tp index,
                        int_tp numImages,
                        kernelConfig* config);
        float timedConvolve(const Dtype *bottom,
                            const Dtype *top, int_tp index,
                            int_tp numImages,
                            kernelConfig* config);
        bool verifyResult(const Dtype *bottom,
                          Dtype *top,
                          int_tp index,
                          int_tp numImages,
                          const Dtype *verify_blob,
                          kernelConfig* config);
        bool tuneLocalSize(const Dtype *bottom,
                           const Dtype *top,
                           kernelConfig*);
        void swizzleWeights(const Dtype *bottom,
                            const Dtype *top,
                            int_tp swizzle_factor,
                            bool interleave = false);
        void generateKey();
        std::string generateSpecificKey(int_tp type, int_tp blockWidth,
                                          int_tp blockHeight,
                                          int_tp blockDepth);
        void computeGlobalSize(int_tp batch,
                               int_tp* workItemOutput,
                               size_t* localSizes,
                               size_t* globalSizes);
        bool loadCachedConfig();
        void prepareKernel();
        void setBufferKernelArg(const Dtype *bottom,
                                const Dtype *top,
                                ocl::Kernel *cl_kernel,
                                const cl_uint &argIdx,
                                ocl::Context *ctx,
                                cl_mem buffer, size_t offset,
                                size_t size, bool readOnly,
                                bool preserved);
        void cleanTmpSubBuffers(const Dtype *bottom,
                                const Dtype *top);

        int_tp group_;
        bool bias_term_;
        std::map<std::tuple<cl_mem, size_t, size_t>, cl_mem> subBufferMap;
        std::vector<cl_mem> tmpSubBuffers;
        const Dtype* bottom_data_;
        Dtype* top_data_;
        const Dtype* weight_;
        Dtype* swizzled_weights_;
        const Dtype* bias_;
        int_tp bottom_index_;
        int_tp output_h_;
        int_tp output_w_;
        int_tp kernel_h_;
        int_tp kernel_w_;
        int_tp height_;
        int_tp width_;
        int_tp pad_h_;
        int_tp pad_w_;
        int_tp stride_h_;
        int_tp stride_w_;
        int_tp dilation_h_;
        int_tp dilation_w_;

        /// M_ is the channel dimension of the output for a single group, which is the
        /// leading dimension of the filter matrix.
        int_tp M_;
        /// K_ is the dimension of an unrolled input for a single group, which is the
        /// leading dimension of the data matrix.
        int_tp K_;
        /// N_ is the spatial dimension of the output, the H x W, which are the last
        /// dimensions of the data and filter matrices.
        int_tp N_;

        bool tuned_;
        std::string key_;
        std::string short_key_;
        std::string kernel_name_;
        std::stringstream cache_path_;
        int_tp kernel_index_;
        std::vector<kernelConfig*> kernelQueue;
        kernelConfig* bestKernelConfig;

        int_tp bottom_dim_;
        int_tp top_dim_;
        int_tp num_;
        int_tp channels_;
        int_tp out_spatial_dim_;
        int_tp num_output_;
        int_tp kernel_dim_;

        int_tp kernelType_;
        int_tp blockM_;
        int_tp blockK_;
        int_tp blockN_;
        std::string options_;
        std::string kernel_;
        int_tp prev_kernel_type_;
};

typedef enum {
    LIBDNN_POOLING_METHOD_MAX                 = 0,
    LIBDNN_POOLING_METHOD_AVE                 = 1,
    LIBDNN_POOLING_METHOD_STO                 = 2
} libdnnPoolingMethod_t;

struct LibDNNPoolConfig
{
    LibDNNPoolConfig() : in_shape(3, 1),
                         out_shape(3, 1),
                         kernel(1, 1),
                         pad(0, 0),
                         stride(1, 1),
                         dilation(1, 1)
    {}
    std::vector<int_tp> in_shape;
    std::vector<int_tp> out_shape;
    std::vector<int_tp> kernel;
    std::vector<int_tp> pad;
    std::vector<int_tp> stride;
    std::vector<int_tp> dilation;

    int_tp channels;
    libdnnPoolingMethod_t pool_method = LIBDNN_POOLING_METHOD_MAX;
    bool global_pooling = false;
};

template<typename Dtype>
class LibDNNPool
{
    public:
        explicit LibDNNPool(LibDNNPoolConfig config);
        ~LibDNNPool();
        bool Forward(const Dtype *bottom_data,
                     Dtype *top_data,
                     Dtype *top_mask = NULL);

    private:
        cl_mem mask_idx_;

        // Pooling parameters
        std::vector<int_tp> pad_;
        std::vector<int_tp> stride_;
        std::vector<int_tp> kernel_shape_;
        std::vector<int_tp> im_in_shape_;
        std::vector<int_tp> im_out_shape_;

        libdnnPoolingMethod_t pool_method_;
        int_tp count_;
        int_tp batch_size_;
        int_tp channels_;
        int_tp kernel_h_;
        int_tp kernel_w_;
        int_tp stride_h_;
        int_tp stride_w_;
        int_tp pad_h_;
        int_tp pad_w_;
        int_tp height_;
        int_tp width_;
        int_tp pooled_height_;
        int_tp pooled_width_;
};

struct LibDNNInnerProductConfig
{
    int_tp num_output;
    int_tp M;
    int_tp K;
    bool bias_term;
    bool transpose = false;
    bool phase_test = true;
};

template<typename Dtype>
class LibDNNInnerProduct
{
    public:
        explicit LibDNNInnerProduct(LibDNNInnerProductConfig config);
        bool Forward(const Dtype* bottom_data,
                     const Dtype* weight,
                     const Dtype* bias,
                     Dtype* top_data);
    private:
        LibDNNInnerProductConfig config_;
        int_tp axis_;
        int_tp num_output_;
        int_tp M_;
        int_tp N_;
        int_tp K_;
        bool bias_term_;
        bool transpose_;
        bool image_copied_;
        bool phase_test_;
        Dtype *bias_multiplier_;
        Dtype *weight_image_;
};

typedef enum {
    LRNParameter_NormRegion_ACROSS_CHANNELS = 0,
    LRNParameter_NormRegion_WITHIN_CHANNEL = 1
} LRNParameter_NormRegion_WITHIN_CHANNEL_t;

struct LibDNNLRNConfig
{
    LibDNNLRNConfig()
    {}
    std::vector<int_tp> in_shape;
    LRNParameter_NormRegion_WITHIN_CHANNEL_t lrn_type;
    bool phase_test = true;
    int_tp local_size;
    float alpha;
    float beta;
    float k;
    bool norm_by_size;
    int_tp batch_size;
    int_tp channels;
    int_tp height;
    int_tp width;
};

template<typename Dtype>
class LibDNNLRN
{
    public:
        explicit LibDNNLRN(LibDNNLRNConfig config);
        bool Forward(const Dtype* bottom_data, Dtype* top_data);

    private:
        void crossChannelForward(const Dtype* bottom_data, Dtype* top_data);
        LRNParameter_NormRegion_WITHIN_CHANNEL_t lrn_type_;
        bool phase_test_;
        int_tp size_;
        Dtype alpha_;
        Dtype beta_;
        Dtype k_;
        int_tp num_;
        int_tp channels_;
        int_tp height_;
        int_tp width_;
        bool norm_by_size_;
};

struct LibDNNSoftmaxConfig
{
    LibDNNSoftmaxConfig()
    {}
    std::vector<int_tp> in_shape;
    int_tp axis;
    int_tp channels;
};

template<typename Dtype>
class LibDNNSoftmax
{
    public:
        explicit LibDNNSoftmax(LibDNNSoftmaxConfig config);
        ~LibDNNSoftmax();
        bool Forward(const Dtype* bottom_data, Dtype* top_data);

    private:
        int_tp softmax_axis_;
        int_tp inner_num_;
        int_tp outer_num_;
        int_tp channels_;
        int_tp count_;
        bool use_slm_;
        Dtype *scale_data_;
};
#endif // HAVE_OPENCL
#endif