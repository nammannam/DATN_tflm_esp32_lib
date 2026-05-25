/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/kernels/internal/common.h"
#include "tensorflow/lite/kernels/internal/quantization_util.h"
#include "tensorflow/lite/kernels/internal/reference/fully_connected.h"
#include "tensorflow/lite/kernels/internal/reference/integer_ops/fully_connected.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/micro/kernels/fully_connected.h"
#include "tensorflow/lite/micro/kernels/kernel_util.h"

namespace tflite {

const int kFullyConnectedInputTensor = 0;
const int kFullyConnectedWeightsTensor = 1;
const int kFullyConnectedBiasTensor = 2;
const int kFullyConnectedOutputTensor = 0;

FullyConnectedParams FullyConnectedParamsQuantized(
    const OpDataFullyConnected& op_data) {
  FullyConnectedParams op_params;
  op_params.input_offset = -op_data.input_zero_point;
  op_params.weights_offset = -op_data.filter_zero_point;
  op_params.output_offset = op_data.output_zero_point;
  op_params.output_multiplier = op_data.output_multiplier;
  op_params.output_shift = op_data.output_shift;
  op_params.quantized_activation_min = op_data.output_activation_min;
  op_params.quantized_activation_max = op_data.output_activation_max;
  return op_params;
}

FullyConnectedParams FullyConnectedParamsFloat(
    TfLiteFusedActivation activation) {
  FullyConnectedParams op_params;
  CalculateActivationRange(activation, &op_params.float_activation_min,
                           &op_params.float_activation_max);
  return op_params;
}

// Editor: Nam Nguyen - 22/05/2026
// This function calculates the quantization parameters for a fully connected layer, 
// including the per-channel multipliers 
// and shifts if the filter tensor is quantized with per-channel quantization. 
// It also calculates the activation range for the fused activation function.
TfLiteStatus CalculateOpDataFullyConnected(
    TfLiteContext* context, TfLiteFusedActivation activation,
    TfLiteType data_type, const TfLiteTensor* input, const TfLiteTensor* filter,
    const TfLiteTensor* bias, TfLiteTensor* output,
    OpDataFullyConnected* data) {
  // Support per-channel affine quantization for FullyConnected.
  // For per-channel quantized filters, the kernel computes a separate
  // multiplier and shift for each output channel.
  bool is_per_channel = false;
  TfLiteAffineQuantization* affine_quantization = nullptr;
  if (filter->quantization.type == kTfLiteAffineQuantization &&
      filter->quantization.params != nullptr) {
    affine_quantization = reinterpret_cast<TfLiteAffineQuantization*>(
        filter->quantization.params);
    TF_LITE_ENSURE(context, affine_quantization->scale);
    if (affine_quantization->scale->size > 1) {
      const int num_channels = filter->dims->data[
          affine_quantization->quantized_dimension];
      TF_LITE_ENSURE_EQ(context, affine_quantization->scale->size,
                        num_channels);
      if (affine_quantization->zero_point != nullptr) {
        TF_LITE_ENSURE_EQ(context, affine_quantization->zero_point->size,
                          num_channels);
        for (int i = 0; i < num_channels; ++i) {
          TF_LITE_ENSURE_EQ(context, affine_quantization->zero_point->data[i],
                            0);
        }
      }
      is_per_channel = true;
    }
  }

  if (data_type != kTfLiteFloat32) {
    if (is_per_channel) {
      const int num_channels = filter->dims->data[
          affine_quantization->quantized_dimension];
      data->per_channel_output_multiplier =
          static_cast<int32_t*>(
              context->AllocatePersistentBuffer(
                  context, num_channels * sizeof(int32_t)));
      data->per_channel_output_shift =
          static_cast<int32_t*>(
              context->AllocatePersistentBuffer(
                context, num_channels * sizeof(int32_t)));
      TF_LITE_ENSURE(context, data->per_channel_output_multiplier != nullptr);
      TF_LITE_ENSURE(context, data->per_channel_output_shift != nullptr);
      TF_LITE_ENSURE_STATUS(
          PopulateConvolutionQuantizationParams(
              context, input, filter, bias, output, activation,
              &data->output_multiplier, &data->output_shift,
              &data->output_activation_min, &data->output_activation_max,
              data->per_channel_output_multiplier,
              data->per_channel_output_shift, num_channels));
    } else {
      double real_multiplier = 0.0;
      TF_LITE_ENSURE_STATUS(GetQuantizedConvolutionMultipler(
          context, input, filter, bias, output, &real_multiplier));
      QuantizeMultiplier(real_multiplier, &data->output_multiplier,
                         &data->output_shift);

      // Filter weights will always be symmetric quantized since we only support
      // int8 quantization. See
      // https://github.com/tensorflow/tensorflow/issues/44912 for additional
      // context.
      TFLITE_DCHECK(filter->params.zero_point == 0);
    }

    data->input_zero_point = input->params.zero_point;
    data->filter_zero_point = filter->params.zero_point;
    data->output_zero_point = output->params.zero_point;

    return CalculateActivationRangeQuantized(context, activation, output,
                                             &data->output_activation_min,
                                             &data->output_activation_max);
  }
  return kTfLiteOk;
}

}  // namespace tflite
