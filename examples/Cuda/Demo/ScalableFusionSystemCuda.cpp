// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include <iostream>
#include <memory>

#include <Core/Core.h>
#include <IO/IO.h>
#include <Visualization/Visualization.h>

#include <Cuda/Integration/ScalableTSDFVolumeCuda.h>
#include <Cuda/Integration/ScalableMeshVolumeCuda.h>

#include "examples/Cuda/Utils.h"

std::tuple<double, double> ComputeStatistics(const std::vector<double> &vals) {
    double mean = 0;
    for (auto &val : vals) {
        mean += val;
    }
    mean /= double(vals.size());

    double std = 0;
    for (auto &val : vals) {
        std += (val - mean) * (val - mean);
    }
    std = std::sqrt(std / (vals.size() - 1));

    return std::make_tuple(mean, std);
}

int main(int argc, char *argv[]) {
    using namespace open3d;

//    SetVerbosityLevel(VerbosityLevel::VerboseDebug);
    std::string base_path =
//        "/home/wei/Work/data/tum/rgbd_dataset_freiburg3_long_office_household/";
//        "/home/wei/Work/data/tum/rgbd_dataset_freiburg2_desk/";
        "/home/wei/Work/data/stanford/burghers/";
//    "/media/wei/Data/data/redwood_simulated/livingroom1-clean/";

    auto camera_trajectory = CreatePinholeCameraTrajectoryFromFile(

        base_path + "/trajectory.log");
    auto rgbd_filenames = ReadDataAssociation(
        base_path + "/data_association.txt");

    int index = 0;
    int save_index = 0;
//    FPSTimer timer("Process RGBD stream",
//                   (int) camera_trajectory->parameters_.size());

    cuda::PinholeCameraIntrinsicCuda intrinsics(
        PinholeCameraIntrinsicParameters::PrimeSenseDefault);

    float voxel_length = 0.01f;
    cuda::TransformCuda extrinsics = cuda::TransformCuda::Identity();
    cuda::ScalableTSDFVolumeCuda<8> tsdf_volume(
        20000, 400000, voxel_length, 3 * voxel_length, extrinsics);

    Image depth, color;
    cuda::RGBDImageCuda rgbd(640, 480, 4.0f, 1000.0f);
    cuda::ScalableMeshVolumeCuda<8> mesher(
        120000, cuda::VertexWithNormalAndColor, 10000000, 20000000);

    Visualizer visualizer;
    if (!visualizer.CreateVisualizerWindow("ScalableFusion", 640, 480, 0, 0)) {
        PrintWarning("Failed creating OpenGL window.\n");
        return 0;
    }
    visualizer.BuildUtilities();
    visualizer.UpdateWindowTitle();

    std::shared_ptr<cuda::TriangleMeshCuda>
        mesh = std::make_shared<cuda::TriangleMeshCuda>();
    visualizer.AddGeometry(mesh);

    Timer timer;
    std::vector<double> times;
    for (int i = 0; i < rgbd_filenames.size() - 1; ++i) {
        PrintDebug("Processing frame %d ...\n", index);
        ReadImage(base_path + rgbd_filenames[i].first, depth);
        ReadImage(base_path + rgbd_filenames[i].second, color);
        rgbd.Upload(depth, color);

        /* Use ground truth trajectory */
        Eigen::Matrix4d extrinsic =
            camera_trajectory->parameters_[index].extrinsic_.inverse();

        extrinsics.FromEigen(extrinsic);
        tsdf_volume.Integrate(rgbd, intrinsics, extrinsics);

        timer.Start();
        mesher.MarchingCubes(tsdf_volume);
        timer.Stop();
        double time = timer.GetDuration();
        PrintInfo("%f ms\n", time);
        times.push_back(time);

        *mesh = mesher.mesh();
        visualizer.PollEvents();
        visualizer.UpdateGeometry();
        visualizer.GetViewControl().ConvertFromPinholeCameraParameters(
            camera_trajectory->parameters_[index]);
        index++;
//
//        if (index % 100 == 0) {
//            visualizer.CaptureScreenImage(std::to_string(index) + ".png");
//        }
    }

    double mean, std;
    std::tie(mean, std) = ComputeStatistics(times);
    PrintInfo("mean = %f, std = %f\n", mean, std);
    return 0;
}
