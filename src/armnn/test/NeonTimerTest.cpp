//
// Copyright © 2017 Arm Ltd. All rights reserved.
// See LICENSE file in the project root for full license information.
//

#include "NeonTimer.hpp"
#include "TensorHelpers.hpp"

#include "armnn/ArmNN.hpp"
#include "armnn/Tensor.hpp"
#include "armnn/TypesUtils.hpp"
#include "backends/CpuTensorHandle.hpp"
#include "backends/NeonWorkloadFactory.hpp"
#include "backends/WorkloadInfo.hpp"
#include "backends/WorkloadFactory.hpp"
#include "backends/test/LayerTests.hpp"
#include "backends/test/TensorCopyUtils.hpp"
#include "backends/test/WorkloadTestUtils.hpp"

#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <algorithm>

using namespace armnn;

BOOST_AUTO_TEST_SUITE(NeonTimerInstrument)


BOOST_AUTO_TEST_CASE(NeonTimerGetName)
{
    NeonTimer neonTimer;
    BOOST_CHECK_EQUAL(neonTimer.GetName(), "NeonKernelTimer");
}

BOOST_AUTO_TEST_CASE(NeonTimerMeasure)
{
    NeonWorkloadFactory workloadFactory;

    unsigned int inputWidth = 4000u;
    unsigned int inputHeight = 5000u;
    unsigned int inputChannels = 1u;
    unsigned int inputBatchSize = 1u;

    float upperBound = 1.0f;
    float lowerBound = -1.0f;

    size_t inputSize = inputWidth * inputHeight * inputChannels * inputBatchSize;
    std::vector<float> inputData(inputSize, 0.f);
    std::generate(inputData.begin(), inputData.end(), [](){
        return (static_cast<float>(rand()) / static_cast<float>(RAND_MAX / 3)) + 1.f; });

    unsigned int outputWidth = inputWidth;
    unsigned int outputHeight = inputHeight;
    unsigned int outputChannels = inputChannels;
    unsigned int outputBatchSize = inputBatchSize;

    armnn::TensorInfo inputTensorInfo({ inputBatchSize, inputChannels, inputHeight, inputWidth },
        armnn::GetDataType<float>());

    armnn::TensorInfo outputTensorInfo({ outputBatchSize, outputChannels, outputHeight, outputWidth },
        armnn::GetDataType<float>());

    LayerTestResult<float, 4> result(inputTensorInfo);

    auto input = MakeTensor<float, 4>(inputTensorInfo, inputData);

    std::unique_ptr<armnn::ITensorHandle> inputHandle = workloadFactory.CreateTensorHandle(inputTensorInfo);
    std::unique_ptr<armnn::ITensorHandle> outputHandle = workloadFactory.CreateTensorHandle(outputTensorInfo);

    // Setup bounded ReLu
    armnn::ActivationQueueDescriptor descriptor;
    armnn::WorkloadInfo workloadInfo;
    AddInputToWorkload(descriptor, workloadInfo, inputTensorInfo, inputHandle.get());
    AddOutputToWorkload(descriptor, workloadInfo, outputTensorInfo, outputHandle.get());

    descriptor.m_Parameters.m_Function = armnn::ActivationFunction::BoundedReLu;
    descriptor.m_Parameters.m_A = upperBound;
    descriptor.m_Parameters.m_B = lowerBound;

    std::unique_ptr<armnn::IWorkload> workload = workloadFactory.CreateActivation(descriptor, workloadInfo);

    inputHandle->Allocate();
    outputHandle->Allocate();

    CopyDataToITensorHandle(inputHandle.get(), &input[0][0][0][0]);

    NeonTimer neonTimer;
    // Start the timer.
    neonTimer.Start();
    // Execute the workload.
    workload->Execute();
    // Stop the timer.
    neonTimer.Stop();

    std::vector<Measurement> measurements = neonTimer.GetMeasurements();

    BOOST_CHECK_EQUAL(measurements.size(), 2);
    BOOST_CHECK_EQUAL(measurements[0].m_Name, "NeonKernelTimer/0: NEFillBorderKernel");
    BOOST_CHECK(measurements[0].m_Value > 0.0);
    BOOST_CHECK_EQUAL(measurements[1].m_Name, "NeonKernelTimer/1: NEActivationLayerKernel");
    BOOST_CHECK(measurements[1].m_Value > 0.0);
}

BOOST_AUTO_TEST_SUITE_END()
